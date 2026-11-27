#ifndef CALIB612ANALYSIS_H
#define CALIB612ANALYSIS_H

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

// ════════════════════════════════════════════════════════════════
//  6+12 位置标定（离线）
//
//  移植自 FORSENSE_LIB/Calibration 的
//    accel_calib.m（加计 12 位置：±X/±Y/±Z 六个姿态各两轮）
//    gyro_calib.m （陀螺 6 位置：绕 X/Y/Z 以 ±100°/s 正反各一转）
//
//  输入：6 列等长数据 [ax ay az gx gy gz]，加计单位 g，陀螺单位 deg/s
//
//  段起点不靠固定下标，而是按数值特征自动判定：
//    加计：ZUPT（1 s 窗陀螺 std<0.3、|mean|<5 °/s）+ 该轴 |a∓1 g|<0.1
//    陀螺：1 s 窗 std(‖ω‖)<0.2 且 ‖ω‖≈标定速率±10 °/s 保持 >1 s
//  命中点即该段起点，往后取 10 s 求均值。
//
//  参数求解：最小二乘 M = L·Aᵀ·(A·Aᵀ)⁻¹（A 为 4×6 真值矩阵，末行全 1）
//    m = S·t + b  ⇒  S = M(:,1:3)（含标度+交叉耦合），b = M(:,4)
// ════════════════════════════════════════════════════════════════
class Calib612Analysis
{
public:
    /** 检测到的一段（加计或陀螺） */
    struct Segment {
        int    dir        = -1;      // 0..5 = +X,-X,+Y,-Y,+Z,-Z
        int    startIndex = 0;       // 0-based 采样点
        int    sampleCount = 0;
        double mean[3]    = {0, 0, 0}; // 该段三轴测量均值（加计 g / 陀螺 deg/s）
    };

    struct Result {
        bool   accelOk = false;
        bool   gyroOk  = false;
        double fs      = 0;
        double gyroRate = 0;         // 标定角速率 deg/s
        int    totalSamples = 0;
        int    segmentSamples = 0;   // 每段采样点数

        // 加计
        double accelScale[3]  = {0, 0, 0};  // 标度误差 (S-1)*1000，千分比
        double accelBias[3]   = {0, 0, 0};  // 零偏 mg
        double accelInstall[3] = {0, 0, 0}; // 安装角 α β γ (deg)
        double accelMaxNonOrtho = 0;        // 最大非正交角 (deg)

        // 陀螺
        double gyroScale[3]   = {0, 0, 0};  // 标度误差 (S-1)*1000，千分比
        double gyroBias[3]    = {0, 0, 0};  // 零偏 deg/s
        double gyroMaxNonOrtho = 0;         // 最大非正交角 (deg)

        QVector<Segment> accelSegments;
        QVector<Segment> gyroSegments;
        QStringList      notes;   // 诊断信息（缺段原因等）
    };

    /**
     * @param cols          6 列：[ax ay az gx gy gz]
     * @param fs            采样率 (Hz)
     * @param gyroRate      陀螺标定角速率 (deg/s)，默认 100
     * @param segmentSeconds 每段时长 (s)，默认 10
     */
    static Result compute(const QVector<QVector<double>> &cols, double fs,
                          double gyroRate, double segmentSeconds = 10.0);

    static QString dirLabel(int dir);
};

Q_DECLARE_METATYPE(Calib612Analysis::Result)

#endif // CALIB612ANALYSIS_H
