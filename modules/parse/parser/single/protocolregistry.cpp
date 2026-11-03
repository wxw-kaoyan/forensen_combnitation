#include "modules/parse/parser/single/protocolregistry.h"
#include "modules/parse/parser/parserbase.h"
#include "modules/parse/parser/core/frameparser.h"
#include <QStringList>

// ════════════════════════════════════════════════════════════════
//  各协议字段列表
// ════════════════════════════════════════════════════════════════

static QList<FieldDescriptor> ahrsBasicFields()
{
    return {
        {"Timer(us)",  [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.timer); }},
        {"Pitch(deg)", [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.pitch,  'f', 6); }},
        {"Roll(deg)",  [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.roll,   'f', 6); }},
        {"Yaw(deg)",   [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.yaw,    'f', 6); }},
        {"Ax(g)",      [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.ax,     'f', 6); }},
        {"Ay(g)",      [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.ay,     'f', 6); }},
        {"Az(g)",      [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.az,     'f', 6); }},
        {"Gx(dps)",    [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.gx,     'f', 6); }},
        {"Gy(dps)",    [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.gy,     'f', 6); }},
        {"Gz(dps)",    [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.gz,     'f', 6); }},
        {"Temp(C)",    [](const ParsedFrame &f) { return QString::number(f.ahrsBasic.temp,   'f', 2); }},
    };
}

static QList<FieldDescriptor> ahrsExtendedFields()
{
    auto base = ahrsBasicFields();
    base.append({
        {"Flag",       [](const ParsedFrame &f) { return QString::number(f.ahrsExtended.flag); }},
        {"Status",     [](const ParsedFrame &f) { return QStringLiteral("0x") + QString::number(f.ahrsExtended.status, 16); }},
    });
    return base;
}

static QList<FieldDescriptor> quaternionFields()
{
    return {
        {"Timer(us)",  [](const ParsedFrame &f) { return QString::number(f.quat.timer); }},
        {"Qw",         [](const ParsedFrame &f) { return QString::number(f.quat.qw, 'f', 8); }},
        {"Qx",         [](const ParsedFrame &f) { return QString::number(f.quat.qx, 'f', 8); }},
        {"Qy",         [](const ParsedFrame &f) { return QString::number(f.quat.qy, 'f', 8); }},
        {"Qz",         [](const ParsedFrame &f) { return QString::number(f.quat.qz, 'f', 8); }},
        {"Ax(g)",      [](const ParsedFrame &f) { return QString::number(f.quat.ax,    'f', 6); }},
        {"Ay(g)",      [](const ParsedFrame &f) { return QString::number(f.quat.ay,    'f', 6); }},
        {"Az(g)",      [](const ParsedFrame &f) { return QString::number(f.quat.az,    'f', 6); }},
        {"Gx(dps)",    [](const ParsedFrame &f) { return QString::number(f.quat.gx,    'f', 4); }},
        {"Gy(dps)",    [](const ParsedFrame &f) { return QString::number(f.quat.gy,    'f', 4); }},
        {"Gz(dps)",    [](const ParsedFrame &f) { return QString::number(f.quat.gz,    'f', 4); }},
        {"Temp(C)",    [](const ParsedFrame &f) { return QString::number(f.quat.temp,  'f', 2); }},
        {"Flag",       [](const ParsedFrame &f) { return QString::number(f.quat.flag); }},
        {"Status",     [](const ParsedFrame &f) { return QStringLiteral("0x") + QString::number(f.quat.status, 16); }},
    };
}

static QList<FieldDescriptor> agDataFields()
{
    return {
        {"GPS(ms)",    [](const ParsedFrame &f) { return QString::number(f.agData.gpsTime); }},
        {"Roll(deg)",  [](const ParsedFrame &f) { return QString::number(f.agData.roll,   'f', 6); }},
        {"Pitch(deg)", [](const ParsedFrame &f) { return QString::number(f.agData.pitch,  'f', 6); }},
        {"Yaw(deg)",   [](const ParsedFrame &f) { return QString::number(f.agData.yaw,    'f', 6); }},
        {"GyroX(dps)", [](const ParsedFrame &f) { return QString::number(f.agData.gyroX,  'f', 4); }},
        {"GyroY(dps)", [](const ParsedFrame &f) { return QString::number(f.agData.gyroY,  'f', 4); }},
        {"GyroZ(dps)", [](const ParsedFrame &f) { return QString::number(f.agData.gyroZ,  'f', 4); }},
        {"AccelX(g)",  [](const ParsedFrame &f) { return QString::number(f.agData.accelX, 'f', 6); }},
        {"AccelY(g)",  [](const ParsedFrame &f) { return QString::number(f.agData.accelY, 'f', 6); }},
        {"AccelZ(g)",  [](const ParsedFrame &f) { return QString::number(f.agData.accelZ, 'f', 6); }},
        {"RTK状态",    [](const ParsedFrame &f) { return ParserBase::rtkStatusName(f.agData.rtkStatus); }},
        {"姿态有效",   [](const ParsedFrame &f) { return ParserBase::formatAttValid(f.agData.attValid); }},
        {"状态标志",   [](const ParsedFrame &f) { return ParserBase::formatStatusFlags(f.agData.statusFlag); }},
    };
}

static QList<FieldDescriptor> fusionResultFields()
{
    return {
        {"GPS周秒(ms)",  [](const ParsedFrame &f) { return QString::number(f.fusion.gpsWeekSecond); }},
        {"GPS周",        [](const ParsedFrame &f) { return QString::number(f.fusion.gpsWeek); }},
        {"纬度(deg)",    [](const ParsedFrame &f) { return QString::number(f.fusion.lat / 1e7, 'f', 7); }},
        {"经度(deg)",    [](const ParsedFrame &f) { return QString::number(f.fusion.lon / 1e7, 'f', 7); }},
        {"高度(m)",      [](const ParsedFrame &f) { return QString::number(f.fusion.alt / 1000.0, 'f', 3); }},
        {"北向速度(m/s)",[](const ParsedFrame &f) { return QString::number(f.fusion.velN, 'f', 3); }},
        {"东向速度(m/s)",[](const ParsedFrame &f) { return QString::number(f.fusion.velE, 'f', 3); }},
        {"地向速度(m/s)",[](const ParsedFrame &f) { return QString::number(f.fusion.velD, 'f', 3); }},
        {"横滚(deg)",    [](const ParsedFrame &f) { return QString::number(f.fusion.roll,   'f', 3); }},
        {"俯仰(deg)",    [](const ParsedFrame &f) { return QString::number(f.fusion.pitch,  'f', 3); }},
        {"航向(deg)",    [](const ParsedFrame &f) { return QString::number(f.fusion.yaw,    'f', 3); }},
        {"航向角(deg)",  [](const ParsedFrame &f) { return QString::number(f.fusion.heading,'f', 3); }},
        {"前轮转角(deg)",[](const ParsedFrame &f) { return QString::number(f.fusion.wheelAngle,'f', 3); }},
        {"Ax(g)",        [](const ParsedFrame &f) { return QString::number(f.fusion.accelX, 'f', 4); }},
        {"Ay(g)",        [](const ParsedFrame &f) { return QString::number(f.fusion.accelY, 'f', 4); }},
        {"Az(g)",        [](const ParsedFrame &f) { return QString::number(f.fusion.accelZ, 'f', 4); }},
        {"Gx(dps)",      [](const ParsedFrame &f) { return QString::number(f.fusion.gyroX,  'f', 3); }},
        {"Gy(dps)",      [](const ParsedFrame &f) { return QString::number(f.fusion.gyroY,  'f', 3); }},
        {"Gz(dps)",      [](const ParsedFrame &f) { return QString::number(f.fusion.gyroZ,  'f', 3); }},
        {"温度(C)",      [](const ParsedFrame &f) { return QString::number(f.fusion.temp,   'f', 1); }},
        {"RTK状态",      [](const ParsedFrame &f) { return ParserBase::rtkStatusName(f.fusion.rtkStatus); }},
        {"卫星数",       [](const ParsedFrame &f) { return QString::number(f.fusion.numSatellites); }},
        {"差分龄期(s)",  [](const ParsedFrame &f) { return QString::number(f.fusion.diffAge); }},
        {"双天线状态",   [](const ParsedFrame &f) { return f.fusion.dualAntStatus == 50 ? QStringLiteral("已定向") : QString::number(f.fusion.dualAntStatus); }},
        {"PosStd(cm)",   [](const ParsedFrame &f) { return QString::number(f.fusion.posStd); }},
        {"状态标志",     [](const ParsedFrame &f) { return ParserBase::formatStatusFlags(f.fusion.statusFlag); }},
    };
}

