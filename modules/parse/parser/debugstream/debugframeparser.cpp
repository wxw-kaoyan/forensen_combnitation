#include "modules/parse/parser/debugstream/debugframeparser.h"

#include <QTextStream>
#include <QStringList>

// ════════════════════════════════════════════════════════════════
//  DebugFrameParser — DEBUG 调试数据流 (0x51~0x56)
// ════════════════════════════════════════════════════════════════

QList<ParsedFrame> DebugFrameParser::parse(const QByteArray &rawData)
{
    FrameParser fp;
    QList<ParsedFrame> all = fp.parseAll(rawData, m_skipCrc);
    QList<ParsedFrame> filtered;
    for (const auto &f : all) {
        if (isDebugProtocol(f.protocol))
            filtered.append(f);
    }
    return filtered;
}

QString DebugFrameParser::csvHeader() const
{
    return csvHeaderFor(ProtocolID::Debug_IMU);
}

QString DebugFrameParser::formatFrame(const ParsedFrame &f) const
{
    // 界面只显示 0x51 主轴
    if (f.protocol != ProtocolID::Debug_IMU)
        return QString();
    return formatFrameFor(f.protocol, f);
}

QString DebugFrameParser::csvHeaderFor(ProtocolID pid)
{
    switch (pid) {
    case ProtocolID::Debug_IMU:
        return QStringLiteral(
            "周内毫秒,GPS周,陀螺X(deg/s),陀螺Y(deg/s),陀螺Z(deg/s),"
            "加表X(g),加表Y(g),加表Z(g),温度(C),状态,"
            "AHRS横滚(deg),AHRS俯仰(deg),AHRS航向(deg),"
            "组合导航横滚(deg),组合导航俯仰(deg),组合导航航向(deg),"
            "北向速度(m/s),东向速度(m/s),地向速度(m/s),纬度(deg),经度(deg),高度(m)\n");
    case ProtocolID::Debug_GPS:
        return QStringLiteral(
            "周内毫秒,GPS周,PPS触发时刻(us),接收时刻(ms),定位类型,解算星数,多频星数,"
            "纬度(deg),经度(deg),高度(m),北向速度(m/s),东向速度(m/s),地向速度(m/s),"
            "地速(m/s),运动航向(deg),水平位置精度,垂直位置精度,水平速度精度,垂直速度精度,差分龄期\n");
    case ProtocolID::Debug_DRTK:
        return QStringLiteral(
            "周内毫秒,定位类型,航向(deg),航向标准差(deg),俯仰(deg),俯仰标准差(deg),基线长度,星数\n");
    case ProtocolID::Debug_OD:
        return QStringLiteral(
            "周内毫秒,前左轮速(cm/s),前右轮速(cm/s),后左轮速(cm/s),后右轮速(cm/s),挡位,转向角\n");
    case ProtocolID::Debug_KF: {
        QString h;
        QTextStream s(&h);
        // pk[18]：状态协方差 P 对角线，按状态分组
        for (int i = 0; i < 3;  ++i) s << "失准角协方差" << i << "(deg),";
        for (int i = 0; i < 3;  ++i) s << "速度协方差" << i << "(m/s),";
        for (int i = 0; i < 3;  ++i) s << "位置协方差" << i << "(m),";
        for (int i = 0; i < 3;  ++i) s << "陀螺零偏协方差" << i << "(deg/s),";
        for (int i = 0; i < 3;  ++i) s << "加表零偏协方差" << i << "(mg),";
        for (int i = 0; i < 3;  ++i) s << "安装刻度协方差" << i << ',';
        // imu_bias[6]：陀螺零偏(3) + 加表零偏(3)
        for (int i = 0; i < 3;  ++i) s << "陀螺零偏" << i << "(deg/s),";
        for (int i = 0; i < 3;  ++i) s << "加表零偏" << i << "(mg),";
        // innov[6]：量测新息（速度 + 位置）
        for (int i = 0; i < 3;  ++i) s << "速度新息" << i << ',';
        for (int i = 0; i < 3;  ++i) s << "位置新息" << i << ',';
        s << "里程计刻度系数,安装俯仰角,安装偏航角,解算耗时,任务周期,CPU占用,";
        for (int i = 0; i < 3;  ++i) s << "位置量测噪声" << i << ',';
        for (int i = 0; i < 3;  ++i) s << "速度量测噪声" << i << ',';
        for (int i = 0; i < 3;  ++i) s << "零偏量测噪声" << i << ',';
        for (int i = 0; i < 3;  ++i) s << "预留" << i << ',';
        s << "预留3\n";
        return h;
    }
    case ProtocolID::Debug_Param:
        return QStringLiteral(
            "SETTING,GPS_HACC_FIX,GPS_HACC_SINGLE,GPS_SACC,GPS_HEADING_ACC,"
            "GYRO_STD,ACCEL_STD,GYRO_ZUPT_VALUE,MEAS_CONSTRAIN_AHRS,ALIGN_VEL,"
            "PARAM_ANTENNA_INST_ANGLE,PARAM_ANTENNA_MODIFY_ANGLE,ANTENNA_LENGTH,PARAM_OD_K,"
            "PARAM_INST_ROLL,PARAM_INST_PITCH,PARAM_INST_YAW,"
            "PARAM_GPS_POS_X,PARAM_GPS_POS_Y,PARAM_GPS_POS_Z,"
            "PARAM_OD_POS_X,PARAM_OD_POS_Y,PARAM_OD_POS_Z,ODR,"
            "rev0,rev1,rev2,rev3,rev4,rev5,rev6,rev7,rev8,rev9,VERSION\n");
    default:
        return QString();
    }
}

