// ============================================================================
//  nav/gins/core/gins_types.hpp
//  GINS 松组合模块：类型、状态索引、配置、观测结构
//
//  来源：KF-GINS (i2Nav Group, Wuhan University), GPL-3.0
//  重构：合并 types.h(GNSS/IMU) 与 kf_gins_types.h；
//        删除 print_options() 的 cout 输出；
//        状态维度改为运行期确定（rank_），为后续 SLAM 扩维预留槽位
// ============================================================================
#ifndef NAV_GINS_GINS_TYPES_HPP
#define NAV_GINS_GINS_TYPES_HPP

#include <Eigen/Dense>

#include "../../common/angle.hpp"
#include "../../common/nav_types.hpp"

namespace nav {
namespace gins {

using Eigen::Matrix3d;
using Eigen::Vector3d;

// ── IMU 误差状态（零偏 + 刻度因数）────────────────────────────────────
struct ImuError {
    Vector3d gyrbias   = Vector3d::Zero();   // 陀螺零偏 [rad/s]
    Vector3d accbias   = Vector3d::Zero();   // 加计零偏 [m/s^2]
    Vector3d gyrscale  = Vector3d::Zero();   // 陀螺刻度因数（无量纲，1e-6=1ppm）
    Vector3d accscale  = Vector3d::Zero();   // 加计刻度因数
};

// ── 导航状态（真值量，非误差量）──────────────────────────────────────
struct NavState {
    Vector3d pos   = Vector3d::Zero();   // BLH [rad, rad, m]
    Vector3d vel   = Vector3d::Zero();   // NED [m/s]
    Vector3d euler = Vector3d::Zero();   // roll/pitch/yaw [rad]
    ImuError imuerror;
};

// ── IMU 噪声参数（连续时间，SI 单位）────────────────────────────────
struct ImuNoise {
    Vector3d gyr_arw       = Vector3d::Zero();   // 角度随机游走 [rad/sqrt(s)]
    Vector3d acc_vrw       = Vector3d::Zero();   // 速度随机游走 [m/s/sqrt(s)]
    Vector3d gyrbias_std   = Vector3d::Zero();   // 陀螺零偏稳定性 [rad/s]
    Vector3d accbias_std   = Vector3d::Zero();   // 加计零偏稳定性 [m/s^2]
    Vector3d gyrscale_std  = Vector3d::Zero();   // 陀螺刻度稳定性 [1]
    Vector3d accscale_std  = Vector3d::Zero();   // 加计刻度稳定性 [1]
    double   corr_time     = 3600.0;             // 一阶高斯-马尔科夫相关时间 [s]
};

// ── 状态索引（误差状态，21 维基础）──────────────────────────────────
enum StateID {
    P_ID   = 0,    // 位置误差 [rad, rad, m]（前两项为纬度/经度误差）
    V_ID   = 3,    // 速度误差 [m/s] (NED)
    PHI_ID = 6,    // 失准角 [rad]（phi 角误差模型）
    BG_ID  = 9,    // 陀螺零偏误差 [rad/s]
    BA_ID  = 12,   // 加计零偏误差 [m/s^2]
    SG_ID  = 15,   // 陀螺刻度因数误差 [1]
    SA_ID  = 18,   // 加计刻度因数误差 [1]

    // ── 以下为扩维槽位（当前不启用，占位保留）──
    LIDAR_LEVER_ID = 21,   // 激光相对 IMU 杆臂 (3)
    LIDAR_TIME_ID  = 24,   // 激光时间偏移 (1)
    MAP_ORIGIN_ID  = 25,   // 地图原点偏差 x/y (2)

    RANK_BASE = 21,
    RANK_EXT  = 27
};

// ── 噪声索引（18 维）────────────────────────────────────────────────
// 位置/速度/姿态这 9 维由 IMU 噪声驱动，没有独立的驱动噪声源
enum NoiseID {
    VRW_ID    = 0,    // 加计白噪声 -> 速度
    ARW_ID    = 3,    // 陀螺白噪声 -> 姿态
    BGSTD_ID  = 6,    // 陀螺零偏驱动
    BASTD_ID  = 9,    // 加计零偏驱动
    SGSTD_ID  = 12,   // 陀螺刻度驱动
    SASTD_ID  = 15,   // 加计刻度驱动

    NOISE_RANK = 18
};

// ── 运行模式 ────────────────────────────────────────────────────────
enum MotionMode {
    MODE_UNKNOWN  = 0,
    MODE_STATIC   = 1,   // 静止：ZUPT + ZARU
    MODE_STRAIGHT = 2,   // 直线行驶：NHC 全权重
    MODE_TURNING  = 3    // 转弯：NHC 降权
};

// ── 外部观测：GNSS（位置/速度/航向可分别有效）────────────────────────
struct GnssObservation {
    double time = -1.0;

