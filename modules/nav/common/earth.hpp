// ============================================================================
//  nav/common/earth.hpp
//  WGS84 地球模型：重力、曲率半径、坐标转换、地球自转
//
//  来源：KF-GINS (i2Nav Group, Wuhan University), GPL-3.0
//  重构：types.h -> nav_types.hpp；补 <cmath>；加 namespace；
//        local2global/global2local 依赖 Pose，保留原语义
// ============================================================================
#ifndef NAV_COMMON_EARTH_HPP
#define NAV_COMMON_EARTH_HPP

#include <Eigen/Geometry>
#include <cmath>

#include "angle.hpp"
#include "nav_types.hpp"

namespace nav {

using Eigen::Matrix3d;
using Eigen::Quaterniond;
using Eigen::Vector3d;

// ── WGS84 椭球参数 ───────────────────────────────────────────────────
constexpr double WGS84_WIE = 7.2921151467E-5;        // 地球自转角速度 [rad/s]
constexpr double WGS84_F   = 0.0033528106647474805;  // 扁率
constexpr double WGS84_RA  = 6378137.0000000000;     // 长半轴 a [m]
constexpr double WGS84_RB  = 6356752.3142451793;     // 短半轴 b [m]
constexpr double WGS84_GM0 = 398600441800000.00;     // 地球引力常数
constexpr double WGS84_E1  = 0.0066943799901413156;  // 第一偏心率平方
constexpr double WGS84_E2  = 0.0067394967422764341;  // 第二偏心率平方

class Earth {
public:
    // 正常重力（Torge & Müller, Geodesy, eq. 4.78a / 4.79）
    static double gravity(const Vector3d &blh) {
        const double sinphi = std::sin(blh[0]);
        const double sin2   = sinphi * sinphi;
        const double sin4   = sin2 * sin2;

        const double gamma_a = 9.7803267715;
        const double gamma_0 = gamma_a *
            (1.0 + 0.0052790414 * sin2 + 0.0000232718 * sin4
                 + 0.0000001262 * sin2 * sin4 + 0.0000000007 * sin4 * sin4);

        // 正常重力随高程变化
        return gamma_0 - (3.0877e-6 - 4.3e-9 * sin2) * blh[2] + 0.72e-12 * blh[2] * blh[2];
    }

    // 子午圈半径 RM 与卯酉圈半径 RN
    static Eigen::Vector2d meridianPrimeVerticalRadius(double lat) {
        double tmp = std::sin(lat);
        tmp *= tmp;
        tmp = 1.0 - WGS84_E1 * tmp;
        const double sqrttmp = std::sqrt(tmp);

        return {WGS84_RA * (1.0 - WGS84_E1) / (sqrttmp * tmp), WGS84_RA / sqrttmp};
    }

    // 卯酉圈半径
    static double RN(double lat) {
        const double sinlat = std::sin(lat);
        return WGS84_RA / std::sqrt(1.0 - WGS84_E1 * sinlat * sinlat);
    }

    // n 系 -> e 系 的方向余弦矩阵
    static Matrix3d cne(const Vector3d &blh) {
        const double sinlat = std::sin(blh[0]);
        const double sinlon = std::sin(blh[1]);
        const double coslat = std::cos(blh[0]);
        const double coslon = std::cos(blh[1]);

        Matrix3d dcm;
        dcm(0, 0) = -sinlat * coslon;
        dcm(0, 1) = -sinlon;
        dcm(0, 2) = -coslat * coslon;

        dcm(1, 0) = -sinlat * sinlon;
        dcm(1, 1) =  coslon;
        dcm(1, 2) = -coslat * sinlon;

        dcm(2, 0) =  coslat;
        dcm(2, 1) =  0.0;
        dcm(2, 2) = -sinlat;

        return dcm;
    }

    // n 系 -> e 系 的四元数
    static Quaterniond qne(const Vector3d &blh) {
        Quaterniond quat;

        const double coslon = std::cos(blh[1] * 0.5);
        const double sinlon = std::sin(blh[1] * 0.5);
        const double coslat = std::cos(-PI * 0.25 - blh[0] * 0.5);
        const double sinlat = std::sin(-PI * 0.25 - blh[0] * 0.5);

        quat.w() = coslat * coslon;
        quat.x() = -sinlat * sinlon;
        quat.y() = sinlat * coslon;
        quat.z() = coslat * sinlon;

        return quat.normalized();
    }

    // 由 n->e 四元数与高程反算纬度、经度
    static Vector3d blh(const Quaterniond &qne, double height) {
        return {-2.0 * std::atan(qne.y() / qne.w()) - PI * 0.5,
                 2.0 * std::atan2(qne.z(), qne.w()),
                 height};
    }

