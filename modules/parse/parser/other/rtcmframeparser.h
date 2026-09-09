#ifndef RTCMFRAMEPARSER_H
#define RTCMFRAMEPARSER_H

#include "modules/parse/parser/parserbase.h"
#include "modules/parse/parser/core/rtcmparser.h"

// ════════════════════════════════════════════════════════════════
//  RTCM 3.x 解析器
// ════════════════════════════════════════════════════════════════
class RtcmFrameParser : public ParserBase
{
public:
    QString buttonLabel() const override { return QStringLiteral("RTCM 3.x"); }
    QString category() const override { return QStringLiteral("RTCM"); }
    ProtocolID protocolId() const override { return ProtocolID::Unknown; }

    QList<ParsedFrame> parse(const QByteArray &) override { return {}; }
    QString formatFrame(const ParsedFrame &) const override { return {}; }

    // 深解码路径（非帧解析），重写统一入口
    ParseResult parseToResult(const QByteArray &rawData, const QString &filePath) override;

    QList<RtcmFrame> parseRtcm(const QByteArray &rawData) const
    {
        RtcmParser rp;
        return rp.parseAll(rawData, m_skipCrc);
    }
    static QString formatRtcmFrame(const RtcmFrame &f);
    static QString formatRtcmSummary(const QList<RtcmFrame> &frames);
    static QString formatDeepDecode(const QByteArray &rawData);
};

#endif // RTCMFRAMEPARSER_H