    bool     hasPos = false;
    Vector3d blh    = Vector3d::Zero();   // [rad, rad, m]
    Vector3d stdPos = Vector3d::Zero();   // [m] 位置标准差（NED 三向）

    bool     hasVel = false;
    Vector3d vel    = Vector3d::Zero();   // NED [m/s]
    Vector3d stdVel = Vector3d::Zero();   // [m/s]

    bool   hasHeading  = false;
    double heading     = 0.0;   // [rad] 已折算到本机航向基准
    double stdHeading  = 0.0;   // [rad]

    bool isValid = false;
};

// ── 外部观测：激光先验地图定位（预留）────────────────────────────────
struct LidarObservation {
    double time = -1.0;

    Vector3d pos = Vector3d::Zero();      // [rad, rad, m]
    Vector3d std = Vector3d::Zero();      // [m]
    double   quality = 1.0;               // 匹配质量 0~1，用于 R 自适应

    bool     hasAtt = false;
    Vector3d att    = Vector3d::Zero();   // roll/pitch/yaw [rad]
    Vector3d stdAtt = Vector3d::Zero();   // [rad]

    bool isValid = false;
};

// ── 配置 ────────────────────────────────────────────────────────────
struct GinsConfig {
    // 状态维度：21 = 基础；27 = 扩 SLAM
    int rank = RANK_BASE;

    // 初始状态与初始标准差
    NavState initstate;
    NavState initstateStd;

    // IMU 噪声
    ImuNoise imunoise;

    // 安装参数
    Vector3d antlever = Vector3d::Zero();   // GNSS 天线相对 IMU 杆臂（b 系）[m]

    // ── 外部观测开关 ──
    bool useGnssPos = true;    // 3 维
    bool useGnssVel = true;    // 3 维
    bool useHeading  = true;   // 1 维（双天线）
    bool useLidar    = false;  // 3 维（预留）

    // ── 运动约束开关 ──
    bool useZupt = true;       // 3 维，零速
    bool useZaru = true;       // 3 维，零角速率
    bool useNhc  = true;       // 2 维，非完整
    bool useOd   = false;      // 1 维，轮速（默认关，接口就绪）

    // ── 辅助约束开关（预留）──
    bool useTilt    = false;   // 2 维，加计辅助水平姿态
    bool useBiasObs = false;   // 6 维，静止零偏观测

    // ── ZUPT 判定阈值 ──
    double zuptGyroThreshold = 1.0 * D2R;   // [rad/s]
    double zuptAccThreshold  = 0.5;         // [m/s^2]
    double zuptVelThreshold  = 0.5;         // [m/s]
    double zuptVelStd        = 0.01;        // [m/s]

    // ── ZARU 观测噪声 ──
    double zaruGyroStd = 0.01 * D2R;        // [rad/s]

    // ── NHC 观测噪声 ──
    double nhcLateralStd  = 0.10;           // 横向 [m/s]
    double nhcVerticalStd = 0.05;           // 垂向 [m/s]
    double nhcMinSpeed    = 1.0;            // 低于此速度不做 NHC [m/s]

    // ── 新息卡方门限 ──
    double chi2Threshold = 11.34;           // chi2(3) 99%
    double chi2Threshold1D = 6.63;          // chi2(1) 99%

    // ── 时间对齐容差 ──
    double timeAlignErr = 1e-3;             // [s]

    // ── 协方差保护：对角线超过此值视为发散 ──
    double covMaxDiagonal = 1e10;
};

// ── 调试输出（用于外部对拍分析）────────────────────────────────────
struct GinsDebug {
    double timestamp = 0.0;

    // GNSS 位置
    Vector3d gnssPosInnov  = Vector3d::Zero();   // n 系 [m]
    double   gnssPosChi2   = 0.0;
    bool     gnssPosApplied = false;

    // GNSS 速度
    Vector3d gnssVelInnov  = Vector3d::Zero();   // n 系 [m/s]
    double   gnssVelChi2   = 0.0;
    bool     gnssVelApplied = false;

    // 航向
    double headingInnov    = 0.0;                // [rad]
    bool   headingApplied  = false;

    // 约束
    bool zuptApplied = false;
    bool zaruApplied = false;
    bool nhcApplied  = false;
    bool odApplied   = false;

    // 状态
    MotionMode mode   = MODE_UNKNOWN;
    bool       healthy = true;
};

} // namespace gins
} // namespace nav

#endif // NAV_GINS_GINS_TYPES_HPP
