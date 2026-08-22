#include "modules/parse/parser/other/rtcmframeparser.h"
#include "modules/parse/parser/core/rtcm_deep_decoder.h"

#include <QTextStream>
#include <QStringList>

// ════════════════════════════════════════════════════════════════
//  RtcmFrameParser
// ════════════════════════════════════════════════════════════════

QString RtcmFrameParser::formatRtcmFrame(const RtcmFrame &f)
{
    return RtcmParser::formatFrame(f);
}

QString RtcmFrameParser::formatRtcmSummary(const QList<RtcmFrame> &frames)
{
    return RtcmParser::formatSummary(frames);
}

QString RtcmFrameParser::formatDeepDecode(const QByteArray &rawData)
{
    RtcmDeepDecoder decoder;
    QList<RtcmObsEpoch> epochs = decoder.decode(rawData);

    if (epochs.isEmpty())
        return QStringLiteral("（RTCM 深解码：未发现有效观测值数据）\n");

    return RtcmDeepDecoder::toRinexText(epochs);
}

ParseResult RtcmFrameParser::parseToResult(const QByteArray &rawData, const QString &filePath)
{
    Q_UNUSED(filePath);

    ParseResult result;
    result.protocolId = protocolId();

    QString fullText = formatDeepDecode(rawData);
    if (fullText.isEmpty()) {
        result.displayText = QStringLiteral("（RTCM 深解码：未发现有效观测值）\n");
        result.fullText = result.displayText;
        result.infoText = QStringLiteral("RTCM深解码:0历元");
    } else {
        result.fullText = fullText;
        QStringList lines = fullText.split('\n');
        if (lines.size() <= 210) {
            result.displayText = fullText;
        } else {
            QStringList disp;
            for (int i = 0; i < 200 && i < lines.size(); ++i)
                disp.append(lines[i]);
            disp.append(QStringLiteral("\n  ... (省略 %1 行) ...\n")
                            .arg(lines.size() - 210));
            for (int i = lines.size() - 10; i < lines.size(); ++i)
                disp.append(lines[i]);
            result.displayText = disp.join('\n');
        }
        int epochCount = 0;
        for (const QString &line : lines) {
            if (line.startsWith('>')) ++epochCount;
        }
        result.infoText = QStringLiteral("RTCM深解码:%1历元").arg(epochCount);
    }
    return result;
}