    // 大地坐标 -> 地心地固坐标
    static Vector3d blh2ecef(const Vector3d &blh) {
        const double coslat = std::cos(blh[0]);
        const double sinlat = std::sin(blh[0]);
        const double coslon = std::cos(blh[1]);
        const double sinlon = std::sin(blh[1]);

        const double rn  = RN(blh[0]);
        const double rnh = rn + blh[2];

        return {rnh * coslat * coslon,
                rnh * coslat * sinlon,
                (rnh - rn * WGS84_E1) * sinlat};
    }

    // 地心地固坐标 -> 大地坐标
    static Vector3d ecef2blh(const Vector3d &ecef) {
        const double p = std::sqrt(ecef[0] * ecef[0] + ecef[1] * ecef[1]);

        double lat = std::atan(ecef[2] / (p * (1.0 - WGS84_E1)));
        const double lon = 2.0 * std::atan2(ecef[1], ecef[0] + p);

        double h = 0.0, h2;
        do {
            h2  = h;
            const double rn = RN(lat);
            h   = p / std::cos(lat) - rn;
            lat = std::atan(ecef[2] / (p * (1.0 - WGS84_E1 * rn / (rn + h))));
        } while (std::fabs(h - h2) > 1.0e-4);

        return {lat, lon, h};
    }

    // n 系相对位置 -> 大地坐标相对位置（的变换矩阵）
    static Matrix3d DRi(const Vector3d &blh) {
        Matrix3d dri = Matrix3d::Zero();
        const Eigen::Vector2d rmn = meridianPrimeVerticalRadius(blh[0]);

        dri(0, 0) = 1.0 / (rmn[0] + blh[2]);
        dri(1, 1) = 1.0 / ((rmn[1] + blh[2]) * std::cos(blh[0]));
        dri(2, 2) = -1.0;
        return dri;
    }

    // 大地坐标相对位置 -> n 系相对位置
    static Matrix3d DR(const Vector3d &blh) {
        Matrix3d dr = Matrix3d::Zero();
        const Eigen::Vector2d rmn = meridianPrimeVerticalRadius(blh[0]);

        dr(0, 0) = rmn[0] + blh[2];
        dr(1, 1) = (rmn[1] + blh[2]) * std::cos(blh[0]);
        dr(2, 2) = -1.0;
        return dr;
    }

    // 局部坐标（在 origin 处展开）-> 大地坐标
    static Vector3d local2global(const Vector3d &origin, const Vector3d &local) {
        const Vector3d ecef0 = blh2ecef(origin);
        const Matrix3d cn0e  = cne(origin);
        return ecef2blh(ecef0 + cn0e * local);
    }

    // 大地坐标 -> 局部坐标（在 origin 处展开）
    static Vector3d global2local(const Vector3d &origin, const Vector3d &global) {
        const Vector3d ecef0 = blh2ecef(origin);
        const Matrix3d cn0e  = cne(origin);
        return cn0e.transpose() * (blh2ecef(global) - ecef0);
    }

    static Pose local2global(const Vector3d &origin, const Pose &local) {
        Pose global;
        const Vector3d ecef0 = blh2ecef(origin);
        const Matrix3d cn0e  = cne(origin);
        const Vector3d blh1  = ecef2blh(ecef0 + cn0e * local.t);
        const Matrix3d cn1e  = cne(blh1);

        global.t = blh1;
        global.R = cn1e.transpose() * cn0e * local.R;
        return global;
    }

    static Pose global2local(const Vector3d &origin, const Pose &global) {
        Pose local;
        const Vector3d ecef0 = blh2ecef(origin);
        const Matrix3d cn0e  = cne(origin);
        const Vector3d ecef1 = blh2ecef(global.t);
        const Matrix3d cn1e  = cne(global.t);

        local.t = cn0e.transpose() * (ecef1 - ecef0);
        local.R = cn0e.transpose() * cn1e * global.R;
        return local;
    }

    // 地球自转角速度投影到 e 系
    static Vector3d iewe() {
        return {0.0, 0.0, WGS84_WIE};
    }

    // 地球自转角速度投影到 n 系
    static Vector3d iewn(double lat) {
        return {WGS84_WIE * std::cos(lat), 0.0, -WGS84_WIE * std::sin(lat)};
    }

    static Vector3d iewn(const Vector3d &origin, const Vector3d &local) {
        return iewn(local2global(origin, local)[0]);
    }

    // n 系相对于 e 系转动角速度投影到 n 系（运输率）
    static Vector3d enwn(const Eigen::Vector2d &rmn, const Vector3d &blh, const Vector3d &vel) {
        return { vel[1] / (rmn[1] + blh[2]),
                -vel[0] / (rmn[0] + blh[2]),
                -vel[1] * std::tan(blh[0]) / (rmn[1] + blh[2])};
    }

    static Vector3d enwn(const Vector3d &origin, const Vector3d &local, const Vector3d &vel) {
        const Vector3d global = local2global(origin, local);
        const Eigen::Vector2d rmn = meridianPrimeVerticalRadius(global[0]);
        return enwn(rmn, global, vel);
    }
};

} // namespace nav

#endif // NAV_COMMON_EARTH_HPP
