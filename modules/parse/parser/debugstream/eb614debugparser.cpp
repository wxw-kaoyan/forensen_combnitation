#include "modules/parse/parser/debugstream/eb614debugparser.h"

#include <QTextStream>
#include <QStringList>

// ════════════════════════════════════════════════════════════════
//  Eb614DebugParser — 614E-B 调试数据流 (0x0101/0x0111/0x0131)
// ════════════════════════════════════════════════════════════════

QList<ParsedFrame> Eb614DebugParser::parse(const QByteArray &rawData)
{
    FrameParser fp;
    QList<ParsedFrame> all = fp.parseAll(rawData, m_skipCrc);
    QList<ParsedFrame> filtered;
    for (const auto &f : all) {
        if (isEb614Protocol(f.protocol))
            filtered.append(f);
    }
    return filtered;
}

QString Eb614DebugParser::csvHeader() const
{
    return csvHeaderFor(ProtocolID::Eb614_IMU);
}

QString Eb614DebugParser::formatFrame(const ParsedFrame &f) const
{
    // 界面只显示 0x0101 主轴
    if (f.protocol != ProtocolID::Eb614_IMU)
        return QString();
    return formatFrameFor(f.protocol, f);
}

QString Eb614DebugParser::csvHeaderFor(ProtocolID pid)
{
    switch (pid) {
    case ProtocolID::Eb614_IMU:
        return QStringLiteral(
            "周内毫秒,GPS周,陀螺X(deg/s),陀螺Y(deg/s),陀螺Z(deg/s),"
            "加表X(g),加表Y(g),加表Z(g),温度(C),温度变化率,"
            "IMU类型,RTK类型,零速标志,"
            "QEKF横滚(deg),QEKF俯仰(deg),QEKF航向(deg),"
            "KF横滚(deg),KF俯仰(deg),KF航向(deg)\n");
    case ProtocolID::Eb614_GPS:
        return QStringLiteral(
            "位置ITOW(ms),GPS周,PPS触发时刻(us),接收时刻(ms),定位类型,解算星数,多频星数,"
            "纬度(deg),经度(deg),海拔(m),水平位置精度(mm),垂直位置精度(mm),差分龄期,"
            "速度ITOW(ms),北向速度(m/s),东向速度(m/s),地向速度(m/s),"
            "地速(m/s),运动航向(deg),水平速度精度(mm/s),垂直速度精度(mm/s)\n");
    case ProtocolID::Eb614_KF: {
        QString h;
        QTextStream s(&h);
        for (int i = 0; i < 21; ++i) s << "协方差" << i << ',';
        for (int i = 0; i < 6;  ++i) s << "新息" << i << ',';
        s << "横滚(deg),俯仰(deg),航向(deg),";
        s << "北向速度(m/s),东向速度(m/s),地向速度(m/s),";
        s << "纬度(deg),经度(deg),高程(m),";
        s << "陀螺零偏X(deg/s),陀螺零偏Y(deg/s),陀螺零偏Z(deg/s),";
        s << "加表零偏X(g),加表零偏Y(g),加表零偏Z(g),";
        s << "陀螺标度X(ppm),陀螺标度Y(ppm),陀螺标度Z(ppm),";
        s << "杆臂X(mm),杆臂Y(mm),杆臂Z(mm),ITOW(ms)\n";
        return h;
    }
    default:
        return QString();
    }
}

QString Eb614DebugParser::formatFrameFor(ProtocolID pid, const ParsedFrame &f)
{
    QStringList c;
    switch (pid) {
    case ProtocolID::Eb614_IMU: {
        const auto &d = f.eb614Imu;
        c << QString::number(d.time_gps_ms)
          << QString::number(d.week_num)
          << QString::number(d.gyro[0], 'f', 6) << QString::number(d.gyro[1], 'f', 6) << QString::number(d.gyro[2], 'f', 6)
          << QString::number(d.accel[0], 'f', 6) << QString::number(d.accel[1], 'f', 6) << QString::number(d.accel[2], 'f', 6)
          << QString::number(d.temperature, 'f', 2)
          << QString::number(d.temp_vel)
          << QString::number(d.imu_type)
          << QString::number(d.rtk_type)
          << QString::number(d.zupt)
          << QString::number(d.qekf_att[0] / 100.0, 'f', 2)
          << QString::number(d.qekf_att[1] / 100.0, 'f', 2)
          << QString::number(d.qekf_att[2] / 100.0, 'f', 2)
          << QString::number(d.kf_att[0] / 100.0, 'f', 2)
          << QString::number(d.kf_att[1] / 100.0, 'f', 2)
          << QString::number(d.kf_att[2] / 100.0, 'f', 2);
        break;
    }
    case ProtocolID::Eb614_GPS: {
        const auto &d = f.eb614Gps;
        c << QString::number(d.itow_pos)
          << QString::number(d.week_num)
          << QString::number(d.pps_time_us)
          << QString::number(d.revc_time_ms)
          << QString::number(d.fixtype)
          << QString::number(d.numSV)
          << QString::number(d.multiSVs)
          << QString::number(d.lat, 'f', 8)
          << QString::number(d.lon, 'f', 8)
          << QString::number(d.alt / 1e5, 'f', 3)
          << QString::number(d.p_hacc)
          << QString::number(d.p_vacc)
          << QString::number(d.diff_age)
          << QString::number(d.itow_vel)
          << QString::number(d.vn, 'f', 6)
          << QString::number(d.ve, 'f', 6)
          << QString::number(d.vd, 'f', 6)
          << QString::number(d.gspeed, 'f', 6)
          << QString::number(d.headMot, 'f', 3)
          << QString::number(d.v_hacc)
          << QString::number(d.v_vacc);
        break;
    }
    case ProtocolID::Eb614_KF: {
        const auto &d = f.eb614Kf;
        for (int i = 0; i < 21; ++i) c << QString::number(d.pk[i], 'f', 6);
        for (int i = 0; i < 6;  ++i) c << QString::number(d.innov[i], 'f', 6);
        c << QString::number(d.att[0], 'f', 6) << QString::number(d.att[1], 'f', 6) << QString::number(d.att[2], 'f', 6);
        c << QString::number(d.vel[0], 'f', 6) << QString::number(d.vel[1], 'f', 6) << QString::number(d.vel[2], 'f', 6);
        c << QString::number(d.pos[0], 'f', 8) << QString::number(d.pos[1], 'f', 8) << QString::number(d.pos[2], 'f', 3);
        c << QString::number(d.gyro_bias[0], 'f', 6) << QString::number(d.gyro_bias[1], 'f', 6) << QString::number(d.gyro_bias[2], 'f', 6);
        c << QString::number(d.acceel_bias[0], 'f', 6) << QString::number(d.acceel_bias[1], 'f', 6) << QString::number(d.acceel_bias[2], 'f', 6);
        c << QString::number(d.gyro_scale[0], 'f', 6) << QString::number(d.gyro_scale[1], 'f', 6) << QString::number(d.gyro_scale[2], 'f', 6);
        c << QString::number(d.est_lever_arm[0], 'f', 6) << QString::number(d.est_lever_arm[1], 'f', 6) << QString::number(d.est_lever_arm[2], 'f', 6);
        c << QString::number(d.itow);
        break;
    }
    default:
        return QString();
    }
    return c.join(',') + QStringLiteral("\n");
}
