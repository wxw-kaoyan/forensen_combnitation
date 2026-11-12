// ============================================================================
//  nav/rtk/core/rtk_engine.cpp
//  RTK 主引擎实现：双差 EKF + LAMBDA 固定
// ============================================================================
#include "rtk_engine.hpp"

#include <cmath>
#include <set>

#include "ambiguity.hpp"
#include "atmosphere.hpp"
#include "ephemeris.hpp"
#include "rtk_util.hpp"
#include "spp_solver.hpp"

namespace nav {
namespace rtk {

namespace {

constexpr int POS0 = 0;
constexpr int VEL0 = 3;
constexpr int AMB0 = 6;

inline double wavelength(int f)
{
    return (f == 0) ? LAM1 : LAM2;
}

} // namespace

// ════════════════════════════════════════════════════════════════════
RtkEngine::RtkEngine(const RtkConfig &cfg)
{
    cfg_ = cfg;
    reset();
}

void RtkEngine::reset()
{
    x_ = Eigen::VectorXd::Zero(AMB0);
    P_ = Eigen::MatrixXd::Zero(AMB0, AMB0);

    P_.block<3, 3>(POS0, POS0) = Eigen::Matrix3d::Identity() * cfg_.initPosStd * cfg_.initPosStd;
    P_.block<3, 3>(VEL0, VEL0) = Eigen::Matrix3d::Identity() * cfg_.initVelStd * cfg_.initVelStd;

    ambIdx_.clear();
    ambKeys_.clear();
    ambValue_.clear();
    ambVar_.clear();

    lastTime_ = GTime();
    hasLast_  = false;
    lastPos_.setZero();

    sol_ = Sol();
    ssat_.assign(MAXSAT, SatStat());
}

void RtkEngine::setBasePosition(const Vector3d &baseEcef)
{
    basePos_   = baseEcef;
    baseKnown_ = baseEcef.norm() > 1.0;
}

// ════════════════════════════════════════════════════════════════════
//  状态重建：按本历元可见卫星生成模糊度列
// ════════════════════════════════════════════════════════════════════
void RtkEngine::syncAmbiguities(const std::vector<int> &visibleSats,
                                const std::vector<double> &initCycle,
                                int nf)
{
    // 新键表
    std::vector<AmbKey> newKeys;
    for (size_t i = 0; i < visibleSats.size(); ++i) {
        for (int f = 0; f < nf; ++f) {
            newKeys.push_back(AmbKey(visibleSats[i], f));
        }
    }

    const int na = static_cast<int>(newKeys.size());
    if (na == 0) {
        // 没有可用模糊度：只保留 pos/vel
        x_.conservativeResize(AMB0);
        P_.conservativeResize(AMB0, AMB0);
        ambKeys_.clear();
        ambIdx_.clear();
        return;
    }

    const int oldNx = static_cast<int>(x_.size());
    const int newNx = AMB0 + na;

    Eigen::VectorXd xn = Eigen::VectorXd::Zero(newNx);
    Eigen::MatrixXd Pn = Eigen::MatrixXd::Zero(newNx, newNx);

    // 位置 / 速度直接搬
    xn.head(AMB0) = x_.head(AMB0);
    Pn.topLeftCorner(AMB0, AMB0) = P_.topLeftCorner(AMB0, AMB0);

    // 模糊度按 key 迁移
    std::map<AmbKey, int> newIdx;
    for (int k = 0; k < na; ++k) {
        const AmbKey &key = newKeys[k];
        newIdx[key] = AMB0 + k;

        auto oldIt = ambIdx_.find(key);
        if (oldIt != ambIdx_.end() && oldIt->second < oldNx) {
            // 已有：值 + 全协方差行搬过来
            const int oi = oldIt->second;
            xn[AMB0 + k] = x_[oi];
            for (int r = 0; r < AMB0; ++r) {
                Pn(AMB0 + k, r) = P_(oi, r);
                Pn(r, AMB0 + k) = P_(r, oi);
            }
            for (int j = 0; j < na; ++j) {
                auto oit = ambIdx_.find(newKeys[j]);
                if (oit != ambIdx_.end() && oit->second < oldNx) {
                    Pn(AMB0 + k, AMB0 + j) = P_(oi, oit->second);
                }
            }
        } else {
            // 新增：用相位残差初值
            double init = 0.0;
            for (size_t s = 0; s < visibleSats.size(); ++s) {
                if (visibleSats[s] == key.first) {
                    const size_t idx = s * nf + key.second;
                    if (idx < initCycle.size())
                        init = initCycle[idx];
                    break;
                }
            }
            xn[AMB0 + k] = init;

            const double lam = wavelength(key.second);
            Pn(AMB0 + k, AMB0 + k) =
                cfg_.initBiasStd * cfg_.initBiasStd / (lam * lam);
        }
    }

    x_       = xn;
    P_       = Pn;
    ambKeys_ = newKeys;
    ambIdx_  = newIdx;
}

// ════════════════════════════════════════════════════════════════════
//  时间更新（位置 / 速度 / 模糊度随机游走）
// ════════════════════════════════════════════════════════════════════
void RtkEngine::temporalUpdate(double dt)
{
    if (dt <= 0.0 || !std::isfinite(dt))
        return;

    const double qp = cfg_.posProcStd * cfg_.posProcStd * dt;
    const double qv = cfg_.velProcStd * cfg_.velProcStd * dt;

    for (int i = 0; i < 3; ++i) {
        P_(POS0 + i, POS0 + i) += qp;
        P_(VEL0 + i, VEL0 + i) += qv;
    }

    const int na = static_cast<int>(ambKeys_.size());
    for (int k = 0; k < na; ++k) {
        const double lam = wavelength(ambKeys_[k].second);
        const double s   = cfg_.biasProcStd * dt / lam;
        P_(AMB0 + k, AMB0 + k) += s * s;
    }
}

// ════════════════════════════════════════════════════════════════════
//  单点定位
// ════════════════════════════════════════════════════════════════════
Sol RtkEngine::processSingle(const std::vector<Obs> &roverObs, const Nav &nav)
{
    Sol sol;
    if (roverObs.empty())
        return sol;

    sol.time = roverObs.front().time;

    Vector3d guess = Vector3d::Zero();
    if (x_.size() >= 3 && x_.head<3>().norm() > 1.0)
        guess = x_.head<3>();

    const SppResult r = spp(roverObs, nav, cfg_, guess);
    if (!r.ok)
        return sol;

    sol.rr   = r.rr;
    sol.qr   = r.qr;
    sol.ns   = r.ns;
    sol.stat = SOLQ_SINGLE;

    if (x_.size() < AMB0)
        x_ = Eigen::VectorXd::Zero(AMB0);
    x_.head<3>() = r.rr;

    return sol;
}

// ════════════════════════════════════════════════════════════════════
//  主流程
// ════════════════════════════════════════════════════════════════════
Sol RtkEngine::processEpoch(const std::vector<Obs> &roverObs,
                            const std::vector<Obs> &baseObs,
                            const Nav &nav)
{
    Sol sol;
    if (roverObs.empty())
        return sol;

    sol.time = roverObs.front().time;
    ssat_.assign(MAXSAT, SatStat());

    const int nu = static_cast<int>(roverObs.size());
    const int nr = static_cast<int>(baseObs.size());
    if (nu < 4 || nr < 4)
        return processSingle(roverObs, nav);

    // 频点数按实际数据确定：只有存在 L2/P2 才用双频
    int nf = 1;
    for (const Obs &o : roverObs) {
        if (o.L[1] != 0.0 || o.P[1] != 0.0) { nf = 2; break; }
    }
    if (cfg_.ionoopt == IONOOPT_IFLC)
        nf = 1;   // 无电离层组合只有一路

    // ── 1. 卫星位置 ──
    SatPossResult spu, spr;
    satposs(sol.time, roverObs, nav, spu);
    satposs(baseObs.empty() ? sol.time : baseObs.front().time, baseObs, nav, spr);

    // ── 2. 基站坐标 ──
    if (!baseKnown_) {
        const SppResult rb = spp(baseObs, nav, cfg_);
        if (!rb.ok)
            return processSingle(roverObs, nav);
        basePos_   = rb.rr;
        baseKnown_ = true;
    }

    // ── 3. 基站索引映射 ──
    std::map<int, int> baseIdx;
    for (int j = 0; j < nr; ++j)
        baseIdx[baseObs[j].sat] = j;

    // 流动站线性化点
    if (x_.size() < AMB0) {
        x_ = Eigen::VectorXd::Zero(AMB0);
        P_ = Eigen::MatrixXd::Zero(AMB0, AMB0);
        P_.block<3, 3>(POS0, POS0) =
            Eigen::Matrix3d::Identity() * cfg_.initPosStd * cfg_.initPosStd;
        P_.block<3, 3>(VEL0, VEL0) =
            Eigen::Matrix3d::Identity() * cfg_.initVelStd * cfg_.initVelStd;
    }

    Vector3d rU = x_.head<3>();
    if (rU.norm() < 1.0) {
        const SppResult r = spp(roverObs, nav, cfg_);
        if (!r.ok)
            return sol;
        rU = r.rr;
        x_.head<3>() = rU;
    }
    const Vector3d rB = basePos_;

    Vector3d posU, posB;
    ecef2pos(rU, posU);
    ecef2pos(rB, posB);

    // ── 4. 每颗卫星的几何量 / 无差残差 ──
    //   y_phase = L·λ − r ,  y_code = P − r
    struct SatGeom {
        int      sat = 0;
        int      iu = -1, ir = -1;
        double   el = 0.0;
        Vector3d eU = Vector3d::Zero();
        double   yu[NFREQ * 2] = {0.0};   // 流动站：前 nf 相位，后 nf 伪距
        double   yr[NFREQ * 2] = {0.0};   // 基站
        bool     ok[NFREQ] = {false, false};
        bool     okP[NFREQ] = {false, false};
    };

    std::vector<SatGeom> geoms;
    geoms.reserve(nu);

    for (int i = 0; i < nu; ++i) {
        auto it = baseIdx.find(roverObs[i].sat);
        if (it == baseIdx.end())
            continue;
        const int j = it->second;

        if (spu.rs[i].norm() <= 0.0 || spr.rs[j].norm() <= 0.0)
            continue;
        if (spu.svh[i] > 0 || spr.svh[j] > 0)
            continue;

        SatGeom g;
        g.sat = roverObs[i].sat;
        g.iu  = i;
        g.ir  = j;

        Vector3d eU, eR;
        const double ru = geodist(spu.rs[i], rU, eU);
        const double rb = geodist(spr.rs[j], rB, eR);
        if (ru <= 0.0 || rb <= 0.0)
            continue;

        double azel[2];
        const double el = satazel(posU, eU, azel);
        if (el < cfg_.elmin)
            continue;

        g.el = el;
        g.eU = eU;

        const int idx = g.sat - 1;
        if (idx < 0 || idx >= MAXSAT)
            continue;

        // 大气改正（基站处电离层近似用流动站位置，双差后被抵消大部分）
        double ionU = 0.0, ionR = 0.0;
        if (cfg_.ionoopt == IONOOPT_BRDC) {
            ionU = ionmodel(roverObs[i].time, nav.ion, posU, azel);
            ionR = ionmodel(baseObs[j].time, nav.ion, posB, azel);
        }

        double azelR[2];
        satazel(posB, eR, azelR);

        double trpU = 0.0, trpR = 0.0;
        if (cfg_.tropopt == TROPOPT_SAAS) {
            trpU = tropmodel(roverObs[i].time, posU, azel);
            trpR = tropmodel(baseObs[j].time, posB, azelR);
        }

        // 钟差改正后的几何距离（基站与流动站各自减掉卫星钟差）
        const double corrU = ru - CLIGHT * spu.dts[i] + ionU + trpU;
        const double corrR = rb - CLIGHT * spr.dts[j] + ionR + trpR;

        for (int f = 0; f < nf; ++f) {
            const double lam = nav.lam[idx][f] > 0.0 ? nav.lam[idx][f] : wavelength(f);

            if (cfg_.ionoopt == IONOOPT_IFLC) {
                if (f != 0) break;
                if (roverObs[i].L[0] != 0.0 && roverObs[i].L[1] != 0.0 &&
                    baseObs[j].L[0] != 0.0 && baseObs[j].L[1] != 0.0) {
                    const double f1 = FREQ1, f2 = FREQ2;
                    const double C1 = (f1 * f1) / (f1 * f1 - f2 * f2);
                    const double C2 = -(f2 * f2) / (f1 * f1 - f2 * f2);
                    const double Lu = C1 * roverObs[i].L[0] * LAM1 + C2 * roverObs[i].L[1] * LAM2;
                    const double Lr = C1 * baseObs[j].L[0] * LAM1 + C2 * baseObs[j].L[1] * LAM2;
                    g.yu[0] = Lu - corrU;
                    g.yr[0] = Lr - corrR;
                    g.ok[0] = true;
                }
                if (roverObs[i].P[0] != 0.0 && roverObs[i].P[1] != 0.0 &&
                    baseObs[j].P[0] != 0.0 && baseObs[j].P[1] != 0.0) {
                    const double f1 = FREQ1, f2 = FREQ2;
                    const double C1 = (f1 * f1) / (f1 * f1 - f2 * f2);
                    const double C2 = -(f2 * f2) / (f1 * f1 - f2 * f2);
                    g.yu[NFREQ] = C1 * roverObs[i].P[0] + C2 * roverObs[i].P[1] - corrU;
                    g.yr[NFREQ] = C1 * baseObs[j].P[0] + C2 * baseObs[j].P[1] - corrR;
                    g.okP[0] = true;
                }
                break;
            }

            if (roverObs[i].L[f] != 0.0 && baseObs[j].L[f] != 0.0) {
                g.yu[f]      = roverObs[i].L[f] * lam - corrU;
                g.yr[f]      = baseObs[j].L[f] * lam - corrR;
                g.ok[f]      = true;
            }
            if (roverObs[i].P[f] != 0.0 && baseObs[j].P[f] != 0.0) {
                g.yu[NFREQ + f] = roverObs[i].P[f] - corrU;
                g.yr[NFREQ + f] = baseObs[j].P[f] - corrR;
                g.okP[f]        = true;
            }
        }

        geoms.push_back(g);
    }

    if (geoms.empty())
        return processSingle(roverObs, nav);

    // ── 5. 同步模糊度状态 ──
    std::vector<int>    visSats;
    std::vector<double> initCycle;
    for (const SatGeom &g : geoms) {
        visSats.push_back(g.sat);
        for (int f = 0; f < nf; ++f) {
            double init = 0.0;
            if (g.ok[f]) {
                const double lam = nav.lam[g.sat - 1][f] > 0.0
                                       ? nav.lam[g.sat - 1][f]
                                       : wavelength(f);
                init = (g.yu[f] - g.yr[f]) / lam;   // 单差相位残差（周）
            }
            initCycle.push_back(init);
        }
    }
    syncAmbiguities(visSats, initCycle, nf);

    // 时间更新
    const double dt = hasLast_ ? timeDiff(sol.time, lastTime_) : 0.0;
    temporalUpdate(dt);
    x_.head<3>() = rU;   // 线性化点

    // ── 6. 构造双差观测 ──
    std::vector<double>    v;
    std::vector<Eigen::VectorXd> hrows;
    std::vector<double>    sig;
    int refSatPerFreq[NFREQ] = {0, 0};

    const int nx = static_cast<int>(x_.size());

    for (int f = 0; f < nf; ++f) {
        // 参考星：高度角最高
        int iref = -1;
        double elMax = 0.0;
        for (size_t k = 0; k < geoms.size(); ++k) {
            if (!geoms[k].ok[f])
                continue;
            if (geoms[k].el > elMax) {
                elMax = geoms[k].el;
                iref  = static_cast<int>(k);
            }
        }
        if (iref < 0)
            continue;

        const SatGeom &gr = geoms[iref];
        refSatPerFreq[f] = gr.sat;
        const int nRef  = ambIndex(gr.sat, f);
        if (nRef < 0)
            continue;

        const double lamRef = nav.lam[gr.sat - 1][f] > 0.0
                                  ? nav.lam[gr.sat - 1][f] : wavelength(f);

        for (size_t k = 0; k < geoms.size(); ++k) {
            if (static_cast<int>(k) == iref || !geoms[k].ok[f])
                continue;

            const SatGeom &gj = geoms[k];
            const int nJ = ambIndex(gj.sat, f);
            if (nJ < 0)
                continue;

            const double lamJ = nav.lam[gj.sat - 1][f] > 0.0
                                    ? nav.lam[gj.sat - 1][f] : wavelength(f);

            // v = (yu_i − yr_i) − (yu_j − yr_j) − (λ_i N_i − λ_j N_j)
            double vi = (gr.yu[f] - gr.yr[f]) - (gj.yu[f] - gj.yr[f]);
            vi -= lamRef * x_[nRef] - lamJ * x_[nJ];

            const double s = 2.0 * std::pow(obsSigma(cfg_, gr.el, true), 2.0);

            Eigen::VectorXd h = Eigen::VectorXd::Zero(nx);
            h[POS0 + 0] = -gr.eU[0] + gj.eU[0];
            h[POS0 + 1] = -gr.eU[1] + gj.eU[1];
            h[POS0 + 2] = -gr.eU[2] + gj.eU[2];
            h[nRef] =  lamRef;
            h[nJ]   = -lamJ;

            v.push_back(vi);
            hrows.push_back(h);
            sig.push_back(std::sqrt(s));

            if (cfg_.maxinno > 0.0 && std::fabs(vi) > cfg_.maxinno)
                v.back() = 1e6;   // 强降权（大新息）
        }

        // 伪距双差
        for (size_t k = 0; k < geoms.size(); ++k) {
            if (static_cast<int>(k) == iref || !geoms[k].okP[f])
                continue;

            const SatGeom &gj = geoms[k];
            if (!gr.okP[f])
                break;

            const double vi = (gr.yu[NFREQ + f] - gr.yr[NFREQ + f]) -
                              (gj.yu[NFREQ + f] - gj.yr[NFREQ + f]);

            const double s = 2.0 * std::pow(obsSigma(cfg_, gr.el, false), 2.0);

            Eigen::VectorXd h = Eigen::VectorXd::Zero(nx);
            h[POS0 + 0] = -gr.eU[0] + gj.eU[0];
            h[POS0 + 1] = -gr.eU[1] + gj.eU[1];
            h[POS0 + 2] = -gr.eU[2] + gj.eU[2];

            v.push_back(vi);
            hrows.push_back(h);
            sig.push_back(std::sqrt(s));
        }
    }

    const int nv = static_cast<int>(v.size());
    if (nv < 4) {
        return processSingle(roverObs, nav);
    }

    // ── 7. EKF 更新 ──
    Eigen::MatrixXd H(nv, nx);
    Eigen::VectorXd z(nv);
    for (int i = 0; i < nv; ++i) {
        const double w = 1.0 / sig[i];
        H.row(i) = hrows[i].transpose() * w;
        z[i]     = v[i] * w;
    }

    const Eigen::MatrixXd S = H * P_ * H.transpose() + Eigen::MatrixXd::Identity(nv, nv);
    const Eigen::MatrixXd PHt = P_ * H.transpose();

    // 用 LDLT 解 S
    const Eigen::MatrixXd K = PHt * S.ldlt().solve(Eigen::MatrixXd::Identity(nv, nv));

    x_ += K * z;

    const Eigen::MatrixXd IKH = Eigen::MatrixXd::Identity(nx, nx) - K * H;
    P_ = IKH * P_ * IKH.transpose() + K * K.transpose();   // R 已归一化为 I

    sol.stat = SOLQ_FLOAT;
    sol.rr   = x_.head<3>();
    sol.ns   = static_cast<int>(geoms.size());

    // ── 7.5 基准星模糊度归零 ──
    // 单差模糊度之间存在不可观测的共同偏移（会被双差消掉），若原样送进 LAMBDA，
    // Qbb 在该方向奇异。按 RTKLIB 做法：把基准星模糊度固定为 0，其余取相对值。
    //   N_i' = N_i − N_ref  (i ≠ ref)，N_ref' = 0
    // 该变换是状态空间的线性重参数化，协方差同步做 T·P·Tᵀ。
    for (int f = 0; f < nf; ++f) {
        const int refSat = refSatPerFreq[f];
        if (refSat == 0)
            continue;
        const int ir = ambIndex(refSat, f);
        if (ir < 0)
            continue;

        // 该频率下所有模糊度索引
        std::vector<int> idxList;
        for (size_t k = 0; k < ambKeys_.size(); ++k)
            if (ambKeys_[k].second == f)
                idxList.push_back(static_cast<int>(AMB0 + k));

        for (int j : idxList)
            if (j != ir)
                x_[j] -= x_[ir];
        for (int j : idxList)
            if (j != ir)
                P_.row(j) -= P_.row(ir);
        for (int j : idxList)
            if (j != ir)
                P_.col(j) -= P_.col(ir);

        x_[ir] = 0.0;
        P_.row(ir).setZero();
        P_.col(ir).setZero();
    }

    // ── 8. LAMBDA 固定 ──
    // 只取非基准星的模糊度作为整数估计对象
    std::vector<int> fixIdx;
    for (int f = 0; f < nf; ++f) {
        const int refSat = refSatPerFreq[f];
        for (size_t k = 0; k < ambKeys_.size(); ++k) {
            if (ambKeys_[k].second != f)
                continue;
            if (ambKeys_[k].first == refSat)
                continue;
            fixIdx.push_back(static_cast<int>(AMB0 + k));
        }
    }

    const int na = static_cast<int>(fixIdx.size());
    if (na >= 4 && sol.ns >= cfg_.minFixSats + 1) {

        Eigen::VectorXd a(na);
        for (int k = 0; k < na; ++k)
            a[k] = x_[fixIdx[k]];

        Eigen::MatrixXd Qbb(na, na);
        for (int r = 0; r < na; ++r)
            for (int c = 0; c < na; ++c)
                Qbb(r, c) = P_(fixIdx[r], fixIdx[c]);

        const LambdaResult lr = lambda(na, 2, a, Qbb);

        if (lr.ok && lr.ratio > cfg_.ratioThreshold) {
            sol.ratio = lr.ratio;

            // 条件解：x_fixed = x + Q_xb · Qbb^-1 · (N_fix − N_float)
            const Eigen::VectorXd dN = lr.fixed0 - a;

            Eigen::MatrixXd QxbFull(AMB0, na);
            for (int r = 0; r < AMB0; ++r)
                for (int c = 0; c < na; ++c)
                    QxbFull(r, c) = P_(r, fixIdx[c]);

            const Eigen::VectorXd dxs = QxbFull * Qbb.ldlt().solve(dN);
            const Vector3d rFixed = x_.head<3>() + dxs.head<3>();

            sol.rr   = rFixed;
            sol.stat = SOLQ_FIX;
            sol.qr[0] = std::sqrt(std::fabs(P_(0, 0)));
            sol.qr[1] = std::sqrt(std::fabs(P_(1, 1)));
            sol.qr[2] = std::sqrt(std::fabs(P_(2, 2)));
        } else {
            sol.ratio = lr.ok ? lr.ratio : 0.0;
            sol.qr[0] = std::sqrt(std::fabs(P_(0, 0)));
            sol.qr[1] = std::sqrt(std::fabs(P_(1, 1)));
            sol.qr[2] = std::sqrt(std::fabs(P_(2, 2)));
        }
    }

    // ── 9. 卫星状态输出 ──
    for (const SatGeom &g : geoms) {
        if (g.sat - 1 < 0 || g.sat - 1 >= MAXSAT)
            continue;
        SatStat &ss = ssat_[g.sat - 1];
        ss.sys = satsys(g.sat, &ss.prn);
        ss.azel[1] = g.el;
        for (int f = 0; f < nf; ++f) {
            ss.vsat[f] = g.ok[f] ? 1 : 0;
            if (g.ok[f])
                ss.resc[f] = (g.yu[f] - g.yr[f]);
            if (g.okP[f])
                ss.resp[f] = (g.yu[NFREQ + f] - g.yr[NFREQ + f]);
        }
        ss.fix = (sol.stat == SOLQ_FIX) ? 1 : 0;
    }

    // ── 10. 记录 ──
    lastTime_ = sol.time;
    hasLast_  = true;
    lastPos_  = x_.head<3>();

    // 备份模糊度，供下一历元重建
    ambValue_.clear();
    ambVar_.clear();
    for (int k = 0; k < na; ++k) {
        ambValue_[ambKeys_[k]] = x_[AMB0 + k];
        ambVar_[ambKeys_[k]]   = P_(AMB0 + k, AMB0 + k);
    }

    sol_ = sol;
    return sol;
}

} // namespace rtk
} // namespace nav
