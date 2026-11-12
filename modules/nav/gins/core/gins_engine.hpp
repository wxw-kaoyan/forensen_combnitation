// ============================================================================
//  nav/gins/core/gins_engine.hpp
//  GINS 松组合 EKF 引擎
//
//  来源：KF-GINS (i2Nav Group, Wuhan University), GPL-3.0
//  重构：
//    1. 状态维度运行期确定（rank_），21 基础 / 27 扩 SLAM
//    2. 删除 std::exit() 杀进程，改为健康标志 + 协方差钳位
//    3. 删除 print_options() 的 cout
//    4. 新增位置新息卡方检验（原版只有速度有）
//    5. NHC / ZARU / OD 全部做成配置开关
//    6. 新增 GinsDebug 输出，供外部对拍分析
// ============================================================================
#ifndef NAV_GINS_GINS_ENGINE_HPP
#define NAV_GINS_GINS_ENGINE_HPP

#include <Eigen/Dense>
#include <deque>

#include "gins_types.hpp"

namespace nav {
namespace gins {

class GinsEngine {
public:
    explicit GinsEngine(const GinsConfig &cfg);

    // ── 数据注入 ────────────────────────────────────────────────────
    // IMU：100 Hz，dtheta/dvel 必须是增量（已乘 dt）
    void addImu(const ImuSample &imu);

    // GNSS 观测：位置 / 速度 / 航向可分别有效
    void addGnss(const GnssObservation &obs);

    // 轮速观测（useOd 打开时才生效）
    void addOdo(const OdoSample &odo);

    // 激光先验地图定位（预留，useLidar 打开时生效）
    void addLidar(const LidarObservation &obs);

    // ── 外部可强制运动状态（内部也会自动判定）──────────────────────
    void setStationary(bool on);
    void setMotionMode(MotionMode m);

    // ── 推进一个 IMU 周期 ──────────────────────────────────────────
    // 调用 addImu() 之后调用；内部完成传播 + 所有观测更新 + 反馈
    void step();

    // ── 结果 ────────────────────────────────────────────────────────
    NavState                state() const { return toNavState(); }
    const Eigen::MatrixXd  &covariance() const { return Cov_; }
    double                  timestamp() const { return timestamp_; }
    bool                    healthy() const { return healthy_; }
    const GinsDebug        &debug() const { return debug_; }
    // 姿态（四元数 + DCM + 欧拉角）
    const nav::Attitude    &attitude() const { return pvacur_.att; }

    // 重置为新的配置（重新初始化滤波器）
    void reset(const GinsConfig &cfg);

private:
    // ── 内部流程 ────────────────────────────────────────────────────
    void initialize(const NavState &initstate, const NavState &initstateStd);
    void imuCompensate(ImuSample &imu);
    int  isToUpdate(double imutime1, double imutime2, double updatetime) const;
    void insPropagation(ImuSample &imupre, ImuSample &imucur);

    void gnssUpdate(GnssObservation &obs);
    void lidarUpdate(LidarObservation &obs);
    void odUpdate(const OdoSample &odo);
    void zuptZaruUpdate(const ImuSample &imu);
    void nhcUpdate();

    void EKFPredict(Eigen::MatrixXd &Phi, Eigen::MatrixXd &Qd);
    void EKFUpdate(Eigen::MatrixXd &dz, Eigen::MatrixXd &H, Eigen::MatrixXd &R);
    // 带卡方门限的更新；返回是否被接受
    bool EKFUpdateGated(Eigen::MatrixXd &dz, Eigen::MatrixXd &H,
                        Eigen::MatrixXd &R, double threshold, double *chi2Out);
    void stateFeedback();
    void checkCov();

    NavState toNavState() const;

    // ── 成员 ────────────────────────────────────────────────────────
    GinsConfig cfg_;

    int    rank_      = RANK_BASE;   // 状态维度（运行期）
    int    noiseRank_ = NOISE_RANK;  // 噪声维度

    double timestamp_ = 0.0;
    bool   healthy_   = true;
    bool   imuReady_  = false;

    // 上一次执行 1 Hz 约束检查的整秒数。
    // 不用 fmod(time,1.0)<dt/2 判断整秒边界：累积时间戳是浮点数，
    // 常常整个跳过 0 点，导致 ZUPT/ZARU/NHC 永不触发。
    double lastConstraintSecond_ = -1e30;

    ImuSample imupre_;
    ImuSample imucur_;

    // 待处理观测队列（按时间递增）
    std::deque<GnssObservation>  gnssQueue_;
    std::deque<OdoSample>        odoQueue_;
    std::deque<LidarObservation> lidarQueue_;

    // 运动状态
    MotionMode mode_       = MODE_UNKNOWN;
    bool       stationary_ = false;
    bool       forceStationary_ = false;
    bool       forceModeValid_  = false;
    MotionMode forceMode_       = MODE_UNKNOWN;

    // 导航状态
    PVA      pvacur_;
    PVA      pvapre_;
    ImuError imuerror_;

    // EKF
    Eigen::MatrixXd Cov_;   // rank_ × rank_
    Eigen::MatrixXd Qc_;    // noiseRank_ × noiseRank_
    Eigen::MatrixXd dx_;    // rank_ × 1

    GinsDebug debug_;
};

} // namespace gins
} // namespace nav

#endif // NAV_GINS_GINS_ENGINE_HPP
