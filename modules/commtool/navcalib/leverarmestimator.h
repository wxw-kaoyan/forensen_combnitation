#ifndef NAVCALIB_LEVERARMESTIMATOR_H
#define NAVCALIB_LEVERARMESTIMATOR_H

#include <QPointF>
#include <QString>
#include <QVector>

#include <vector>

#include "modules/parse/parser/core/protocoldefs.h"

// ════════════════════════════════════════════════════════════════
//  水平杆臂标定 —— 纯计算，无 UI 依赖
//
//  两种解算方式，界面可切换：
//
//  ① 位置法（默认，与 MATLAB calibrate_lever_arm.m 一致）
//      N = C_N + lx·cosψ − ly·sinψ
//      E = C_E + lx·sinψ + ly·cosψ
//      4 个未知量（圆心 + 杆臂），每帧两条方程，Householder QR
//      求 coef = A\Y。圆心移动会污染结果，这是原生模型。
//
//  ② 速度法（备选）
//      位置公式对时间求导 v = Ċ + ω·R(ψ)·l，平移常量 Ċ 被消掉，
//      只剩 2 个未知量（杆臂）。整车平移不影响结果，也不需要输入
//      初始杆臂；但转速太慢（|v| < 0.2 m/s）时结果偏小，故带门限。
//
//  输入只接受 0x0166 融合帧，仅固定解参与解算。
// ════════════════════════════════════════════════════════════════
class LeverArmEstimator
{
public:
    enum class Method {
        Position,   // 位置法（4 参数，MATLAB 对齐）
        Velocity    // 速度法（2 参数）
    };

    struct Result {
        bool    valid      = false;  // 是否给出了数值解
        bool    moving     = false;  // 残差过大
        bool    enoughTurn = false;  // 航向跨度 ≥ 180°
        bool    fullTurn   = false;  // 航向跨度 ≥ 360°
        QString reason;              // 不可用原因 / 质量提示
        Method  method     = Method::Position;

        int     totalFrames    = 0;  // 收到的 0x0166 帧
        int     fixedFrames    = 0;  // 其中固定解帧
        int     usedFrames     = 0;  // 参与最终解算的帧（所选方法，3σ 剔除后）
        int     rejectedFrames = 0;  // 3σ 剔除的帧

        double  durationSec = 0.0;
        double  yawSpanDeg  = 0.0;
        double  circles     = 0.0;

        // ── 位置法结果（4 参数）──
        bool    posValid       = false;
        double  centerN        = 0.0;  // 圆心局部北向（m）
        double  centerE        = 0.0;  // 圆心局部东向（m）
        double  centerDrift    = 0.0;  // 分段圆心最大偏移（m）
        double  posLx          = 0.0;
        double  posLy          = 0.0;
        double  posResidualRms = 0.0;  // 位置残差 RMS（m）
        double  posStdLx       = 0.0;
        double  posStdLy       = 0.0;

        // ── 速度法结果（2 参数）──
        bool    velValid       = false;
        double  velLx          = 0.0;
        double  velLy          = 0.0;
        double  velResidualRms = 0.0;  // 速度残差 RMS（m/s）
        double  velStdLx       = 0.0;
        double  velStdLy       = 0.0;
        double  speedGate      = 0.0;  // 实际使用的速度门限（m/s），0 = 未过滤
        double  meanSpeed      = 0.0;  // 参与解算帧的平均速度（m/s）

        // ── 当前所选方法的结果（= pos* 或 vel*）──
        double  lx          = 0.0;
        double  ly          = 0.0;
        double  residualRms = 0.0;     // 位置法 m / 速度法 m/s
        double  leverStdLx  = 0.0;
        double  leverStdLy  = 0.0;

        // ── 分段一致性检查 ──
        // 把样本按时间等分成若干段，每段独立解一次杆臂。
        bool    leverChecked = false; // 数据够才做
        bool    leverStable  = false; // 各段杆臂一致
        int     segCount     = 0;     // 参与检查的分段数
        double  leverSpread  = 0.0;   // 各段杆臂相对整体的最大偏差（m）
    };

    void reset();

    // 喂一帧 0x0166
    void addFusion(const FusionResult &f);

    // 用当前全部样本解一次（两种方法都算，返回所选方法）
    Result solve(Method method = Method::Position) const;

    // 绘图用：原始轨迹 (x=东向 E, y=北向 N)，超过 maxPoints 时等间隔抽稀
    QVector<QPointF> rawTrack(int maxPoints = 800) const;

    // 绘图用：按给定杆臂补偿后的点云（位置法：正常应收缩为一个点）
    QVector<QPointF> projectedTrack(double lx, double ly, int maxPoints = 800) const;

private:
    struct Sample {
        double n;       // 局部北向（m）
        double e;       // 局部东向（m）
        double yawRad;  // 原始航向（rad，直接用组合导航输出）
        double vn;      // 组合导航北向速度（m/s）
        double ve;      // 组合导航东向速度（m/s）
        double wz;      // 陀螺 Z 轴角速度（rad/s）
    };

    static QVector<QPointF> decimate(const QVector<QPointF> &pts, int maxPoints);

    // 通用 Householder QR 最小二乘：解 rows×cols 超定方程，与 MATLAB A\Y 一致
    static bool solveQrLstsq(std::vector<double> &a, std::vector<double> &y,
                             int rows, int cols, std::vector<double> &x);

    // 位置法 4 参数最小二乘；used 为参与解算的样本下标
    static bool solveLeverArm(const QVector<Sample> &samples,
                              const QVector<int> &used,
                              double &centerN, double &centerE,
                              double &lx, double &ly,
                              QVector<double> &residualDist);

    QVector<Sample> m_samples;

    double m_lat0 = 0.0;      // 局部原点（第一帧固定解位置）
    double m_lon0 = 0.0;
    bool   m_hasOrigin = false;

    double m_lastYawRad = 0.0;   // 上一帧原始航向
    bool   m_hasYaw = false;
    double m_yawAccumRad = 0.0;  // 连续化后的累计航向变化（用于圈数/跨度）

    qint64 m_firstMs = -1;    // 第一帧固定解的绝对时间（周*604800000+周内ms）
    qint64 m_lastMs  = -1;

    int m_totalFrames = 0;
    int m_fixedFrames = 0;
};

#endif // NAVCALIB_LEVERARMESTIMATOR_H
