#ifndef AIMRTBAGPARSER_H
#define AIMRTBAGPARSER_H

#include "modules/parse/parser/parserbase.h"

#include <QList>
#include <QString>

// ════════════════════════════════════════════════════════════════
//  AimRT（MCAP）录包解析器 — 非帧格式
//  通过外部 Python 脚本 aimrt_imu_export.py 读取 MCAP 里的 IMU 话题，
//  每个话题导出一个 CSV，并给出按固定频率检查的丢包（丢帧）统计。
//
//  · 只接受单个 .mcap 文件
//  · CSV 体积大（10 万条/话题约 30MB），因此界面只显示统计摘要，
//    CSV 留在脚本输出目录，通过「导出」按钮另存
// ════════════════════════════════════════════════════════════════

// 单个话题的统计
struct AimrtTopicStat {
    QString name;          // 话题短名，如 pelvis_imu
    QString topic;         // 完整话题名
    QString csv;           // 导出的 CSV 路径
    int     rows = 0;      // 消息条数
    double  hz = 0;        // 实测平均频率
    double  durationS = 0; // 首尾 header 时间跨度
    double  intervalMedianUs = 0;
    double  intervalMinUs = 0;
    double  intervalMaxUs = 0;
    double  devMinUs = 0;  // 相对期望间隔的偏差
    double  devMaxUs = 0;
    double  maxGapUs = 0;  // 最大间隔
    qint64  maxGapAtNs = 0;// 最大间隔出现处的 header 时间
    int     missed = 0;    // 估算丢帧数
    qint64  firstStampNs = 0;
    qint64  lastStampNs = 0;
};

struct AimrtResult {
    bool    ok = false;
    QString error;
    qint64  fileSize = 0;
    QString stem;          // 包名（不含扩展名）
    QString outDir;        // CSV 输出目录（导出按钮的目标）
    double  checkRate = 0; // 检查频率（0 = 未检查）
    int     totalRows = 0;
    int     totalMissed = 0;
    QList<AimrtTopicStat> topics;
};

class AimrtBagParser : public ParserBase
{
public:
    QString buttonLabel() const override { return QStringLiteral("AimRT IMU 录包(MCAP)"); }
    QString category() const override { return QStringLiteral("AimRT 录包"); }
    ProtocolID protocolId() const override { return ProtocolID::Aimrt_ImuBag; }

    // 空实现（解析不走帧解析路径，而是通过 runPython 直接导出 CSV）
    QList<ParsedFrame> parse(const QByteArray &) override { return {}; }
    QString formatFrame(const ParsedFrame &) const override { return {}; }

    // 外部脚本路径（非帧解析），重写统一入口
    ParseResult parseToResult(const QByteArray &rawData, const QString &filePath) override;

    // 在调用线程中同步执行（耗时约数十秒，应在工作线程中调用）
    // mcapPath: 单个 .mcap；outDir: CSV 输出目录
    static AimrtResult runPython(const QString &mcapPath, const QString &outDir,
                                 double checkRateHz = 500.0);
};

#endif // AIMRTBAGPARSER_H
