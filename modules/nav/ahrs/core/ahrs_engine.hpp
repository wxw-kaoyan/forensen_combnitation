// ============================================================================
//  nav/ahrs/core/ahrs_engine.hpp
//  AHRS 姿态引擎：四元数积分 + 6 维误差状态 EKF
//
//  状态：δθ(3) 姿态误差 + b_g(3) 陀螺零偏
//  观测：加计水平姿态(2) + 零角速率(3)
//  航向：启动 yaw = 0，无外部航向观测，输出相对航向
// ============================================================================
#ifndef NAV_AHRS_AHRS_ENGINE_HPP
#define NAV_AHRS_AHRS_ENGINE_HPP

#include <Eigen/Dense>

#include "ahrs_types.hpp"

namespace nav {
namespace ahrs {

class AhrsEngine {
public:
    explicit AhrsEngine(const AhrsConfig &cfg);

    // ── 数据注入 ────────────────────────────────────────────────────
    // dtheta / dvel 为增量（已乘 dt）
    void addImu(const ImuSample &imu);

    // ── 推进一个 IMU 周期 ──────────────────────────────────────────
    void step();

    // ── 结果 ────────────────────────────────────────────────────────
    const nav::Attitude   &attitude() const { return att_; }
    Vector3d               euler() const { return att_.euler; }
    double                 yaw() const { return att_.euler[2]; }
    Vector3d               gyroBias() const { return gyrbias_; }
    const Eigen::MatrixXd &covariance() const { return Cov_; }
    double                 timestamp() const { return timestamp_; }
    bool                   healthy() const { return healthy_; }
    const AhrsDebug       &debug() const { return debug_; }

    // ── 航向清零 / 重设 ────────────────────────────────────────────
    // 双天线或 GNSS 航迹可用时，可把相对航向重置到指定值
    void resetYaw(double yawRad);
    // 上电清零：把当前航向直接置 0
    void resetYawToZero();

    // 用当前加计读数重新对准水平姿态
    void alignLevelFromAccel();

    void reset(const AhrsConfig &cfg);

private:
    void applyObservations(const ImuSample &imuRaw, const Vector3d &dthetaComp);
    void tiltUpdate(const Vector3d &fBody);
    void zaruUpdate(const Vector3d &omegaCompensated);
    void alignLevel(const Vector3d &fBody);

    void EKFPredict(const Eigen::MatrixXd &Phi, const Eigen::MatrixXd &Qd);
    void EKFUpdate(Eigen::MatrixXd &dz, Eigen::MatrixXd &H, Eigen::MatrixXd &R);
    void stateFeedback();
    void checkCov();

private:
    AhrsConfig cfg_;

    double timestamp_ = 0.0;
    bool   healthy_   = true;
    bool   imuReady_  = false;

    // 首帧标记：用于加计水平对准
    bool   firstSample_ = true;

    // 上一次已补偿的角增量（圆锥补偿需要 θ_{k-1}）
    Vector3d dthetaPre_ = Vector3d::Zero();

    nav::Attitude att_;
    Vector3d      gyrbias_ = Vector3d::Zero();

    // 静止判定
    bool stationary_ = false;

    // 约束检查的上一"时间槽"（time / constraintInterval 取整）
    double lastConstraintSlot_ = -1e30;

    // 数据缓冲
    ImuSample imuCur_;
    bool      hasCur_   = false;
    Vector3d  fBodyLast_ = Vector3d(0.0, 0.0, 9.80665);   // 最近一次比力（b 系）

    Eigen::MatrixXd Cov_;   // AHRS_RANK × AHRS_RANK
    Eigen::MatrixXd Qc_;    // AHRS_RANK × AHRS_RANK
    Eigen::MatrixXd dx_;    // AHRS_RANK × 1

    AhrsDebug debug_;
};

} // namespace ahrs
} // namespace nav

#endif // NAV_AHRS_AHRS_ENGINE_HPP
