#ifndef SIM_PROTOCOL_GPRMC_H
#define SIM_PROTOCOL_GPRMC_H

#include "modules/simulator/simprotocol.h"
#include "modules/simulator/encoders/nmeaencoder.h"

// $GPRMC —— 推荐最小定位信息（NMEA 文本）
class Gprmc : public SimProtocol
{
public:
    QByteArray encode(const SimClock &c) override
    {
        const double latDeg = params.value("lat").toDouble(39.9042);
        const double lonDeg = params.value("lon").toDouble(116.4074);

        const QString body = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9")
            .arg(NmeaEncoder::utc(c.utcTimeOfDayMs))                         // 时间
            .arg(params.value("status").toString("A"))                       // 状态
            .arg(NmeaEncoder::lat(latDeg))                                   // 纬度值,N/S
            .arg(NmeaEncoder::lon(lonDeg))                                   // 经度值,E/W
            .arg(QString::number(params.value("speedKnots").toDouble(0.5), 'f', 1)) // 速度(节)
            .arg(QString::number(params.value("courseDeg").toDouble(90.0), 'f', 1))  // 航向(度)
            .arg(c.utcDate.toString("ddMMyy"))                               // 日期 ddmmyy（真实 UTC）
            .arg(QString::number(params.value("magVar").toDouble(0.0), 'f', 1))     // 磁偏角
            .arg(params.value("magVarDir").toString("E"));                   // 磁偏角方向

        return NmeaEncoder::pack("GPRMC", body);
    }
};

#endif // SIM_PROTOCOL_GPRMC_H
