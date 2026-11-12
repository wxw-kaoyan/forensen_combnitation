// ============================================================================
//  nav/common/nav_types.hpp
//  三个模块共用的基础数据结构
//
//  来源：KF-GINS (i2Nav Group, Wuhan University), GPL-3.0
//  重构：types.h 与 kf_gins_types.h 中的共用部分抽出；补默认初始化；
//        去掉仅 GINS 使用的 GINSOptions（移至 gins/gins_types.hpp）
// ============================================================================
#ifndef NAV_COMMON_NAV_TYPES_HPP
#define NAV_COMMON_NAV_TYPES_HPP

#include <Eigen/Dense>

namespace nav {

using Eigen::Matrix3d;
using Eigen::Quaterniond;
using Eigen::Vector3d;

// ── 姿态 ──────────────────────────────────────────────────────────────
// qbn : b 系到 n 系的旋转四元数
// cbn : 对应的方向余弦矩阵
// euler: [roll, pitch, yaw]，ZYX 顺序，单位 rad
struct Attitude {
    Quaterniond qbn   = Quaterniond::Identity();
    Matrix3d    cbn   = Matrix3d::Identity();
    Vector3d    euler = Vector3d::Zero();
};

// ── 位置 / 速度 / 姿态 ───────────────────────────────────────────────
struct PVA {
    Vector3d pos = Vector3d::Zero();   // BLH: [lat, lon, height] = [rad, rad, m]
    Vector3d vel = Vector3d::Zero();   // NED: [vn, ve, vd] = [m/s]
    Attitude att;
};

// ── 位姿（局部坐标转换用）───────────────────────────────────────────
struct Pose {
    Matrix3d R = Matrix3d::Identity();
    Vector3d t = Vector3d::Zero();
};

// ── IMU 单周期增量 ───────────────────────────────────────────────────
// 约定：dtheta / dvel 必须是「增量」而非「速率」，即已经乘过 dt
//       这样 insMech 里的圆锥/划桨补偿才能直接使用
struct ImuSample {
    double   time = 0.0;                 // 绝对时间 [s]
    double   dt   = 0.0;                 // 采样间隔 [s]
    Vector3d dtheta = Vector3d::Zero();  // 角增量 [rad]
    Vector3d dvel   = Vector3d::Zero();  // 速度增量 [m/s]
    double   temperature = 0.0;          // 温度 [℃]，供温补使用
    bool     valid = true;
};

// ── 里程计观测 ───────────────────────────────────────────────────────
// 单独建模，不塞进 ImuSample：它是一路独立观测，不是 IMU 的一部分
struct OdoSample {
    double time = 0.0;
    double vel  = 0.0;    // 前向速度 [m/s]
    double std  = 0.1;    // 标准差 [m/s]
    bool   valid = true;
};

} // namespace nav

#endif // NAV_COMMON_NAV_TYPES_HPP
