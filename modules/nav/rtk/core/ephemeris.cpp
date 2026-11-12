// ============================================================================
//  nav/rtk/core/ephemeris.cpp
//  广播星历实现（开普勒轨道外推，逐行对照 RTKLIB）
// ============================================================================
#include "ephemeris.hpp"

#include <cmath>
#include <limits>

#include "rtk_util.hpp"

namespace nav {
namespace rtk {

namespace {

// 由 URA 指数取位置误差方差 [m^2]
double varUraEph(int ura)
{
    if (ura <= 0)  return 2.0 * 2.0;
    if (ura == 1)  return 2.8 * 2.8;
    if (ura == 2)  return 4.0 * 4.0;
    if (ura == 3)  return 5.7 * 5.7;
    if (ura == 4)  return 8.0 * 8.0;
    if (ura <= 6)  return std::pow(static_cast<double>(1 << (ura + 1)), 2.0);
    if (ura <= 8)  return (1 << (ura - 2)) * 0.1 * (1 << (ura - 2)) * 0.1;
    if (ura <= 10) return (1 << (ura - 2)) * 0.1 * (1 << (ura - 2)) * 0.1;
    return std::numeric_limits<double>::max() / 1e10;
}

// 按时间挑选最近的星历（有效期 ±2h）
const Eph *selectEph(const Nav &nav, int sat, const GTime &t)
{
    const Eph *best = nullptr;
    double bestDiff = 7200.0 + 1.0;   // ±2h 拟合间隔

    for (const Eph &e : nav.eph) {
        if (e.sat != sat || !e.valid())
            continue;
        const double dt = std::fabs(timeDiff(t, e.toe));
        if (dt > 7200.0)
            continue;
        if (dt < bestDiff) {
            bestDiff = dt;
            best = &e;
        }
    }
    return best;
}

} // namespace

// ════════════════════════════════════════════════════════════════════
//  Nav 辅助
// ════════════════════════════════════════════════════════════════════
void Nav::updateWavelengths()
{
    for (int i = 0; i < MAXSAT; ++i) {
        lam[i][0] = LAM1;
        lam[i][1] = LAM2;
        const int sys = satsys(i + 1);
        switch (sys) {
        case SYS_GPS:
        case SYS_GAL:
        case SYS_QZS:
        case SYS_CMP:
            lam[i][0] = LAM1;
            lam[i][1] = LAM2;
            break;
        case SYS_GLO:
            // GLONASS FDMA：波长随频点变化，需由观测的 D 或已知频点确定
            // 这里给标称 L1/L2，实际使用时应由上层按通道号修正
            lam[i][0] = LAM1;
            lam[i][1] = LAM2;
            break;
        default:
            lam[i][0] = 0.0;
            lam[i][1] = 0.0;
            break;
        }
    }
}

const Eph *Nav::findEph(int sat, const GTime &t) const
{
    return selectEph(*this, sat, t);
}

// ════════════════════════════════════════════════════════════════════
//  卫星钟差
// ════════════════════════════════════════════════════════════════════
double eph2clk(const GTime &time, const Eph &eph)
{
    double t = timeDiff(time, eph.toc);

    // 迭代两次消除钟差引起的时刻偏差
    for (int i = 0; i < 2; ++i)
        t -= eph.f0 + eph.f1 * t + eph.f2 * t * t;

    return eph.f0 + eph.f1 * t + eph.f2 * t * t;
}

// ════════════════════════════════════════════════════════════════════
//  卫星位置
// ════════════════════════════════════════════════════════════════════
void eph2pos(const GTime &time, const Eph &eph, Vector3d &rs, double &dts, double &var)
{
    rs.setZero();
    dts = 0.0;
    var = 0.0;

    if (eph.A <= 0.0)
        return;

    const double tk = timeDiff(time, eph.toe);

    int prn = 0;
    const int sys = satsys(eph.sat, &prn);

    double mu   = MU_GPS;
    double omge = OMGE_GPS;
    if (sys == SYS_GAL)      { mu = MU_GAL; omge = OMGE_GAL; }
    else if (sys == SYS_CMP) { mu = MU_CMP; omge = OMGE_CMP; }

    // 平近点角
    const double M = eph.M0 + (std::sqrt(mu / (eph.A * eph.A * eph.A)) + eph.deln) * tk;

    // 开普勒方程迭代
    double E  = M;
    double Ek = 0.0;
    for (int n = 0; std::fabs(E - Ek) > RTOL_KEPLER && n < MAXITER_KEPLER; ++n) {
        Ek = E;
        E -= (E - eph.e * std::sin(E) - M) / (1.0 - eph.e * std::cos(E));
    }

    const double sinE = std::sin(E);
    const double cosE = std::cos(E);

    // 真近点角
    const double u = std::atan2(std::sqrt(1.0 - eph.e * eph.e) * sinE, cosE - eph.e) + eph.omg;
    const double r = eph.A * (1.0 - eph.e * cosE);
    const double i = eph.i0 + eph.idot * tk;

    const double sin2u = std::sin(2.0 * u);
    const double cos2u = std::cos(2.0 * u);

    double uu = u + eph.cus * sin2u + eph.cuc * cos2u;
    double rr = r + eph.crs * sin2u + eph.crc * cos2u;
    double ii = i + eph.cis * sin2u + eph.cic * cos2u;

    const double x    = rr * std::cos(uu);
    const double y    = rr * std::sin(uu);
    const double cosi = std::cos(ii);

    if (sys == SYS_CMP && prn <= 5) {
        // 北斗 GEO 卫星特殊处理
        const double O = eph.OMG0 + eph.OMGd * tk - omge * eph.toes;
        const double sinO = std::sin(O), cosO = std::cos(O);
        const double xg = x * cosO - y * cosi * sinO;
        const double yg = x * sinO + y * cosi * cosO;
        const double zg = y * std::sin(ii);
        const double sino = std::sin(omge * tk), coso = std::cos(omge * tk);
        const double COS_5 = std::cos(-5.0 * D2R), SIN_5 = std::sin(-5.0 * D2R);

        rs[0] =  xg * coso + yg * sino * COS_5 + zg * sino * SIN_5;
        rs[1] = -xg * sino + yg * coso * COS_5 + zg * coso * SIN_5;
        rs[2] = -yg * SIN_5 + zg * COS_5;
    } else {
        const double O = eph.OMG0 + (eph.OMGd - omge) * tk - omge * eph.toes;
        const double sinO = std::sin(O), cosO = std::cos(O);

        rs[0] = x * cosO - y * cosi * sinO;
        rs[1] = x * sinO + y * cosi * cosO;
        rs[2] = y * std::sin(ii);
    }

    // 钟差
    const double tkc = timeDiff(time, eph.toc);
    dts = eph.f0 + eph.f1 * tkc + eph.f2 * tkc * tkc;

    // 相对论改正
    dts -= 2.0 * std::sqrt(mu * eph.A) * eph.e * sinE / (CLIGHT * CLIGHT);

    var = varUraEph(eph.sva);
}

// ════════════════════════════════════════════════════════════════════
//  单星 / 批量
// ════════════════════════════════════════════════════════════════════
int satpos(const GTime &time, int sat, const Nav &nav,
           Vector3d &rs, double &dts, double &var, int &svh)
{
    rs.setZero();
    dts = 0.0;
    var = 0.0;
    svh = 0;

    const Eph *eph = nav.findEph(sat, time);
    if (!eph)
        return 0;

    eph2pos(time, *eph, rs, dts, var);
    svh = eph->svh;
    return rs.norm() > 0.0 ? 1 : 0;
}

void satposs(const GTime &teph, const std::vector<Obs> &obs, const Nav &nav,
             SatPossResult &out)
{
    const size_t n = obs.size();
    out.rs.assign(n, Vector3d::Zero());
    out.dts.assign(n, 0.0);
    out.var.assign(n, 0.0);
    out.svh.assign(n, -1);

    for (size_t i = 0; i < n; ++i) {
        // 先用观测时刻算，再按信号传播时间迭代一次
        GTime t = obs[i].time.valid() ? obs[i].time : teph;

        double dts = 0.0, var = 0.0;
        int    svh = 0;
        Vector3d rs;

        if (!satpos(t, obs[i].sat, nav, rs, dts, var, svh)) {
            continue;
        }

        // 迭代：卫星位置应在 t - 传播时间 时刻
        for (int iter = 0; iter < 2; ++iter) {
            const double pr = obs[i].P[0] > 0.0 ? obs[i].P[0] : 2.0e7;
            const GTime  tt = timeAdd(t, -pr / CLIGHT);
            Vector3d rs2;
            double dts2 = 0.0, var2 = 0.0;
            int svh2 = 0;
            if (satpos(tt, obs[i].sat, nav, rs2, dts2, var2, svh2)) {
                rs  = rs2;
                dts = dts2;
                var = var2;
                svh = svh2;
            }
        }

        out.rs[i]  = rs;
        out.dts[i] = dts;
        out.var[i] = var;
        out.svh[i] = svh;
    }
}

} // namespace rtk
} // namespace nav
