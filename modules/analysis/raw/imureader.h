#ifndef ANALYSIS_RAW_IMUREADER_H
#define ANALYSIS_RAW_IMUREADER_H

#include <QString>
#include <QVector>

namespace Analysis {

// ════════════════════════════════════════════════════════════════
//  IMU 原始数据读取与质量统计
//  支持两种输入格式，按文件内容自动判别：
//   · CSV/TXT  —— 时间 + 加计三轴 + 陀螺三轴（列名自动识别，单位自动识别）
//   · RAWIMUA  —— 诺瓦泰二进制（AA 44 13 同步头 / MsgID 0x05B6 / 56 字节帧）
//  检查项：丢数（采样间隔异常）、回退（时间戳倒退）
// ════════════════════════════════════════════════════════════════

// 输入格式
enum class ImuFormat {
    Csv = 0,      // 文本（CSV/空格/Tab 分隔）
    RawImua = 1   // 诺瓦泰 RAWIMUA 二进制
};

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
    ImuFormat format    = ImuFormat::Csv;
    QString formatText;        // 格式描述（用于报告）

    // ── 丢数检查 ──
    int    gapEvents   = 0;    // 丢数事件次数（间隔超容差的次数）
    int    gapCount    = 0;    // 丢样本总数（估算）
    double totalGapSec = 0;    // 总间断时长（秒）

    // ── 回退检查 ──
    int    rollbackCount    = 0;    // 时间倒退次数
    double maxRollbackSec   = 0;    // 单次最大倒退量（秒）
    double totalRollbackSec = 0;    // 累计倒退量（秒）

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
    // 读取并统计 IMU 文件（自动判别 CSV / RAWIMUA）。
    // skipCrc 仅对 RAWIMUA 生效：为 true 时不做帧 CRC 校验。
    static bool analyze(const QString &file, ImuSummary *out, QString *err,
                        bool skipCrc = false);
};

} // namespace Analysis

#endif // ANALYSIS_RAW_IMUREADER_H
