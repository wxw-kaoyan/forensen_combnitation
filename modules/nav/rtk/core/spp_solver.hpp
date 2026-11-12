// ============================================================================
//  nav/rtk/core/spp_solver.hpp
//  单点定位（SPP）：迭代加权最小二乘
//
//  来源：RTKLIB pntpos.c 的 rescode / estpos / pntpos（去掉 SBAS/PPP/RAIM 分支）
// ============================================================================
#ifndef NAV_RTK_SPP_SOLVER_HPP
#define NAV_RTK_SPP_SOLVER_HPP

#include <Eigen/Dense>
#include <vector>

#include "rtk_types.hpp"

namespace nav {
namespace rtk {

struct SppResult {
    bool     ok      = false;
    Vector3d rr      = Vector3d::Zero();   // ECEF [m]
    double   dtr     = 0.0;                // 接收机钟差 [s]
    Vector3d qr      = Vector3d::Zero();   // 位置标准差 [m]
    int      ns      = 0;                  // 有效卫星数
    std::vector<int>    vsat;              // 每颗星是否参与（按 obs 顺序）
    std::vector<double> azel;              // 每颗星 [az, el]，长度 2*n
    std::vector<double> resp;              // 伪距后验残差
};

// 单点定位
//   obs     : 观测列表（同一历元）
//   nav     : 星历与电离层参数
//   cfg     : 配置
//   initPos : 初值（可为 0，内部会用球心/首个有效解迭代）
SppResult spp(const std::vector<Obs> &obs, const Nav &nav,
              const RtkConfig &cfg, const Vector3d &initPos = Vector3d::Zero());

// 观测噪声（按高度角加权）[m]
double obsSigma(const RtkConfig &cfg, double el, bool phase);

} // namespace rtk
} // namespace nav

#endif // NAV_RTK_SPP_SOLVER_HPP
