// ============================================================================
//  nav/gins/core/gins_engine.cpp
//  GINS 松组合 EKF 引擎实现
//
//  来源：KF-GINS (i2Nav Group, Wuhan University), GPL-3.0
//  算法逐行保留；新增项见文件头注释
// ============================================================================
#include "gins_engine.hpp"

#include <cmath>
#include <limits>

#include "../../common/earth.hpp"
#include "../../common/rotation.hpp"
#include "ins_mech.hpp"

namespace nav {
namespace gins {

namespace {

// 把当前 IMU 增量线性内插到指定时刻（同时截断当前 IMU 数据）
void imuInterpolate(const ImuSample &imu1, ImuSample &imu2,
                    double timestamp, ImuSample &midimu)
{
    if (imu1.time > timestamp || imu2.time < timestamp) {
        midimu = ImuSample();
        midimu.time = timestamp;
        return;
    }

    const double lamda = (timestamp - imu1.time) / (imu2.time - imu1.time);

    midimu.time     = timestamp;
    midimu.dtheta   = imu2.dtheta * lamda;
    midimu.dvel     = imu2.dvel * lamda;
    midimu.dt       = timestamp - imu1.time;
    midimu.valid    = imu2.valid;
    midimu.temperature = imu2.temperature;

    imu2.dtheta = imu2.dtheta - midimu.dtheta;
    imu2.dvel   = imu2.dvel - midimu.dvel;
    imu2.dt     = imu2.dt - midimu.dt;
}

} // namespace

// ════════════════════════════════════════════════════════════════════
//  构造 / 重置
// ════════════════════════════════════════════════════════════════════
GinsEngine::GinsEngine(const GinsConfig &cfg)
{
    reset(cfg);
}

void GinsEngine::reset(const GinsConfig &cfg)
{
    cfg_ = cfg;

    // 状态维度运行期确定（当前只支持 RANK_BASE，扩维槽位已预留）
    rank_      = cfg_.rank;
    noiseRank_ = NOISE_RANK;
    if (rank_ < RANK_BASE)
        rank_ = RANK_BASE;

    timestamp_ = 0.0;
    healthy_   = true;
    imuReady_  = false;
    lastConstraintSecond_ = -1e30;

    Cov_.resize(rank_, rank_);
    Qc_.resize(noiseRank_, noiseRank_);
    dx_.resize(rank_, 1);
    Cov_.setZero();
    Qc_.setZero();
    dx_.setZero();

    // 系统噪声阵：六个对角块
    //   白噪声（ARW/VRW）直接取方差本身
    //   零偏/刻度按一阶高斯-马尔科夫取 2/τ·σ²
    const ImuNoise &n    = cfg_.imunoise;
    const double    corr = (n.corr_time > 1e-6) ? n.corr_time : 1e-6;

    Qc_.block(ARW_ID, ARW_ID, 3, 3) = n.gyr_arw.cwiseProduct(n.gyr_arw).asDiagonal();
    Qc_.block(VRW_ID, VRW_ID, 3, 3) = n.acc_vrw.cwiseProduct(n.acc_vrw).asDiagonal();
    Qc_.block(BGSTD_ID, BGSTD_ID, 3, 3) =
        2.0 / corr * n.gyrbias_std.cwiseProduct(n.gyrbias_std).asDiagonal();
    Qc_.block(BASTD_ID, BASTD_ID, 3, 3) =
        2.0 / corr * n.accbias_std.cwiseProduct(n.accbias_std).asDiagonal();
    Qc_.block(SGSTD_ID, SGSTD_ID, 3, 3) =
        2.0 / corr * n.gyrscale_std.cwiseProduct(n.gyrscale_std).asDiagonal();
    Qc_.block(SASTD_ID, SASTD_ID, 3, 3) =
        2.0 / corr * n.accscale_std.cwiseProduct(n.accscale_std).asDiagonal();

    initialize(cfg_.initstate, cfg_.initstateStd);

    gnssQueue_.clear();
    odoQueue_.clear();
    lidarQueue_.clear();

    mode_            = MODE_UNKNOWN;
    stationary_      = false;
    forceStationary_ = false;
    forceModeValid_  = false;
    forceMode_       = MODE_UNKNOWN;

    debug_ = GinsDebug();
}

void GinsEngine::initialize(const NavState &initstate, const NavState &initstateStd)
{
    // 导航状态初值
    pvacur_.pos       = initstate.pos;
    pvacur_.vel       = initstate.vel;
    pvacur_.att.euler = initstate.euler;
    pvacur_.att.qbn   = Rotation::euler2quaternion(pvacur_.att.euler);
    pvacur_.att.cbn   = Rotation::euler2matrix(pvacur_.att.euler);
    imuerror_         = initstate.imuerror;

    pvapre_ = pvacur_;

    // 协方差初值
    const ImuError &eStd       = initstateStd.imuerror;
    Cov_.block(P_ID, P_ID, 3, 3)     = initstateStd.pos.cwiseProduct(initstateStd.pos).asDiagonal();
    Cov_.block(V_ID, V_ID, 3, 3)     = initstateStd.vel.cwiseProduct(initstateStd.vel).asDiagonal();
    Cov_.block(PHI_ID, PHI_ID, 3, 3) = initstateStd.euler.cwiseProduct(initstateStd.euler).asDiagonal();
    Cov_.block(BG_ID, BG_ID, 3, 3)   = eStd.gyrbias.cwiseProduct(eStd.gyrbias).asDiagonal();
    Cov_.block(BA_ID, BA_ID, 3, 3)   = eStd.accbias.cwiseProduct(eStd.accbias).asDiagonal();
    Cov_.block(SG_ID, SG_ID, 3, 3)   = eStd.gyrscale.cwiseProduct(eStd.gyrscale).asDiagonal();
    Cov_.block(SA_ID, SA_ID, 3, 3)   = eStd.accscale.cwiseProduct(eStd.accscale).asDiagonal();
}

// ════════════════════════════════════════════════════════════════════
//  数据注入
// ════════════════════════════════════════════════════════════════════
void GinsEngine::addImu(const ImuSample &imu)
{
    if (!imu.valid || imu.dt <= 0.0)
        return;

    if (!imuReady_) {
        // 首个 IMU 只作为基准，不推进
        imucur_  = imu;
        imupre_  = imu;
        timestamp_ = imu.time;
        imuReady_ = true;
        return;
    }

    imupre_ = imucur_;
    imucur_ = imu;
}

void GinsEngine::addGnss(const GnssObservation &obs)
{
    if (!obs.isValid)
        return;
    gnssQueue_.push_back(obs);

    // 队列过长时丢弃最旧的（防止上游异常导致内存增长）
    while (gnssQueue_.size() > 200)
        gnssQueue_.pop_front();
}

void GinsEngine::addOdo(const OdoSample &odo)
{
    if (!odo.valid)
        return;
    odoQueue_.push_back(odo);
    while (odoQueue_.size() > 200)
        odoQueue_.pop_front();
}

void GinsEngine::addLidar(const LidarObservation &obs)
{
    if (!obs.isValid)
        return;
    lidarQueue_.push_back(obs);
    while (lidarQueue_.size() > 200)
        lidarQueue_.pop_front();
}

void GinsEngine::setStationary(bool on)
{
    forceStationary_ = on;
}

void GinsEngine::setMotionMode(MotionMode m)
{
    forceModeValid_ = (m != MODE_UNKNOWN);
    forceMode_      = m;
}

// ════════════════════════════════════════════════════════════════════
//  主流程：推进一个 IMU 周期
// ════════════════════════════════════════════════════════════════════
void GinsEngine::step()
{
    if (!imuReady_ || imucur_.dt <= 0.0 || imucur_.time <= imupre_.time)
        return;

    timestamp_        = imucur_.time;
    debug_.timestamp  = timestamp_;
    debug_.zuptApplied = false;
    debug_.zaruApplied = false;
    debug_.nhcApplied  = false;
    debug_.odApplied   = false;
    debug_.gnssPosApplied = false;
    debug_.gnssVelApplied = false;
    debug_.headingApplied = false;

    // ── 1. 取本周期内到达的 GNSS 观测（10 Hz vs 100 Hz，每周期最多一个）──
    GnssObservation obs;
    bool hasObs = false;
    while (!gnssQueue_.empty() &&
           gnssQueue_.front().time <= imucur_.time + cfg_.timeAlignErr) {
        if (gnssQueue_.front().isValid) {
            obs    = gnssQueue_.front();
            hasObs = true;
        }
        gnssQueue_.pop_front();
    }

    // ── 2. 时间对齐 + 传播 + 观测更新 ──
    const int res = hasObs ? isToUpdate(imupre_.time, imucur_.time, obs.time) : 0;

    if (res == 0) {
        insPropagation(imupre_, imucur_);
    } else if (res == 1) {
        // 观测靠近上一历元：先对上一历元更新，再传播
        gnssUpdate(obs);
        stateFeedback();
        pvapre_ = pvacur_;
        insPropagation(imupre_, imucur_);
    } else if (res == 2) {
        // 观测靠近当前历元：先传播，再更新
        insPropagation(imupre_, imucur_);
        gnssUpdate(obs);
        stateFeedback();
    } else {
        // 观测落在两历元之间：把 IMU 内插到观测时刻，分两段传播
        ImuSample midimu;
        imuInterpolate(imupre_, imucur_, obs.time, midimu);

        insPropagation(imupre_, midimu);
        gnssUpdate(obs);
        stateFeedback();

        pvapre_ = pvacur_;
        insPropagation(midimu, imucur_);
    }

    // ── 3. 激光观测（预留，useLidar 打开才生效）──
    if (cfg_.useLidar && !lidarQueue_.empty()) {
        LidarObservation lo;
        bool hasLo = false;
        while (!lidarQueue_.empty() &&
               lidarQueue_.front().time <= imucur_.time + cfg_.timeAlignErr) {
            if (lidarQueue_.front().isValid) {
                lo    = lidarQueue_.front();
                hasLo = true;
            }
            lidarQueue_.pop_front();
        }
        if (hasLo) {
            lidarUpdate(lo);
            stateFeedback();
        }
    }

    // ── 4. 轮速观测（useOd 打开才生效）──
    if (cfg_.useOd && !odoQueue_.empty()) {
        OdoSample odo;
        bool hasOdo = false;
        while (!odoQueue_.empty() &&
               odoQueue_.front().time <= imucur_.time + cfg_.timeAlignErr) {
            if (odoQueue_.front().valid) {
                odo    = odoQueue_.front();
                hasOdo = true;
            }
            odoQueue_.pop_front();
        }
        if (hasOdo)
            odUpdate(odo);
    }

    // ── 5. 1 Hz 检查：静止判定 + ZUPT / ZARU / NHC ──
    // 用"整秒数递增"判定，不依赖 fmod(time,1) 的浮点对齐
    const double secNow = std::floor(imucur_.time);
    if (secNow > lastConstraintSecond_) {
        lastConstraintSecond_ = secNow;

        const Vector3d omega = imucur_.dtheta / imucur_.dt;
        const Vector3d acc   = imucur_.dvel / imucur_.dt;

        // 静止判定：角速度小 + 比力模长≈g + 当前速度估计也小。
        // 只用陀螺/加计无法区分"静止"与"直线匀速行驶"，必须再约束速度
        const bool detected =
            omega.norm() < cfg_.zuptGyroThreshold &&
            std::fabs(acc.norm() - Earth::gravity(pvacur_.pos)) < cfg_.zuptAccThreshold &&
            pvacur_.vel.norm() < cfg_.zuptVelThreshold;

        stationary_ = forceStationary_ || detected;

        // 运动模式
        if (forceModeValid_) {
            mode_ = forceMode_;
        } else if (stationary_) {
            mode_ = MODE_STATIC;
        } else if (pvacur_.vel.norm() < cfg_.nhcMinSpeed) {
            mode_ = MODE_UNKNOWN;
        } else {
            // 用 n 系 Z 轴角速度判断转弯（>5°/s 视为转弯）
            const double yawRate = omega[2];
            mode_ = (std::fabs(yawRate) > 5.0 * D2R) ? MODE_TURNING : MODE_STRAIGHT;
        }
        debug_.mode = mode_;

        if (stationary_)
            zuptZaruUpdate(imucur_);

        if (cfg_.useNhc)
            nhcUpdate();
    }

    // ── 6. 协方差健康检查（原版 std::exit，这里改为钳位 + 标志）──
    checkCov();

    // ── 7. 记录上一时刻 ──
    pvapre_ = pvacur_;
    imupre_ = imucur_;
}

int GinsEngine::isToUpdate(double imutime1, double imutime2, double updatetime) const
{
    const double eps = cfg_.timeAlignErr;

    if (std::fabs(imutime1 - updatetime) < eps)
        return 1;   // 靠近上一历元
    if (std::fabs(imutime2 - updatetime) <= eps)
        return 2;   // 靠近当前历元
    if (imutime1 < updatetime && updatetime < imutime2)
        return 3;   // 落在两历元之间，需要内插
    return 0;       // 不在区间内，忽略
}

// ════════════════════════════════════════════════════════════════════
//  IMU 误差补偿
// ════════════════════════════════════════════════════════════════════
void GinsEngine::imuCompensate(ImuSample &imu)
{
    // 零偏
    imu.dtheta -= imuerror_.gyrbias * imu.dt;
    imu.dvel   -= imuerror_.accbias * imu.dt;

    // 刻度因数
    const Vector3d gyrscale = Vector3d::Ones() + imuerror_.gyrscale;
    const Vector3d accscale = Vector3d::Ones() + imuerror_.accscale;
    imu.dtheta = imu.dtheta.cwiseProduct(gyrscale.cwiseInverse());
    imu.dvel   = imu.dvel.cwiseProduct(accscale.cwiseInverse());
}

// ════════════════════════════════════════════════════════════════════
//  INS 传播 + F/Q 矩阵 + EKF 预测
// ════════════════════════════════════════════════════════════════════
void GinsEngine::insPropagation(ImuSample &imupre, ImuSample &imucur)
{
    imuCompensate(imucur);
    InsMech::insMech(pvapre_, pvacur_, imupre, imucur);

    Eigen::MatrixXd Phi, F, Qd, G;
    Phi.resizeLike(Cov_);
    F.resizeLike(Cov_);
    Qd.resizeLike(Cov_);
    G.resize(rank_, noiseRank_);
    Phi.setIdentity();
    F.setZero();
    Qd.setZero();
    G.setZero();

    const Eigen::Vector2d rmrn = Earth::meridianPrimeVerticalRadius(pvapre_.pos[0]);
    const double gravity       = Earth::gravity(pvapre_.pos);
    Vector3d wie_n, wen_n;
    wie_n << WGS84_WIE * std::cos(pvapre_.pos[0]), 0.0, -WGS84_WIE * std::sin(pvapre_.pos[0]);
    wen_n << pvapre_.vel[1] / (rmrn[1] + pvapre_.pos[2]),
            -pvapre_.vel[0] / (rmrn[0] + pvapre_.pos[2]),
            -pvapre_.vel[1] * std::tan(pvapre_.pos[0]) / (rmrn[1] + pvapre_.pos[2]);

    const double rmh = rmrn[0] + pvapre_.pos[2];
    const double rnh = rmrn[1] + pvapre_.pos[2];
    const Vector3d accel = imucur.dvel / imucur.dt;
    const Vector3d omega = imucur.dtheta / imucur.dt;

    Matrix3d temp;

    // ── 位置误差 ──
    temp.setZero();
    temp(0, 0) = -pvapre_.vel[2] / rmh;
    temp(0, 2) =  pvapre_.vel[0] / rmh;
    temp(1, 0) =  pvapre_.vel[1] * std::tan(pvapre_.pos[0]) / rnh;
    temp(1, 1) = -(pvapre_.vel[2] + pvapre_.vel[0] * std::tan(pvapre_.pos[0])) / rnh;
    temp(1, 2) =  pvapre_.vel[1] / rnh;
    F.block(P_ID, P_ID, 3, 3) = temp;
    F.block(P_ID, V_ID, 3, 3) = Matrix3d::Identity();

    // ── 速度误差 ──
    temp.setZero();
    temp(0, 0) = -2.0 * pvapre_.vel[1] * WGS84_WIE * std::cos(pvapre_.pos[0]) / rmh
                 - std::pow(pvapre_.vel[1], 2) / rmh / rnh
                       / std::pow(std::cos(pvapre_.pos[0]), 2);
    temp(0, 2) = pvapre_.vel[0] * pvapre_.vel[2] / rmh / rmh
                 - std::pow(pvapre_.vel[1], 2) * std::tan(pvapre_.pos[0]) / rnh / rnh;
    temp(1, 0) = 2.0 * WGS84_WIE * (pvapre_.vel[0] * std::cos(pvapre_.pos[0])
                                    - pvapre_.vel[2] * std::sin(pvapre_.pos[0])) / rmh
                 + pvapre_.vel[0] * pvapre_.vel[1] / rmh / rnh
                       / std::pow(std::cos(pvapre_.pos[0]), 2);
    temp(1, 2) = (pvapre_.vel[1] * pvapre_.vel[2]
                  + pvapre_.vel[0] * pvapre_.vel[1] * std::tan(pvapre_.pos[0])) / rnh / rnh;
    temp(2, 0) = 2.0 * WGS84_WIE * pvapre_.vel[1] * std::sin(pvapre_.pos[0]) / rmh;
    temp(2, 2) = -std::pow(pvapre_.vel[1], 2) / rnh / rnh
                 - std::pow(pvapre_.vel[0], 2) / rmh / rmh
                 + 2.0 * gravity / (std::sqrt(rmrn[0] * rmrn[1]) + pvapre_.pos[2]);
    F.block(V_ID, P_ID, 3, 3) = temp;

    temp.setZero();
    temp(0, 0) = pvapre_.vel[2] / rmh;
    temp(0, 1) = -2.0 * (WGS84_WIE * std::sin(pvapre_.pos[0])
                        + pvapre_.vel[1] * std::tan(pvapre_.pos[0]) / rnh);
    temp(0, 2) = pvapre_.vel[0] / rmh;
    temp(1, 0) = 2.0 * WGS84_WIE * std::sin(pvapre_.pos[0])
                 + pvapre_.vel[1] * std::tan(pvapre_.pos[0]) / rnh;
    temp(1, 1) = (pvapre_.vel[2] + pvapre_.vel[0] * std::tan(pvapre_.pos[0])) / rnh;
    temp(1, 2) = 2.0 * WGS84_WIE * std::cos(pvapre_.pos[0]) + pvapre_.vel[1] / rnh;
    temp(2, 0) = -2.0 * pvapre_.vel[0] / rmh;
    temp(2, 1) = -2.0 * (WGS84_WIE * std::cos(pvapre_.pos[0]) + pvapre_.vel[1] / rnh);
    F.block(V_ID, V_ID, 3, 3)   = temp;
    F.block(V_ID, PHI_ID, 3, 3) = Rotation::skewSymmetric(pvapre_.att.cbn * accel);
    F.block(V_ID, BA_ID, 3, 3)  = pvapre_.att.cbn;
    F.block(V_ID, SA_ID, 3, 3)  = pvapre_.att.cbn * accel.asDiagonal();

    // ── 姿态误差 ──
    temp.setZero();
    temp(0, 0) = -WGS84_WIE * std::sin(pvapre_.pos[0]) / rmh;
    temp(0, 2) = pvapre_.vel[1] / rnh / rnh;
    temp(1, 2) = -pvapre_.vel[0] / rmh / rmh;
    temp(2, 0) = -WGS84_WIE * std::cos(pvapre_.pos[0]) / rmh
                 - pvapre_.vel[1] / rmh / rnh / std::pow(std::cos(pvapre_.pos[0]), 2);
    temp(2, 2) = -pvapre_.vel[1] * std::tan(pvapre_.pos[0]) / rnh / rnh;
    F.block(PHI_ID, P_ID, 3, 3) = temp;

    temp.setZero();
    temp(0, 1) = 1.0 / rnh;
    temp(1, 0) = -1.0 / rmh;
    temp(2, 1) = -std::tan(pvapre_.pos[0]) / rnh;
    F.block(PHI_ID, V_ID, 3, 3)   = temp;
    F.block(PHI_ID, PHI_ID, 3, 3) = -Rotation::skewSymmetric(wie_n + wen_n);
    F.block(PHI_ID, BG_ID, 3, 3)  = -pvapre_.att.cbn;
    F.block(PHI_ID, SG_ID, 3, 3)  = -pvapre_.att.cbn * omega.asDiagonal();

    // ── IMU 零偏 / 刻度：一阶高斯-马尔科夫 ──
    const double corr = (cfg_.imunoise.corr_time > 1e-6) ? cfg_.imunoise.corr_time : 1e-6;
    F.block(BG_ID, BG_ID, 3, 3) = -1.0 / corr * Matrix3d::Identity();
    F.block(BA_ID, BA_ID, 3, 3) = -1.0 / corr * Matrix3d::Identity();
    F.block(SG_ID, SG_ID, 3, 3) = -1.0 / corr * Matrix3d::Identity();
    F.block(SA_ID, SA_ID, 3, 3) = -1.0 / corr * Matrix3d::Identity();

    // ── 噪声驱动矩阵 ──
    G.block(V_ID, VRW_ID, 3, 3)    = pvapre_.att.cbn;
    G.block(PHI_ID, ARW_ID, 3, 3)  = pvapre_.att.cbn;
    G.block(BG_ID, BGSTD_ID, 3, 3) = Matrix3d::Identity();
    G.block(BA_ID, BASTD_ID, 3, 3) = Matrix3d::Identity();
    G.block(SG_ID, SGSTD_ID, 3, 3) = Matrix3d::Identity();
    G.block(SA_ID, SASTD_ID, 3, 3) = Matrix3d::Identity();

    // ── 状态转移矩阵（一阶近似）──
    Phi = Phi + F * imucur.dt;

    // ── 传播噪声 ──
    Qd = G * Qc_ * G.transpose() * imucur.dt;
    Qd = (Phi * Qd * Phi.transpose() + Qd) / 2.0;

    EKFPredict(Phi, Qd);
}

// ════════════════════════════════════════════════════════════════════
//  GNSS 更新：位置（带卡方）/ 速度（带卡方）/ 航向
// ════════════════════════════════════════════════════════════════════
void GinsEngine::gnssUpdate(GnssObservation &obs)
{
    // ── 位置 ──
    if (cfg_.useGnssPos && obs.hasPos) {
        const Matrix3d Dr_inv = Earth::DRi(pvacur_.pos);
        const Matrix3d Dr     = Earth::DR(pvacur_.pos);

        // IMU 位置 -> GNSS 天线相位中心
        const Vector3d ant = pvacur_.pos + Dr_inv * pvacur_.att.cbn * cfg_.antlever;

        Eigen::MatrixXd dz = Dr * (ant - obs.blh);

        Eigen::MatrixXd H(3, rank_);
        H.setZero();
        H.block(0, P_ID, 3, 3)   = Matrix3d::Identity();
        H.block(0, PHI_ID, 3, 3) = Rotation::skewSymmetric(pvacur_.att.cbn * cfg_.antlever);

        Eigen::MatrixXd R = obs.stdPos.cwiseProduct(obs.stdPos).asDiagonal();
        // 防止上游给 0 → R 奇异
        for (int i = 0; i < 3; ++i)
            if (R(i, i) < 1e-6) R(i, i) = 1e-6;

        debug_.gnssPosInnov = dz;
        debug_.gnssPosApplied =
            EKFUpdateGated(dz, H, R, cfg_.chi2Threshold, &debug_.gnssPosChi2);
    }

    // ── 速度 ──
    if (cfg_.useGnssVel && obs.hasVel) {
        Eigen::MatrixXd dz = pvacur_.vel - obs.vel;

        Eigen::MatrixXd H(3, rank_);
        H.setZero();
        H.block(0, V_ID, 3, 3) = Matrix3d::Identity();

        Eigen::MatrixXd R = obs.stdVel.cwiseProduct(obs.stdVel).asDiagonal();
        for (int i = 0; i < 3; ++i)
            if (R(i, i) < 1e-6) R(i, i) = 1e-6;

        debug_.gnssVelInnov = dz;
        debug_.gnssVelApplied =
            EKFUpdateGated(dz, H, R, cfg_.chi2Threshold, &debug_.gnssVelChi2);
    }

    // ── 双天线航向 ──
    if (cfg_.useHeading && obs.hasHeading) {
        double dyaw = obs.heading - pvacur_.att.euler[2];
        while (dyaw > PI)  dyaw -= 2.0 * PI;
        while (dyaw < -PI) dyaw += 2.0 * PI;

        Eigen::MatrixXd dz(1, 1);
        dz(0, 0) = dyaw;

        Eigen::MatrixXd H(1, rank_);
        H.setZero();
        H(0, PHI_ID + 2) = 1.0;

        Eigen::MatrixXd R(1, 1);
        const double s = (obs.stdHeading > 1e-6) ? obs.stdHeading : 1e-6;
        R(0, 0) = s * s;

        debug_.headingInnov   = dyaw;
        debug_.headingApplied =
            EKFUpdateGated(dz, H, R, cfg_.chi2Threshold1D, nullptr);
    }

    obs.isValid = false;
}

// ════════════════════════════════════════════════════════════════════
//  激光先验地图定位（3 维位置，与 GNSS 位置同构）
// ════════════════════════════════════════════════════════════════════
void GinsEngine::lidarUpdate(LidarObservation &obs)
{
    const Matrix3d Dr_inv = Earth::DRi(pvacur_.pos);
    const Matrix3d Dr     = Earth::DR(pvacur_.pos);

    const Vector3d ant = pvacur_.pos + Dr_inv * pvacur_.att.cbn * cfg_.antlever;

    Eigen::MatrixXd dz = Dr * (ant - obs.pos);

    Eigen::MatrixXd H(3, rank_);
    H.setZero();
    H.block(0, P_ID, 3, 3)   = Matrix3d::Identity();
    H.block(0, PHI_ID, 3, 3) = Rotation::skewSymmetric(pvacur_.att.cbn * cfg_.antlever);

    // R 自适应：匹配质量 q 越低，噪声放大越多（q→0.1 时放大 10 倍）
    const double q = (obs.quality > 0.1) ? obs.quality : 0.1;
    Eigen::MatrixXd R = (obs.std.cwiseProduct(obs.std) / (q * q)).asDiagonal();
    for (int i = 0; i < 3; ++i)
        if (R(i, i) < 1e-6) R(i, i) = 1e-6;

    EKFUpdateGated(dz, H, R, cfg_.chi2Threshold, nullptr);

    obs.isValid = false;
}

// ════════════════════════════════════════════════════════════════════
//  轮速观测（1 维，车体系前向速度）
// ════════════════════════════════════════════════════════════════════
void GinsEngine::odUpdate(const OdoSample &odo)
{
    const Matrix3d cnb = pvacur_.att.cbn.transpose();
    const Vector3d v_n = pvacur_.vel;
    const double   v_bx = (cnb * v_n)[0];

    Eigen::MatrixXd dz(1, 1);
    dz(0, 0) = v_bx - odo.vel;

    Eigen::MatrixXd H(1, rank_);
    H.setZero();
    H.block(0, V_ID, 1, 3)   = cnb.block(0, 0, 1, 3);
    H.block(0, PHI_ID, 1, 3) = (-cnb * Rotation::skewSymmetric(v_n)).block(0, 0, 1, 3);

    Eigen::MatrixXd R(1, 1);
    const double s = (odo.std > 1e-6) ? odo.std : 1e-6;
    R(0, 0) = s * s;

    debug_.odApplied = EKFUpdateGated(dz, H, R, cfg_.chi2Threshold1D, nullptr);
    if (debug_.odApplied)
        stateFeedback();
}

// ════════════════════════════════════════════════════════════════════
//  ZUPT（零速）+ ZARU（零角速率）
// ════════════════════════════════════════════════════════════════════
void GinsEngine::zuptZaruUpdate(const ImuSample &imu)
{
    // ── ZUPT：速度观测为 0 ──
    if (cfg_.useZupt) {
        Eigen::MatrixXd dz = pvacur_.vel;

        Eigen::MatrixXd H(3, rank_);
        H.setZero();
        H.block(0, V_ID, 3, 3) = Matrix3d::Identity();

        Eigen::MatrixXd R = Matrix3d::Identity() * cfg_.zuptVelStd * cfg_.zuptVelStd;

        EKFUpdate(dz, H, R);
        stateFeedback();
        debug_.zuptApplied = true;
    }

    // ── ZARU：角速率观测为 0（角度增量已做过零偏补偿）──
    //     ω_comp = ω_true + b_true − b̂ = dx_BG，故 H 在零偏块取 +I
    if (cfg_.useZaru) {
        const Vector3d omega = imu.dtheta / imu.dt;

        Eigen::MatrixXd dz(3, 1);
        dz.block(0, 0, 3, 1) = omega;

        Eigen::MatrixXd H(3, rank_);
        H.setZero();
        H.block(0, BG_ID, 3, 3) = Matrix3d::Identity();

        Eigen::MatrixXd R = Matrix3d::Identity() * cfg_.zaruGyroStd * cfg_.zaruGyroStd;

        EKFUpdate(dz, H, R);
        stateFeedback();
        debug_.zaruApplied = true;
    }
}

// ════════════════════════════════════════════════════════════════════
//  NHC 非完整约束：车体横向 / 垂向速度为零（2 维）
//
//  v^b = C_n^b · v^n，取 y、z 两行
//  δv^b = C_n^b · δv^n − C_n^b · [v^n ×] · δφ
// ════════════════════════════════════════════════════════════════════
void GinsEngine::nhcUpdate()
{
    if (stationary_)
        return;                                  // 静止交给 ZUPT
    if (pvacur_.vel.norm() < cfg_.nhcMinSpeed)
        return;                                  // 低速时 NHC 不成立

    const Matrix3d cnb = pvacur_.att.cbn.transpose();
    const Vector3d v_n = pvacur_.vel;
    const Vector3d v_b = cnb * v_n;

    Eigen::MatrixXd dz(2, 1);
    dz(0, 0) = v_b[1];   // 横向
    dz(1, 0) = v_b[2];   // 垂向

    Eigen::MatrixXd H(2, rank_);
    H.setZero();
    H.block(0, V_ID, 2, 3)   = cnb.block(1, 0, 2, 3);
    H.block(0, PHI_ID, 2, 3) = (-cnb * Rotation::skewSymmetric(v_n)).block(1, 0, 2, 3);

    double sigLat = cfg_.nhcLateralStd;
    double sigVer = cfg_.nhcVerticalStd;
    if (mode_ == MODE_TURNING) {
        // 转弯时侧滑明显，NHC 降权
        sigLat *= 2.0;
        sigVer *= 2.0;
    }

    Eigen::MatrixXd R(2, 2);
    R.setZero();
    R(0, 0) = sigLat * sigLat;
    R(1, 1) = sigVer * sigVer;

    EKFUpdate(dz, H, R);
    stateFeedback();
    debug_.nhcApplied = true;
}

// ════════════════════════════════════════════════════════════════════
//  EKF 预测 / 更新 / 反馈
// ════════════════════════════════════════════════════════════════════
void GinsEngine::EKFPredict(Eigen::MatrixXd &Phi, Eigen::MatrixXd &Qd)
{
    Cov_ = Phi * Cov_ * Phi.transpose() + Qd;
    dx_  = Phi * dx_;
}

void GinsEngine::EKFUpdate(Eigen::MatrixXd &dz, Eigen::MatrixXd &H, Eigen::MatrixXd &R)
{
    const Eigen::MatrixXd S = H * Cov_ * H.transpose() + R;
    const Eigen::MatrixXd K = Cov_ * H.transpose() * S.inverse();

    Eigen::MatrixXd I = Eigen::MatrixXd::Identity(rank_, rank_);
    I = I - K * H;

    dx_  = dx_ + K * (dz - H * dx_);
    Cov_ = I * Cov_ * I.transpose() + K * R * K.transpose();
}

bool GinsEngine::EKFUpdateGated(Eigen::MatrixXd &dz, Eigen::MatrixXd &H,
                                Eigen::MatrixXd &R, double threshold, double *chi2Out)
{
    const Eigen::MatrixXd S = H * Cov_ * H.transpose() + R;
    const double gamma      = (dz.transpose() * S.inverse() * dz)(0, 0);

    if (chi2Out)
        *chi2Out = gamma;

    if (!std::isfinite(gamma) || gamma >= threshold)
        return false;

    EKFUpdate(dz, H, R);
    return true;
}

void GinsEngine::stateFeedback()
{
    // 位置（BLH 误差 -> 位置增量）
    const Vector3d delta_r = dx_.block(P_ID, 0, 3, 1);
    pvacur_.pos -= Earth::DRi(pvacur_.pos) * delta_r;

    // 速度
    pvacur_.vel -= dx_.block(V_ID, 0, 3, 1);

    // 姿态（乘性误差，做左乘旋转）
    const Vector3d dphi = dx_.block(PHI_ID, 0, 3, 1);
    const Quaterniond qpn = Rotation::rotvec2quaternion(dphi);
    pvacur_.att.qbn   = (qpn * pvacur_.att.qbn).normalized();
    pvacur_.att.cbn   = Rotation::quaternion2matrix(pvacur_.att.qbn);
    pvacur_.att.euler = Rotation::matrix2euler(pvacur_.att.cbn);

    // IMU 零偏 / 刻度
    imuerror_.gyrbias  += dx_.block(BG_ID, 0, 3, 1);
    imuerror_.accbias  += dx_.block(BA_ID, 0, 3, 1);
    imuerror_.gyrscale += dx_.block(SG_ID, 0, 3, 1);
    imuerror_.accscale += dx_.block(SA_ID, 0, 3, 1);

    dx_.setZero();
}

// ════════════════════════════════════════════════════════════════════
//  协方差健康检查：不杀进程，改为钳位 + 健康标志
// ════════════════════════════════════════════════════════════════════
void GinsEngine::checkCov()
{
    healthy_ = true;
    for (int i = 0; i < rank_; ++i) {
        double &d = Cov_(i, i);
        if (!std::isfinite(d) || d < 0.0) {
            d        = 1e-6;
            healthy_ = false;
        } else if (d > cfg_.covMaxDiagonal) {
            d        = cfg_.covMaxDiagonal;
            healthy_ = false;
        }
    }
    debug_.healthy = healthy_;
}

NavState GinsEngine::toNavState() const
{
    NavState s;
    s.pos      = pvacur_.pos;
    s.vel      = pvacur_.vel;
    s.euler    = pvacur_.att.euler;
    s.imuerror = imuerror_;
    return s;
}

} // namespace gins
} // namespace nav
