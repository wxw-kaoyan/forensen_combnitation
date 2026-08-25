#ifndef ANALYSIS_RAW_TRUTHREADER_H
#define ANALYSIS_RAW_TRUTHREADER_H

#include <QString>
#include <QVector>

namespace Analysis {

// ════════════════════════════════════════════════════════════════
//  真值文件（GINS 后处理输出）读取与统计
//  特点：头部元信息（英文旧版 / 中文新版）行数不固定、一直在变，
//  因此不按固定行数跳过，而是扫描定位「列名表头行」：
//  · 找到同时含 Latitude 与 Longitude 的行 = 列名表头
//  · 下一行是单位行（含括号），跳过
//  · 再往下全部是空格分隔的数据行
// ════════════════════════════════════════════════════════════════

// 单个真值样本
struct TruthSample {
    double t = 0;        // 相对首样本时间（秒）
    double lat = 0;      // 纬度 (deg)
    double lon = 0;      // 经度 (deg)
    double h   = 0;      // 椭球高 (m)
    double velN = 0;     // 北向速度 (m/s)
    double velE = 0;     // 东向速度 (m/s)
    double velU = 0;     // 天向速度 (m/s)
    double roll = 0;     // 横滚 (deg)
    double pitch = 0;    // 俯仰 (deg)
    double heading = 0;  // 航向 (deg)
    int    satUse = 0;   // 使用卫星数
};

// 真值数据摘要
struct TruthSummary {
    int    totalSamples = 0;
    double firstTime    = 0;   // 首样本原始 GPSTime（GPS 周内秒）
    double lastTime     = 0;
    double durationSec  = 0;
    double sampleRate   = 0;   // Hz（按中位数间隔）

    // 位置范围（报告 + 轨迹轴范围）
    double minLat = 0, maxLat = 0;
    double minLon = 0, maxLon = 0;
    double minH = 0, maxH = 0;

    QVector<TruthSample> samples;
};

class TruthReader
{
public:
    // 读取并统计真值文件。成功返回 true，失败在 err 里写原因。
    static bool analyze(const QString &file, TruthSummary *out, QString *err);
};

} // namespace Analysis

#endif // ANALYSIS_RAW_TRUTHREADER_H
