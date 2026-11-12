// ============================================================================
//  nav/rtk/core/atmosphere.hpp
//  大气改正：Klobuchar 电离层 + Saastamoinen 对流层
//
//  来源：RTKLIB rtkcmn.c 的 ionmodel / tropmodel / ionmapf
// ============================================================================
#ifndef NAV_RTK_ATMOSPHERE_HPP
#define NAV_RTK_ATMOSPHERE_HPP

#include "rtk_types.hpp"

namespace nav {
namespace rtk {

// 默认 Klobuchar 参数（2004/1/1 起算的旧参数，无广播参数时兜底）
extern const double kIonDefault[8];

// Klobuchar 电离层延迟
//   time  : 观测时刻（GPST）
//   ion   : 8 个广播参数，全 0 时使用默认值
//   pos   : 接收机 [lat, lon, h] (rad, rad, m)
//   azel  : [方位角, 高度角] (rad)
// 返回：L1 上的电离层延迟 [m]
double ionmodel(const GTime &time, const double *ion,
                const Vector3d &pos, const double *azel);

// 电离层投影函数（单层模型）
double ionmapf(const Vector3d &pos, const double *azel);

// Saastamoinen 对流层延迟
//   humi : 相对湿度（0~1）
double tropmodel(const GTime &time, const Vector3d &pos,
                 const double *azel, double humi = 0.7);

} // namespace rtk
} // namespace nav

#endif // NAV_RTK_ATMOSPHERE_HPP
