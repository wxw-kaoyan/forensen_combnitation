// ============================================================================
//  nav/ahrs/core/ahrs_engine.cpp
//  AHRS 姿态引擎实现
//
//  姿态积分与 GINS 共用 Rotation::coningDeltaQuaternion（二阶圆锥补偿）
//  误差状态：δθ(3) + b_g(3)
// ============================================================================
#include "ahrs_engine.hpp"

#include <cmath>

#include "../../common/earth.hpp"
#include "../../common/rotation.hpp"

namespace nav {
namespace ahrs {

namespace {
constexpr int N = AHRS_RANK;
}

AhrsEngine::AhrsEngine(const AhrsConfig &cfg)
{
    reset(cfg);
}

void AhrsEngine::reset(const AhrsConfig &cfg)
{
    cfg_ = cfg;

    timestamp_   = 0.0;
    healthy_     = true;
    imuReady_    = false;
    firstSample_ = true;
    hasCur_      = false;
    stationary_  = false;
    dthetaPre_.setZero();
    gyrbias_.setZero();
    fBodyLast_ = Vector3d(0.0, 0.0, cfg_.gravityRef);
    lastConstraintSlot_ = -1e30;

    // 初始姿态：yaw 恒为 0
    att_.euler    = cfg_.initEuler;
    att_.euler[2] = 0.0;
    att_.qbn      = Rotation::euler2quaternion(att_.euler);
    att_.cbn      = Rotation::euler2matrix(att_.euler);

    Cov_.resize(N, N);
    Qc_.resize(N, N);
    dx_.resize(N, 1);
    Cov_.setZero();
    Qc_.setZero();
    dx_.setZero();

    const double corr = (cfg_.corrTime > 1e-6) ? cfg_.corrTime : 1e-6;

    Qc_.block(ATT_ID, ATT_ID, 3, 3) =
        cfg_.gyrArw.cwiseProduct(cfg_.gyrArw).asDiagonal();
    Qc_.block(GB_ID, GB_ID, 3, 3) =
        2.0 / corr * cfg_.gyrBiasStd.cwiseProduct(cfg_.gyrBiasStd).asDiagonal();

    Cov_.block(ATT_ID, ATT_ID, 3, 3) =
        cfg_.initAttStd * cfg_.initAttStd * Eigen::Matrix3d::Identity();
    Cov_.block(GB_ID, GB_ID, 3, 3) =
        cfg_.initGyrBiasStd.cwiseProduct(cfg_.initGyrBiasStd).asDiagonal();

    debug_ = AhrsDebug();
}

// ════════════════════════════════════════════════════════════════════
//  数据注入
// ════════════════════════════════════════════════════════════════════
void AhrsEngine::addImu(const ImuSample &imu)
{
    if (!imu.valid || imu.dt <= 0.0)
        return;
    imuCur_ = imu;
    hasCur_ = true;
}

// ════════════════════════════════════════════════════════════════════
//  主流程
// ════════════════════════════════════════════════════════════════════
void AhrsEngine::step()
{
    if (!hasCur_)
        return;

    const ImuSample imu = imuCur_;
    hasCur_ = false;

    timestamp_       = imu.time;
    debug_.timestamp = imu.time;
    debug_.tiltApplied = false;
    debug_.zaruApplied = false;

    // ── 角增量补偿：零偏（可选：地球自转）──
    Vector3d dthetaComp = imu.dtheta - gyrbias_ * imu.dt;
    if (cfg_.compensateEarthRate) {
        dthetaComp -= att_.cbn.transpose() * Earth::iewn(cfg_.latitudeRad) * imu.dt;
    }

    fBodyLast_ = imu.dvel / imu.dt;

    // ── 首帧：水平对准，yaw = 0 ──
    if (firstSample_) {
        firstSample_ = false;
        imuReady_    = true;
        dthetaPre_   = dthetaComp;

        if (cfg_.alignFromAccel)
            alignLevel(fBodyLast_);
        else {
            att_.euler    = cfg_.initEuler;
            att_.euler[2] = 0.0;
            att_.qbn      = Rotation::euler2quaternion(att_.euler);
            att_.cbn      = Rotation::euler2matrix(att_.euler);
        }
        return;
    }

    // ── 姿态积分（与 GINS 共用圆锥补偿）──
    const Quaterniond qbb = Rotation::coningDeltaQuaternion(dthetaPre_, dthetaComp);
    att_.qbn   = (att_.qbn * qbb).normalized();
    att_.cbn   = Rotation::quaternion2matrix(att_.qbn);
    att_.euler = Rotation::matrix2euler(att_.cbn);
    dthetaPre_ = dthetaComp;

    // ── EKF 预测 ──
    //   φ̇ = −C_b^n·δb_g   （n 系静止冻结，无 ω_in^n 项）
    //   δḃ_g = −1/τ·δb_g
    const double corr = (cfg_.corrTime > 1e-6) ? cfg_.corrTime : 1e-6;

    Eigen::MatrixXd F = Eigen::MatrixXd::Zero(N, N);
    F.block(ATT_ID, GB_ID, 3, 3) = -att_.cbn;
    F.block(GB_ID, GB_ID, 3, 3)  = -1.0 / corr * Eigen::Matrix3d::Identity();

    Eigen::MatrixXd Phi = Eigen::MatrixXd::Identity(N, N) + F * imu.dt;

    Eigen::MatrixXd G = Eigen::MatrixXd::Zero(N, N);
    G.block(ATT_ID, ATT_ID, 3, 3) = att_.cbn;
    G.block(GB_ID, GB_ID, 3, 3)   = Eigen::Matrix3d::Identity();

    Eigen::MatrixXd Qd = G * Qc_ * G.transpose() * imu.dt;
    Qd = (Phi * Qd * Phi.transpose() + Qd) / 2.0;

    EKFPredict(Phi, Qd);

    // ── 观测更新 ──
    applyObservations(imu, dthetaComp);

    // ── 健康检查 ──
    checkCov();
}

// ════════════════════════════════════════════════════════════════════
//  观测：加计水平姿态 + 零角速率
// ════════════════════════════════════════════════════════════════════
void AhrsEngine::applyObservations(const ImuSample &imuRaw, const Vector3d &dthetaComp)
{
    const double interval = (cfg_.constraintInterval > 1e-3) ? cfg_.constraintInterval : 1.0;
    const double slot     = std::floor(imuRaw.time / interval);
    if (slot <= lastConstraintSlot_)
        return;
    lastConstraintSlot_ = slot;

    const Vector3d fBody = imuRaw.dvel / imuRaw.dt;
    const Vector3d omega = dthetaComp / imuRaw.dt;

    const bool accelClean =
        std::fabs(fBody.norm() - cfg_.gravityRef) < cfg_.tiltAccThreshold;

    // 静止：角速率小 + 比力模长接近重力
    stationary_ = (omega.norm() < cfg_.zaruGyroThreshold) && accelClean;
    debug_.stationary = stationary_;

    if (cfg_.useTilt && accelClean)
        tiltUpdate(fBody);

    if (cfg_.useZaru && stationary_)
        zaruUpdate(omega);
}

// ── 加计水平姿态观测（2 维）────────────────────────────────────────
// 静止时 f^b = g·(sin p, −cos p·sin r, −cos p·cos r)
//   roll  = atan2(−f_y, −f_z)
//   pitch = atan2(f_x, √(f_y²+f_z²))
void AhrsEngine::tiltUpdate(const Vector3d &fBody)
{
    const double fx = fBody[0], fy = fBody[1], fz = fBody[2];

    const double rollAcc  = std::atan2(-fy, -fz);
    const double pitchAcc = std::atan2(fx, std::sqrt(fy * fy + fz * fz));

    const double dRoll  = normalizeAngle(rollAcc - att_.euler[0]);
    const double dPitch = normalizeAngle(pitchAcc - att_.euler[1]);

    Eigen::MatrixXd dz(2, 1);
    dz(0, 0) = dRoll;
    dz(1, 0) = dPitch;

    Eigen::MatrixXd H = Eigen::MatrixXd::Zero(2, N);
    H.block(0, ATT_ID, 2, 2) = Eigen::Matrix2d::Identity();

    Eigen::MatrixXd R = cfg_.tiltStd * cfg_.tiltStd * Eigen::Matrix2d::Identity();

    EKFUpdate(dz, H, R);
    stateFeedback();

    debug_.tiltInnovRoll  = dRoll;
    debug_.tiltInnovPitch = dPitch;
    debug_.tiltApplied    = true;
}

// ── ZARU（3 维）────────────────────────────────────────────────────
// 静止时真实角速率为 0，扣掉当前零偏估计后的角速率为
//   ω_comp = ω_true + b_true − b̂ = dx_BG（即零偏的"修正量"）
// 因此 dz = ω_comp，H 在零偏块上取 +I
void AhrsEngine::zaruUpdate(const Vector3d &omegaCompensated)
{
    Eigen::MatrixXd dz(3, 1);
    dz.block(0, 0, 3, 1) = omegaCompensated;

    Eigen::MatrixXd H = Eigen::MatrixXd::Zero(3, N);
    H.block(0, GB_ID, 3, 3) = Eigen::Matrix3d::Identity();

    Eigen::MatrixXd R =
        cfg_.zaruGyroStd * cfg_.zaruGyroStd * Eigen::Matrix3d::Identity();

    EKFUpdate(dz, H, R);
    stateFeedback();

    debug_.zaruApplied = true;
}

// ════════════════════════════════════════════════════════════════════
//  加计水平对准（首帧 / 手动）
// ════════════════════════════════════════════════════════════════════
void AhrsEngine::alignLevel(const Vector3d &fBody)
{
    const double roll  = std::atan2(-fBody[1], -fBody[2]);
    const double pitch = std::atan2(fBody[0], std::sqrt(fBody[1] * fBody[1] +
                                                        fBody[2] * fBody[2]));

    att_.euler    = Vector3d(roll, pitch, 0.0);   // 启动 yaw = 0
    att_.qbn      = Rotation::euler2quaternion(att_.euler);
    att_.cbn      = Rotation::euler2matrix(att_.euler);
}

void AhrsEngine::alignLevelFromAccel()
{
    alignLevel(fBodyLast_);
}

// ════════════════════════════════════════════════════════════════════
//  航向重设
// ════════════════════════════════════════════════════════════════════
void AhrsEngine::resetYaw(double yawRad)
{
    att_.euler[2] = normalizeHeading(yawRad);
    att_.qbn      = Rotation::euler2quaternion(att_.euler);
    att_.cbn      = Rotation::euler2matrix(att_.euler);
}

void AhrsEngine::resetYawToZero()
{
    resetYaw(0.0);
}

// ════════════════════════════════════════════════════════════════════
//  EKF
// ════════════════════════════════════════════════════════════════════
void AhrsEngine::EKFPredict(const Eigen::MatrixXd &Phi, const Eigen::MatrixXd &Qd)
{
    Cov_ = Phi * Cov_ * Phi.transpose() + Qd;
    dx_  = Phi * dx_;
}

void AhrsEngine::EKFUpdate(Eigen::MatrixXd &dz, Eigen::MatrixXd &H, Eigen::MatrixXd &R)
{
    const Eigen::MatrixXd S = H * Cov_ * H.transpose() + R;
    const Eigen::MatrixXd K = Cov_ * H.transpose() * S.inverse();

    Eigen::MatrixXd I = Eigen::MatrixXd::Identity(N, N);
    I = I - K * H;

    dx_  = dx_ + K * (dz - H * dx_);
    Cov_ = I * Cov_ * I.transpose() + K * R * K.transpose();
}

void AhrsEngine::stateFeedback()
{
    // 姿态：乘性误差，左乘旋转（与 GINS 一致）
    const Vector3d dphi   = dx_.block(ATT_ID, 0, 3, 1);
    const Quaterniond qpn = Rotation::rotvec2quaternion(dphi);

    att_.qbn   = (qpn * att_.qbn).normalized();
    att_.cbn   = Rotation::quaternion2matrix(att_.qbn);
    att_.euler = Rotation::matrix2euler(att_.cbn);

    // 陀螺零偏
    gyrbias_ += dx_.block(GB_ID, 0, 3, 1);

    dx_.setZero();
    debug_.gyrbias = gyrbias_;
}

void AhrsEngine::checkCov()
{
    healthy_ = true;
    for (int i = 0; i < N; ++i) {
        double &d = Cov_(i, i);
        if (!std::isfinite(d) || d < 0.0) {
            d        = 1e-12;
            healthy_ = false;
        } else if (d > cfg_.covMaxDiagonal) {
            d        = cfg_.covMaxDiagonal;
            healthy_ = false;
        }
    }
    debug_.healthy = healthy_;
}

} // namespace ahrs
} // namespace nav
