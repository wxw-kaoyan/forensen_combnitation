// ============================================================================
//  nav/rtk/core/spp_solver.cpp
//  单点定位实现
// ============================================================================
#include "spp_solver.hpp"

#include <cmath>

#include "atmosphere.hpp"
#include "ephemeris.hpp"
#include "rtk_util.hpp"

namespace nav {
namespace rtk {

namespace {

constexpr double SIN_EL_MIN = 0.05;   // 高度角加权下限（≈2.9°）

// 伪距组合（单频 / 无电离层）
bool codeObs(const Obs &o, const RtkConfig &cfg, double &P)
{
    if (cfg.ionoopt == IONOOPT_IFLC) {
        if (o.P[0] == 0.0 || o.P[1] == 0.0)
            return false;
        const double f1 = FREQ1, f2 = FREQ2;
        const double C1 = (f1 * f1) / (f1 * f1 - f2 * f2);
        const double C2 = -(f2 * f2) / (f1 * f1 - f2 * f2);
        P = C1 * o.P[0] + C2 * o.P[1];
        return true;
    }
    if (o.P[0] == 0.0)
        return false;
    P = o.P[0];
    return true;
}

} // namespace

double obsSigma(const RtkConfig &cfg, double el, bool phase)
{
    double s = std::sin(el);
    if (s < SIN_EL_MIN)
        s = SIN_EL_MIN;
    const double base = phase ? cfg.errPhaseBase : cfg.errCodeBase;
    return base * cfg.errElFactor / s;
}

// ════════════════════════════════════════════════════════════════════
SppResult spp(const std::vector<Obs> &obs, const Nav &nav,
              const RtkConfig &cfg, const Vector3d &initPos)
{
    SppResult res;
    const int n = static_cast<int>(obs.size());
    if (n < 4)
        return res;

    // 卫星位置
    GTime teph = obs.front().time;
    SatPossResult sp;
    satposs(teph, obs, nav, sp);

    // 初值：给了就用，否则从地心起算（此时 satazel 会把高度角全部视为天顶，
    // 保证第一次迭代不会因为高度角门限把所有卫星滤掉）
    Vector3d x = (initPos.norm() > 1.0) ? initPos : Vector3d::Zero();
    double   dtr = 0.0;

    res.vsat.assign(n, 0);
    res.azel.assign(n * 2, 0.0);
    res.resp.assign(n, 0.0);

    Eigen::MatrixXd Q;   // 最后一次的法方程逆

    for (int iter = 0; iter < MAXITR; ++iter) {

        Vector3d posR;
        ecef2pos(x, posR);

        std::vector<double> v;      // 残差
        std::vector<double> sig;    // 噪声

        std::vector<Eigen::Matrix<double, 1, 4>> Hrows;
        std::vector<int> satIdx;

        for (int i = 0; i < n; ++i) {

            res.vsat[i] = 0;

            if (sp.rs[i].norm() <= 0.0 || sp.svh[i] > 0)
                continue;

            // 重复卫星跳过
            if (i + 1 < n && obs[i].sat == obs[i + 1].sat)
                continue;

            Vector3d e;
            const double r = geodist(sp.rs[i], x, e);
            if (r <= 0.0)
                continue;

            double azel[2];
            const double el = satazel(posR, e, azel);
            if (iter > 0 && el < cfg.elmin)   // 首次迭代不做高度角筛选
                continue;

            double P = 0.0;
            if (!codeObs(obs[i], cfg, P))
                continue;

            // 卫星钟差（GPS/QZS/北斗单频需扣 TGD）
            double dts = sp.dts[i];
            const int sys = satsys(obs[i].sat);
            if (cfg.ionoopt != IONOOPT_IFLC) {
                const Eph *ep = nav.findEph(obs[i].sat, teph);
                if (ep && (sys == SYS_GPS || sys == SYS_QZS || sys == SYS_CMP))
                    dts -= ep->tgd[0];
            }

            // 电离层 / 对流层
            double dion = 0.0, dtrp = 0.0;
            if (cfg.ionoopt == IONOOPT_BRDC)
                dion = ionmodel(obs[i].time, nav.ion, posR, azel);
            if (cfg.tropopt == TROPOPT_SAAS)
                dtrp = tropmodel(obs[i].time, posR, azel);

            // 残差
            const double res_i = P - (r + dtr - CLIGHT * dts + dion + dtrp);

            Eigen::Matrix<double, 1, 4> row;
            row(0) = -e[0];
            row(1) = -e[1];
            row(2) = -e[2];
            row(3) = 1.0;

            v.push_back(res_i);
            sig.push_back(obsSigma(cfg, el, false));
            Hrows.push_back(row);
            satIdx.push_back(i);

            res.vsat[i]    = 1;
            res.azel[i * 2]     = azel[0];
            res.azel[i * 2 + 1] = azel[1];
        }

        const int nv = static_cast<int>(v.size());
        if (nv < 4)
            return res;

        // 加权最小二乘
        Eigen::MatrixXd H(nv, 4);
        Eigen::VectorXd z(nv);
        for (int j = 0; j < nv; ++j) {
            const double w = 1.0 / sig[j];
            H.row(j) = Hrows[j] * w;
            z[j]     = v[j] * w;
        }

        const Eigen::Matrix4d N = H.transpose() * H;
        if (std::fabs(N.determinant()) < 1e-12)
            return res;

        Q = N.inverse();
        const Eigen::Vector4d dx = Q * (H.transpose() * z);

        x   += dx.head<3>();
        dtr += dx[3];

        if (dx.norm() < 1e-4) {
            // 收敛
            res.ok  = true;
            res.rr  = x;
            res.dtr = dtr;
            res.ns  = nv;

            // 后验残差
            double vv = 0.0;
            for (int j = 0; j < nv; ++j) {
                res.resp[satIdx[j]] = v[j];
                vv += v[j] * v[j] / (sig[j] * sig[j]);
            }

            // 卡方检验（自由度 nv-4）
            const int dof = nv - 4;
            if (dof > 0 && vv > 3.0 * dof + 10.0) {
                // 残差异常大，标记不可用但仍返回浮点解
                res.ok = false;
            }

            // 位置标准差
            res.qr[0] = std::sqrt(std::fabs(Q(0, 0)));
            res.qr[1] = std::sqrt(std::fabs(Q(1, 1)));
            res.qr[2] = std::sqrt(std::fabs(Q(2, 2)));
            return res;
        }
    }

    return res;   // 迭代未收敛
}

} // namespace rtk
} // namespace nav
