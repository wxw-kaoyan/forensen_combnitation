// ============================================================================
//  nav/rtk/core/ephemeris.hpp
//  广播星历：卫星位置、钟差、方差
//
//  来源：RTKLIB ephemeris.c 的 eph2pos / eph2clk / satpos / satposs
// ============================================================================
#ifndef NAV_RTK_EPHEMERIS_HPP
#define NAV_RTK_EPHEMERIS_HPP

#include <Eigen/Dense>

#include "rtk_types.hpp"

namespace nav {
namespace rtk {

using Eigen::Vector3d;

// 卫星钟差（含相对论改正前的多项式部分）[s]
double eph2clk(const GTime &time, const Eph &eph);

// 卫星位置（ECEF）与钟差（含相对论改正）
//   rs  : 输出位置 [m]
//   dts : 输出钟差 [s]
//   var : 输出位置+钟差方差 [m^2]
void eph2pos(const GTime &time, const Eph &eph, Vector3d &rs, double &dts, double &var);

// 单颗卫星：位置 / 钟差 / 方差 / 健康
// 返回 1 成功，0 失败
int satpos(const GTime &time, int sat, const Nav &nav,
           Vector3d &rs, double &dts, double &var, int &svh);

// 批量：按观测列表算卫星位置
struct SatPossResult {
    std::vector<Vector3d> rs;    // ECEF 位置
    std::vector<double>   dts;   // 钟差
    std::vector<double>   var;   // 方差
    std::vector<int>      svh;
};

void satposs(const GTime &teph, const std::vector<Obs> &obs, const Nav &nav,
             SatPossResult &out);

} // namespace rtk
} // namespace nav

#endif // NAV_RTK_EPHEMERIS_HPP