QString DebugFrameParser::formatFrameFor(ProtocolID pid, const ParsedFrame &f)
{
    QStringList c;
    switch (pid) {
    case ProtocolID::Debug_IMU: {
        const auto &d = f.debugImu;
        c << QString::number(d.time_gps_ms)
          << QString::number(d.week_num)
          << QString::number(d.gyro[0], 'f', 6) << QString::number(d.gyro[1], 'f', 6) << QString::number(d.gyro[2], 'f', 6)
          << QString::number(d.accel[0], 'f', 6) << QString::number(d.accel[1], 'f', 6) << QString::number(d.accel[2], 'f', 6)
          << QString::number(d.temperature, 'f', 2)
          << QString::number(d.status)
          << QString::number(d.att_ahrs[0], 'f', 6) << QString::number(d.att_ahrs[1], 'f', 6) << QString::number(d.att_ahrs[2], 'f', 6)
          << QString::number(d.att[0], 'f', 6) << QString::number(d.att[1], 'f', 6) << QString::number(d.att[2], 'f', 6)
          << QString::number(d.vel[0], 'f', 6) << QString::number(d.vel[1], 'f', 6) << QString::number(d.vel[2], 'f', 6)
          << QString::number(d.pos[0] / 1e7, 'f', 7)   // 纬度
          << QString::number(d.pos[1] / 1e7, 'f', 7)   // 经度
          << QString::number(d.pos[2] / 1000.0, 'f', 3); // 高度 m
        break;
    }
    case ProtocolID::Debug_GPS: {
        const auto &d = f.debugGps;
        c << QString::number(d.itow)
          << QString::number(d.week_num)
          << QString::number(d.pps_time_us)
          << QString::number(d.revc_time_ms)
          << QString::number(d.fixtype)
          << QString::number(d.numSV)
          << QString::number(d.multiSVs)
          << QString::number(d.lat / 1e7, 'f', 7)
          << QString::number(d.lon / 1e7, 'f', 7)
          << QString::number(d.alt / 1000.0, 'f', 3)
          << QString::number(d.vn, 'f', 6) << QString::number(d.ve, 'f', 6) << QString::number(d.vd, 'f', 6)
          << QString::number(d.gspeed, 'f', 6)
          << QString::number(d.headMot, 'f', 3)
          << QString::number(d.p_hacc) << QString::number(d.p_vacc)
          << QString::number(d.v_hacc) << QString::number(d.v_vacc)
          << QString::number(d.diff_age);
        break;
    }
    case ProtocolID::Debug_DRTK: {
        const auto &d = f.debugDrtk;
        c << QString::number(d.itow)
          << QString::number(d.fixtype)
          << QString::number(d.heading, 'f', 6) << QString::number(d.heading_std, 'f', 6)
          << QString::number(d.pitch, 'f', 6) << QString::number(d.pitch_std, 'f', 6)
          << QString::number(d.length) << QString::number(d.svnum);
        break;
    }
    case ProtocolID::Debug_OD: {
        const auto &d = f.debugOd;
        c << QString::number(d.time_gps_ms)
          << QString::number(d.vel_fl_cm_s) << QString::number(d.vel_fr_cm_s)
          << QString::number(d.vel_bl_cm_s) << QString::number(d.vel_br_cm_s)
          << QString::number(d.gear) << QString::number(d.steer_angle);
        break;
    }
    case ProtocolID::Debug_KF: {
        const auto &d = f.debugKf;
        for (int i = 0; i < 18; ++i) c << QString::number(d.pk[i], 'f', 6);
        for (int i = 0; i < 6;  ++i) c << QString::number(d.imu_bias[i], 'f', 6);
        for (int i = 0; i < 6;  ++i) c << QString::number(d.innov[i], 'f', 6);
        c << QString::number(d.od_k) << QString::number(d.inst_pitch) << QString::number(d.inst_yaw)
          << QString::number(d.time_consume, 'f', 6) << QString::number(d.task_period) << QString::number(d.cpu_usage);
        for (int i = 0; i < 3; ++i) c << QString::number(d.RK_p[i]);
        for (int i = 0; i < 3; ++i) c << QString::number(d.Rk_v[i]);
        for (int i = 0; i < 3; ++i) c << QString::number(d.Rk_b[i]);
        for (int i = 0; i < 4; ++i) c << QString::number(d.rev[i]);
        break;
    }
    case ProtocolID::Debug_Param: {
        const auto &d = f.debugParam;
        c << QString::number(d.SETTING, 'f', 6)
          << QString::number(d.GPS_HACC_FIX, 'f', 6) << QString::number(d.GPS_HACC_SINGLE, 'f', 6)
          << QString::number(d.GPS_SACC, 'f', 6) << QString::number(d.GPS_HEADING_ACC, 'f', 6)
          << QString::number(d.GYRO_STD, 'f', 6) << QString::number(d.ACCEL_STD, 'f', 6)
          << QString::number(d.GYRO_ZUPT_VALUE, 'f', 6) << QString::number(d.MEAS_CONSTRAIN_AHRS, 'f', 6)
          << QString::number(d.ALIGN_VEL, 'f', 6)
          << QString::number(d.PARAM_ANTENNA_INST_ANGLE, 'f', 6) << QString::number(d.PARAM_ANTENNA_MODIFY_ANGLE, 'f', 6)
          << QString::number(d.ANTENNA_LENGTH, 'f', 6) << QString::number(d.PARAM_OD_K, 'f', 6)
          << QString::number(d.PARAM_INST_ROLL, 'f', 6) << QString::number(d.PARAM_INST_PITCH, 'f', 6) << QString::number(d.PARAM_INST_YAW, 'f', 6)
          << QString::number(d.PARAM_GPS_POS_X, 'f', 6) << QString::number(d.PARAM_GPS_POS_Y, 'f', 6) << QString::number(d.PARAM_GPS_POS_Z, 'f', 6)
          << QString::number(d.PARAM_OD_POS_X, 'f', 6) << QString::number(d.PARAM_OD_POS_Y, 'f', 6) << QString::number(d.PARAM_OD_POS_Z, 'f', 6)
          << QString::number(d.ODR, 'f', 6);
        for (int i = 0; i < 10; ++i) c << QString::number(d.rev[i], 'f', 6);
        c << QString::number(d.VERSION, 'f', 6);
        break;
    }
    default:
        return QString();
    }
    return c.join(',') + QStringLiteral("\n");
}
