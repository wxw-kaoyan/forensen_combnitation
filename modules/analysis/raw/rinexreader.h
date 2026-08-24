#ifndef ANALYSIS_RAW_RINEXREADER_H
#define ANALYSIS_RAW_RINEXREADER_H

#include <QString>
#include <QMap>
#include <QSet>
#include <QVector>

namespace Analysis {

// ════════════════════════════════════════════════════════════════
//  RINEX 观测文件（.O / .obs）读取与质量统计
//  复用 RTKLIB 的 readrnx() 读取原始观测量，做数据质量检核：
//  · 观测值统计（历元数、观测记录数、各系统卫星数）
//  · 丢数检测（丢历元、掉星）
//  · SNR 信噪比统计（按系统 / 频点聚合）
// ════════════════════════════════════════════════════════════════

// 系统位掩码（与 RTKLIB SYS_* 一致）
namespace Sys {
    constexpr int GPS = 0x01;
    constexpr int GLO = 0x04;
    constexpr int GAL = 0x08;
    constexpr int QZS = 0x10;
    constexpr int BDS = 0x20;
}

// 单个频点的 SNR 统计
struct SnrStat {
    double min = 0;
    double max = 0;
    double sum = 0;
    int    count = 0;
    double mean() const { return count > 0 ? sum / count : 0.0; }
};

// RINEX 观测文件质量摘要
struct RinexSummary {
    // ── 基本信息 ──
    int    totalObsRecords = 0;   // 观测记录总数（obs_t.n，卫星×历元）
    int    totalEpochs     = 0;   // 历元总数
    double firstTime       = 0;   // 首历元（GPS 周秒）
    double lastTime        = 0;   // 末历元（GPS 周秒）
    double durationSec     = 0;   // 时长（秒）
    double sampleRate      = 0;   // 采样率（Hz，按中位数间隔）

    // ── 系统 / 卫星 ──
    int  systemMask = 0;                          // 出现的系统位掩码
    QMap<int, QSet<int>> satsPerSys;              // 每系统的卫星 PRN 集合
    QMap<int, QSet<int>> missingSatsPerSys;       // 中途消失的卫星（掉星）

    // ── 丢数 ──
    int    epochGapCount = 0;      // 丢历元数（间隔 > 2×中位数间隔）
    double totalGapSec   = 0;      // 总间断时长（秒）

    // ── SNR ──
    // 外层 key = 系统掩码，内层 key = 频点码（如 "1C"、"2W"、"5Q"）
    QMap<int, QMap<QString, SnrStat>> snrPerSysCode;

    // 辅助：系统掩码 → 显示名
    static QString sysName(int mask);
    static QString sysLetter(int mask);
};

class RinexReader
{
public:
    // 读取并统计 RINEX 观测文件
    // 成功返回 true，失败在 err 里写原因
    static bool analyze(const QString &file, RinexSummary *out, QString *err);
};

} // namespace Analysis

#endif // ANALYSIS_RAW_RINEXREADER_H
