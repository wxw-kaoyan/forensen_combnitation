#ifndef EB614DEBUGPARSER_H
#define EB614DEBUGPARSER_H

#include "modules/parse/parser/parserbase.h"
#include "modules/parse/parser/core/frameparser.h"

// ════════════════════════════════════════════════════════════════
//  614E-B 调试数据流解析器 (0x0101/0x0111/0x0131) — 与 DEBUG 并列
//  一个按钮聚合解析三帧；界面显示 0x0101 主轴，导出 3 个 CSV
// ════════════════════════════════════════════════════════════════
class Eb614DebugParser : public ParserBase
{
public:
    QString buttonLabel() const override { return QStringLiteral("614E-B调试数据流"); }
    ProtocolID protocolId() const override { return ProtocolID::Eb614_IMU; }

    // 解析出全部 0x0101/0x0111/0x0131 帧
    QList<ParsedFrame> parse(const QByteArray &rawData) override;

    // 界面只显示 0x0101（主轴）
    QString formatFrame(const ParsedFrame &f) const override;
    QString csvHeader() const override;

    // 按帧 ID 生成单个 CSV 表头/行（导出 3 个 CSV 用）
    static QString csvHeaderFor(ProtocolID pid);
    static QString formatFrameFor(ProtocolID pid, const ParsedFrame &f);
};

#endif // EB614DEBUGPARSER_H
