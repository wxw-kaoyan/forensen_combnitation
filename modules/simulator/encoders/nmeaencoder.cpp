#include "modules/simulator/encoders/nmeaencoder.h"

QByteArray NmeaEncoder::pack(const QString &sentence, const QString &body)
{
    const QString line = "$" + sentence + "," + body;

    // 校验和：'$' 与 '*' 之间所有字符的异或
    quint8 cs = 0;
    for (int i = 1; i < line.size(); ++i)
        cs ^= static_cast<quint8>(line.at(i).toLatin1());

    const QString result = line + "*"
            + QString("%1").arg(cs, 2, 16, QLatin1Char('0')).toUpper()
            + "\r\n";
    return result.toLatin1();
}

QString NmeaEncoder::lat(double deg)
{
    const char ns = (deg >= 0) ? 'N' : 'S';
    const double a = qAbs(deg);
    const int d = static_cast<int>(a);
    const double m = (a - d) * 60.0;
    // ddmm.mmmm
    return QString::number(d * 100.0 + m, 'f', 4) + "," + QLatin1Char(ns);
}

QString NmeaEncoder::lon(double deg)
{
    const char ew = (deg >= 0) ? 'E' : 'W';
    const double a = qAbs(deg);
    const int d = static_cast<int>(a);
    const double m = (a - d) * 60.0;
    // dddmm.mmmm
    return QString::number(d * 100.0 + m, 'f', 4) + "," + QLatin1Char(ew);
}

QString NmeaEncoder::utc(uint32_t utcMs)
{
    const uint32_t ms = utcMs % 1000;
    const uint32_t totalSec = utcMs / 1000;
    const uint32_t h = totalSec / 3600;
    const uint32_t m = (totalSec % 3600) / 60;
    const uint32_t s = totalSec % 60;

    return QString("%1%2%3.%4")
        .arg(h, 2, 10, QLatin1Char('0'))
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'))
        .arg(ms, 3, 10, QLatin1Char('0'));
}