// ── NAV_APPEND：实时接收区只给一条"结论"，内部含丢数判断 ──
namespace {

// 以 GPS 周计数 + 周内秒为绝对时间基准，自适应估计正常帧间隔
struct NavGapState {
    qint64 lastMs    = -1;   // 上一帧绝对时间 = 周*604800000 + 周内ms
    qint64 interval  = 0;    // 正常帧间隔(ms)
    qint64 totalLost = 0;    // 累计丢帧数
};
NavGapState g_navGap;

const char *naviStatusText(uint8_t s)
{
    switch (s) {
        case 0:  return "未初始化";
        case 1:  return "收敛中";
        case 2:  return "精度优";
        case 3:  return "INS推算";
        case 4:  return "精度差";
        default: return "未知状态";
    }
}

// 返回"帧间隔 + 丢数"描述，并更新累计统计
QString navGapUpdate(const NavAppendData &d)
{
    const qint64 t = qint64(d.gpsWeek) * 604800000LL + qint64(d.gpsWeekMs);
    QString txt = QStringLiteral("首帧");
    if (g_navGap.lastMs >= 0) {
        qint64 dt = t - g_navGap.lastMs;
        if (dt < 0 && -dt > 300000000LL)   // 跨周回绕
            dt += 604800000LL;
        if (g_navGap.interval <= 0)
            g_navGap.interval = dt;
        else if (dt > 0 && dt * 2 <= g_navGap.interval * 3)
            g_navGap.interval = dt;        // 跟随正常间隔
        int lost = 0;
        if (g_navGap.interval > 0 && dt * 2 > g_navGap.interval * 3)
            lost = int((dt + g_navGap.interval / 2) / g_navGap.interval) - 1;
        if (lost < 0) lost = 0;
        g_navGap.totalLost += lost;
        txt = QStringLiteral("间隔%1ms %2｜累计丢%3帧")
                  .arg(dt)
                  .arg(lost > 0 ? QStringLiteral("丢%1帧").arg(lost) : QStringLiteral("无丢数"))
                  .arg(g_navGap.totalLost);
    }
    g_navGap.lastMs = t;
    return txt;
}

} // namespace

void resetNavGapStats()
{
    g_navGap = NavGapState{};
}

static QList<FieldDescriptor> navAppendFields()
{
    return {
        {"GPS周",            [](const ParsedFrame &f) { return QString::number(f.navAppend.gpsWeek); }},
        {"周内秒(ms)",       [](const ParsedFrame &f) { return QString::number(f.navAppend.gpsWeekMs); }},
        {"纬度(deg)",        [](const ParsedFrame &f) { return QString::number(f.navAppend.lat, 'f', 9); }},
        {"经度(deg)",        [](const ParsedFrame &f) { return QString::number(f.navAppend.lon, 'f', 9); }},
        {"椭球高(m)",        [](const ParsedFrame &f) { return QString::number(f.navAppend.height, 'f', 3); }},
        {"Vn(m/s)",          [](const ParsedFrame &f) { return QString::number(f.navAppend.vn, 'f', 3); }},
        {"Ve(m/s)",          [](const ParsedFrame &f) { return QString::number(f.navAppend.ve, 'f', 3); }},
        {"Vd(m/s)",          [](const ParsedFrame &f) { return QString::number(f.navAppend.vd, 'f', 3); }},
        {"Roll(deg)",        [](const ParsedFrame &f) { return QString::number(f.navAppend.roll, 'f', 3); }},
        {"Pitch(deg)",       [](const ParsedFrame &f) { return QString::number(f.navAppend.pitch, 'f', 3); }},
        {"Yaw(deg)",         [](const ParsedFrame &f) { return QString::number(f.navAppend.yaw, 'f', 3); }},
        {"纬度标准差(m)",    [](const ParsedFrame &f) { return QString::number(f.navAppend.latStd, 'f', 4); }},
        {"经度标准差(m)",    [](const ParsedFrame &f) { return QString::number(f.navAppend.lonStd, 'f', 4); }},
        {"高程标准差(m)",    [](const ParsedFrame &f) { return QString::number(f.navAppend.heightStd, 'f', 4); }},
        {"Vn标准差",         [](const ParsedFrame &f) { return QString::number(f.navAppend.vnStd, 'f', 4); }},
        {"Ve标准差",         [](const ParsedFrame &f) { return QString::number(f.navAppend.veStd, 'f', 4); }},
        {"Vd标准差",         [](const ParsedFrame &f) { return QString::number(f.navAppend.vdStd, 'f', 4); }},
        {"Roll标准差",       [](const ParsedFrame &f) { return QString::number(f.navAppend.rollStd, 'f', 4); }},
        {"Pitch标准差",      [](const ParsedFrame &f) { return QString::number(f.navAppend.pitchStd, 'f', 4); }},
        {"Yaw标准差",        [](const ParsedFrame &f) { return QString::number(f.navAppend.yawStd, 'f', 4); }},
        {"加表X(g)",         [](const ParsedFrame &f) { return QString::number(f.navAppend.accel[0], 'f', 4); }},
        {"加表Y(g)",         [](const ParsedFrame &f) { return QString::number(f.navAppend.accel[1], 'f', 4); }},
        {"加表Z(g)",         [](const ParsedFrame &f) { return QString::number(f.navAppend.accel[2], 'f', 4); }},
        {"陀螺X(deg/s)",     [](const ParsedFrame &f) { return QString::number(f.navAppend.gyro[0], 'f', 3); }},
        {"陀螺Y(deg/s)",     [](const ParsedFrame &f) { return QString::number(f.navAppend.gyro[1], 'f', 3); }},
        {"陀螺Z(deg/s)",     [](const ParsedFrame &f) { return QString::number(f.navAppend.gyro[2], 'f', 3); }},
        {"温度(C)",          [](const ParsedFrame &f) { return QString::number(f.navAppend.imuTemp, 'f', 1); }},
        {"定位状态",         [](const ParsedFrame &f) { return ParserBase::rtkStatusName(f.navAppend.gnssPosType); }},
        {"航迹角(deg)",      [](const ParsedFrame &f) { return QString::number(f.navAppend.gnssTrackAngle, 'f', 3); }},
        {"主星数",           [](const ParsedFrame &f) { return QString::number(f.navAppend.numSVsMain); }},
        {"HDOP",             [](const ParsedFrame &f) { return QString::number(f.navAppend.hdop); }},
        {"差分龄期(s)",      [](const ParsedFrame &f) { return QString::number(f.navAppend.diffAge); }},
        {"双天线周内ms",     [](const ParsedFrame &f) { return QString::number(f.navAppend.dualWeekMs); }},
        {"双天线定位",       [](const ParsedFrame &f) { return ParserBase::rtkStatusName(f.navAppend.dualPosType); }},
        {"副星数",           [](const ParsedFrame &f) { return QString::number(f.navAppend.numSVsSlave); }},
        {"双天线航向(deg)",  [](const ParsedFrame &f) { return QString::number(f.navAppend.dualHeading, 'f', 3); }},
        {"状态位",           [](const ParsedFrame &f) { return QStringLiteral("0x%1").arg(f.navAppend.statusBits, 4, 16, QLatin1Char('0')); }},
        {"导航状态",         [](const ParsedFrame &f) { return QString::number(f.navAppend.naviStatus); }},
        {"结论", [](const ParsedFrame &f) {
            const auto &d = f.navAppend;
            QStringList v;
            v << ParserBase::rtkStatusName(d.gnssPosType);
            v << QStringLiteral("星%1/%2").arg(d.numSVsMain).arg(d.numSVsSlave);
            v << QStringLiteral("%1N %2E").arg(d.lat, 0, 'f', 7).arg(d.lon, 0, 'f', 7);
            v << QStringLiteral("高%1m").arg(d.height, 0, 'f', 2);
            v << QStringLiteral("航向%1°").arg(d.yaw, 0, 'f', 2);
            v << QString::fromUtf8(naviStatusText(d.naviStatus));
            v << navGapUpdate(d);
            return v.join(QStringLiteral(" ｜ "));
        }},
    };
}

