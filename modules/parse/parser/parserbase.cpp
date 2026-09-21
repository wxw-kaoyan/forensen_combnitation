#include "modules/parse/parser/parserbase.h"
#include <QTextStream>

// ════════════════════════════════════════════════════════════════
//  默认统一入口：走「帧解析」路径（标准帧 / CAN / DEBUG 流）
//  外部脚本（凯芯）与深解码（RTCM）子类重写此方法
// ════════════════════════════════════════════════════════════════
ParseResult ParserBase::parseToResult(const QByteArray &rawData,
                                      const QString &filePath)
{
    Q_UNUSED(filePath);

    ParseResult result;
    result.protocolId = protocolId();
    result.frames = parse(rawData);

    QString text;
    QTextStream s(&text);
    if (!csvHeader().isEmpty()) {
        s << csvHeader();
        for (const auto &f : result.frames)
            s << formatFrame(f);
    } else {
        for (const auto &f : result.frames) {
            s << QStringLiteral("[#%1] %2\n")
                 .arg(f.offsetInFile)
                 .arg(formatFrame(f));
        }
    }
    s.flush();

    if (text.isEmpty())
        text = QStringLiteral("（无匹配帧）\n");

    int crcErr = 0;
    for (const auto &f : result.frames) {
        if (!f.crcOk)
            ++crcErr;
    }

    result.displayText = text;
    result.fullText = text;
    result.infoText = QStringLiteral("帧:%1 (CRC错:%2)")
                          .arg(result.frames.size()).arg(crcErr);
    return result;
}

QString ParserBase::protocolNameStatic(ProtocolID pid)
{
    return QString::fromLatin1(protocolName(pid));
}

QString ParserBase::rtkStatusName(uint8_t s)
{
    switch (s) {
        case 0: return QStringLiteral("未定位");
        case 1: return QStringLiteral("单点");
        case 2: return QStringLiteral("伪距差分");
        case 4: return QStringLiteral("固定解");
        case 5: return QStringLiteral("浮点解");
        default: return QString::number(s);
    }
}

QString ParserBase::formatStatusFlags(uint32_t flags)
{
    QStringList parts;
    if (flags & 0x0001) parts << QStringLiteral("RTK有效");
    if (flags & 0x0002) parts << QStringLiteral("PPS有效");
    if (flags & 0x0004) parts << QStringLiteral("已初始化");
    if (flags & 0x0008) parts << QStringLiteral("前轮转角有效");
    if (flags & 0x0010) parts << QStringLiteral("已收敛");
    if (flags & 0x0020) parts << QStringLiteral("前轮陀螺有效");
    if (flags & 0x0040) parts << QStringLiteral("方向盘电机有效");

    uint8_t dir = (flags >> 7) & 0x03;
    if (dir == 1) parts << QStringLiteral("前进");
    else if (dir == 2) parts << QStringLiteral("后退");

    if (parts.isEmpty()) return QStringLiteral("0x%1").arg(flags, 0, 16);
    return QStringLiteral("0x%1[%2]").arg(flags, 0, 16).arg(parts.join(','));
}

QString ParserBase::formatAttValid(uint8_t v)
{
    QStringList parts;
    if (v & 0x01) parts << QStringLiteral("姿态有效");
    if (v & 0x02) parts << QStringLiteral("前轮转角有效");
    if (parts.isEmpty()) return QStringLiteral("0x%1").arg(v, 0, 16);
    return QStringLiteral("0x%1[%2]").arg(v, 0, 16).arg(parts.join(','));
}
