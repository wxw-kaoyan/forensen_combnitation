// ============================================================================
//  nav/common/angle.hpp
//  角度常量与单位转换
//
//  来源：KF-GINS (i2Nav Group, Wuhan University), GPL-3.0
//  重构：去除对 Eigen/Dense 的整体依赖，仅需 Eigen/Core；补 PI 常量避免
//        M_PI 在部分编译器下需要 _USE_MATH_DEFINES
// ============================================================================
#ifndef NAV_COMMON_ANGLE_HPP
#define NAV_COMMON_ANGLE_HPP

#include <Eigen/Core>

namespace nav {

constexpr double PI  = 3.14159265358979323846;
constexpr double D2R = PI / 180.0;
constexpr double R2D = 180.0 / PI;

class Angle {
public:
    static double rad2deg(double rad) { return rad * R2D; }
    static double deg2rad(double deg) { return deg * D2R; }

    static float rad2deg(float rad) { return static_cast<float>(rad * R2D); }
    static float deg2rad(float deg) { return static_cast<float>(deg * D2R); }

    template <typename T, int Rows, int Cols>
    static Eigen::Matrix<T, Rows, Cols> rad2deg(const Eigen::Matrix<T, Rows, Cols> &m) {
        return m * static_cast<T>(R2D);
    }

    template <typename T, int Rows, int Cols>
    static Eigen::Matrix<T, Rows, Cols> deg2rad(const Eigen::Matrix<T, Rows, Cols> &m) {
        return m * static_cast<T>(D2R);
    }
};

// 角度归一化到 [-PI, PI)
inline double normalizeAngle(double angle) {
    while (angle >= PI)  angle -= 2.0 * PI;
    while (angle < -PI)  angle += 2.0 * PI;
    return angle;
}

// 航向归一化到 [0, 2PI)
inline double normalizeHeading(double heading) {
    while (heading >= 2.0 * PI) heading -= 2.0 * PI;
    while (heading < 0.0)       heading += 2.0 * PI;
    return heading;
}

} // namespace nav

#endif // NAV_COMMON_ANGLE_HPP
