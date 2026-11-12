// ============================================================================
//  nav/ahrs/core/ahrs_types.hpp
//  AHRS 纯姿态模块：类型、状态索引、配置
//
//  定位：不依赖 GNSS / 磁力计 / 双天线，上电即 yaw = 0，
//        输出的是相对航向（相对上电清零方向）
// ============================================================================
#ifndef NAV_AHRS_AHRS_TYPES_HPP
#define NAV_AHRS_AHRS_TYPES_HPP

#include <Eigen/Dense>

#include "../../common/angle.hpp"
#include "../../common/nav_types.hpp"

namespace nav {
namespace ahrs {

using Eigen::Vector3d;

// ── 状态索引（6 维误差状态）────────────────────────────────────────
enum AhrsStateID {
    ATT_ID  = 0,   // 姿态误差 [rad]
    GB_ID   = 3,   // 陀螺零偏误差 [rad/s]

    AHRS_RANK = 6
};

// ── 配置 ────────────────────────────────────────────────────────────
struct AhrsConfig {
    // 初始姿态（yaw 恒为 0，roll/pitch 可由加计对准覆盖）
    Vector3d initEuler = Vector3d::Zero();

    // 启动时是否用首帧加计做水平对准（roll/pitch）
    bool alignFromAccel = true;

    // 初始标准差
    double   initAttStd       = 1.0 * D2R;    // [rad]
    Vector3d initGyrBiasStd   = Vector3d::Constant(1.0 * D2R);  // [rad/s]

    // 陀螺噪声
    Vector3d gyrArw     = Vector3d::Zero();   // 角度随机游走 [rad/sqrt(s)]
    Vector3d gyrBiasStd = Vector3d::Zero();   // 零偏稳定性 [rad/s]
    double   corrTime   = 3600.0;             // 相关时间 [s]

    // ── 观测开关 ──
    bool useTilt = true;    // 2 维，加计辅助水平姿态
    bool useZaru = true;    // 3 维，零角速率

    // ── 倾斜观测参数 ──
    double tiltAccThreshold = 0.3;            // |‖f‖−g| 小于此值才用 [m/s²]
    double tiltStd          = 1.0 * D2R;      // 观测噪声 [rad]
    double gravityRef       = 9.80665;        // 参考重力（AHRS 无位置，用常数）

    // ── ZARU 参数 ──
    double zaruGyroThreshold = 1.0 * D2R;     // 静止判定 [rad/s]
    double zaruGyroStd       = 0.01 * D2R;    // 观测噪声 [rad/s]

    // ── 可选：扣除地球自转（默认关，纯陀螺积分）──
    // 打开后需要提供纬度，否则会以 15°/h 量级漂移
    bool   compensateEarthRate = false;
    double latitudeRad         = 0.0;

    // 约束检查频率（秒）
    double constraintInterval = 1.0;

    // 协方差保护
    double covMaxDiagonal = 1e6;
};

// ── 调试输出 ────────────────────────────────────────────────────────
struct AhrsDebug {
    double timestamp = 0.0;

    double   tiltInnovRoll  = 0.0;   // [rad]
    double   tiltInnovPitch = 0.0;   // [rad]
    bool     tiltApplied    = false;

    bool     zaruApplied    = false;
    bool     stationary     = false;
    bool     healthy        = true;

    Vector3d gyrbias = Vector3d::Zero();   // 当前零偏估计，便于对拍
};

} // namespace ahrs
} // namespace nav

#endif // NAV_AHRS_AHRS_TYPES_HPP