static QList<FieldDescriptor> tiltMeasureFields()
{
    return {
        {"纬度(deg)",        [](const ParsedFrame &f) { return QString::number(f.tilt.lat, 'f', 7); }},
        {"经度(deg)",        [](const ParsedFrame &f) { return QString::number(f.tilt.lon, 'f', 7); }},
        {"高程(m)",          [](const ParsedFrame &f) { return QString::number(f.tilt.elev, 'f', 3); }},
        {"激光偏角X",        [](const ParsedFrame &f) { return QString::number(f.tilt.laserBias[0]); }},
        {"激光偏角Y",        [](const ParsedFrame &f) { return QString::number(f.tilt.laserBias[1]); }},
        {"激光距离(mm)",     [](const ParsedFrame &f) { return QString::number(f.tilt.laserDist); }},
        {"采点数",           [](const ParsedFrame &f) { return QString::number(f.tilt.sampleCnt); }},
        {"坐标标志",         [](const ParsedFrame &f) { return QString::number(f.tilt.coordFlag); }},
        {"温度(℃*100)",     [](const ParsedFrame &f) { return QString::number(f.tilt.temp); }},
        {"横滚标准差",       [](const ParsedFrame &f) { return QString::number(f.tilt.rollStd); }},
        {"俯仰标准差",       [](const ParsedFrame &f) { return QString::number(f.tilt.pitchStd); }},
        {"对中杆倾角(deg)", [](const ParsedFrame &f) { return QString::number(f.tilt.tiltRoll, 'f', 3); }},
        {"精度因子",         [](const ParsedFrame &f) { return QString::number(f.tilt.tiltPitch, 'f', 3); }},
        {"陀螺X(deg/s)",     [](const ParsedFrame &f) { return QString::number(f.tilt.gyroRaw[0], 'f', 4); }},
        {"陀螺Y(deg/s)",     [](const ParsedFrame &f) { return QString::number(f.tilt.gyroRaw[1], 'f', 4); }},
        {"陀螺Z(deg/s)",     [](const ParsedFrame &f) { return QString::number(f.tilt.gyroRaw[2], 'f', 4); }},
        {"加表X(g)",         [](const ParsedFrame &f) { return QString::number(f.tilt.accelRaw[0], 'f', 4); }},
        {"加表Y(g)",         [](const ParsedFrame &f) { return QString::number(f.tilt.accelRaw[1], 'f', 4); }},
        {"加表Z(g)",         [](const ParsedFrame &f) { return QString::number(f.tilt.accelRaw[2], 'f', 4); }},
        {"时间戳",           [](const ParsedFrame &f) { return QString::number(f.tilt.timestamp); }},
        {"系统状态",         [](const ParsedFrame &f) { return QString::number(f.tilt.sysStatus); }},
        {"当前杆长(mm)",     [](const ParsedFrame &f) { return QString::number(f.tilt.poleHeight); }},
        {"标定进度(%)",      [](const ParsedFrame &f) { return QString::number(f.tilt.solutionRate, 'f', 3); }},
        {"GNSS定位",         [](const ParsedFrame &f) { return ParserBase::rtkStatusName(f.tilt.gnssFix); }},
        {"卫星数",           [](const ParsedFrame &f) { return QString::number(f.tilt.gnssSatCnt); }},
        {"RTK差分延时(s)",   [](const ParsedFrame &f) { return QString::number(f.tilt.rtkAge); }},
        {"杆安装偏差X",      [](const ParsedFrame &f) { return QString::number(f.tilt.poleBias[0]); }},
        {"杆安装偏差Y",      [](const ParsedFrame &f) { return QString::number(f.tilt.poleBias[1]); }},
        {"方位角(deg)",      [](const ParsedFrame &f) { return QString::number(f.tilt.azimuth, 'f', 3); }},
        {"横滚角(deg)",      [](const ParsedFrame &f) { return QString::number(f.tilt.rollDeg, 'f', 3); }},
        {"俯仰角(deg)",      [](const ParsedFrame &f) { return QString::number(f.tilt.pitchDeg, 'f', 3); }},
        {"航向角(deg)",      [](const ParsedFrame &f) { return QString::number(f.tilt.yawDeg, 'f', 3); }},
    };
}

static QList<FieldDescriptor> laserFields()
{
    return {
        {"激光距离(m)",   [](const ParsedFrame &f) { return QString::number(f.laser.laserDist, 'f', 3); }},
        {"激光偏角X(deg)",[](const ParsedFrame &f) { return QString::number(f.laser.laserBias[0], 'f', 3); }},
        {"激光偏角Y(deg)",[](const ParsedFrame &f) { return QString::number(f.laser.laserBias[1], 'f', 3); }},
        {"目标纬度(deg)", [](const ParsedFrame &f) { return QString::number(f.laser.targetLat, 'f', 7); }},
        {"目标经度(deg)", [](const ParsedFrame &f) { return QString::number(f.laser.targetLon, 'f', 7); }},
        {"目标高程(m)",   [](const ParsedFrame &f) { return QString::number(f.laser.targetElev, 'f', 3); }},
        {"杆纬度(deg)",   [](const ParsedFrame &f) { return QString::number(f.laser.poleLat, 'f', 7); }},
        {"杆经度(deg)",   [](const ParsedFrame &f) { return QString::number(f.laser.poleLon, 'f', 7); }},
        {"杆高程(m)",     [](const ParsedFrame &f) { return QString::number(f.laser.poleElev, 'f', 3); }},
        {"解算类型",      [](const ParsedFrame &f) { return QString::number(f.laser.solutionType); }},
        {"倾斜角(deg)",   [](const ParsedFrame &f) { return QString::number(f.laser.tiltAngle, 'f', 3); }},
    };
}

static QList<FieldDescriptor> nanfangLaserFields()
{
    return {
        {"时间戳(ms)",      [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.timestamp); }},
        {"激光距离(mm)",    [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.laserDist, 'f', 3); }},
        {"横滚(deg)",       [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.roll, 'f', 6); }},
        {"俯仰(deg)",       [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.pitch, 'f', 6); }},
        {"航向(deg)",       [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.yaw, 'f', 6); }},
        {"发射点纬度(deg)", [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.emitLat, 'f', 8); }},
        {"发射点经度(deg)", [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.emitLon, 'f', 8); }},
        {"发射点高程(m)",   [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.emitAlt, 'f', 3); }},
        {"光心纬度(deg)",   [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.centerLat, 'f', 8); }},
        {"光心经度(deg)",   [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.centerLon, 'f', 8); }},
        {"光心高程(m)",     [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.centerAlt, 'f', 3); }},
        {"激光故障码",      [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.faultCode); }},
        {"精度因子(deg)",   [](const ParsedFrame &f) { return QString::number(f.nanfangLaser.precisionFactor, 'f', 6); }},
    };
}

