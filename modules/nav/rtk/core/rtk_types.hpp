// ============================================================================
//  nav/rtk/core/rtk_types.hpp
//  RTK 纯定位模块：常量、时间、观测、星历、解结构、配置
//
//  来源：RTKLIB (T.TAKASU, BSD-2)，改写为 C++ / Eigen，去掉全局变量与 IO
//  状态：x = [ p_ecef(3)  v_ecef(3)  N_1..N_m ]，m 随卫星数运行期变化
// ============================================================================
#ifndef NAV_RTK_RTK_TYPES_HPP
#define NAV_RTK_RTK_TYPES_HPP

#include <Eigen/Dense>
#include <array>
#include <vector>

#include "../../common/angle.hpp"

namespace nav {
namespace rtk {

using Eigen::Vector3d;

// ── 物理常量 ────────────────────────────────────────────────────────
constexpr double CLIGHT   = 299792458.0;            // 光速 [m/s]
constexpr double FREQ1    = 1.57542e9;              // L1 [Hz]
constexpr double FREQ2    = 1.22760e9;              // L2 [Hz]
constexpr double LAM1     = CLIGHT / FREQ1;         // L1 波长 [m]
constexpr double LAM2     = CLIGHT / FREQ2;         // L2 波长 [m]
constexpr double MU_GPS   = 3.986005000e14;         // 地球引力常数 [m^3/s^2]
constexpr double MU_GAL   = 3.986004418e14;
constexpr double MU_CMP   = 3.986004418e14;
constexpr double OMGE_GPS = 7.2921151467e-5;        // 地球自转 [rad/s]
constexpr double OMGE_GAL = 7.2921151467e-5;
constexpr double OMGE_CMP = 7.292115e-5;
constexpr double RE_WGS84 = 6378137.0;              // 长半轴 [m]
constexpr double FE_WGS84 = 1.0 / 298.257223563;    // 扁率
constexpr double HION     = 350000.0;               // 电离层单层高度 [m]
constexpr double RTOL_KEPLER = 1e-13;               // 开普勒迭代收敛门限
constexpr int    MAXITER_KEPLER = 30;

constexpr int    MAXSAT   = 64;
constexpr int    NFREQ    = 2;
constexpr int    MAXOBS   = 96;
constexpr int    MAXITR   = 8;      // SPP 最大迭代

// ── 卫星系统 ────────────────────────────────────────────────────────
enum SatSys : int {
    SYS_NONE = 0,
    SYS_GPS  = 1,
    SYS_GLO  = 2,
    SYS_GAL  = 4,
    SYS_CMP  = 8,
    SYS_QZS  = 16,
    SYS_SBS  = 32
};

// 卫星编号基址：sat = base + prn
constexpr int SAT_BASE_GPS = 0;
constexpr int SAT_BASE_GLO = 100;
constexpr int SAT_BASE_GAL = 200;
constexpr int SAT_BASE_CMP = 300;
constexpr int SAT_BASE_QZS = 400;
constexpr int SAT_BASE_SBS = 500;

// 解状态
enum SolStat : int {
    SOLQ_NONE   = 0,
    SOLQ_FIX    = 1,
    SOLQ_FLOAT  = 2,
    SOLQ_SBAS   = 3,
    SOLQ_DGPS   = 4,
    SOLQ_SINGLE = 5
};

// 定位模式
enum PosMode : int {
    PMODE_SINGLE = 0,   // 单点
    PMODE_DGPS   = 1,   // 差分伪距
    PMODE_KINEMA = 2,   // 动基线 RTK
    PMODE_STATIC = 3    // 静基线 RTK
};

// 电离层处理
enum IonoOpt : int {
    IONOOPT_NONE  = 0,
    IONOOPT_BRDC  = 1,   // 广播 Klobuchar
    IONOOPT_IFLC  = 3    // L1/L2 无电离层组合
};

// 对流层处理
enum TropOpt : int {
    TROPOPT_NONE = 0,
    TROPOPT_SAAS = 1
};

// ── 时间（GPS 周 + 周内秒）──────────────────────────────────────────
struct GTime {
    int    week = 0;
    double tow  = 0.0;    // s

    bool valid() const { return week != 0 || tow != 0.0; }
};

// ── 广播星历 ────────────────────────────────────────────────────────
struct Eph {
    int    sat  = 0;
    int    iode = -1;
    GTime  toc;           // 钟差参考时刻
    GTime  toe;           // 星历参考时刻
    GTime  ttr;           // 播发时刻

