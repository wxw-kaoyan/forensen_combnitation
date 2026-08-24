#ifndef ANALYSIS_CONVERTER_H
#define ANALYSIS_CONVERTER_H

#include "datamodel.h"
#include "../../parse/parser/core/protocoldefs.h"
#include <QList>
#include <algorithm>

namespace Analysis {

// ════════════════════════════════════════════════════════════════
//  ParsedFrame 列表 → DataSet 转换器
//  每种协议取自己能提供的字段，其余为零
// ════════════════════════════════════════════════════════════════

inline DataSet convertAhRsBasic(const QList<ParsedFrame> &frames, const QString &typeName)
{
    DataSet ds;
    ds.sourceType = typeName;
    for (const auto &f : frames) {
        DataPoint dp;
        dp.gpstMs = f.ahrsBasic.timer / 1000.0;  // us → ms
        dp.roll   = f.ahrsBasic.roll;
        dp.pitch  = f.ahrsBasic.pitch;
        dp.yaw    = f.ahrsBasic.yaw;
        dp.accelX = f.ahrsBasic.ax;
        dp.accelY = f.ahrsBasic.ay;
        dp.accelZ = f.ahrsBasic.az;
        dp.gyroX  = f.ahrsBasic.gx;
        dp.gyroY  = f.ahrsBasic.gy;
        dp.gyroZ  = f.ahrsBasic.gz;
        dp.temp   = f.ahrsBasic.temp;
        dp.hasAttitude = true;
        dp.hasImu      = true;
        ds.points.append(dp);
    }
    return ds;
}

inline DataSet convertAhRsExtended(const QList<ParsedFrame> &frames, const QString &typeName)
{
    DataSet ds;
    ds.sourceType = typeName;
    for (const auto &f : frames) {
        DataPoint dp;
        dp.gpstMs    = f.ahrsExtended.basic.timer / 1000.0;
        dp.roll      = f.ahrsExtended.basic.roll;
        dp.pitch     = f.ahrsExtended.basic.pitch;
        dp.yaw       = f.ahrsExtended.basic.yaw;
        dp.accelX    = f.ahrsExtended.basic.ax;
        dp.accelY    = f.ahrsExtended.basic.ay;
        dp.accelZ    = f.ahrsExtended.basic.az;
        dp.gyroX     = f.ahrsExtended.basic.gx;
        dp.gyroY     = f.ahrsExtended.basic.gy;
        dp.gyroZ     = f.ahrsExtended.basic.gz;
        dp.temp      = f.ahrsExtended.basic.temp;
        dp.statusFlag = f.ahrsExtended.status;
        dp.hasAttitude = true;
        dp.hasImu      = true;
        ds.points.append(dp);
    }
    return ds;
}

inline DataSet convertQuaternion(const QList<ParsedFrame> &frames, const QString &typeName)
{
    DataSet ds;
    ds.sourceType = typeName;
    for (const auto &f : frames) {
        DataPoint dp;
        dp.gpstMs    = f.quat.timer / 1000.0;
        dp.accelX    = f.quat.ax;
        dp.accelY    = f.quat.ay;
        dp.accelZ    = f.quat.az;
        dp.gyroX     = f.quat.gx;
        dp.gyroY     = f.quat.gy;
        dp.gyroZ     = f.quat.gz;
        dp.temp      = f.quat.temp;
        dp.statusFlag = f.quat.status;
        dp.hasImu      = true;
        // 四元数 → 欧拉角 (简化的转换，q归一化后)
        double qw = f.quat.qw, qx = f.quat.qx, qy = f.quat.qy, qz = f.quat.qz;
        double norm = std::sqrt(qw*qw + qx*qx + qy*qy + qz*qz);
        if (norm > 0) { qw /= norm; qx /= norm; qy /= norm; qz /= norm; }
        dp.roll  = std::atan2(2.0*(qw*qx + qy*qz), 1.0 - 2.0*(qx*qx + qy*qy)) * RAD_TO_DEG;
        dp.pitch = std::asin(2.0*(qw*qy - qz*qx)) * RAD_TO_DEG;
        dp.yaw   = std::atan2(2.0*(qw*qz + qx*qy), 1.0 - 2.0*(qy*qy + qz*qz)) * RAD_TO_DEG;
        dp.hasAttitude = true;
        ds.points.append(dp);
    }
    return ds;
}

inline DataSet convertAGData(const QList<ParsedFrame> &frames, const QString &typeName)
{
    DataSet ds;
    ds.sourceType = typeName;
    for (const auto &f : frames) {
        DataPoint dp;
        dp.gpstMs     = f.agData.gpsTime;
        dp.roll       = f.agData.roll;
        dp.pitch      = f.agData.pitch;
        dp.yaw        = f.agData.yaw;
        dp.accelX     = f.agData.accelX;
        dp.accelY     = f.agData.accelY;
        dp.accelZ     = f.agData.accelZ;
        dp.gyroX      = f.agData.gyroX;
        dp.gyroY      = f.agData.gyroY;
        dp.gyroZ      = f.agData.gyroZ;
        dp.fixType    = f.agData.rtkStatus;
        dp.statusFlag = f.agData.statusFlag;
        dp.hasAttitude = true;
        dp.hasImu      = true;
        ds.points.append(dp);
    }
    return ds;
}

inline DataSet convertFusionResult(const QList<ParsedFrame> &frames, const QString &typeName)
{
    DataSet ds;
    ds.sourceType = typeName;
    for (const auto &f : frames) {
        DataPoint dp;
        dp.gpstMs        = f.fusion.gpsWeekSecond;
        dp.lat           = f.fusion.lat / 1e7;
        dp.lon           = f.fusion.lon / 1e7;
        dp.height        = f.fusion.alt / 1000.0;
        dp.velN          = f.fusion.velN;
        dp.velE          = f.fusion.velE;
        dp.velD          = f.fusion.velD;
        dp.roll          = f.fusion.roll;
        dp.pitch         = f.fusion.pitch;
        dp.yaw           = f.fusion.yaw;
        dp.heading       = f.fusion.heading;
        dp.accelX        = f.fusion.accelX;
        dp.accelY        = f.fusion.accelY;
        dp.accelZ        = f.fusion.accelZ;
        dp.gyroX         = f.fusion.gyroX;
        dp.gyroY         = f.fusion.gyroY;
        dp.gyroZ         = f.fusion.gyroZ;
        dp.temp          = f.fusion.temp;
        dp.fixType       = f.fusion.rtkStatus;
        dp.numSatellites = f.fusion.numSatellites;
        dp.diffAge       = f.fusion.diffAge;
        dp.posStd        = f.fusion.posStd;
        dp.statusFlag    = f.fusion.statusFlag;
        dp.hasPosition   = true;
        dp.hasVelocity   = true;
        dp.hasAttitude   = true;
        dp.hasImu        = true;
        ds.points.append(dp);
    }
    return ds;
}

inline DataSet convertTiltMeasure(const QList<ParsedFrame> &frames, const QString &typeName)
{
    DataSet ds;
    ds.sourceType = typeName;
    for (const auto &f : frames) {
        DataPoint dp;
        dp.gpstMs    = f.tilt.timestamp;
        dp.lat       = f.tilt.lat;
        dp.lon       = f.tilt.lon;
        dp.height    = f.tilt.elev;
        dp.roll      = f.tilt.rollDeg;
        dp.pitch     = f.tilt.pitchDeg;
        dp.yaw       = f.tilt.yawDeg;
        dp.accelX    = f.tilt.accelRaw[0];
        dp.accelY    = f.tilt.accelRaw[1];
        dp.accelZ    = f.tilt.accelRaw[2];
        dp.gyroX     = f.tilt.gyroRaw[0];
        dp.gyroY     = f.tilt.gyroRaw[1];
        dp.gyroZ     = f.tilt.gyroRaw[2];
        dp.fixType   = f.tilt.gnssFix;
        dp.numSatellites = f.tilt.gnssSatCnt;
        dp.hasPosition   = true;
        dp.hasAttitude   = true;
        dp.hasImu        = true;
        ds.points.append(dp);
    }
    return ds;
}

inline DataSet convertAttitude(const QList<ParsedFrame> &frames, const QString &typeName)
{
    DataSet ds;
    ds.sourceType = typeName;
    for (const auto &f : frames) {
        DataPoint dp;
        dp.gpstMs = f.attitude.timestamp;
        dp.roll   = f.attitude.roll;
        dp.pitch  = f.attitude.pitch;
        dp.yaw    = f.attitude.yaw;
        dp.hasAttitude = true;
        ds.points.append(dp);
    }
    return ds;
}

inline DataSet convertCanRtk(const QList<ParsedFrame> &frames, const QString &typeName)
{
    DataSet ds;
    ds.sourceType = typeName;
    for (const auto &f : frames) {
        DataPoint dp;
        // UTC天数 + 天内秒 → 近似GPS周秒
        // 简化处理：直接存UTC时间戳(ms)
        dp.gpstMs        = (f.canRtk.utcDays * 86400.0 + f.canRtk.secondsOfDay) * 1000.0;
        dp.lat           = f.canRtk.latitude;
        dp.lon           = f.canRtk.longitude;
        dp.height        = f.canRtk.altitude;
        dp.velN          = f.canRtk.forwardSpeed;  // 前向速度 (不拆N/E)
        dp.yaw           = f.canRtk.speedDirection;
        if (f.canRtk.rollValid)  dp.roll  = f.canRtk.roll;
        if (f.canRtk.pitchValid) dp.pitch = f.canRtk.pitch;
        if (f.canRtk.yawValid)   dp.yaw   = f.canRtk.yaw;
        dp.fixType       = f.canRtk.fixType;
        dp.diffAge       = f.canRtk.diffAge;
        dp.numSatellites = f.canRtk.numSatellites;
        dp.hasPosition   = true;
        dp.hasAttitude   = f.canRtk.rollValid || f.canRtk.pitchValid || f.canRtk.yawValid;
        ds.points.append(dp);
    }
    return ds;
}

// ════════════════════════════════════════════════════════════════
//  统一入口：根据协议类型做分发
// ════════════════════════════════════════════════════════════════
inline DataSet convert(const QList<ParsedFrame> &frames, ProtocolID pid, const QString &typeName)
{
    switch (pid) {
        case ProtocolID::AHRS_Basic:    return convertAhRsBasic(frames, typeName);
        case ProtocolID::AHRS_Extended: return convertAhRsExtended(frames, typeName);
        case ProtocolID::Quaternion:    return convertQuaternion(frames, typeName);
        case ProtocolID::AG_Data:       return convertAGData(frames, typeName);
        case ProtocolID::FusionResult:  return convertFusionResult(frames, typeName);
        case ProtocolID::TiltMeasure:   return convertTiltMeasure(frames, typeName);
        case ProtocolID::Attitude:      return convertAttitude(frames, typeName);
        case ProtocolID::Can_RTK:       return convertCanRtk(frames, typeName);
        default: return DataSet{};
    }
}

// ════════════════════════════════════════════════════════════════
//  后处理：排序 + 计算摘要
// ════════════════════════════════════════════════════════════════
inline void finalizeDataSet(DataSet &ds)
{
    if (ds.points.isEmpty()) return;

    // 按时间排序
    std::sort(ds.points.begin(), ds.points.end(),
              [](const DataPoint &a, const DataPoint &b) { return a.gpstMs < b.gpstMs; });

    // 去重（同一时间戳只保留第一条）
    ds.points.erase(
        std::unique(ds.points.begin(), ds.points.end(),
                    [](const DataPoint &a, const DataPoint &b) {
                        return std::abs(a.gpstMs - b.gpstMs) < 0.5;
                    }),
        ds.points.end());

    ds.totalPoints = ds.points.size();
    if (ds.totalPoints == 0) return;

    ds.startGpstMs = ds.points.first().gpstMs;
    ds.endGpstMs   = ds.points.last().gpstMs;
    ds.durationSec = (ds.endGpstMs - ds.startGpstMs) / 1000.0;
    ds.avgDataRate = ds.durationSec > 0 ? ds.totalPoints / ds.durationSec : 0;

    // 统计 RTK 固定/浮点帧数
    for (const auto &p : ds.points) {
        if (p.fixType == 4) ds.fixCount++;
        else if (p.fixType == 5) ds.floatCount++;
    }

    // 统计时间断点 (间隔 > 2×中值间隔)
    if (ds.totalPoints >= 2) {
        QVector<double> intervals;
        for (int i = 1; i < ds.totalPoints; ++i)
            intervals.append(ds.points[i].gpstMs - ds.points[i-1].gpstMs);
        std::sort(intervals.begin(), intervals.end());
        double medianInterval = intervals[intervals.size() / 2];
        double threshold = qMax(medianInterval * 2.0, 500.0);

        for (int i = 1; i < ds.totalPoints; ++i) {
            double dt = ds.points[i].gpstMs - ds.points[i-1].gpstMs;
            if (dt > threshold) {
                ds.gapCount++;
                ds.totalGapSec += dt / 1000.0;
            }
        }
    }
}

} // namespace Analysis

#endif // ANALYSIS_CONVERTER_H