static QList<FieldDescriptor> zhdLaserFields()
{
    return {
        {"时间戳(ms)",         [](const ParsedFrame &f) { return QString::number(f.zhdLaser.timestamp); }},
        {"激光距离(m)",        [](const ParsedFrame &f) { return QString::number(f.zhdLaser.laserDist, 'f', 3); }},
        {"横滚(deg)",          [](const ParsedFrame &f) { return QString::number(f.zhdLaser.roll, 'f', 6); }},
        {"俯仰(deg)",          [](const ParsedFrame &f) { return QString::number(f.zhdLaser.pitch, 'f', 6); }},
        {"航向(deg)",          [](const ParsedFrame &f) { return QString::number(f.zhdLaser.yaw, 'f', 6); }},
        {"激光点纬度(deg)",    [](const ParsedFrame &f) { return QString::number(f.zhdLaser.laserLat, 'f', 8); }},
        {"激光点经度(deg)",    [](const ParsedFrame &f) { return QString::number(f.zhdLaser.laserLon, 'f', 8); }},
        {"激光点高程(m)",      [](const ParsedFrame &f) { return QString::number(f.zhdLaser.laserAlt, 'f', 3); }},
        {"IMU纬度(deg)",       [](const ParsedFrame &f) { return QString::number(f.zhdLaser.imuLat, 'f', 8); }},
        {"IMU经度(deg)",       [](const ParsedFrame &f) { return QString::number(f.zhdLaser.imuLon, 'f', 8); }},
        {"IMU高程(m)",         [](const ParsedFrame &f) { return QString::number(f.zhdLaser.imuAlt, 'f', 3); }},
        {"激光点纬度中误差(mm)",[](const ParsedFrame &f) { return QString::number(f.zhdLaser.laserLatStd); }},
        {"激光点经度中误差(mm)",[](const ParsedFrame &f) { return QString::number(f.zhdLaser.laserLonStd); }},
        {"激光点高程中误差(mm)",[](const ParsedFrame &f) { return QString::number(f.zhdLaser.laserAltStd); }},
        {"横滚中误差(0.01deg)", [](const ParsedFrame &f) { return QString::number(f.zhdLaser.rollStd); }},
        {"俯仰中误差(0.01deg)", [](const ParsedFrame &f) { return QString::number(f.zhdLaser.pitchStd); }},
        {"航向中误差(0.01deg)", [](const ParsedFrame &f) { return QString::number(f.zhdLaser.yawStd); }},
        {"IMU纬度中误差(mm)",  [](const ParsedFrame &f) { return QString::number(f.zhdLaser.imuLatStd); }},
        {"IMU经度中误差(mm)",  [](const ParsedFrame &f) { return QString::number(f.zhdLaser.imuLonStd); }},
        {"IMU高程中误差(mm)",  [](const ParsedFrame &f) { return QString::number(f.zhdLaser.imuAltStd); }},
        {"激光错误类型",       [](const ParsedFrame &f) { return QString::number(f.zhdLaser.laserError); }},
        {"精度因子(deg)",      [](const ParsedFrame &f) { return QString::number(f.zhdLaser.precisionFactor, 'f', 6); }},
    };
}

static QList<FieldDescriptor> attitudeFields()
{
    return {
        {"Timestamp(us)",[](const ParsedFrame &f) { return QString::number(f.attitude.timestamp); }},
        {"Roll(deg)",    [](const ParsedFrame &f) { return QString::number(f.attitude.roll,  'f', 6); }},
        {"Pitch(deg)",   [](const ParsedFrame &f) { return QString::number(f.attitude.pitch, 'f', 6); }},
        {"Yaw(deg)",     [](const ParsedFrame &f) { return QString::number(f.attitude.yaw,   'f', 6); }},
        {"RollStd(deg)", [](const ParsedFrame &f) { return QString::number(f.attitude.rollStd,  'f', 3); }},
        {"PitchStd(deg)",[](const ParsedFrame &f) { return QString::number(f.attitude.pitchStd, 'f', 3); }},
        {"YawStd(deg)",  [](const ParsedFrame &f) { return QString::number(f.attitude.yawStd,   'f', 3); }},
    };
}

static QList<FieldDescriptor> quat614PFields()
{
    return {
        {"GPS周内毫秒(ms)",[](const ParsedFrame &f) { return QString::number(f.quat614p.gpsWeekMs); }},
        {"INS纬度(deg)",   [](const ParsedFrame &f) { return QString::number(f.quat614p.insLat, 'f', 8); }},
        {"INS经度(deg)",   [](const ParsedFrame &f) { return QString::number(f.quat614p.insLon, 'f', 8); }},
        {"INS高程(m)",     [](const ParsedFrame &f) { return QString::number(f.quat614p.insAlt, 'f', 3); }},
        {"四元数W",        [](const ParsedFrame &f) { return QString::number(f.quat614p.quatW, 'f', 8); }},
        {"四元数X",        [](const ParsedFrame &f) { return QString::number(f.quat614p.quatX, 'f', 8); }},
        {"四元数Y",        [](const ParsedFrame &f) { return QString::number(f.quat614p.quatY, 'f', 8); }},
        {"四元数Z",        [](const ParsedFrame &f) { return QString::number(f.quat614p.quatZ, 'f', 8); }},
        {"IMU精度因子",    [](const ParsedFrame &f) { return QString::number(f.quat614p.imuPrecision, 'f', 4); }},
        {"系统状态",       [](const ParsedFrame &f) { return QStringLiteral("0x") + QString::number(f.quat614p.systemStatus, 16); }},
    };
}

