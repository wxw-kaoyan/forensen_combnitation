#include "modules/parse/parser/single/protocolregistry.h"
#include "modules/parse/parser/parserbase.h"
#include "modules/parse/parser/core/frameparser.h"

// ════════════════════════════════════════════════════════════════
//  各协议字段列表
// ════════════════════════════════════════════════════════════════

static QList<FieldDescriptor> ahrsBasicFields()
{
    return {
        {"Timer(us)",  "timer",  [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.timer); }},
        {"Pitch(deg)", "pitch",  [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.pitch,  'f', 6); }},
        {"Roll(deg)",  "roll",   [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.roll,   'f', 6); }},
        {"Yaw(deg)",   "yaw",    [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.yaw,    'f', 6); }},
        {"Ax(g)",      "ax",     [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.ax,     'f', 6); }},
        {"Ay(g)",      "ay",     [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.ay,     'f', 6); }},
        {"Az(g)",      "az",     [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.az,     'f', 6); }},
        {"Gx(dps)",    "gx",     [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.gx,     'f', 6); }},
        {"Gy(dps)",    "gy",     [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.gy,     'f', 6); }},
        {"Gz(dps)",    "gz",     [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.gz,     'f', 6); }},
        {"Temp(C)",    "temp",   [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.temp,   'f', 2); }},
    };
}

static QList<FieldDescriptor> ahrsExtendedFields()
{
    auto base = ahrsBasicFields();
    base.append({
        {"Flag",       "flag",   [](const ParsedFrame &f) { return QString::number(f.ahrsExtended.flag); }},
        {"Status",     "status", [](const ParsedFrame &f) { return QStringLiteral("0x") + QString::number(f.ahrsExtended.status, 16); }},
    });
    return base;
}

static QList<FieldDescriptor> quaternionFields()
{
    return {
        {"Timer(us)",  "timer",  [](const ParsedFrame &f) { return QString::number(f.quat.timer); }},
        {"Qw",         "qw",     [](const ParsedFrame &f) { return QString::number(f.quat.qw, 'f', 8); }},
        {"Qx",         "qx",     [](const ParsedFrame &f) { return QString::number(f.quat.qx, 'f', 8); }},
        {"Qy",         "qy",     [](const ParsedFrame &f) { return QString::number(f.quat.qy, 'f', 8); }},
        {"Qz",         "qz",     [](const ParsedFrame &f) { return QString::number(f.quat.qz, 'f', 8); }},
        {"Ax(g)",      "ax",     [](const ParsedFrame &f) { return QString::number(f.quat.ax,    'f', 6); }},
        {"Ay(g)",      "ay",     [](const ParsedFrame &f) { return QString::number(f.quat.ay,    'f', 6); }},
        {"Az(g)",      "az",     [](const ParsedFrame &f) { return QString::number(f.quat.az,    'f', 6); }},
        {"Gx(dps)",    "gx",     [](const ParsedFrame &f) { return QString::number(f.quat.gx,    'f', 4); }},
        {"Gy(dps)",    "gy",     [](const ParsedFrame &f) { return QString::number(f.quat.gy,    'f', 4); }},
        {"Gz(dps)",    "gz",     [](const ParsedFrame &f) { return QString::number(f.quat.gz,    'f', 4); }},
        {"Temp(C)",    "temp",   [](const ParsedFrame &f) { return QString::number(f.quat.temp,  'f', 2); }},
        {"Flag",       "flag",   [](const ParsedFrame &f) { return QString::number(f.quat.flag); }},
        {"Status",     "status", [](const ParsedFrame &f) { return QStringLiteral("0x") + QString::number(f.quat.status, 16); }},
    };
}

static QList<FieldDescriptor> agDataFields()
{
    return {
        {"GPS(ms)",    "gpsTime",  [](const ParsedFrame &f) { return QString::number(f.agData.gpsTime); }},
        {"Roll(deg)",  "roll",     [](const ParsedFrame &f) { return QString::number(f.agData.roll,   'f', 6); }},
        {"Pitch(deg)", "pitch",    [](const ParsedFrame &f) { return QString::number(f.agData.pitch,  'f', 6); }},
        {"Yaw(deg)",   "yaw",      [](const ParsedFrame &f) { return QString::number(f.agData.yaw,    'f', 6); }},
        {"GyroX(dps)", "gyroX",    [](const ParsedFrame &f) { return QString::number(f.agData.gyroX,  'f', 4); }},
        {"GyroY(dps)", "gyroY",    [](const ParsedFrame &f) { return QString::number(f.agData.gyroY,  'f', 4); }},
        {"GyroZ(dps)", "gyroZ",    [](const ParsedFrame &f) { return QString::number(f.agData.gyroZ,  'f', 4); }},
        {"AccelX(g)",  "accelX",   [](const ParsedFrame &f) { return QString::number(f.agData.accelX, 'f', 6); }},
        {"AccelY(g)",  "accelY",   [](const ParsedFrame &f) { return QString::number(f.agData.accelY, 'f', 6); }},
        {"AccelZ(g)",  "accelZ",   [](const ParsedFrame &f) { return QString::number(f.agData.accelZ, 'f', 6); }},
        {"RTK状态",    "rtkStatus",[](const ParsedFrame &f) { return ParserBase::rtkStatusName(f.agData.rtkStatus); }},
        {"姿态有效",   "attValid", [](const ParsedFrame &f) { return QString::number(f.agData.attValid); }},
        {"状态标志",   "statusFlag",[](const ParsedFrame &f){ return QStringLiteral("0x") + QString::number(f.agData.statusFlag, 16); }},
    };
}

