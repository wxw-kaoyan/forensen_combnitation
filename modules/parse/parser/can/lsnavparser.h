#ifndef LSNAVPARSER_H
#define LSNAVPARSER_H

#include "modules/parse/parser/parserbase.h"
#include "modules/parse/parser/core/canblfparser.h"

// ════════════════════════════════════════════════════════════════
//  NAVDATA_联适 — 从 BLF 解析 CAN 0x1CFF9B9A (debug 数据流, 14 包)
//  一个按钮聚合三组：
//    包1~3  IMU 原始 (100Hz)  → LsNav_Imu
//    包4~11 GNSS + 导航 (20Hz) → LsNav_Gps
//    包12~14 杆臂/版本/重启原因 → LsNav_Param
//  界面显示 GNSS 主轴；导出 3 个 CSV
// ════════════════════════════════════════════════════════════════

class LsNavParser : public ParserBase
{
public:
    QString buttonLabel() const override { return QStringLiteral("NAVDATA_联适"); }
    QString category() const override { return QStringLiteral("CAN RTK"); }
    ProtocolID protocolId() const override { return ProtocolID::LsNav_Gps; }

    QList<ParsedFrame> parse(const QByteArray &rawData) override
    {
        int canFrames = 0, validGroups = 0;
        return m_blfParser.parseLsNav(rawData, canFrames, validGroups);
    }

    QString csvHeader() const override { return csvHeaderFor(ProtocolID::LsNav_Gps); }

    QString formatFrame(const ParsedFrame &f) const override
    {
        return formatFrameFor(f.protocol, f);
    }

    // 按分组生成 CSV 表头 / 数据行（导出 3 个 CSV 用）
    static QString csvHeaderFor(ProtocolID pid)
    {
        switch (pid) {
        case ProtocolID::LsNav_Imu:
            return QStringLiteral(
                "GPS周内毫秒(ms),IMU周,加表X(g),加表Y(g),加表Z(g),"
                "陀螺X(deg/s),陀螺Y(deg/s),陀螺Z(deg/s),温度(C),KF初始化\n");
        case ProtocolID::LsNav_Gps:
            return QStringLiteral(
                "GPS周内毫秒(ms),GPS周,定位类型,定位类型名,接收时刻(ms),卫星数,双频星数,"
                "纬度(deg),经度(deg),海拔(m),"
                "北向速度(m/s),东向速度(m/s),地向速度(m/s),地速(m/s),运动航向(deg),"
                "水平位置精度,垂直位置精度,水平速度精度,ll_extend,差分龄期,滤波器索引\n");
        case ProtocolID::LsNav_Param:
            return QStringLiteral(
                "主天线杆臂X(m),主天线杆臂Y(m),主天线杆臂Z(m),RTK标记,"
                "后轮杆臂X(m),后轮杆臂Y(m),后轮杆臂Z(m),版本,重启原因\n");
        default:
            return {};
        }
    }

    static QString formatFrameFor(ProtocolID pid, const ParsedFrame &f)
    {
        switch (pid) {
        case ProtocolID::LsNav_Imu: {
            const auto &d = f.lsNavImu;
            return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10\n")
                .arg(d.timeGpsMs).arg(d.weekNumImu)
                .arg(d.accel[0], 0, 'f', 5).arg(d.accel[1], 0, 'f', 5).arg(d.accel[2], 0, 'f', 5)
                .arg(d.gyro[0], 0, 'f', 5).arg(d.gyro[1], 0, 'f', 5).arg(d.gyro[2], 0, 'f', 5)
                .arg(d.temperature, 0, 'f', 1).arg(d.kfInitDone);
        }
        case ProtocolID::LsNav_Gps: {
            const auto &d = f.lsNavGps;
            return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21\n")
                .arg(d.itow).arg(d.weekNumGps).arg(d.fixType).arg(novatelPosTypeName(d.fixType))
                .arg(d.revcTimeMs).arg(d.numSV).arg(d.multiSVs)
                .arg(d.lat, 0, 'f', 7).arg(d.lon, 0, 'f', 7).arg(d.alt, 0, 'f', 3)
                .arg(d.vn, 0, 'f', 3).arg(d.ve, 0, 'f', 3).arg(d.vd, 0, 'f', 3)
                .arg(d.gspeed, 0, 'f', 3).arg(d.headMot, 0, 'f', 3)
                .arg(d.pHacc).arg(d.pVacc).arg(d.vHacc).arg(d.llExtend)
                .arg(d.diffAge).arg(d.kfIndexGet);
        }
        case ProtocolID::LsNav_Param: {
            const auto &d = f.lsNavParam;
            return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9\n")
                .arg(d.gpsPosX / 100.0, 0, 'f', 2)
                .arg(d.gpsPosY / 100.0, 0, 'f', 2)
                .arg(d.gpsPosZ / 100.0, 0, 'f', 2)
                .arg(d.rtkFlag)
                .arg(d.odPosX / 100.0, 0, 'f', 2)
                .arg(d.odPosY / 100.0, 0, 'f', 2)
                .arg(d.odPosZ / 100.0, 0, 'f', 2)
                .arg(d.version).arg(d.resetReason);
        }
        default:
            return {};
        }
    }

private:
    CanBlfParser m_blfParser;
};

#endif // LSNAVPARSER_H
