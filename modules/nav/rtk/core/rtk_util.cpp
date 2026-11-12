// ============================================================================
//  nav/rtk/core/rtk_util.cpp
//  RTK 基础层实现
// ============================================================================
#include "rtk_util.hpp"

#include <cmath>
#include <cstdio>

namespace nav {
namespace rtk {

namespace {
constexpr double DAY_SEC = 86400.0;
constexpr double WEEK_SEC = 604800.0;
}

// ════════════════════════════════════════════════════════════════════
//  卫星编号
// ════════════════════════════════════════════════════════════════════
int satsys(int sat, int *prn)
{
    int sys = SYS_NONE;

    if (sat <= 0 || sat > SAT_BASE_SBS + 64)
        return sys;

    if (sat <= SAT_BASE_GLO - 1)      { sys = SYS_GPS; if (prn) *prn = sat - SAT_BASE_GPS; }
    else if (sat <= SAT_BASE_GAL - 1) { sys = SYS_GLO; if (prn) *prn = sat - SAT_BASE_GLO; }
    else if (sat <= SAT_BASE_CMP - 1) { sys = SYS_GAL; if (prn) *prn = sat - SAT_BASE_GAL; }
    else if (sat <= SAT_BASE_QZS - 1) { sys = SYS_CMP; if (prn) *prn = sat - SAT_BASE_CMP; }
    else if (sat <= SAT_BASE_SBS - 1) { sys = SYS_QZS; if (prn) *prn = sat - SAT_BASE_QZS; }
    else                              { sys = SYS_SBS; if (prn) *prn = sat - SAT_BASE_SBS; }

    return sys;
}

int satId(int sys, int prn)
{
    switch (sys) {
    case SYS_GPS: return SAT_BASE_GPS + prn;
    case SYS_GLO: return SAT_BASE_GLO + prn;
    case SYS_GAL: return SAT_BASE_GAL + prn;
    case SYS_CMP: return SAT_BASE_CMP + prn;
    case SYS_QZS: return SAT_BASE_QZS + prn;
    case SYS_SBS: return SAT_BASE_SBS + prn;
    default:      return 0;
    }
}

std::string satName(int sat)
{
    int prn = 0;
    const int sys = satsys(sat, &prn);
    char t = '?';
    switch (sys) {
    case SYS_GPS: t = 'G'; break;
    case SYS_GLO: t = 'R'; break;
    case SYS_GAL: t = 'E'; break;
    case SYS_CMP: t = 'C'; break;
    case SYS_QZS: t = 'J'; break;
    case SYS_SBS: t = 'S'; break;
    default: break;
    }
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%c%02d", t, prn);
    return std::string(buf);
}

// ════════════════════════════════════════════════════════════════════
//  时间
// ════════════════════════════════════════════════════════════════════
double time2gpst(const GTime &t, int *week)
{
    double sec = t.tow;
    int    w   = t.week;

    // 允许 tow 越界，归算到 [0, 604800)
    while (sec < 0.0)      { sec += WEEK_SEC; w -= 1; }
    while (sec >= WEEK_SEC) { sec -= WEEK_SEC; w += 1; }

    if (week) *week = w;
    return sec;
}

GTime gpst2time(int week, double sec)
{
    GTime t;
    t.week = week;
    t.tow  = sec;
    return t;
}

GTime timeAdd(const GTime &t, double sec)
{
    GTime out = t;
    out.tow += sec;
    // 归算
    const double tow = time2gpst(out, &out.week);
    out.tow = tow;
    return out;
}

double timeDiff(const GTime &t1, const GTime &t2)
{
    int w1 = t1.week, w2 = t2.week;
    const double s1 = time2gpst(t1, &w1);
    const double s2 = time2gpst(t2, &w2);
    return static_cast<double>(w1 - w2) * WEEK_SEC + (s1 - s2);
}

GTime epoch2time(int y, int mon, int d, int h, int min, double sec)
{
    // 公历 -> GPS 周（GPS 起算 1980-01-06 00:00:00 UTC）
    static const int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    int days = 0;
    for (int yy = 1980; yy < y; ++yy) {
        const bool leap = ((yy % 4 == 0 && yy % 100 != 0) || yy % 400 == 0);
        days += leap ? 366 : 365;
    }
    for (int mm = 1; mm < mon; ++mm) {
        days += mdays[mm - 1];
        if (mm == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0))
            days += 1;
    }
    days += d - 1;
    // 1980-01-06 是 GPS 第 0 周第 0 秒；1 月 6 日 = days 5
    days -= 5;

