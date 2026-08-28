#ifndef SIM_PROTOCOL_GPGGA_H
#define SIM_PROTOCOL_GPGGA_H

#include "modules/simulator/simprotocol.h"
#include "modules/simulator/encoders/nmeaencoder.h"

// $GPGGA —— 全球定位系统固定数据（NMEA 文本）
class Gpgga : public SimProtocol
{
public:
    QByteArray encode(const SimClock &c) override
    {
        const double latDeg = params.value("lat").toDouble(39.9042);
        const double lonDeg = params.value("lon").toDouble(116.4074);

        const QString body = QString("%1,%2,%3,%4,%5,%6,%7,M,%8,M,,")
            .arg(NmeaEncoder::utc(c.utcTimeOfDayMs))                          // 时间
            .arg(NmeaEncoder::lat(latDeg))                                    // 纬度值,N/S
            .arg(NmeaEncoder::lon(lonDeg))                                    // 经度值,E/W
            .arg(params.value("quality").toInt(1))                            // 定位质量
            .arg(params.value("numSats").toInt(18))                           // 卫星数
            .arg(QString::number(params.value("hdop").toDouble(1.0), 'f', 1)) // HDOP
            .arg(QString::number(params.value("alt").toDouble(50.0), 'f', 2)) // 天线高程(m)
            .arg(QString::number(params.value("geoidSep").toDouble(0.0), 'f', 2)); // 大地水准面差距(m)

        return NmeaEncoder::pack("GPGGA", body);
    }
};

#endif // SIM_PROTOCOL_GPGGA_H
