#ifndef TEMPDRIFTANALYSIS_H
#define TEMPDRIFTANALYSIS_H

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

// ════════════════════════════════════════════════════════════════
//  拉温（温度循环）分析
//
//  移植自桌面「拉温」目录的 test.m / test_temp.m：
//    1) 对 6 轴 + 温度做 mv_filter（窗口 fs*10+1 点，步长一半，50% 重叠）
//    2) 平滑后序列统计：峰峰值 max-min、标准差 std
//       加计 g → mg（×1000），陀螺保持 deg/s
//    3) 按温度变化率自动区分「斜坡段 / 保温平台段」，
//       零偏指标只取保温平台段（斜坡段的热梯度瞬态不计入）
//    4) 保温段上做 值-温度 线性拟合，得到温漂斜率（每 ℃）
//
//  输入 7 列：[ax ay az gx gy gz temp]，加计 g、陀螺 deg/s、温度 ℃
// ════════════════════════════════════════════════════════════════
class TempDriftAnalysis
{
public:
    /** 单轴指标（加计单位 mg，陀螺单位 deg/s） */
    struct AxisMetric {
        double ppAll    = 0;   // 全段峰峰值
        double stdAll   = 0;   // 全段标准差
        double ppSoak   = 0;   // 保温段峰峰值
        double stdSoak  = 0;   // 保温段标准差
        double slopePerC = 0;  // 保温段 值-温度 拟合斜率（每 ℃）
        int    soakPoints = 0;
        bool   ok = false;
    };

    /** 连续保温平台 */
    struct SoakSegment {
        int    startIndex = 0;   // 平滑序列起始索引
        int    count      = 0;   // 平滑点数
        double meanTemp   = 0;   // 平均温度 ℃
        double startSec   = 0;   // 起始时刻 s
        double durationSec = 0;  // 持续时长 s
    };

    struct Result {
        bool   ok = false;
        double fs = 0;
        int    totalSamples  = 0;
        int    windowSamples = 0;   // mv_filter 窗口点数 (count+1)
        int    stepSamples   = 0;   // mv_filter 步长点数
        int    outSamples    = 0;   // 平滑后点数
        double dtSec         = 0;   // 平滑后采样间隔 s
        double rampThreshold = 0;   // 保温判定阈值 ℃/min

        QVector<double> timeSec;            // 平滑序列中心时刻
        QVector<double> temp;               // 平滑温度 ℃
        QVector<QVector<double>> axes;      // 6 轴（加计 mg / 陀螺 deg/s）
        QVector<quint8> soakMask;           // 1 = 保温平台段
        AxisMetric metrics[6];
        QVector<SoakSegment> soakSegments;
        QStringList notes;
    };

    /**
     * @param cols              7 列：[ax ay az gx gy gz temp]
     * @param fs                采样率 Hz
     * @param rampThresholdCPerMin 保温判定阈值（|dT/dt| 小于它算保温），默认 0.5 ℃/min
     */
    static Result compute(const QVector<QVector<double>> &cols, double fs,
                          double rampThresholdCPerMin);

    static QString axisLabel(int i);   // 0..5 = 加计X/Y/Z、陀螺X/Y/Z
};

Q_DECLARE_METATYPE(TempDriftAnalysis::Result)

#endif // TEMPDRIFTANALYSIS_H