    GTime t;
    const double secOfDay = static_cast<double>(h) * 3600.0
                            + static_cast<double>(min) * 60.0 + sec;
    t.week = days / 7;
    t.tow  = static_cast<double>(days % 7) * DAY_SEC + secOfDay;
    return t;
}

// ════════════════════════════════════════════════════════════════════
//  坐标转换
// ════════════════════════════════════════════════════════════════════
void ecef2pos(const Vector3d &r, Vector3d &pos)
{
    const double e2 = FE_WGS84 * (2.0 - FE_WGS84);
    const double r2 = r[0] * r[0] + r[1] * r[1];

    double z    = r[2];
    double zk   = 0.0;
    double v    = RE_WGS84;
    double sinp = 0.0;

    // 不动点迭代：z = r[2] + N·e²·sinφ
    for (int i = 0; i < 20; ++i) {
        zk   = z;
        sinp = z / std::sqrt(r2 + z * z);
        v    = RE_WGS84 / std::sqrt(1.0 - e2 * sinp * sinp);
        z    = r[2] + v * e2 * sinp;
        if (std::fabs(z - zk) < 1e-4)
            break;
    }

    pos[0] = (r2 > 1e-12) ? std::atan(z / std::sqrt(r2))
                          : (r[2] > 0.0 ? PI / 2.0 : -PI / 2.0);
    pos[1] = (r2 > 1e-12) ? std::atan2(r[1], r[0]) : 0.0;
    pos[2] = std::sqrt(r2 + z * z) - v;
}

Vector3d pos2ecef(const Vector3d &pos)
{
    const double e2 = FE_WGS84 * (2.0 - FE_WGS84);
    const double sinp = std::sin(pos[0]);
    const double cosp = std::cos(pos[0]);
    const double sinl = std::sin(pos[1]);
    const double cosl = std::cos(pos[1]);
    const double v    = RE_WGS84 / std::sqrt(1.0 - e2 * sinp * sinp);

    return Vector3d((v + pos[2]) * cosp * cosl,
                    (v + pos[2]) * cosp * sinl,
                    (v * (1.0 - e2) + pos[2]) * sinp);
}

void ecef2enu(const Vector3d &pos, const Vector3d &r, Vector3d &e)
{
    const double sinp = std::sin(pos[0]), cosp = std::cos(pos[0]);
    const double sinl = std::sin(pos[1]), cosl = std::cos(pos[1]);

    // ECEF -> ENU 旋转矩阵（行序：E, N, U）
    Matrix3d E;
    E << -sinl,          cosl,         0.0,
         -sinp * cosl,  -sinp * sinl,  cosp,
          cosp * cosl,   cosp * sinl,  sinp;

    e = E * r;
}

void ecef2enuv(const Vector3d &pos, const Vector3d &d, Vector3d &e)
{
    ecef2enu(pos, d, e);   // 对方向矢量同样适用（无平移项）
}

void cofactor(const Vector3d &pos, Matrix3d &Q)
{
    const double sinp = std::sin(pos[0]), cosp = std::cos(pos[0]);
    const double sinl = std::sin(pos[1]), cosl = std::cos(pos[1]);

    Matrix3d E;
    E << -sinl,          cosl,         0.0,
         -sinp * cosl,  -sinp * sinl,  cosp,
          cosp * cosl,   cosp * sinl,  sinp;

    Q = E * Q * E.transpose();
}

// ════════════════════════════════════════════════════════════════════
//  几何量
// ════════════════════════════════════════════════════════════════════
double geodist(const Vector3d &rs, const Vector3d &rr, Vector3d &e)
{
    if (rs.norm() < RE_WGS84)
        return -1.0;    // 卫星位置异常

    e = rs - rr;
    const double r = e.norm();
    if (r <= 0.0)
        return -1.0;
    e /= r;

    // Sagnac 效应：信号传播期间地球自转
    return r + OMGE_GPS * (rs[0] * rr[1] - rs[1] * rr[0]) / CLIGHT;
}

double satazel(const Vector3d &pos, const Vector3d &e, double *azel)
{
    double az = 0.0;
    double el = PI / 2.0;

    if (pos[2] > -RE_WGS84) {
        Vector3d enu;
        ecef2enu(pos, e, enu);

        const double horiz = std::sqrt(enu[0] * enu[0] + enu[1] * enu[1]);
        az = (horiz < 1e-12) ? 0.0 : std::atan2(enu[0], enu[1]);
        if (az < 0.0)
            az += 2.0 * PI;
        el = (enu.norm() < 1e-12) ? PI / 2.0 : std::asin(enu[2] / enu.norm());
    }

    if (azel) {
        azel[0] = az;
        azel[1] = el;
    }
    return el;
}

double baseline(const Vector3d &ru, const Vector3d &rb, Vector3d &dr)
{
    dr = ru - rb;
    return dr.norm();
}

} // namespace rtk
} // namespace nav