static QList<FieldDescriptor> canRtkFields()
{
    return {
        {"UTC天数",     [](const ParsedFrame &f) { return QString::number(f.canRtk.utcDays); }},
        {"UTC天内秒",   [](const ParsedFrame &f) { return QString::number(f.canRtk.secondsOfDay, 'f', 3); }},
        {"定位类型",    [](const ParsedFrame &f) { return ParserBase::rtkStatusName(f.canRtk.fixType); }},
        {"差分龄期",    [](const ParsedFrame &f) { return QString::number(f.canRtk.diffAge); }},
        {"卫星数",      [](const ParsedFrame &f) { return QString::number(f.canRtk.numSatellites); }},
        {"纬度(deg)",   [](const ParsedFrame &f) { return QString::number(f.canRtk.latitude, 'f', 8); }},
        {"经度(deg)",   [](const ParsedFrame &f) { return QString::number(f.canRtk.longitude, 'f', 8); }},
        {"海拔(m)",     [](const ParsedFrame &f) { return QString::number(f.canRtk.altitude, 'f', 3); }},
        {"前向速度(m/s)",[](const ParsedFrame &f) { return QString::number(f.canRtk.forwardSpeed, 'f', 3); }},
        {"速度方向(deg)",[](const ParsedFrame &f){ return (f.canRtk.sdRaw != 0xFFFF) ? QString::number(f.canRtk.speedDirection, 'f', 2) : QStringLiteral("N/A"); }},
        {"横滚角(deg)", [](const ParsedFrame &f) { return f.canRtk.rollValid ? QString::number(f.canRtk.roll, 'f', 3) : QStringLiteral("N/A"); }},
        {"俯仰角(deg)", [](const ParsedFrame &f) { return f.canRtk.pitchValid ? QString::number(f.canRtk.pitch, 'f', 3) : QStringLiteral("N/A"); }},
        {"航向角(deg)", [](const ParsedFrame &f) { return f.canRtk.yawValid ? QString::number(f.canRtk.yaw, 'f', 3) : QStringLiteral("N/A"); }},
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
//  联适 NAVDATA / IMULOG 字段列表
// ════════════════════════════════════════════════════════════════
static QList<FieldDescriptor> lsNavImuFields()
{
    return {
        {"GPS周内毫秒(ms)", [](const ParsedFrame &f) { return QString::number(f.lsNavImu.timeGpsMs); }},
        {"IMU周",           [](const ParsedFrame &f) { return QString::number(f.lsNavImu.weekNumImu); }},
        {"加表X(g)",        [](const ParsedFrame &f) { return QString::number(f.lsNavImu.accel[0], 'f', 5); }},
        {"加表Y(g)",        [](const ParsedFrame &f) { return QString::number(f.lsNavImu.accel[1], 'f', 5); }},
        {"加表Z(g)",        [](const ParsedFrame &f) { return QString::number(f.lsNavImu.accel[2], 'f', 5); }},
        {"陀螺X(deg/s)",    [](const ParsedFrame &f) { return QString::number(f.lsNavImu.gyro[0], 'f', 5); }},
        {"陀螺Y(deg/s)",    [](const ParsedFrame &f) { return QString::number(f.lsNavImu.gyro[1], 'f', 5); }},
        {"陀螺Z(deg/s)",    [](const ParsedFrame &f) { return QString::number(f.lsNavImu.gyro[2], 'f', 5); }},
        {"温度(C)",         [](const ParsedFrame &f) { return QString::number(f.lsNavImu.temperature, 'f', 1); }},
        {"KF初始化",        [](const ParsedFrame &f) { return QString::number(f.lsNavImu.kfInitDone); }},
    };
}

static QList<FieldDescriptor> lsNavGpsFields()
{
    return {
        {"GPS周内毫秒(ms)", [](const ParsedFrame &f) { return QString::number(f.lsNavGps.itow); }},
        {"GPS周",           [](const ParsedFrame &f) { return QString::number(f.lsNavGps.weekNumGps); }},
        {"定位类型",        [](const ParsedFrame &f) { return QString::number(f.lsNavGps.fixType); }},
        {"定位类型名",      [](const ParsedFrame &f) { return novatelPosTypeName(f.lsNavGps.fixType); }},
        {"接收时刻(ms)",    [](const ParsedFrame &f) { return QString::number(f.lsNavGps.revcTimeMs); }},
        {"卫星数",          [](const ParsedFrame &f) { return QString::number(f.lsNavGps.numSV); }},
        {"双频星数",        [](const ParsedFrame &f) { return QString::number(f.lsNavGps.multiSVs); }},
        {"纬度(deg)",       [](const ParsedFrame &f) { return QString::number(f.lsNavGps.lat, 'f', 7); }},
        {"经度(deg)",       [](const ParsedFrame &f) { return QString::number(f.lsNavGps.lon, 'f', 7); }},
        {"海拔(m)",         [](const ParsedFrame &f) { return QString::number(f.lsNavGps.alt, 'f', 3); }},
        {"北向速度(m/s)",   [](const ParsedFrame &f) { return QString::number(f.lsNavGps.vn, 'f', 3); }},
        {"东向速度(m/s)",   [](const ParsedFrame &f) { return QString::number(f.lsNavGps.ve, 'f', 3); }},
        {"地向速度(m/s)",   [](const ParsedFrame &f) { return QString::number(f.lsNavGps.vd, 'f', 3); }},
        {"地速(m/s)",       [](const ParsedFrame &f) { return QString::number(f.lsNavGps.gspeed, 'f', 3); }},
        {"运动航向(deg)",   [](const ParsedFrame &f) { return QString::number(f.lsNavGps.headMot, 'f', 3); }},
        {"水平位置精度",    [](const ParsedFrame &f) { return QString::number(f.lsNavGps.pHacc); }},
        {"垂直位置精度",    [](const ParsedFrame &f) { return QString::number(f.lsNavGps.pVacc); }},
        {"水平速度精度",    [](const ParsedFrame &f) { return QString::number(f.lsNavGps.vHacc); }},
        {"ll_extend",       [](const ParsedFrame &f) { return QString::number(f.lsNavGps.llExtend); }},
        {"差分龄期",        [](const ParsedFrame &f) { return QString::number(f.lsNavGps.diffAge); }},
        {"滤波器索引",      [](const ParsedFrame &f) { return QString::number(f.lsNavGps.kfIndexGet); }},
    };
}

static QList<FieldDescriptor> lsNavParamFields()
{
    return {
        {"主天线杆臂X(m)", [](const ParsedFrame &f) { return QString::number(f.lsNavParam.gpsPosX / 100.0, 'f', 2); }},
        {"主天线杆臂Y(m)", [](const ParsedFrame &f) { return QString::number(f.lsNavParam.gpsPosY / 100.0, 'f', 2); }},
        {"主天线杆臂Z(m)", [](const ParsedFrame &f) { return QString::number(f.lsNavParam.gpsPosZ / 100.0, 'f', 2); }},
        {"RTK标记",        [](const ParsedFrame &f) { return QString::number(f.lsNavParam.rtkFlag); }},
        {"后轮杆臂X(m)",   [](const ParsedFrame &f) { return QString::number(f.lsNavParam.odPosX / 100.0, 'f', 2); }},
        {"后轮杆臂Y(m)",   [](const ParsedFrame &f) { return QString::number(f.lsNavParam.odPosY / 100.0, 'f', 2); }},
        {"后轮杆臂Z(m)",   [](const ParsedFrame &f) { return QString::number(f.lsNavParam.odPosZ / 100.0, 'f', 2); }},
        {"版本",           [](const ParsedFrame &f) { return QString::number(f.lsNavParam.version); }},
        {"重启原因",       [](const ParsedFrame &f) { return QString::number(f.lsNavParam.resetReason); }},
    };
}

static QList<FieldDescriptor> lsImuLogFields()
{
    return {
        {"初始化状态",         [](const ParsedFrame &f) { return QString::number(f.lsImuLog.initStatus); }},
        {"滤波器筛选值",       [](const ParsedFrame &f) { return QString::number(f.lsImuLog.filterSelect); }},
        {"初始化航向角(deg)",  [](const ParsedFrame &f) { return QString::number(f.lsImuLog.initHeading, 'f', 2); }},
        {"设备状态",           [](const ParsedFrame &f) { return QString::number(f.lsImuLog.deviceStatus); }},
        {"betpos超时间隔数",   [](const ParsedFrame &f) { return QString::number(f.lsImuLog.betposGapCnt); }},
        {"psrvel超时间隔数",   [](const ParsedFrame &f) { return QString::number(f.lsImuLog.psrvelGapCnt); }},
        {"ZUPT计数",           [](const ParsedFrame &f) { return QString::number(f.lsImuLog.zuptCnt); }},
        {"动静态标记",         [](const ParsedFrame &f) { return QString::number(f.lsImuLog.staticFlag); }},
        {"直行计数",           [](const ParsedFrame &f) { return QString::number(f.lsImuLog.straightCnt); }},
        {"航向安装偏差(deg)",  [](const ParsedFrame &f) { return QString::number(f.lsImuLog.yawInstallBias, 'f', 2); }},
        {"betpos数量",         [](const ParsedFrame &f) { return QString::number(f.lsImuLog.betposCnt); }},
        {"陀螺Z零偏(deg/h)",   [](const ParsedFrame &f) { return QString::number(f.lsImuLog.gyroZBias, 'f', 0); }},
        {"延迟(ms)",           [](const ParsedFrame &f) { return QString::number(f.lsImuLog.delayMs); }},
        {"psrvel数量",         [](const ParsedFrame &f) { return QString::number(f.lsImuLog.psrvelCnt); }},
        {"IMU静态计数",        [](const ParsedFrame &f) { return QString::number(f.lsImuLog.imuStaticCnt); }},
        {"滤波器状态",         [](const ParsedFrame &f) { return QString::number(f.lsImuLog.filterState); }},
        {"重置标记位",         [](const ParsedFrame &f) { return QString::number(f.lsImuLog.resetFlag); }},
        {"俯仰安装偏差(deg)",  [](const ParsedFrame &f) { return QString::number(f.lsImuLog.pitchInstallBias, 'f', 2); }},
        {"北向残差(m)",        [](const ParsedFrame &f) { return QString::number(f.lsImuLog.resN, 'f', 2); }},
        {"东向残差(m)",        [](const ParsedFrame &f) { return QString::number(f.lsImuLog.resE, 'f', 2); }},
        {"天向残差(m)",        [](const ParsedFrame &f) { return QString::number(f.lsImuLog.resU, 'f', 2); }},
    };
}

// ════════════════════════════════════════════════════════════════
//  诺瓦泰 NovAtel 字段列表
// ════════════════════════════════════════════════════════════════
static QList<FieldDescriptor> novatelBestPosFields()
{
    return {
        {"GPS周",           [](const ParsedFrame &f) { return QString::number(f.novatelBestPos.gpsWeek); }},
        {"GPS周内毫秒(ms)", [](const ParsedFrame &f) { return QString::number(f.novatelBestPos.gpsWeekMs); }},
        {"解状态",          [](const ParsedFrame &f) { return QString::number(f.novatelBestPos.solStatus); }},
        {"解状态名",        [](const ParsedFrame &f) { return novatelSolStatusName(f.novatelBestPos.solStatus); }},
        {"定位类型",        [](const ParsedFrame &f) { return QString::number(f.novatelBestPos.posType); }},
        {"定位类型名",      [](const ParsedFrame &f) { return novatelPosTypeName(f.novatelBestPos.posType); }},
        {"纬度(deg)",       [](const ParsedFrame &f) { return QString::number(f.novatelBestPos.lat, 'f', 9); }},
        {"经度(deg)",       [](const ParsedFrame &f) { return QString::number(f.novatelBestPos.lon, 'f', 9); }},
        {"椭球高(m)",       [](const ParsedFrame &f) { return QString::number(f.novatelBestPos.height, 'f', 4); }},
        {"高程异常(m)",     [](const ParsedFrame &f) { return QString::number(f.novatelBestPos.undulation, 'f', 3); }},
        {"基准",            [](const ParsedFrame &f) { return QString::number(f.novatelBestPos.datumId); }},
        {"纬度标准差(m)",   [](const ParsedFrame &f) { return QString::number(f.novatelBestPos.latStd, 'f', 4); }},
        {"经度标准差(m)",   [](const ParsedFrame &f) { return QString::number(f.novatelBestPos.lonStd, 'f', 4); }},
        {"高程标准差(m)",   [](const ParsedFrame &f) { return QString::number(f.novatelBestPos.heightStd, 'f', 4); }},
        {"差分龄期(s)",     [](const ParsedFrame &f) { return QString::number(f.novatelBestPos.diffAge, 'f', 1); }},
        {"解龄期(s)",       [](const ParsedFrame &f) { return QString::number(f.novatelBestPos.solAge, 'f', 1); }},
        {"卫星数",          [](const ParsedFrame &f) { return QString::number(f.novatelBestPos.numSVs); }},
        {"参与解算星数",    [](const ParsedFrame &f) { return QString::number(f.novatelBestPos.numSolnSVs); }},
        {"L1星数",          [](const ParsedFrame &f) { return QString::number(f.novatelBestPos.numSolnL1SVs); }},
        {"多频星数",        [](const ParsedFrame &f) { return QString::number(f.novatelBestPos.numSolnMultiSVs); }},
    };
}

static QList<FieldDescriptor> novatelPsrVelFields()
{
    return {
        {"GPS周",           [](const ParsedFrame &f) { return QString::number(f.novatelPsrVel.gpsWeek); }},
        {"GPS周内毫秒(ms)", [](const ParsedFrame &f) { return QString::number(f.novatelPsrVel.gpsWeekMs); }},
        {"解状态",          [](const ParsedFrame &f) { return QString::number(f.novatelPsrVel.solStatus); }},
        {"解状态名",        [](const ParsedFrame &f) { return novatelSolStatusName(f.novatelPsrVel.solStatus); }},
        {"速度类型",        [](const ParsedFrame &f) { return QString::number(f.novatelPsrVel.velType); }},
        {"速度类型名",      [](const ParsedFrame &f) { return novatelVelTypeName(f.novatelPsrVel.velType); }},
        {"延迟(s)",         [](const ParsedFrame &f) { return QString::number(f.novatelPsrVel.latency, 'f', 3); }},
        {"差分龄期(s)",     [](const ParsedFrame &f) { return QString::number(f.novatelPsrVel.diffAge, 'f', 1); }},
        {"水平速度(m/s)",   [](const ParsedFrame &f) { return QString::number(f.novatelPsrVel.horSpeed, 'f', 5); }},
        {"地面航向(deg)",   [](const ParsedFrame &f) { return QString::number(f.novatelPsrVel.trackGround, 'f', 5); }},
        {"垂直速度(m/s)",   [](const ParsedFrame &f) { return QString::number(f.novatelPsrVel.verSpeed, 'f', 5); }},
    };
}

// ════════════════════════════════════════════════════════════════
//  A3 IMU 异常测试协议 (CANFD ID=0x272, 64B, 小端) 字段列表
// ════════════════════════════════════════════════════════════════
static QList<FieldDescriptor> a3ImuFields()
{
    return {
        {"Cycle",                   [](const ParsedFrame &f) { return QString::number(f.a3Imu.cycle); }},
        {"GPS周",                    [](const ParsedFrame &f) { return QString::number(f.a3Imu.gpsWeek); }},
        {"GPS周内毫秒(ms)",         [](const ParsedFrame &f) { return QString::number(f.a3Imu.gpsWeekMs); }},
        {"陀螺X(deg/s)",            [](const ParsedFrame &f) { return QString::number(f.a3Imu.gyro[0], 'f', 5); }},
        {"陀螺Y(deg/s)",            [](const ParsedFrame &f) { return QString::number(f.a3Imu.gyro[1], 'f', 5); }},
        {"陀螺Z(deg/s)",            [](const ParsedFrame &f) { return QString::number(f.a3Imu.gyro[2], 'f', 5); }},
        {"加表X(m/s²)",             [](const ParsedFrame &f) { return QString::number(f.a3Imu.accel[0], 'f', 5); }},
        {"加表Y(m/s²)",             [](const ParsedFrame &f) { return QString::number(f.a3Imu.accel[1], 'f', 5); }},
        {"加表Z(m/s²)",             [](const ParsedFrame &f) { return QString::number(f.a3Imu.accel[2], 'f', 5); }},
        {"温度(C)",                 [](const ParsedFrame &f) { return QString::number(f.a3Imu.temperature, 'f', 2); }},
        {"Q0",                      [](const ParsedFrame &f) { return QString::number(f.a3Imu.quat[0], 'f', 5); }},
        {"Q1",                      [](const ParsedFrame &f) { return QString::number(f.a3Imu.quat[1], 'f', 5); }},
        {"Q2",                      [](const ParsedFrame &f) { return QString::number(f.a3Imu.quat[2], 'f', 5); }},
        {"Q3",                      [](const ParsedFrame &f) { return QString::number(f.a3Imu.quat[3], 'f', 5); }},
        {"RunStatus",               [](const ParsedFrame &f) { return QStringLiteral("0x") + QString::number(f.a3Imu.runStatus, 16).rightJustified(2, '0'); }},
        {"陀螺硬件失效计数",        [](const ParsedFrame &f) { return QString::number(f.a3Imu.flags[0]); }},
        {"加计硬件失效计数",        [](const ParsedFrame &f) { return QString::number(f.a3Imu.flags[1]); }},
        {"陀螺超量程计数",          [](const ParsedFrame &f) { return QString::number(f.a3Imu.flags[2]); }},
        {"协方差奇异重置计数",      [](const ParsedFrame &f) { return QString::number(f.a3Imu.flags[3]); }},
        {"协方差对角线负数计数",    [](const ParsedFrame &f) { return QString::number(f.a3Imu.flags[4]); }},
        {"航向方差下溢计数",        [](const ParsedFrame &f) { return QString::number(f.a3Imu.flags[5]); }},
        {"EKF健康状态位寄存器",     [](const ParsedFrame &f) { return QString::number(f.a3Imu.flags[6]); }},
        {"姿态步突跳超限计数",      [](const ParsedFrame &f) { return QString::number(f.a3Imu.flags[7]); }},
        {"CAN报文+秒脉冲同步",      [](const ParsedFrame &f) { return QString::number(f.a3Imu.flags[8]); }},
        {"系统异常状态寄存器",      [](const ParsedFrame &f) { return QString::number(f.a3Imu.flags[9]); }},
        {"CRC",                     [](const ParsedFrame &f) { return QStringLiteral("0x") + QString::number(f.a3Imu.crc, 16).rightJustified(8, '0'); }},
    };
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
                return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13\n")
                    .arg(d.gpsTime)
                    .arg(d.roll, 0, 'f', 6).arg(d.pitch, 0, 'f', 6).arg(d.yaw, 0, 'f', 6)
                    .arg(d.gyroX, 0, 'f', 4).arg(d.gyroY, 0, 'f', 4).arg(d.gyroZ, 0, 'f', 4)
                    .arg(d.accelX, 0, 'f', 6).arg(d.accelY, 0, 'f', 6).arg(d.accelZ, 0, 'f', 6)
                    .arg(ParserBase::rtkStatusName(d.rtkStatus))
                    .arg(ParserBase::formatAttValid(d.attValid))
                    .arg(ParserBase::formatStatusFlags(d.statusFlag));
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
            QStringLiteral("导航附加(NAV_APPEND)"),
            ProtocolID::Nav_Append,
            true,   // CRC32 初值1/多项式0xEDB88320/无最终异或，与 computeCrc32 一致
            QStringLiteral("周,周内秒(ms),纬度,经度,椭球高(m),Vn,Ve,Vd,Roll,Pitch,Yaw,"
                           "纬度σ(m),经度σ(m),高程σ(m),Vnσ,Veσ,Vdσ,Rollσ,Pitchσ,Yawσ,"
                           "Ax(g),Ay(g),Az(g),Gx(dps),Gy(dps),Gz(dps),温度(C),"
                           "定位状态,航迹角,主星数,HDOP,差分龄期,双天线周内ms,双天线定位,"
                           "副星数,双天线航向,状态位,导航状态\n"),
            [](const QByteArray &d, bool s) { return genericFrameParse(ProtocolID::Nav_Append, d, s); },
            [](const ParsedFrame &f) -> QString {
                const auto &d = f.navAppend;
                QStringList v;
                v << QString::number(d.gpsWeek)
                  << QString::number(d.gpsWeekMs)
                  << QString::number(d.lat, 'f', 9)
                  << QString::number(d.lon, 'f', 9)
                  << QString::number(d.height, 'f', 3)
                  << QString::number(d.vn, 'f', 3)
                  << QString::number(d.ve, 'f', 3)
                  << QString::number(d.vd, 'f', 3)
                  << QString::number(d.roll, 'f', 3)
                  << QString::number(d.pitch, 'f', 3)
                  << QString::number(d.yaw, 'f', 3)
                  << QString::number(d.latStd, 'f', 4)
                  << QString::number(d.lonStd, 'f', 4)
                  << QString::number(d.heightStd, 'f', 4)
                  << QString::number(d.vnStd, 'f', 4)
                  << QString::number(d.veStd, 'f', 4)
                  << QString::number(d.vdStd, 'f', 4)
                  << QString::number(d.rollStd, 'f', 4)
                  << QString::number(d.pitchStd, 'f', 4)
                  << QString::number(d.yawStd, 'f', 4)
                  << QString::number(d.accel[0], 'f', 4)
                  << QString::number(d.accel[1], 'f', 4)
                  << QString::number(d.accel[2], 'f', 4)
                  << QString::number(d.gyro[0], 'f', 3)
                  << QString::number(d.gyro[1], 'f', 3)
                  << QString::number(d.gyro[2], 'f', 3)
                  << QString::number(d.imuTemp, 'f', 1)
                  << ParserBase::rtkStatusName(d.gnssPosType)
                  << QString::number(d.gnssTrackAngle, 'f', 3)
                  << QString::number(d.numSVsMain)
                  << QString::number(d.hdop)
                  << QString::number(d.diffAge)
                  << QString::number(d.dualWeekMs)
                  << ParserBase::rtkStatusName(d.dualPosType)
                  << QString::number(d.numSVsSlave)
                  << QString::number(d.dualHeading, 'f', 3)
                  << QStringLiteral("0x%1").arg(d.statusBits, 4, 16, QLatin1Char('0'))
                  << QString::number(d.naviStatus);
                return v.join(QLatin1Char(',')) + QLatin1Char('\n');
            },
            navAppendFields(),
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

// ── 联适 NAVDATA / IMULOG 也注册描述符（仅用于字段/CSV 查询，解析走专用解析器） ──
static const ProtocolDescriptor kLsNavImuDescriptor = {
    QStringLiteral("NAVDATA_联适-IMU"),
    ProtocolID::LsNav_Imu,
    false,
    QStringLiteral("GPS周内毫秒(ms),IMU周,加表X(g),加表Y(g),加表Z(g),陀螺X(deg/s),陀螺Y(deg/s),陀螺Z(deg/s),温度(C),KF初始化\n"),
    {}, {}, lsNavImuFields(),
};

static const ProtocolDescriptor kLsNavGpsDescriptor = {
    QStringLiteral("NAVDATA_联适-GNSS"),
    ProtocolID::LsNav_Gps,
    false,
    QStringLiteral("GPS周内毫秒(ms),GPS周,定位类型,定位类型名,接收时刻(ms),卫星数,双频星数,纬度(deg),经度(deg),海拔(m),北向速度(m/s),东向速度(m/s),地向速度(m/s),地速(m/s),运动航向(deg),水平位置精度,垂直位置精度,水平速度精度,ll_extend,差分龄期,滤波器索引\n"),
    {}, {}, lsNavGpsFields(),
};

static const ProtocolDescriptor kLsNavParamDescriptor = {
    QStringLiteral("NAVDATA_联适-参数"),
    ProtocolID::LsNav_Param,
    false,
    QStringLiteral("主天线杆臂X(m),主天线杆臂Y(m),主天线杆臂Z(m),RTK标记,后轮杆臂X(m),后轮杆臂Y(m),后轮杆臂Z(m),版本,重启原因\n"),
    {}, {}, lsNavParamFields(),
};

static const ProtocolDescriptor kLsImuLogDescriptor = {
    QStringLiteral("IMULOG_联适"),
    ProtocolID::LsImuLog,
    false,
    QStringLiteral("初始化状态,滤波器筛选值,初始化航向角(deg),设备状态,betpos超时间隔数,psrvel超时间隔数,ZUPT计数,动静态标记,直行计数,航向安装偏差(deg),betpos数量,陀螺Z零偏(deg/h),延迟(ms),psrvel数量,IMU静态计数,滤波器状态,重置标记位,俯仰安装偏差(deg),北向残差(m),东向残差(m),天向残差(m)\n"),
    {}, {}, lsImuLogFields(),
};

// ── 诺瓦泰 NovAtel 也注册描述符（仅用于字段/CSV 查询，解析走专用解析器） ──
static const ProtocolDescriptor kNovatelBestPosDescriptor = {
    QStringLiteral("BESTPOS_诺瓦泰"),
    ProtocolID::Novatel_BestPos,
    false,
    QStringLiteral("GPS周,GPS周内毫秒(ms),解状态,解状态名,定位类型,定位类型名,纬度(deg),经度(deg),椭球高(m),高程异常(m),基准,纬度标准差(m),经度标准差(m),高程标准差(m),差分龄期(s),解龄期(s),卫星数,参与解算星数,L1星数,多频星数\n"),
    {}, {}, novatelBestPosFields(),
};

static const ProtocolDescriptor kNovatelPsrVelDescriptor = {
    QStringLiteral("PSRVEL_诺瓦泰"),
    ProtocolID::Novatel_PsrVel,
    false,
    QStringLiteral("GPS周,GPS周内毫秒(ms),解状态,解状态名,速度类型,速度类型名,延迟(s),差分龄期(s),水平速度(m/s),地面航向(deg),垂直速度(m/s)\n"),
    {}, {}, novatelPsrVelFields(),
};

// ── A3 IMU 异常测试协议（仅用于字段/CSV 查询，解析走专用解析器） ──
static const ProtocolDescriptor kA3ImuDescriptor = {
    QStringLiteral("A3协议解析"),
    ProtocolID::A3_Imu,
    false,
    QStringLiteral("Cycle,GPS周,GPS周内毫秒(ms),陀螺X(deg/s),陀螺Y(deg/s),陀螺Z(deg/s),加表X(m/s²),加表Y(m/s²),加表Z(m/s²),温度(C),Q0,Q1,Q2,Q3,RunStatus,陀螺硬件失效计数,加计硬件失效计数,陀螺超量程计数,协方差奇异重置计数,协方差对角线负数计数,航向方差下溢计数,EKF健康状态位寄存器,姿态步突跳超限计数,CAN报文+秒脉冲同步,系统异常状态寄存器,CRC\n"),
    {}, {}, a3ImuFields(),
};

// ── 同星 CAN 扩展帧 三组（仅用于字段/CSV 查询，解析走 TxCanParser） ──
static QList<FieldDescriptor> txAttFields()
{
    return {
        {"时间(s)",    [](const ParsedFrame &f) { return QString::number(f.txAtt.ascTime, 'f', 6); }},
        {"横滚角(deg)",[](const ParsedFrame &f) { return QString::number(f.txAtt.roll, 'f', 3); }},
        {"俯仰角(deg)",[](const ParsedFrame &f) { return QString::number(f.txAtt.pitch, 'f', 3); }},
        {"航向角(deg)",[](const ParsedFrame &f) { return QString::number(f.txAtt.yaw, 'f', 3); }},
        {"航迹角(deg)",[](const ParsedFrame &f) { return QString::number(f.txAtt.track, 'f', 3); }},
    };
}

static QList<FieldDescriptor> txGyroFields()
{
    return {
        {"时间(s)",       [](const ParsedFrame &f) { return QString::number(f.txGyro.ascTime, 'f', 6); }},
        {"gyro_x(deg/s)", [](const ParsedFrame &f) { return QString::number(f.txGyro.gyro[0], 'f', 4); }},
        {"gyro_y(deg/s)", [](const ParsedFrame &f) { return QString::number(f.txGyro.gyro[1], 'f', 4); }},
        {"gyro_z(deg/s)", [](const ParsedFrame &f) { return QString::number(f.txGyro.gyro[2], 'f', 4); }},
        {"预留",          [](const ParsedFrame &f) { return QString::number(f.txGyro.reserved); }},
    };
}

static QList<FieldDescriptor> txAccelFields()
{
    return {
        {"时间(s)",   [](const ParsedFrame &f) { return QString::number(f.txAccel.ascTime, 'f', 6); }},
        {"acc_x(g)",  [](const ParsedFrame &f) { return QString::number(f.txAccel.accel[0], 'f', 4); }},
        {"acc_y(g)",  [](const ParsedFrame &f) { return QString::number(f.txAccel.accel[1], 'f', 4); }},
        {"acc_z(g)",  [](const ParsedFrame &f) { return QString::number(f.txAccel.accel[2], 'f', 4); }},
        {"状态位",    [](const ParsedFrame &f) { return QStringLiteral("0x") + QString::number(f.txAccel.status, 16).rightJustified(4, '0'); }},
    };
}

static QList<FieldDescriptor> txSecFields()
{
    return {
        {"时间(s)",           [](const ParsedFrame &f) { return QString::number(f.txSec.ascTime, 'f', 6); }},
        {"周内秒(ms)",        [](const ParsedFrame &f) { return QString::number(f.txSec.weekMs); }},
        {"估计天线安装偏差角", [](const ParsedFrame &f) { return QString::number(f.txSec.antBiasRaw); }},
        {"速度方向",          [](const ParsedFrame &f) { return QString::number(f.txSec.velDirection); }},
        {"预留",              [](const ParsedFrame &f) { return QString::number(f.txSec.reserved); }},
    };
}

static QList<FieldDescriptor> txPosFields()
{
    return {
        {"时间(s)",    [](const ParsedFrame &f) { return QString::number(f.txPos.ascTime, 'f', 6); }},
        {"经度(deg)",  [](const ParsedFrame &f) { return QString::number(f.txPos.lon, 'f', 8); }},
        {"纬度(deg)",  [](const ParsedFrame &f) { return QString::number(f.txPos.lat, 'f', 8); }},
        {"lon_raw",    [](const ParsedFrame &f) { return QString::number(f.txPos.lonRaw); }},
        {"lat_raw",    [](const ParsedFrame &f) { return QString::number(f.txPos.latRaw); }},
    };
}

static QList<FieldDescriptor> txHeightFields()
{
    return {
        {"时间(s)",      [](const ParsedFrame &f) { return QString::number(f.txHeight.ascTime, 'f', 6); }},
        {"椭球高(m)",    [](const ParsedFrame &f) { return QString::number(f.txHeight.height, 'f', 4); }},
        {"周内秒(ms)",   [](const ParsedFrame &f) { return QString::number(f.txHeight.weekMs); }},
        {"height_raw",   [](const ParsedFrame &f) { return QString::number(f.txHeight.heightRaw); }},
    };
}

static QList<FieldDescriptor> txVelFields()
{
    return {
        {"时间(s)",       [](const ParsedFrame &f) { return QString::number(f.txVel.ascTime, 'f', 6); }},
        {"北向速度(m/s)", [](const ParsedFrame &f) { return QString::number(f.txVel.vn, 'f', 3); }},
        {"东向速度(m/s)", [](const ParsedFrame &f) { return QString::number(f.txVel.ve, 'f', 3); }},
        {"地向速度(m/s)", [](const ParsedFrame &f) { return QString::number(f.txVel.vd, 'f', 3); }},
        {"地速(m/s)",     [](const ParsedFrame &f) { return QString::number(f.txVel.gspeed, 'f', 3); }},
    };
}

static const ProtocolDescriptor kTxAttDescriptor = {
    QStringLiteral("同星CAN-姿态"),
    ProtocolID::Tx_Att,
    false,
    QStringLiteral("时间(s),帧ID,横滚角(deg),俯仰角(deg),航向角(deg),航迹角(deg),raw_roll,raw_pitch,raw_yaw,raw_track\n"),
    {}, {}, txAttFields(),
};

static const ProtocolDescriptor kTxGyroDescriptor = {
    QStringLiteral("同星CAN-陀螺"),
    ProtocolID::Tx_Gyro,
    false,
    QStringLiteral("时间(s),帧ID,gyro_x(deg/s),gyro_y(deg/s),gyro_z(deg/s),预留,raw_x,raw_y,raw_z\n"),
    {}, {}, txGyroFields(),
};

static const ProtocolDescriptor kTxAccelDescriptor = {
    QStringLiteral("同星CAN-加表"),
    ProtocolID::Tx_Accel,
    false,
    QStringLiteral("时间(s),帧ID,acc_x(g),acc_y(g),acc_z(g),状态位,RTK板卡有效,PPS有效,前轮转角有效,前轮陀螺有效,RTK固定解,RTK定向固定解,航向已初始化,raw_x,raw_y,raw_z\n"),
    {}, {}, txAccelFields(),
};

static const ProtocolDescriptor kTxSecDescriptor = {
    QStringLiteral("同星CAN-周内秒"),
    ProtocolID::Tx_Sec,
    false,
    QStringLiteral("时间(s),帧ID,周内秒(ms),估计天线安装偏差角(raw),速度方向,预留\n"),
    {}, {}, txSecFields(),
};

static const ProtocolDescriptor kTxPosDescriptor = {
    QStringLiteral("同星CAN-经纬度"),
    ProtocolID::Tx_Pos,
    false,
    QStringLiteral("时间(s),帧ID,经度(deg),纬度(deg),lon_raw,lat_raw\n"),
    {}, {}, txPosFields(),
};

static const ProtocolDescriptor kTxHeightDescriptor = {
    QStringLiteral("同星CAN-高程"),
    ProtocolID::Tx_Height,
    false,
    QStringLiteral("时间(s),帧ID,椭球高(m),周内秒(ms),height_raw\n"),
    {}, {}, txHeightFields(),
};

static const ProtocolDescriptor kTxVelDescriptor = {
    QStringLiteral("同星CAN-速度"),
    ProtocolID::Tx_Vel,
    false,
    QStringLiteral("时间(s),帧ID,北向速度(m/s),东向速度(m/s),地向速度(m/s),地速(m/s),raw_n,raw_e,raw_d,raw_g\n"),
    {}, {}, txVelFields(),
};

const ProtocolDescriptor *findProtocol(ProtocolID pid)
{
    for (const auto &d : protocolRegistry()) {
        if (d.pid == pid) return &d;
    }
    if (pid == ProtocolID::Can_RTK)  return &kCanRtkDescriptor;
    if (pid == ProtocolID::LsNav_Imu)   return &kLsNavImuDescriptor;
    if (pid == ProtocolID::LsNav_Gps)   return &kLsNavGpsDescriptor;
    if (pid == ProtocolID::LsNav_Param) return &kLsNavParamDescriptor;
    if (pid == ProtocolID::LsImuLog)    return &kLsImuLogDescriptor;
    if (pid == ProtocolID::Novatel_BestPos) return &kNovatelBestPosDescriptor;
    if (pid == ProtocolID::Novatel_PsrVel)  return &kNovatelPsrVelDescriptor;
    if (pid == ProtocolID::A3_Imu)          return &kA3ImuDescriptor;
    if (pid == ProtocolID::Tx_Att)          return &kTxAttDescriptor;
    if (pid == ProtocolID::Tx_Gyro)         return &kTxGyroDescriptor;
    if (pid == ProtocolID::Tx_Accel)        return &kTxAccelDescriptor;
    if (pid == ProtocolID::Tx_Sec)          return &kTxSecDescriptor;
    if (pid == ProtocolID::Tx_Pos)          return &kTxPosDescriptor;
    if (pid == ProtocolID::Tx_Height)       return &kTxHeightDescriptor;
    if (pid == ProtocolID::Tx_Vel)          return &kTxVelDescriptor;
    return nullptr;
}

QList<FieldDescriptor> fieldsForProtocol(ProtocolID pid)
{
    const auto *desc = findProtocol(pid);
    return desc ? desc->fields : QList<FieldDescriptor>{};
}
