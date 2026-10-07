#ifndef LSIMULOGPARSER_H
#define LSIMULOGPARSER_H

#include "modules/parse/parser/parserbase.h"
#include "modules/parse/parser/core/canblfparser.h"

// ════════════════════════════════════════════════════════════════
//  IMULOG_联适 — 从 BLF 解析 CAN 0x1FFFFF9A (排查数据帧, 6 包, 1Hz)
//  滤波器 / IMU 内部状态：滤波器状态、重置标记、动静态标记、
//  ZUPT/直行/静态计数、陀螺 z 零偏、安装偏差角、位置残差
// ════════════════════════════════════════════════════════════════
class LsImuLogParser : public ParserBase
{
public:
    QString buttonLabel() const override { return QStringLiteral("IMULOG_联适"); }
    QString category() const override { return QStringLiteral("CAN RTK"); }
    ProtocolID protocolId() const override { return ProtocolID::LsImuLog; }

    QList<ParsedFrame> parse(const QByteArray &rawData) override
    {
        int canFrames = 0, validGroups = 0;
        return m_blfParser.parseLsImuLog(rawData, canFrames, validGroups);
    }

    QString csvHeader() const override
    {
        return QStringLiteral(
            "初始化状态,滤波器筛选值,初始化航向角(deg),设备状态,"
            "betpos超时间隔数,psrvel超时间隔数,"
            "ZUPT计数,动静态标记,直行计数,航向安装偏差(deg),betpos数量,"
            "陀螺Z零偏(deg/h),延迟(ms),psrvel数量,"
            "IMU静态计数,滤波器状态,重置标记位,俯仰安装偏差(deg),"
            "北向残差(m),东向残差(m),天向残差(m)\n");
    }

    QString formatFrame(const ParsedFrame &f) const override
    {
        const auto &d = f.lsImuLog;
        return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21\n")
            .arg(d.initStatus).arg(d.filterSelect)
            .arg(d.initHeading, 0, 'f', 2)
            .arg(d.deviceStatus).arg(d.betposGapCnt).arg(d.psrvelGapCnt)
            .arg(d.zuptCnt).arg(d.staticFlag).arg(d.straightCnt)
            .arg(d.yawInstallBias, 0, 'f', 2)
            .arg(d.betposCnt)
            .arg(d.gyroZBias, 0, 'f', 0)
            .arg(d.delayMs).arg(d.psrvelCnt)
            .arg(d.imuStaticCnt).arg(d.filterState).arg(d.resetFlag)
            .arg(d.pitchInstallBias, 0, 'f', 2)
            .arg(d.resN, 0, 'f', 2).arg(d.resE, 0, 'f', 2).arg(d.resU, 0, 'f', 2);
    }

private:
    CanBlfParser m_blfParser;
};

#endif // LSIMULOGPARSER_H