    double toes   = 0.0;  // toe 的周内秒
    double A      = 0.0;  // 长半轴 [m]
    double e      = 0.0;  // 偏心率
    double i0     = 0.0;  // 倾角 [rad]
    double OMG0   = 0.0;  // 升交点赤经 [rad]
    double omg    = 0.0;  // 近地点角距 [rad]
    double M0     = 0.0;  // 平近点角 [rad]
    double deln   = 0.0;  // 平均角速度改正 [rad/s]
    double OMGd   = 0.0;  // 升交点赤经变化率 [rad/s]
    double idot   = 0.0;  // 倾角变化率 [rad/s]
    double crc    = 0.0;
    double crs    = 0.0;
    double cuc    = 0.0;
    double cus    = 0.0;
    double cic    = 0.0;
    double cis    = 0.0;
    double f0     = 0.0;  // 钟差 [s]
    double f1     = 0.0;  // 钟漂 [s/s]
    double f2     = 0.0;  // 钟漂率 [s/s^2]
    double tgd[2] = {0.0, 0.0};   // 群延迟
    int    sva    = 0;    // URA 指数
    double fit    = 0.0;
    int    svh    = 0;    // 健康标志 0=健康

    bool valid() const { return A > 0.0; }
};

// ── 观测值 ──────────────────────────────────────────────────────────
struct Obs {
    GTime  time;
    int    sat = 0;
    double L[NFREQ]   = {0.0, 0.0};   // 载波相位（周）
    double P[NFREQ]   = {0.0, 0.0};   // 伪距 [m]
    double D[NFREQ]   = {0.0, 0.0};   // 多普勒 [Hz]
    double SNR[NFREQ] = {0.0, 0.0};   // 载噪比 [dBHz]
    int    LLI[NFREQ] = {0, 0};
    int    code[NFREQ] = {0, 0};

    bool hasL(int f) const { return L[f] != 0.0; }
    bool hasP(int f) const { return P[f] != 0.0; }
};

// ── 导航数据 ────────────────────────────────────────────────────────
struct Nav {
    std::vector<Eph> eph;                 // 广播星历
    double ion[8]     = {0.0};            // Klobuchar 参数
    double lam[MAXSAT][NFREQ] = {{0.0}};  // 各卫星波长

    void updateWavelengths();
    const Eph *findEph(int sat, const GTime &t) const;
};

// ── 解 ──────────────────────────────────────────────────────────────
struct Sol {
    GTime  time;
    int    stat = SOLQ_NONE;
    int    ns   = 0;          // 参与解算的卫星数
    Vector3d rr = Vector3d::Zero();   // ECEF 位置 [m]
    Vector3d qr = Vector3d::Zero();   // 位置标准差对角线 [m]
    double ratio = 0.0;       // LAMBDA 比率检验值
    double age   = 0.0;
};

// ── 卫星状态（用于调试输出）──────────────────────────────────────────
struct SatStat {
    int      sys = SYS_NONE;
    int      prn = 0;
    double   azel[2] = {0.0, 0.0};   // 方位角/高度角 [rad]
    double   resp[NFREQ] = {0.0, 0.0};  // 伪距残差
    double   resc[NFREQ] = {0.0, 0.0};  // 载波残差
    int      vsat[NFREQ] = {0, 0};
    int      fix = 0;
};

// ── 配置 ────────────────────────────────────────────────────────────
struct RtkConfig {
    PosMode  mode     = PMODE_KINEMA;
    IonoOpt  ionoopt  = IONOOPT_BRDC;
    TropOpt  tropopt  = TROPOPT_SAAS;

    double   elmin       = 10.0 * D2R;   // 高度角门限 [rad]
    double   snrmin      = 0.0;          // 载噪比门限 [dBHz]
    double   maxinno     = 5.0;          // 新息门限（米/周），<=0 关闭
    double   maxgdop     = 30.0;
    int      minSats     = 5;            // 最少卫星数

    // 观测噪声模型
    double   errCodeBase = 1.0;          // 伪距基准噪声 [m]
    double   errPhaseBase = 0.003;       // 载波基准噪声 [m]
    double   errElFactor = 1.0;          // 高度角加权系数

    // 状态噪声
    double   posProcStd  = 30.0;         // 位置过程噪声 [m/s]
    double   velProcStd  = 10.0;         // 速度过程噪声 [m/s^2]
    double   biasProcStd = 1e-4;         // 模糊度过程噪声 [m/s]

    // LAMBDA
    int      minFixSats  = 4;
    double   ratioThreshold = 3.0;       // 固定解比率检验门限

    // 初始协方差
    double   initPosStd = 30.0;
    double   initVelStd = 10.0;
    double   initBiasStd = 30.0;

    // 时间同步
    double   maxDiff = 0.5;              // 流动站/基站最大时差 [s]

    // 天线相位中心偏移（可留 0）
    Vector3d antPos = Vector3d::Zero();
};

} // namespace rtk
} // namespace nav

#endif // NAV_RTK_RTK_TYPES_HPP
