#ifndef NMEAENCODER_H
#define NMEAENCODER_H

#include <QByteArray>
#include <QString>
#include <cstdint>

// NMEA 0183 文本编码器：负责 $ 头、校验和、换行
class NmeaEncoder
{
public:
    // 打包成 "$sentence,body*CS\r\n"
    static QByteArray pack(const QString &sentence, const QString &body);

    // 十进制度 → NMEA 纬度 "ddmm.mmmm,N/S"
    static QString lat(double deg);
    // 十进制度 → NMEA 经度 "dddmm.mmmm,E/W"
    static QString lon(double deg);
    // UTC 日内毫秒 → "hhmmss.sss"
    static QString utc(uint32_t utcMs);
};

#endif // NMEAENCODER_H