static QList<FieldDescriptor> fusionResultFields()
{
    return {
        {"GPS周秒(ms)",  "gpsWeekSecond", [](const ParsedFrame &f) { return QString::number(f.fusion.gpsWeekSecond); }},
        {"GPS周",        "gpsWeek",       [](const ParsedFrame &f) { return QString::number(f.fusion.gpsWeek); }},
        {"纬度(deg)",    "lat",           [](const ParsedFrame &f) { return QString::number(f.fusion.lat / 1e7, 'f', 7); }},
        {"经度(deg)",    "lon",           [](const ParsedFrame &f) { return QString::number(f.fusion.lon / 1e7, 'f', 7); }},
        {"高度(m)",      "alt",           [](const ParsedFrame &f) { return QString::number(f.fusion.alt / 1000.0, 'f', 3); }},
        {"北向速度(m/s)","velN",          [](const ParsedFrame &f) { return QString::number(f.fusion.velN, 'f', 3); }},
        {"东向速度(m/s)","velE",          [](const ParsedFrame &f) { return QString::number(f.fusion.velE, 'f', 3); }},
        {"地向速度(m/s)","velD",          [](const ParsedFrame &f) { return QString::number(f.fusion.velD, 'f', 3); }},
        {"横滚(deg)",    "roll",          [](const ParsedFrame &f) { return QString::number(f.fusion.roll,   'f', 3); }},
        {"俯仰(deg)",    "pitch",         [](const ParsedFrame &f) { return QString::number(f.fusion.pitch,  'f', 3); }},
        {"航向(deg)",    "yaw",           [](const ParsedFrame &f) { return QString::number(f.fusion.yaw,    'f', 3); }},
        {"航向角(deg)",  "heading",       [](const ParsedFrame &f) { return QString::number(f.fusion.heading,'f', 3); }},
        {"前轮转角(deg)","wheelAngle",    [](const ParsedFrame &f) { return QString::number(f.fusion.wheelAngle,'f', 3); }},
        {"Ax(g)",        "accelX",        [](const ParsedFrame &f) { return QString::number(f.fusion.accelX, 'f', 4); }},
        {"Ay(g)",        "accelY",        [](const ParsedFrame &f) { return QString::number(f.fusion.accelY, 'f', 4); }},
        {"Az(g)",        "accelZ",        [](const ParsedFrame &f) { return QString::number(f.fusion.accelZ, 'f', 4); }},
        {"Gx(dps)",      "gyroX",         [](const ParsedFrame &f) { return QString::number(f.fusion.gyroX,  'f', 3); }},
        {"Gy(dps)",      "gyroY",         [](const ParsedFrame &f) { return QString::number(f.fusion.gyroY,  'f', 3); }},
        {"Gz(dps)",      "gyroZ",         [](const ParsedFrame &f) { return QString::number(f.fusion.gyroZ,  'f', 3); }},
        {"温度(C)",      "temp",          [](const ParsedFrame &f) { return QString::number(f.fusion.temp,   'f', 1); }},
        {"RTK状态",      "rtkStatus",     [](const ParsedFrame &f) { return ParserBase::rtkStatusName(f.fusion.rtkStatus); }},
        {"卫星数",       "numSatellites", [](const ParsedFrame &f) { return QString::number(f.fusion.numSatellites); }},
        {"差分龄期(s)",  "diffAge",       [](const ParsedFrame &f) { return QString::number(f.fusion.diffAge); }},
        {"双天线状态",   "dualAntStatus", [](const ParsedFrame &f) { return f.fusion.dualAntStatus == 50 ? QStringLiteral("已定向") : QString::number(f.fusion.dualAntStatus); }},
        {"PosStd(cm)",   "posStd",        [](const ParsedFrame &f) { return QString::number(f.fusion.posStd); }},
        {"状态标志",     "statusFlag",    [](const ParsedFrame &f) { return ParserBase::formatStatusFlags(f.fusion.statusFlag); }},
    };
}

