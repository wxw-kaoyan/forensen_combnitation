#pragma once

#include <QString>
#include <QVector>

/**
 * @brief Allan 方差分析核心算法
 *
 * 移植自 FORSENSE_LIB/ALLAN/allan_variance.m + allan_good_6axis.m
 *
 * 输入约定：每行 6 列 [ax ay az gx gy gz]，单位：加计 g，陀螺 deg/s
 *          每列独立计算 Allan 曲线
 *
 * 支持：
 *   - 非重叠 (non-overlapping) Allan 标准差
 *   - 自动 / 自定义 τ 数组
 *   - 噪声参数提取（随机游走、偏置不稳定性×3、偏置稳定性×1）
 */
class AllanAnalysis
{
public:
    /** 单轴计算结果 */
    struct AxisResult {
        QVector<double> tau;       // 聚类时间 (s)
        QVector<double> sigma;     // Allan 标准差 (原始单位)
        QVector<double> sigmaErr;  // 统计相对误差 (无量纲)
        QVector<int>    K;         // 每个 τ 的窗口数
    };

    /** 多轴结果：6 轴或任意 D 轴，按输入列顺序 */
    struct MultiResult {
        int    axisCount = 0;
        double fs        = 0;
        int    totalLen  = 0;
        QVector<AxisResult> axes;  // axes[0..D-1]
    };

    /** 加计噪声参数（输入 g → 输出 μg 与 m/s/√h） */
    struct AccelParams {
        double randomWalk          = 0;  // @ τ=1s   m/s/√h
        double biasInstability_10s = 0;  // @ τ=10s  μg
        double biasInstability_60s = 0;  // @ τ=60s  μg
        double biasInstability_200s = 0; // @ τ=200s μg
        double biasStabilityStatic = 0;  // 静态, μg
        bool   ok = false;
        QString notes;
    };

    /** 陀螺噪声参数（输入 deg/s → 输出 deg/h / deg/√h） */
    struct GyroParams {
        double randomWalk          = 0;  // @ τ=1s   deg/√h
        double biasInstability_10s = 0;  // @ τ=10s  deg/h
        double biasInstability_60s = 0;  // @ τ=60s  deg/h
        double biasInstability_200s = 0; // @ τ=200s deg/h
        double biasStabilityStatic = 0;  // 静态, deg/h
        bool   ok = false;
        QString notes;
    };

    // ---- τ 生成 ----

    /**
     * @brief logspace 风格自动生成 τ 数组
     * @param fs      采样率 (Hz)
     * @param N       数据长度 (样本数，每轴)
     * @param density 每个 decade 的点数，默认 200 与 MATLAB 版一致
     */
    static QVector<double> generateTau(double fs, int N, int density = 200);

    // ---- 核心计算 ----

    /**
     * @brief 非重叠 Allan 标准差 (单轴)
     * @param data 单轴样本序列
     * @param fs   采样率 (Hz)
     * @param tau  聚类时间数组 (s)，若为空则自动生成
     */
    static AxisResult compute(const QVector<double> &data, double fs,
                              const QVector<double> &tau = {});

    /**
     * @brief 6 轴同时计算
     * @param cols  6 列：[ax, ay, az, gx, gy, gz]，每列长度必须相同
     * @param fs    采样率 (Hz)
     * @param tau   若为空则自动生成
     */
    static MultiResult compute6(const QVector<QVector<double>> &cols, double fs,
                                const QVector<double> &tau = {});

    // ---- 噪声参数提取 ----

    /** 加计 3 轴 (输入单位 g) */
    static QVector<AccelParams> extractAccel3(const MultiResult &mr,
                                              const QVector<QVector<double>> &cols,
                                              double fs);

    /** 陀螺 3 轴 (输入单位 deg/s) */
    static QVector<GyroParams> extractGyro3(const MultiResult &mr,
                                            const QVector<QVector<double>> &cols,
                                            double fs);
};
