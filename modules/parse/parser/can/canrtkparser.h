#ifndef CANRTKPARSER_H
#define CANRTKPARSER_H

#include "modules/parse/parser/parserbase.h"
#include "modules/parse/parser/core/canblfparser.h"

// ════════════════════════════════════════════════════════════════
//  CAN-RTK 解析器 — 从 BLF 文件解析 0x1CEBFF9A CAN 帧
// ════════════════════════════════════════════════════════════════
class CanRtkParser : public ParserBase
{
public:
    QString buttonLabel() const override { return QStringLiteral("联试_CAN981"); }
    ProtocolID protocolId() const override { return ProtocolID::Can_RTK; }

    QList<ParsedFrame> parse(const QByteArray &rawData) override
    {
        int canFrames = 0, validGroups = 0;
        return m_blfParser.parseBlfFile(rawData, canFrames, validGroups);
    }

    QString csvHeader() const override
    {
        return QStringLiteral(
            "UTC天数,UTC天内秒,"
            "定位类型,定位类型名,差分龄期,卫星数,"
            "纬度,经度,海拔(m),"
            "前向速度(m/s),速度方向(deg),"
            "横滚角(deg),俯仰角(deg),航向角(deg),"
            "横滚有效,俯仰有效,航向有效\n");
    }

    QString formatFrame(const ParsedFrame &f) const override
    {
        const auto &d = f.canRtk;
        return QStringLiteral(
            "%1,%2,"
            "%3,%4,%5,%6,"
            "%7,%8,%9,"
            "%10,%11,"
            "%12,%13,%14,"
            "%15,%16,%17\n")
            .arg(d.utcDays).arg(d.secondsOfDay, 0, 'f', 3)
            .arg(d.fixType).arg(ParserBase::rtkStatusName(d.fixType))
            .arg(d.diffAge).arg(d.numSatellites)
            .arg(d.latitude, 0, 'f', 8).arg(d.longitude, 0, 'f', 8).arg(d.altitude, 0, 'f', 3)
            .arg(d.forwardSpeed, 0, 'f', 3)
            .arg((d.sdRaw != 0xFFFF) ? QString::number(d.speedDirection, 'f', 2) : QStringLiteral("N/A"))
            .arg(d.rollValid ? QString::number(d.roll, 'f', 3) : QStringLiteral("N/A"))
            .arg(d.pitchValid ? QString::number(d.pitch, 'f', 3) : QStringLiteral("N/A"))
            .arg(d.yawValid ? QString::number(d.yaw, 'f', 3) : QStringLiteral("N/A"))
            .arg(d.rollValid ? 1 : 0)
            .arg(d.pitchValid ? 1 : 0)
            .arg(d.yawValid ? 1 : 0);
    }

private:
    CanBlfParser m_blfParser;
};

#endif // CANRTKPARSER_H