static QList<FieldDescriptor> tiltMeasureFields()
{
    return {
        {"纬度(deg)",        "lat",         [](const ParsedFrame &f) { return QString::number(f.tilt.lat, 'f', 7); }},
        {"经度(deg)",        "lon",         [](const ParsedFrame &f) { return QString::number(f.tilt.lon, 'f', 7); }},
        {"高程(m)",          "elev",        [](const ParsedFrame &f) { return QString::number(f.tilt.elev, 'f', 3); }},
        {"激光偏角X",        "laserBias0",  [](const ParsedFrame &f) { return QString::number(f.tilt.laserBias[0]); }},
        {"激光偏角Y",        "laserBias1",  [](const ParsedFrame &f) { return QString::number(f.tilt.laserBias[1]); }},
        {"激光距离(mm)",     "laserDist",   [](const ParsedFrame &f) { return QString::number(f.tilt.laserDist); }},
        {"采点数",           "sampleCnt",   [](const ParsedFrame &f) { return QString::number(f.tilt.sampleCnt); }},
        {"坐标标志",         "coordFlag",   [](const ParsedFrame &f) { return QString::number(f.tilt.coordFlag); }},
        {"温度(℃*100)",     "temp",        [](const ParsedFrame &f) { return QString::number(f.tilt.temp); }},
        {"横滚标准差",       "rollStd",     [](const ParsedFrame &f) { return QString::number(f.tilt.rollStd); }},
        {"俯仰标准差",       "pitchStd",    [](const ParsedFrame &f) { return QString::number(f.tilt.pitchStd); }},
        {"对中杆倾角(deg)", "tiltRoll",    [](const ParsedFrame &f) { return QString::number(f.tilt.tiltRoll, 'f', 3); }},
        {"精度因子",         "tiltPitch",   [](const ParsedFrame &f) { return QString::number(f.tilt.tiltPitch, 'f', 3); }},
        {"陀螺X(deg/s)",     "gyroRaw0",    [](const ParsedFrame &f) { return QString::number(f.tilt.gyroRaw[0], 'f', 4); }},
        {"陀螺Y(deg/s)",     "gyroRaw1",    [](const ParsedFrame &f) { return QString::number(f.tilt.gyroRaw[1], 'f', 4); }},
        {"陀螺Z(deg/s)",     "gyroRaw2",    [](const ParsedFrame &f) { return QString::number(f.tilt.gyroRaw[2], 'f', 4); }},
        {"加表X(g)",         "accelRaw0",   [](const ParsedFrame &f) { return QString::number(f.tilt.accelRaw[0], 'f', 4); }},
        {"加表Y(g)",         "accelRaw1",   [](const ParsedFrame &f) { return QString::number(f.tilt.accelRaw[1], 'f', 4); }},
        {"加表Z(g)",         "accelRaw2",   [](const ParsedFrame &f) { return QString::number(f.tilt.accelRaw[2], 'f', 4); }},
        {"时间戳",           "timestamp",   [](const ParsedFrame &f) { return QString::number(f.tilt.timestamp); }},
        {"系统状态",         "sysStatus",   [](const ParsedFrame &f) { return QString::number(f.tilt.sysStatus); }},
        {"当前杆长(mm)",     "poleHeight",  [](const ParsedFrame &f) { return QString::number(f.tilt.poleHeight); }},
        {"标定进度(%)",      "solutionRate",[](const ParsedFrame &f) { return QString::number(f.tilt.solutionRate, 'f', 3); }},
        {"GNSS定位",         "gnssFix",     [](const ParsedFrame &f) { return ParserBase::rtkStatusName(f.tilt.gnssFix); }},
        {"卫星数",           "gnssSatCnt",  [](const ParsedFrame &f) { return QString::number(f.tilt.gnssSatCnt); }},
        {"RTK差分延时(s)",   "rtkAge",      [](const ParsedFrame &f) { return QString::number(f.tilt.rtkAge); }},
        {"杆安装偏差X",      "poleBias0",   [](const ParsedFrame &f) { return QString::number(f.tilt.poleBias[0]); }},
        {"杆安装偏差Y",      "poleBias1",   [](const ParsedFrame &f) { return QString::number(f.tilt.poleBias[1]); }},
        {"方位角(deg)",      "azimuth",     [](const ParsedFrame &f) { return QString::number(f.tilt.azimuth, 'f', 3); }},
        {"横滚角(deg)",      "rollDeg",     [](const ParsedFrame &f) { return QString::number(f.tilt.rollDeg, 'f', 3); }},
        {"俯仰角(deg)",      "pitchDeg",    [](const ParsedFrame &f) { return QString::number(f.tilt.pitchDeg, 'f', 3); }},
        {"航向角(deg)",      "yawDeg",      [](const ParsedFrame &f) { return QString::number(f.tilt.yawDeg, 'f', 3); }},
    };
}

static QList<FieldDescriptor> laserFields()
{
    return {
        {"激光距离(m)",   "laserDist",   [](const ParsedFrame &f) { return QString::number(f.laser.laserDist, 'f', 3); }},
        {"激光偏角X(deg)","laserBias0",  [](const ParsedFrame &f) { return QString::number(f.laser.laserBias[0], 'f', 3); }},
        {"激光偏角Y(deg)","laserBias1",  [](const ParsedFrame &f) { return QString::number(f.laser.laserBias[1], 'f', 3); }},
        {"目标纬度(deg)", "targetLat",   [](const ParsedFrame &f) { return QString::number(f.laser.targetLat, 'f', 7); }},
        {"目标经度(deg)", "targetLon",   [](const ParsedFrame &f) { return QString::number(f.laser.targetLon, 'f', 7); }},
        {"目标高程(m)",   "targetElev",  [](const ParsedFrame &f) { return QString::number(f.laser.targetElev, 'f', 3); }},
        {"杆纬度(deg)",   "poleLat",     [](const ParsedFrame &f) { return QString::number(f.laser.poleLat, 'f', 7); }},
        {"杆经度(deg)",   "poleLon",     [](const ParsedFrame &f) { return QString::number(f.laser.poleLon, 'f', 7); }},
        {"杆高程(m)",     "poleElev",    [](const ParsedFrame &f) { return QString::number(f.laser.poleElev, 'f', 3); }},
        {"解算类型",      "solutionType",[](const ParsedFrame &f) { return QString::number(f.laser.solutionType); }},
        {"倾斜角(deg)",   "tiltAngle",   [](const ParsedFrame &f) { return QString::number(f.laser.tiltAngle, 'f', 3); }},
    };
}

