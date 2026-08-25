#ifndef ANALYSIS_RAW_IMUREADER_H
#define ANALYSIS_RAW_IMUREADER_H

#include <QString>
#include <QVector>

namespace Analysis {

// ════════════════════════════════════════════════════════════════
//  IMU 原始数据（New Code 导出的 CSV）读取与质量统计
//  固定格式：时间 + 加计三轴 + 陀螺三轴（列名自动识别，单位自动识别）
//  · 丢包检测（采样间隔 > 中位数间隔 × 容差）
//  · 各轴统计（min/max/mean）
//  · 加速度模值 / 角速度模值（判静止、判异常）
// ════════════════════════════════════════════════════════════════

// 单轴统计
struct AxisStat {
    double min = 0;
    double max = 0;
    double sum = 0;
    int    count = 0;
    double mean()  const { return count > 0 ? sum / count : 0.0; }
    double range() const { return max - min; }
};

// 单个 IMU 样本（已换算为统一单位：加计 m/s²，陀螺 rad/s）
struct ImuSample {
    double t = 0;                    // 相对首样本的时间（秒）
    double ax = 0, ay = 0, az = 0;   // 加速度 m/s²
    double gx = 0, gy = 0, gz = 0;   // 角速度 rad/s
};

// IMU 原始数据质量摘要
struct ImuSummary {
    // ── 基本信息 ──
    int    totalSamples = 0;   // 样本总数
    double firstTime    = 0;   // 首样本原始时间
    double lastTime     = 0;   // 末样本原始时间
    double durationSec  = 0;   // 时长（秒）
    double sampleRate   = 0;   // 采样率（Hz，按中位数间隔）

    // ── 丢包 ──
    int    gapCount   = 0;     // 丢包数（间隔超容差的样本数）
    double totalGapSec = 0;    // 总间断时长（秒）

    // ── 单位识别结果（仅展示） ──
    QString accelUnitText;   // 如 "m/s²" 或 "g（已换算为 m/s²）"
    QString gyroUnitText;    // 如 "rad/s" 或 "°/s（已换算为 rad/s）"

    // ── 各轴统计（统一单位后） ──
    AxisStat accX, accY, accZ;
    AxisStat gyrX, gyrY, gyrZ;

    // ── 时间序列 ──
    QVector<ImuSample> samples;                    // 三轴曲线用
    QVector<std::pair<double, double>> accelMag;   // 加速度模值（m/s²）
    QVector<std::pair<double, double>> gyroMag;    // 角速度模值（rad/s）
};

class ImuReader
{
public:
    // 读取并统计 IMU CSV 文件。
    // 成功返回 true，失败在 err 里写原因。
    static bool analyze(const QString &file, ImuSummary *out, QString *err);
};

} // namespace Analysis

#endif // ANALYSIS_RAW_IMUREADER_H
