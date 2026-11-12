// ============================================================================
//  nav/rtk/core/rtk_util.hpp
//  RTK 基础层：时间系统、坐标转换、卫星编号、几何量
//
//  来源：RTKLIB rtkcmn.c 的对应部分，改写为 C++ / Eigen
// ============================================================================
#ifndef NAV_RTK_RTK_UTIL_HPP
#define NAV_RTK_RTK_UTIL_HPP

#include <Eigen/Dense>
#include <string>

#include "rtk_types.hpp"

namespace nav {
namespace rtk {

using Eigen::Matrix3d;
using Eigen::Vector3d;

// ── 卫星系统 / 编号 ─────────────────────────────────────────────────
int  satsys(int sat, int *prn = nullptr);
std::string satName(int sat);
int  satId(int sys, int prn);

// ── 时间 ────────────────────────────────────────────────────────────
// 以 GPS 周 + 周内秒表示；周内秒允许越界，内部自动归算
GTime  timeAdd(const GTime &t, double sec);
double timeDiff(const GTime &t1, const GTime &t2);      // t1 - t2 [s]
double time2gpst(const GTime &t, int *week = nullptr);  // 周内秒
GTime  gpst2time(int week, double sec);
GTime  epoch2time(int y, int mon, int d, int h, int min, double sec);

// ── 坐标转换 ────────────────────────────────────────────────────────
void     ecef2pos(const Vector3d &r, Vector3d &pos);     // pos = [lat, lon, h]
Vector3d pos2ecef(const Vector3d &pos);
void     ecef2enu(const Vector3d &pos, const Vector3d &r, Vector3d &e);
void     ecef2enuv(const Vector3d &pos, const Vector3d &d, Vector3d &e);
void     cofactor(const Vector3d &pos, Matrix3d &Q);     // ECEF -> ENU 协方差

// ── 几何量 ──────────────────────────────────────────────────────────
// 卫星到接收机的几何距离（含地球自转 / Sagnac 改正），e 为视线单位矢量
double geodist(const Vector3d &rs, const Vector3d &rr, Vector3d &e);
// 由视线单位矢量算方位角、高度角
double satazel(const Vector3d &pos, const Vector3d &e, double *azel);
// 基线向量与长度
double baseline(const Vector3d &ru, const Vector3d &rb, Vector3d &dr);

} // namespace rtk
} // namespace nav

#endif // NAV_RTK_RTK_UTIL_HPP