static QList<FieldDescriptor> nanfangLaserFields()
{
    return {
        {"时间戳(ms)",      "timestamp",       [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.timestamp); }},
        {"激光距离(mm)",    "laserDist",       [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.laserDist, 'f', 3); }},
        {"横滚(deg)",       "roll",            [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.roll, 'f', 6); }},
        {"俯仰(deg)",       "pitch",           [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.pitch, 'f', 6); }},
        {"航向(deg)",       "yaw",             [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.yaw, 'f', 6); }},
        {"发射点纬度(deg)", "emitLat",         [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.emitLat, 'f', 8); }},
        {"发射点经度(deg)", "emitLon",         [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.emitLon, 'f', 8); }},
        {"发射点高程(m)",   "emitAlt",         [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.emitAlt, 'f', 3); }},
        {"光心纬度(deg)",   "centerLat",       [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.centerLat, 'f', 8); }},
        {"光心经度(deg)",   "centerLon",       [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.centerLon, 'f', 8); }},
        {"光心高程(m)",     "centerAlt",       [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.centerAlt, 'f', 3); }},
        {"激光故障码",      "faultCode",       [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.faultCode); }},
        {"精度因子(deg)",   "precisionFactor", [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.precisionFactor, 'f', 6); }},
    };
}

static QList<FieldDescriptor> zhdLaserFields()
{
    return {
        {"时间戳(ms)",         "timestamp",       [](const ParsedFrame &f) { return QString::number(f.zhdLaser.timestamp); }},
        {"激光距离(m)",        "laserDist",       [](const ParsedFrame &f) { return QString::number(f.zhdLaser.laserDist, 'f', 3); }},
        {"横滚(deg)",          "roll",            [](const ParsedFrame &f) { return QString::number(f.zhdLaser.roll, 'f', 6); }},
        {"俯仰(deg)",          "pitch",           [](const ParsedFrame &f) { return QString::number(f.zhdLaser.pitch, 'f', 6); }},
        {"航向(deg)",          "yaw",             [](const ParsedFrame &f) { return QString::number(f.zhdLaser.yaw, 'f', 6); }},
        {"激光点纬度(deg)",    "laserLat",        [](const ParsedFrame &f) { return QString::number(f.zhdLaser.laserLat, 'f', 8); }},
        {"激光点经度(deg)",    "laserLon",        [](const ParsedFrame &f) { return QString::number(f.zhdLaser.laserLon, 'f', 8); }},
        {"激光点高程(m)",      "laserAlt",        [](const ParsedFrame &f) { return QString::number(f.zhdLaser.laserAlt, 'f', 3); }},
        {"IMU纬度(deg)",       "imuLat",          [](const ParsedFrame &f) { return QString::number(f.zhdLaser.imuLat, 'f', 8); }},
        {"IMU经度(deg)",       "imuLon",          [](const ParsedFrame &f) { return QString::number(f.zhdLaser.imuLon, 'f', 8); }},
        {"IMU高程(m)",         "imuAlt",          [](const ParsedFrame &f) { return QString::number(f.zhdLaser.imuAlt, 'f', 3); }},
        {"激光点纬度中误差(mm)","laserLatStd",     [](const ParsedFrame &f) { return QString::number(f.zhdLaser.laserLatStd); }},
        {"激光点经度中误差(mm)","laserLonStd",     [](const ParsedFrame &f) { return QString::number(f.zhdLaser.laserLonStd); }},
        {"激光点高程中误差(mm)","laserAltStd",     [](const ParsedFrame &f) { return QString::number(f.zhdLaser.laserAltStd); }},
        {"横滚中误差(0.01deg)","rollStd",         [](const ParsedFrame &f) { return QString::number(f.zhdLaser.rollStd); }},
        {"俯仰中误差(0.01deg)","pitchStd",        [](const ParsedFrame &f) { return QString::number(f.zhdLaser.pitchStd); }},
        {"航向中误差(0.01deg)","yawStd",          [](const ParsedFrame &f) { return QString::number(f.zhdLaser.yawStd); }},
        {"IMU纬度中误差(mm)",  "imuLatStd",       [](const ParsedFrame &f) { return QString::number(f.zhdLaser.imuLatStd); }},
        {"IMU经度中误差(mm)",  "imuLonStd",       [](const ParsedFrame &f) { return QString::number(f.zhdLaser.imuLonStd); }},
        {"IMU高程中误差(mm)",  "imuAltStd",       [](const ParsedFrame &f) { return QString::number(f.zhdLaser.imuAltStd); }},
        {"激光错误类型",       "laserError",      [](const ParsedFrame &f) { return QString::number(f.zhdLaser.laserError); }},
        {"精度因子(deg)",      "precisionFactor", [](const ParsedFrame &f) { return QString::number(f.zhdLaser.precisionFactor, 'f', 6); }},
    };
}

static QList<FieldDescriptor> attitudeFields()
{
    return {
        {"Timestamp(us)","timestamp", [](const ParsedFrame &f) { return QString::number(f.attitude.timestamp); }},
        {"Roll(deg)",    "roll",      [](const ParsedFrame &f) { return QString::number(f.attitude.roll,  'f', 6); }},
        {"Pitch(deg)",   "pitch",     [](const ParsedFrame &f) { return QString::number(f.attitude.pitch, 'f', 6); }},
        {"Yaw(deg)",     "yaw",       [](const ParsedFrame &f) { return QString::number(f.attitude.yaw,   'f', 6); }},
        {"RollStd(deg)", "rollStd",   [](const ParsedFrame &f) { return QString::number(f.attitude.rollStd,  'f', 3); }},
        {"PitchStd(deg)","pitchStd",  [](const ParsedFrame &f) { return QString::number(f.attitude.pitchStd, 'f', 3); }},
        {"YawStd(deg)",  "yawStd",    [](const ParsedFrame &f) { return QString::number(f.attitude.yawStd,   'f', 3); }},
    };
}

