#ifndef DEBUGFRAMEPARSER_H
#define DEBUGFRAMEPARSER_H

#include "modules/parse/parser/parserbase.h"
#include "modules/parse/parser/core/frameparser.h"

// ════════════════════════════════════════════════════════════════
//  DEBUG 调试数据流解析器 (0x51~0x56) — 与 CAN/RTCM 并列
//  一次性解析所有调试帧；界面只显示 0x51 主轴，导出 6 个 CSV
// ════════════════════════════════════════════════════════════════
class DebugFrameParser : public ParserBase
{
public:
    QString buttonLabel() const override { return QStringLiteral("AG调试数据流"); }
    ProtocolID protocolId() const override { return ProtocolID::Debug_IMU; }

    // 解析出全部 0x51~0x56 帧
    QList<ParsedFrame> parse(const QByteArray &rawData) override;

    // 界面只显示 0x51（主轴）
    QString formatFrame(const ParsedFrame &f) const override;
    QString csvHeader() const override;

    // 按帧 ID 生成单个 CSV 表头/行（导出 6 个 CSV 用）
    static QString csvHeaderFor(ProtocolID pid);
    static QString formatFrameFor(ProtocolID pid, const ParsedFrame &f);
};

#endif // DEBUGFRAMEPARSER_H
