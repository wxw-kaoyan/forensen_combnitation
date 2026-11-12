// ============================================================================
//  nav/rtk/core/atmosphere.cpp
//  大气改正实现（逐行对照 RTKLIB）
// ============================================================================
#include "atmosphere.hpp"

#include <cmath>

#include "rtk_util.hpp"

namespace nav {
namespace rtk {

const double kIonDefault[8] = {
    /* 2004/1/1 */
    0.1118E-07, -0.7451E-08, -0.5961E-07, 0.1192E-06,
    0.1167E+06, -0.2294E+06, -0.1311E+06, 0.1049E+07
};

// ════════════════════════════════════════════════════════════════════
//  Klobuchar 电离层模型
// ════════════════════════════════════════════════════════════════════
double ionmodel(const GTime &time, const double *ion,
                const Vector3d &pos, const double *azel)
{
    // 高程过低 / 高度角非正：不改正
    if (pos[2] < -1e3 || azel[1] <= 0.0)
        return 0.0;

    double sum = 0.0;
    for (int i = 0; i < 8; ++i)
        sum += std::fabs(ion[i]);
    const double *p = (sum <= 0.0) ? kIonDefault : ion;

    // 地球中心角（半圆单位）
    const double psi = 0.0137 / (azel[1] / PI + 0.11) - 0.022;

    // 电离层穿刺点纬度/经度（半圆单位）
    double phi = pos[0] / PI + psi * std::cos(azel[0]);
    if (phi > 0.416)       phi = 0.416;
    else if (phi < -0.416) phi = -0.416;

    const double lam = pos[1] / PI + psi * std::sin(azel[0]) / std::cos(phi * PI);

    // 地磁纬度
    phi += 0.064 * std::cos((lam - 1.617) * PI);

    // 当地时间 [s]
    int week = 0;
    double tt = 43200.0 * lam + time2gpst(time, &week);
    tt -= std::floor(tt / 86400.0) * 86400.0;

    // 倾斜因子
    const double f = 1.0 + 16.0 * std::pow(0.53 - azel[1] / PI, 3.0);

    // 振幅与周期
    double amp = p[0] + phi * (p[1] + phi * (p[2] + phi * p[3]));
    double per = p[4] + phi * (p[5] + phi * (p[6] + phi * p[7]));
    amp = amp < 0.0 ? 0.0 : amp;
    per = per < 72000.0 ? 72000.0 : per;

    const double x = 2.0 * PI * (tt - 50400.0) / per;

    const double delay =
        (std::fabs(x) < 1.57)
            ? (5e-9 + amp * (1.0 + x * x * (-0.5 + x * x / 24.0)))
            : 5e-9;

    return CLIGHT * f * delay;
}

double ionmapf(const Vector3d &pos, const double *azel)
{
    if (pos[2] >= HION)
        return 1.0;
    return 1.0 / std::cos(std::asin((RE_WGS84 + pos[2]) / (RE_WGS84 + HION) *
                                    std::sin(PI / 2.0 - azel[1])));
}

// ════════════════════════════════════════════════════════════════════
//  Saastamoinen 对流层模型
// ════════════════════════════════════════════════════════════════════
double tropmodel(const GTime &time, const Vector3d &pos,
                 const double *azel, double humi)
{
    (void)time;

    const double temp0 = 15.0;   // 海平面温度 [℃]

    if (pos[2] < -100.0 || pos[2] > 1e4 || azel[1] <= 0.0)
        return 0.0;

    const double hgt = pos[2] < 0.0 ? 0.0 : pos[2];

    // 标准大气
    const double pres = 1013.25 * std::pow(1.0 - 2.2557e-5 * hgt, 5.2568);
    const double temp = temp0 - 6.5e-3 * hgt + 273.16;
    const double e    = 6.108 * humi * std::exp((17.15 * temp - 4684.0) / (temp - 38.45));

    const double z = PI / 2.0 - azel[1];

    const double trph = 0.0022768 * pres /
                        (1.0 - 0.00266 * std::cos(2.0 * pos[0]) - 0.00028 * hgt / 1e3) /
                        std::cos(z);
    const double trpw = 0.002277 * (1255.0 / temp + 0.05) * e / std::cos(z);

    return trph + trpw;
}

} // namespace rtk
} // namespace nav