static QList<FieldDescriptor> quat614PFields()
{
    return {
        {"GPS周内毫秒(ms)","gpsWeekMs",   [](const ParsedFrame &f) { return QString::number(f.quat614p.gpsWeekMs); }},
        {"INS纬度(deg)",   "insLat",      [](const ParsedFrame &f) { return QString::number(f.quat614p.insLat, 'f', 8); }},
        {"INS经度(deg)",   "insLon",      [](const ParsedFrame &f) { return QString::number(f.quat614p.insLon, 'f', 8); }},
        {"INS高程(m)",     "insAlt",      [](const ParsedFrame &f) { return QString::number(f.quat614p.insAlt, 'f', 3); }},
        {"四元数W",        "quatW",       [](const ParsedFrame &f) { return QString::number(f.quat614p.quatW, 'f', 8); }},
        {"四元数X",        "quatX",       [](const ParsedFrame &f) { return QString::number(f.quat614p.quatX, 'f', 8); }},
        {"四元数Y",        "quatY",       [](const ParsedFrame &f) { return QString::number(f.quat614p.quatY, 'f', 8); }},
        {"四元数Z",        "quatZ",       [](const ParsedFrame &f) { return QString::number(f.quat614p.quatZ, 'f', 8); }},
        {"IMU精度因子",    "imuPrecision",[](const ParsedFrame &f) { return QString::number(f.quat614p.imuPrecision, 'f', 4); }},
        {"系统状态",       "systemStatus",[](const ParsedFrame &f) { return QStringLiteral("0x") + QString::number(f.quat614p.systemStatus, 16); }},
    };
}

static QList<FieldDescriptor> canRtkFields()
{
    return {
        {"UTC天数",     "utcDays",       [](const ParsedFrame &f) { return QString::number(f.canRtk.utcDays); }},
        {"UTC天内秒",   "secondsOfDay",  [](const ParsedFrame &f) { return QString::number(f.canRtk.secondsOfDay, 'f', 3); }},
        {"定位类型",    "fixType",       [](const ParsedFrame &f) { return ParserBase::rtkStatusName(f.canRtk.fixType); }},
        {"差分龄期",    "diffAge",       [](const ParsedFrame &f) { return QString::number(f.canRtk.diffAge); }},
        {"卫星数",      "numSatellites", [](const ParsedFrame &f) { return QString::number(f.canRtk.numSatellites); }},
        {"纬度(deg)",   "latitude",      [](const ParsedFrame &f) { return QString::number(f.canRtk.latitude, 'f', 8); }},
        {"经度(deg)",   "longitude",     [](const ParsedFrame &f) { return QString::number(f.canRtk.longitude, 'f', 8); }},
        {"海拔(m)",     "altitude",      [](const ParsedFrame &f) { return QString::number(f.canRtk.altitude, 'f', 3); }},
        {"前向速度(m/s)","forwardSpeed", [](const ParsedFrame &f) { return QString::number(f.canRtk.forwardSpeed, 'f', 3); }},
        {"速度方向(deg)","speedDirection",[](const ParsedFrame &f){ return (f.canRtk.sdRaw != 0xFFFF) ? QString::number(f.canRtk.speedDirection, 'f', 2) : QStringLiteral("N/A"); }},
        {"横滚角(deg)", "roll",          [](const ParsedFrame &f) { return f.canRtk.rollValid ? QString::number(f.canRtk.roll, 'f', 3) : QStringLiteral("N/A"); }},
        {"俯仰角(deg)", "pitch",         [](const ParsedFrame &f) { return f.canRtk.pitchValid ? QString::number(f.canRtk.pitch, 'f', 3) : QStringLiteral("N/A"); }},
        {"航向角(deg)", "yaw",           [](const ParsedFrame &f) { return f.canRtk.yawValid ? QString::number(f.canRtk.yaw, 'f', 3) : QStringLiteral("N/A"); }},
    };
}

// ════════════════════════════════════════════════════════════════
//  标准帧协议通用解析函数
// ════════════════════════════════════════════════════════════════
static QList<ParsedFrame> genericFrameParse(ProtocolID pid, const QByteArray &rawData, bool skipCrc)
{
    FrameParser fp;
    QList<ParsedFrame> all = fp.parseAll(rawData, skipCrc);
    QList<ParsedFrame> filtered;
    for (const auto &f : all) {
        if (f.protocol == pid)
            filtered.append(f);
    }
    return filtered;
}

