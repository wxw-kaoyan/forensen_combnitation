#ifndef NAVCALIB_LEVERARMESTIMATOR_H
#define NAVCALIB_LEVERARMESTIMATOR_H

#include <QPointF>
#include <QString>
#include <QVector>

#include "modules/parse/parser/core/protocoldefs.h"

// ════════════════════════════════════════════════════════════════
//  水平杆臂标定 —— 纯计算，无 UI 依赖
//
//  物理模型（车辆绕竖直轴原地旋转）：
//      观测点位置 = 旋转中心 + R(ψ)·杆臂
//      N(t) = Xc + lx·cosψ − ly·sinψ
//      E(t) = Yc + lx·sinψ + ly·cosψ
//
//  四个未知量 [Xc, Yc, lx, ly]，每帧给两条线性方程，
//  最小二乘一次解出（与 MATLAB calibrate_lever_arm.m 同模型）。
//
//  输入只接受 0x0166 融合帧（位置 + 航向自洽），仅固定解参与解算。
// ════════════════════════════════════════════════════════════════
class LeverArmEstimator
{
public:
    struct Result {
        bool    valid      = false;  // 是否给出了数值解
        bool    moving     = false;  // 残差过大，车体可能在移动
        bool    enoughTurn = false;  // 航向跨度 ≥ 180°
        bool    fullTurn   = false;  // 航向跨度 ≥ 360°
        QString reason;              // 不可用原因 / 质量提示

        int     totalFrames    = 0;  // 收到的 0x0166 帧
        int     fixedFrames    = 0;  // 其中固定解帧
        int     usedFrames     = 0;  // 参与最终解算的帧（3σ 剔除后）
        int     rejectedFrames = 0;  // 3σ 剔除的帧

        double  durationSec = 0.0;
        double  yawSpanDeg  = 0.0;
        double  circles     = 0.0;

        double  centerN        = 0.0;  // 圆心局部北向（m）
        double  centerE        = 0.0;  // 圆心局部东向（m）
        double  lx             = 0.0;  // 观测点相对圆心：车体前向（m）
        double  ly             = 0.0;  // 观测点相对圆心：车体右向（m）
        double  residualRms    = 0.0;  // 拟合残差 RMS（m）
        double  srcSpreadN     = 0.0;  // 原始轨迹北向标准差（m）
        double  srcSpreadE     = 0.0;  // 原始轨迹东向标准差（m）
        double  projSpreadRms  = 0.0;  // 扣掉杆臂后的点云半径 RMS（m）
    };

    void reset();

    // 喂一帧 0x0166
    void addFusion(const FusionResult &f);

    // 用当前全部样本解一次（含一轮 3σ 野值剔除）
    Result solve() const;

    // 绘图用：原始轨迹 (x=东向 E, y=北向 N)，超过 maxPoints 时等间隔抽稀
    QVector<QPointF> rawTrack(int maxPoints = 800) const;

    // 绘图用：按给定杆臂补偿后的点云（应收缩为一个点）
    QVector<QPointF> projectedTrack(double lx, double ly, int maxPoints = 800) const;

private:
    struct Sample {
        double n;       // 局部北向（m）
        double e;       // 局部东向（m）
        double yawRad;  // 原始航向（rad；cos/sin 对 2π 周期不敏感，无需连续化）
    };

    static QVector<QPointF> decimate(const QVector<QPointF> &pts, int maxPoints);

    // 解 4 参数最小二乘；used 为参与解算的样本下标
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