// ════════════════════════════════════════════════════════════════
//  协议注册表（所有标准帧协议）
// ════════════════════════════════════════════════════════════════
const QList<ProtocolDescriptor> &protocolRegistry()
{
    static const QList<ProtocolDescriptor> reg = {
        {
            QStringLiteral("AHRS基本"),
            ProtocolID::AHRS_Basic,
            false,
            QStringLiteral("Timer(us),Pitch,Roll,Yaw,Ax(g),Ay(g),Az(g),Gx(dps),Gy(dps),Gz(dps),Temp(C)\n"),
            [](const QByteArray &d, bool s) { return genericFrameParse(ProtocolID::AHRS_Basic, d, s); },
            [](const ParsedFrame &f) -> QString {
                const auto &d = f.ahrsBasic;
                return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11\n")
                    .arg(d.timer)
                    .arg(d.pitch, 0, 'f', 6).arg(d.roll, 0, 'f', 6).arg(d.yaw, 0, 'f', 6)
                    .arg(d.ax, 0, 'f', 6).arg(d.ay, 0, 'f', 6).arg(d.az, 0, 'f', 6)
                    .arg(d.gx, 0, 'f', 6).arg(d.gy, 0, 'f', 6).arg(d.gz, 0, 'f', 6)
                    .arg(d.temp, 0, 'f', 2);
            },
            ahrsBasicFields(),
        },
        {
            QStringLiteral("AHRS扩展"),
            ProtocolID::AHRS_Extended,
            false,
            QStringLiteral("Timer(us),Pitch,Roll,Yaw,Ax(g),Ay(g),Az(g),Gx(dps),Gy(dps),Gz(dps),Temp(C),Flag,Status\n"),
            [](const QByteArray &d, bool s) { return genericFrameParse(ProtocolID::AHRS_Extended, d, s); },
            [](const ParsedFrame &f) -> QString {
                const auto &d = f.ahrsExtended;
                auto &b = d.basic;
                return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,0x%13\n")
                    .arg(b.timer)
                    .arg(b.pitch, 0, 'f', 6).arg(b.roll, 0, 'f', 6).arg(b.yaw, 0, 'f', 6)
                    .arg(b.ax, 0, 'f', 6).arg(b.ay, 0, 'f', 6).arg(b.az, 0, 'f', 6)
                    .arg(b.gx, 0, 'f', 6).arg(b.gy, 0, 'f', 6).arg(b.gz, 0, 'f', 6)
                    .arg(b.temp, 0, 'f', 2)
                    .arg(d.flag).arg(d.status, 0, 8);
            },
            ahrsExtendedFields(),
        },
        {
            QStringLiteral("四元数"),
            ProtocolID::Quaternion,
            false,
            QStringLiteral("Timer(us),Qw,Qx,Qy,Qz,Ax(g),Ay(g),Az(g),Gx,Gy,Gz,Temp(C),Flag,Status\n"),
            [](const QByteArray &d, bool s) { return genericFrameParse(ProtocolID::Quaternion, d, s); },
            [](const ParsedFrame &f) -> QString {
                const auto &d = f.quat;
                return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,0x%14\n")
                    .arg(d.timer)
                    .arg(d.qw, 0, 'f', 8).arg(d.qx, 0, 'f', 8).arg(d.qy, 0, 'f', 8).arg(d.qz, 0, 'f', 8)
                    .arg(d.ax, 0, 'f', 6).arg(d.ay, 0, 'f', 6).arg(d.az, 0, 'f', 6)
                    .arg(d.gx, 0, 'f', 4).arg(d.gy, 0, 'f', 4).arg(d.gz, 0, 'f', 4)
                    .arg(d.temp, 0, 'f', 2)
                    .arg(d.flag).arg(d.status, 0, 8);
            },
            quaternionFields(),
        },
        {
            QStringLiteral("AG数据"),
            ProtocolID::AG_Data,
            false,
            QStringLiteral("GPS(ms),Roll,Pitch,Yaw,GyroX,GyroY,GyroZ,AccelX,AccelY,AccelZ,RTK,AttValid,Status\n"),
            [](const QByteArray &d, bool s) { return genericFrameParse(ProtocolID::AG_Data, d, s); },
            [](const ParsedFrame &f) -> QString {
                const auto &d = f.agData;
                return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,0x%13\n")
                    .arg(d.gpsTime)
                    .arg(d.roll, 0, 'f', 6).arg(d.pitch, 0, 'f', 6).arg(d.yaw, 0, 'f', 6)
                    .arg(d.gyroX, 0, 'f', 4).arg(d.gyroY, 0, 'f', 4).arg(d.gyroZ, 0, 'f', 4)
                    .arg(d.accelX, 0, 'f', 6).arg(d.accelY, 0, 'f', 6).arg(d.accelZ, 0, 'f', 6)
                    .arg(ParserBase::rtkStatusName(d.rtkStatus))
                    .arg(d.attValid).arg(d.statusFlag, 0, 8);
            },
            agDataFields(),
        },
        {
            QStringLiteral("组合导航"),
            ProtocolID::FusionResult,
            false,
            QStringLiteral("GPS周秒(ms),周,纬度,经度,高度(m),VelN,VelE,VelD,Roll,Pitch,Yaw,Heading,Wheel,Ax,Ay,Az,Gx,Gy,Gz,Temp,RTK,Sat,DiffAge,DualAnt,PosStd(cm),Status\n"),
            [](const QByteArray &d, bool s) { return genericFrameParse(ProtocolID::FusionResult, d, s); },
            [](const ParsedFrame &f) -> QString {
                const auto &d = f.fusion;
                double latDeg = d.lat / 1e7;
                double lonDeg = d.lon / 1e7;
                double altM = d.alt / 1000.0;
                return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23,%24,%25,%26\n")
                    .arg(d.gpsWeekSecond).arg(d.gpsWeek)
                    .arg(latDeg, 0, 'f', 7).arg(lonDeg, 0, 'f', 7).arg(altM, 0, 'f', 3)
                    .arg(d.velN, 0, 'f', 3).arg(d.velE, 0, 'f', 3).arg(d.velD, 0, 'f', 3)
                    .arg(d.roll, 0, 'f', 3).arg(d.pitch, 0, 'f', 3).arg(d.yaw, 0, 'f', 3)
                    .arg(d.heading, 0, 'f', 3).arg(d.wheelAngle, 0, 'f', 3)
                    .arg(d.accelX, 0, 'f', 4).arg(d.accelY, 0, 'f', 4).arg(d.accelZ, 0, 'f', 4)
                    .arg(d.gyroX, 0, 'f', 3).arg(d.gyroY, 0, 'f', 3).arg(d.gyroZ, 0, 'f', 3)
                    .arg(d.temp, 0, 'f', 1)
                    .arg(ParserBase::rtkStatusName(d.rtkStatus)).arg(d.numSatellites).arg(d.diffAge)
                    .arg(d.dualAntStatus == 50 ? QStringLiteral("已定向") : QString::number(d.dualAntStatus))
                    .arg(d.posStd)
                    .arg(ParserBase::formatStatusFlags(d.statusFlag));
            },
            fusionResultFields(),
        },
        {
            QStringLiteral("倾斜测量"),
            ProtocolID::TiltMeasure,
            false,
            QStringLiteral("纬度(deg),经度(deg),高程(m),激光偏角X,激光偏角Y,激光距离(mm),采点数,坐标标志,温度(℃*100),横滚标准差,俯仰标准差,对中杆倾角(deg),精度因子,陀螺X(deg/s),陀螺Y(deg/s),陀螺Z(deg/s),加表X(g),加表Y(g),加表Z(g),时间戳,系统状态,当前杆长(mm),标定进度(%),GNSS定位,卫星数,RTK差分延时(s),杆安装偏差X,杆安装偏差Y,方位角(deg),横滚角(deg),俯仰角(deg),航向角(deg)\n"),
            [](const QByteArray &d, bool s) { return genericFrameParse(ProtocolID::TiltMeasure, d, s); },
            [](const ParsedFrame &f) -> QString {
                const auto &d = f.tilt;
                return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23,%24,%25,%26,%27,%28,%29,%30,%31,%32\n")
                    .arg(d.lat, 0, 'f', 7).arg(d.lon, 0, 'f', 7).arg(d.elev, 0, 'f', 3)
                    .arg(d.laserBias[0]).arg(d.laserBias[1])
                    .arg(d.laserDist).arg(d.sampleCnt).arg(d.coordFlag)
                    .arg(d.temp)
                    .arg(d.rollStd).arg(d.pitchStd)
                    .arg(d.tiltRoll, 0, 'f', 3).arg(d.tiltPitch, 0, 'f', 3)
                    .arg(d.gyroRaw[0], 0, 'f', 4).arg(d.gyroRaw[1], 0, 'f', 4).arg(d.gyroRaw[2], 0, 'f', 4)
                    .arg(d.accelRaw[0], 0, 'f', 4).arg(d.accelRaw[1], 0, 'f', 4).arg(d.accelRaw[2], 0, 'f', 4)
                    .arg(d.timestamp).arg(d.sysStatus).arg(d.poleHeight)
                    .arg(d.solutionRate, 0, 'f', 3)
                    .arg(ParserBase::rtkStatusName(d.gnssFix)).arg(d.gnssSatCnt).arg(d.rtkAge)
                    .arg(d.poleBias[0]).arg(d.poleBias[1])
                    .arg(d.azimuth, 0, 'f', 3)
                    .arg(d.rollDeg, 0, 'f', 3).arg(d.pitchDeg, 0, 'f', 3).arg(d.yawDeg, 0, 'f', 3);
            },
            tiltMeasureFields(),
        },
        {
            QStringLiteral("激光"),
            ProtocolID::Laser,
            false,
            {}, // 非 CSV
            [](const QByteArray &d, bool s) { return genericFrameParse(ProtocolID::Laser, d, s); },
            [](const ParsedFrame &f) -> QString {
                const auto &d = f.laser;
                return QStringLiteral("LaserDist:%1m Bias(%2,%3)deg Target(%4,%5,%6) Pole(%7,%8,%9) Sol:%10 Tilt:%11deg Std(%12,%13,%14)mm")
                    .arg(d.laserDist, 0, 'f', 3)
                    .arg(d.laserBias[0], 0, 'f', 3).arg(d.laserBias[1], 0, 'f', 3)
                    .arg(d.targetLat, 0, 'f', 7).arg(d.targetLon, 0, 'f', 7).arg(d.targetElev, 0, 'f', 3)
                    .arg(d.poleLat, 0, 'f', 7).arg(d.poleLon, 0, 'f', 7).arg(d.poleElev, 0, 'f', 3)
                    .arg(d.solutionType).arg(d.tiltAngle, 0, 'f', 3)
                    .arg(d.latStd).arg(d.lonStd).arg(d.elevStd);
            },
            laserFields(),
        },
        {
            QStringLiteral("南方激光"),
            ProtocolID::NanfangLaser,
            true,  // 累加和校验
            QStringLiteral("时间戳(ms),激光距离(mm),横滚(deg),俯仰(deg),航向(deg),发射点纬度(deg),发射点经度(deg),发射点高程(m),光心纬度(deg),光心经度(deg),光心高程(m),激光故障码,精度因子(deg)\n"),
            [](const QByteArray &d, bool s) { return genericFrameParse(ProtocolID::NanfangLaser, d, s); },
            [](const ParsedFrame &f) -> QString {
                const auto &d = f.nanfangLaser;
                return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13\n")
                    .arg(d.timestamp)
                    .arg(d.laserDist, 0, 'f', 3)
                    .arg(d.roll, 0, 'f', 6).arg(d.pitch, 0, 'f', 6).arg(d.yaw, 0, 'f', 6)
                    .arg(d.emitLat, 0, 'f', 8).arg(d.emitLon, 0, 'f', 8).arg(d.emitAlt, 0, 'f', 3)
                    .arg(d.centerLat, 0, 'f', 8).arg(d.centerLon, 0, 'f', 8).arg(d.centerAlt, 0, 'f', 3)
                    .arg(d.faultCode)
                    .arg(d.precisionFactor, 0, 'f', 6);
            },
            nanfangLaserFields(),
        },
        {
            QStringLiteral("中海达激光"),
            ProtocolID::ZhdLaser,
            true,  // 累加和校验
            QStringLiteral("时间戳(ms),激光距离(m),横滚(deg),俯仰(deg),航向(deg),激光点纬度(deg),激光点经度(deg),激光点高程(m),IMU纬度(deg),IMU经度(deg),IMU高程(m),激光点纬度中误差(mm),激光点经度中误差(mm),激光点高程中误差(mm),横滚中误差(0.01deg),俯仰中误差(0.01deg),航向中误差(0.01deg),IMU纬度中误差(mm),IMU经度中误差(mm),IMU高程中误差(mm),激光错误类型,精度因子(deg)\n"),
            [](const QByteArray &d, bool s) { return genericFrameParse(ProtocolID::ZhdLaser, d, s); },
            [](const ParsedFrame &f) -> QString {
                const auto &d = f.zhdLaser;
                return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22\n")
                    .arg(d.timestamp)
                    .arg(d.laserDist, 0, 'f', 3)
                    .arg(d.roll, 0, 'f', 6).arg(d.pitch, 0, 'f', 6).arg(d.yaw, 0, 'f', 6)
                    .arg(d.laserLat, 0, 'f', 8).arg(d.laserLon, 0, 'f', 8).arg(d.laserAlt, 0, 'f', 3)
                    .arg(d.imuLat, 0, 'f', 8).arg(d.imuLon, 0, 'f', 8).arg(d.imuAlt, 0, 'f', 3)
                    .arg(d.laserLatStd).arg(d.laserLonStd).arg(d.laserAltStd)
                    .arg(d.rollStd).arg(d.pitchStd).arg(d.yawStd)
                    .arg(d.imuLatStd).arg(d.imuLonStd).arg(d.imuAltStd)
                    .arg(d.laserError)
                    .arg(d.precisionFactor, 0, 'f', 6);
            },
            zhdLaserFields(),
        },
        {
            QStringLiteral("姿态"),
            ProtocolID::Attitude,
            false,
            QStringLiteral("Timestamp(us),Roll,Pitch,Yaw,RollStd,PitchStd,YawStd\n"),
            [](const QByteArray &d, bool s) { return genericFrameParse(ProtocolID::Attitude, d, s); },
            [](const ParsedFrame &f) -> QString {
                const auto &d = f.attitude;
                return QStringLiteral("%1,%2,%3,%4,%5,%6,%7\n")
                    .arg(d.timestamp)
                    .arg(d.roll, 0, 'f', 6).arg(d.pitch, 0, 'f', 6).arg(d.yaw, 0, 'f', 6)
                    .arg(d.rollStd, 0, 'f', 3).arg(d.pitchStd, 0, 'f', 3).arg(d.yawStd, 0, 'f', 3);
            },
            attitudeFields(),
        },
        {
            QStringLiteral("614P-pro_四元数"),
            ProtocolID::Quat614P,
            true,  // 使用累加和校验而非 CRC32，默认跳过
            QStringLiteral("GPS周内毫秒(ms),INS纬度,INS经度,INS高程(m),QuatW,QuatX,QuatY,QuatZ,IMU精度因子,系统状态\n"),
            [](const QByteArray &d, bool s) { return genericFrameParse(ProtocolID::Quat614P, d, s); },
            [](const ParsedFrame &f) -> QString {
                const auto &q = f.quat614p;
                return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,0x%10\n")
                    .arg(q.gpsWeekMs)
                    .arg(q.insLat, 0, 'f', 8).arg(q.insLon, 0, 'f', 8).arg(q.insAlt, 0, 'f', 3)
                    .arg(q.quatW, 0, 'f', 8).arg(q.quatX, 0, 'f', 8)
                    .arg(q.quatY, 0, 'f', 8).arg(q.quatZ, 0, 'f', 8)
                    .arg(q.imuPrecision, 0, 'f', 4)
                    .arg(q.systemStatus, 0, 16);
            },
            quat614PFields(),
        },
    };
    return reg;
}

// CAN-RTK 也注册到表中（仅用于字段查询，解析走 CanRtkParser 专有逻辑）
static const ProtocolDescriptor kCanRtkDescriptor = {
    QStringLiteral("联试_CAN981"),
    ProtocolID::Can_RTK,
    false,
    QStringLiteral("UTC天数,UTC天内秒,定位类型,定位类型名,差分龄期,卫星数,纬度,经度,海拔(m),前向速度(m/s),速度方向(deg),横滚角(deg),俯仰角(deg),航向角(deg),横滚有效,俯仰有效,航向有效\n"),
    {},  // parseFn 不在此处使用
    {},  // formatFn 不在此处使用
    canRtkFields(),
};

const ProtocolDescriptor *findProtocol(ProtocolID pid)
{
    for (const auto &d : protocolRegistry()) {
        if (d.pid == pid) return &d;
    }
    if (pid == ProtocolID::Can_RTK) return &kCanRtkDescriptor;
    return nullptr;
}

QList<FieldDescriptor> fieldsForProtocol(ProtocolID pid)
{
    const auto *desc = findProtocol(pid);
    return desc ? desc->fields : QList<FieldDescriptor>{};
}
