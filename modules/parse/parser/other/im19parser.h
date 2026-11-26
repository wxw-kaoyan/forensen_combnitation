#ifndef IM19PARSER_H
#define IM19PARSER_H

#include "modules/parse/parser/parserbase.h"
#include <QByteArray>
#include <QList>
#include <cstring>

// ════════════════════════════════════════════════════════════════
//  IM19 惯性模组串口协议（《IM19 使用手册 V1.3.8》第 3.1 节）
//
//  三路二进制帧共用一套外壳（小端）：
//    [0..2] 帧头 'f' 'm' 'i'
//    [3]    类型字符  'm'=MEMS  'g'=GNSS  'n'=NAVI
//    [4..]  数据区（时间 double + 各字段）
//    [len-4..len-3] 校验和 uint16 = 前 len-4 字节逐字节累加（低字节在前）
//    [len-2..len-1] 帧尾 'e' 'd'
//
//  帧长：MEMS 52B / GNSS 95B / NAVI 100B
//  MEMS 单位：加速度 g、陀螺 rad/s（注意与 AHRS 协议的 deg/s 不同）
//
//  另有一路字符格式 NAVI：$GPFMI,...,Status*CS（NMEA-0813 异或校验）
// ════════════════════════════════════════════════════════════════
namespace im19_detail {

constexpr uint8_t kH0    = 'f';
constexpr uint8_t kH1    = 'm';
constexpr uint8_t kH2    = 'i';
constexpr uint8_t kTail0 = 'e';
constexpr uint8_t kTail1 = 'd';

inline int frameLenFor(uint8_t type)
{
    switch (type) {
    case 'm': return kIm19MemsLen;   // 52
    case 'g': return kIm19GnssLen;   // 95
    case 'n': return kIm19NaviLen;   // 100
    default:  return 0;
    }
}

// ── 按小端显式拼装，避免依赖主机字节序 ──
inline uint16_t readU16LE(const uint8_t *p)
{
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

inline uint32_t readU32LE(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

inline float readFloatLE(const uint8_t *p)
{
    const uint32_t u = readU32LE(p);
    float v;
    std::memcpy(&v, &u, sizeof(v));
    return v;
}

inline double readDoubleLE(const uint8_t *p)
{
    uint64_t u = 0;
    for (int i = 7; i >= 0; --i)
        u = (u << 8) | p[i];
    double v;
    std::memcpy(&v, &u, sizeof(v));
    return v;
}

inline uint16_t sum16(const uint8_t *p, int n)
{
    uint32_t s = 0;
    for (int i = 0; i < n; ++i)
        s += p[i];
    return static_cast<uint16_t>(s & 0xFFFFu);
}

// ── 数据区解码（d 指向帧首，偏移与手册 1-based 位置对应）──
inline Im19MemsData decodeMems(const uint8_t *d)
{
    Im19MemsData v;
    std::memset(&v, 0, sizeof(v));
    v.time    = readDoubleLE(d + 4);          // 字段1
    v.timeSec = im19TimeOfDay(v.time);
    for (int i = 0; i < 3; ++i)
        v.accel[i] = readFloatLE(d + 12 + 4 * i);   // 字段2~4
    for (int i = 0; i < 3; ++i)
        v.gyro[i] = readFloatLE(d + 24 + 4 * i);    // 字段5~7
    for (int i = 0; i < 3; ++i)
        v.reserved[i] = readFloatLE(d + 36 + 4 * i); // 字段8~10
    return v;
}

inline Im19GnssData decodeGnss(const uint8_t *d)
{
    Im19GnssData v;
    std::memset(&v, 0, sizeof(v));
    v.time    = readDoubleLE(d + 4);
    v.timeSec = im19TimeOfDay(v.time);
    v.lat     = readDoubleLE(d + 12);
    v.lon     = readDoubleLE(d + 20);
    v.hgt     = readDoubleLE(d + 28);
    for (int i = 0; i < 3; ++i)
        v.vel[i] = readFloatLE(d + 36 + 4 * i);       // 北/东/地向速度
    for (int i = 0; i < 3; ++i)
        v.posVar[i] = readFloatLE(d + 48 + 4 * i);    // 北/东/高位置方差
    v.reserved[0] = readFloatLE(d + 60);
    v.reserved[1] = readFloatLE(d + 64);
    v.heading = readFloatLE(d + 68);                  // 双天线航向
    v.hrms    = readFloatLE(d + 72);
    v.vrms    = readFloatLE(d + 76);
    v.hdop    = readFloatLE(d + 80);
    v.vdop    = readFloatLE(d + 84);
    v.satNum  = d[88];
    v.solStat = d[89];
    v.diffAge = d[90];
    return v;
}

inline Im19NaviData decodeNavi(const uint8_t *d)
{
    Im19NaviData v;
    std::memset(&v, 0, sizeof(v));
    v.time    = readDoubleLE(d + 4);
    v.timeSec = im19TimeOfDay(v.time);
    v.lat     = readDoubleLE(d + 12);
    v.lon     = readDoubleLE(d + 20);
    v.hgt     = readDoubleLE(d + 28);
    for (int i = 0; i < 3; ++i)
        v.vel[i] = readFloatLE(d + 36 + 4 * i);       // NED 速度
    v.roll    = readFloatLE(d + 48);
    v.pitch   = readFloatLE(d + 52);
    v.yaw     = readFloatLE(d + 56);
    v.posAcc  = readFloatLE(d + 60);
    for (int i = 0; i < 3; ++i)
        v.accBias[i] = readFloatLE(d + 64 + 4 * i);   // 加计零偏
    for (int i = 0; i < 3; ++i)
        v.gyroBias[i] = readFloatLE(d + 76 + 4 * i);  // 陀螺零偏
    v.temperature = readFloatLE(d + 88);
    v.status      = readU32LE(d + 92);
    return v;
}

// ── 组装一帧（含校验）──
inline ParsedFrame makeFrame(const QByteArray &raw, int pos, uint8_t type)
{
    const uint8_t *d = reinterpret_cast<const uint8_t *>(raw.constData()) + pos;
    const int len = frameLenFor(type);

    ParsedFrame f;
    std::memset(&f, 0, sizeof(f));
    f.frameId       = type;                       // 用类型字符当帧 ID
    f.payloadLength = static_cast<uint16_t>(len - 8);  // 去掉 4B 帧头 + 2B 校验 + 2B 帧尾
    f.crc32         = readU16LE(d + len - 4);
    f.crc32Expected = sum16(d, len - 4);
    f.crcOk         = (f.crc32 == f.crc32Expected);
    f.offsetInFile  = pos;

    switch (type) {
    case 'm':
        f.protocol = ProtocolID::Im19_Mems;
        f.im19Mems = decodeMems(d);
        f.im19Mems.checksum     = static_cast<uint16_t>(f.crc32);
        f.im19Mems.checksumCalc = static_cast<uint16_t>(f.crc32Expected);
        break;
    case 'g':
        f.protocol = ProtocolID::Im19_Gnss;
        f.im19Gnss = decodeGnss(d);
        f.im19Gnss.checksum     = static_cast<uint16_t>(f.crc32);
        f.im19Gnss.checksumCalc = static_cast<uint16_t>(f.crc32Expected);
        break;
    default:
        f.protocol = ProtocolID::Im19_Navi;
        f.im19Navi = decodeNavi(d);
        f.im19Navi.checksum     = static_cast<uint16_t>(f.crc32);
        f.im19Navi.checksumCalc = static_cast<uint16_t>(f.crc32Expected);
        break;
    }
    return f;
}

// ── 扫描二进制流：按帧长边界推进，只输出 wantType 的帧 ──
//  同一串口上三路输出常混在一起，其它类型的帧按长度跳过，不做解析
inline QList<ParsedFrame> scan(const QByteArray &raw, uint8_t wantType,
                               bool skipCrc, int &crcFail)
{
    QList<ParsedFrame> out;
    crcFail = 0;
    const uint8_t *d = reinterpret_cast<const uint8_t *>(raw.constData());
    const int n = raw.size();

    int i = 0;
    while (i + 4 <= n) {
        if (d[i] != kH0 || d[i + 1] != kH1 || d[i + 2] != kH2) {
            ++i;
            continue;
        }
        const uint8_t type = d[i + 3];
        const int len = frameLenFor(type);
        if (len == 0) {
            ++i;                       // 未知类型字符：前移重新同步
            continue;
        }
        if (i + len > n)
            break;                     // 半包，等待后续数据
        if (d[i + len - 2] != kTail0 || d[i + len - 1] != kTail1) {
            ++i;                       // 帧尾不对：误同步
            continue;
        }

        ParsedFrame f = makeFrame(raw, i, type);
        if (!f.crcOk && !skipCrc) {
            ++crcFail;
            ++i;
            continue;
        }
        if (type == wantType)
            out.append(f);
        i += len;
    }
    return out;
}

// ── 字符格式 NAVI（$GPFMI）解析 ──
//  $GPFMI,Utc,Lat,Lon,Hgt,Roll,Pitch,Yaw,HRms,NumSat,AgeDiff,NavMod,Status*CS
inline QList<ParsedFrame> scanNasc(const QByteArray &raw, bool skipCrc,
                                   int &crcFail)
{
    QList<ParsedFrame> out;
    crcFail = 0;

    int pos = 0;
    while (true) {
        const int start = raw.indexOf("$GPFMI,", pos);
        if (start < 0)
            break;

        const int star = raw.indexOf('*', start);
        if (star < 0 || star + 2 >= raw.size())
            break;                                    // 末尾不完整，等后续（此处直接结束）
        const int nl = raw.indexOf('\n', start);
        if (nl >= 0 && nl < star) {                   // 该行没有校验，跳到下一行
            pos = nl + 1;
            continue;
        }

        // NMEA 校验：'$' 与 '*' 之间所有字符异或
        uint8_t cs = 0;
        for (int k = start + 1; k < star; ++k)
            cs = static_cast<uint8_t>(cs ^ static_cast<uint8_t>(raw.at(k)));

        bool ok = false;
        const uint8_t recv = static_cast<uint8_t>(
            QByteArray(raw.mid(star + 1, 2)).toUShort(&ok, 16));

        const QByteArray body = raw.mid(start + 1, star - start - 1);
        const QList<QByteArray> parts = body.split(',');
        if (parts.size() >= 13) {
            ParsedFrame f;
            std::memset(&f, 0, sizeof(f));
            f.protocol      = ProtocolID::Im19_Nasc;
            f.frameId       = 0;
            f.payloadLength = static_cast<uint16_t>(body.size());
            f.crc32         = recv;
            f.crc32Expected = cs;
            f.crcOk         = ok && (recv == cs);
            f.offsetInFile  = start;

            Im19NascData &v = f.im19Nasc;
            std::strncpy(v.utc, parts.at(1).constData(), sizeof(v.utc) - 1);
            bool numOk = false;
            const double utc = QString::fromLatin1(parts.at(1)).toDouble(&numOk);
            v.timeSec = numOk ? im19TimeOfDay(utc) : 0.0;
            v.lat     = parts.at(2).toDouble();
            v.lon     = parts.at(3).toDouble();
            v.hgt     = parts.at(4).toDouble();
            v.roll    = parts.at(5).toFloat();
            v.pitch   = parts.at(6).toFloat();
            v.yaw     = parts.at(7).toFloat();
            v.hrms    = parts.at(8).toFloat();
            v.satNum  = parts.at(9).toInt();
            v.diffAge = parts.at(10).toInt();
            v.navMode = parts.at(11).toInt();
            std::strncpy(v.status, parts.at(12).constData(), sizeof(v.status) - 1);
            v.checksum     = recv;
            v.checksumCalc = cs;

            if (f.crcOk || skipCrc) {
                if (!f.crcOk)
                    ++crcFail;
                out.append(f);
            } else {
                ++crcFail;
            }
        }
        pos = star + 3;
    }
    return out;
}

// 字符格式的 Status 字段是数值文本（十进制或 0x 十六进制），
// 转成与二进制 NAVI 一致的中文状态字；解析不出来就原样返回。
inline QString statusTextFromField(const QByteArray &raw)
{
    const QString t = QString::fromLatin1(raw).trimmed();
    bool ok = false;
    const uint v = t.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)
                       ? t.mid(2).toUInt(&ok, 16)
                       : t.toUInt(&ok, 10);
    return ok ? im19StatusText(v) : t;
}

// 手册中姿态字段单位为 rad，界面与 CSV 统一换算为 deg（结构体仍保存原始 rad）
constexpr double kRadToDeg = 57.29577951308232;

} // namespace im19_detail

// ════════════════════════════════════════════════════════════════
//  MEMS 原始数据
// ════════════════════════════════════════════════════════════════
class Im19MemsParser : public ParserBase
{
public:
    QString buttonLabel() const override { return QStringLiteral("IM19-MEMS原始(m)"); }
    QString category() const override { return QStringLiteral("IM19 惯性模组"); }
    ProtocolID protocolId() const override { return ProtocolID::Im19_Mems; }

    QList<ParsedFrame> parse(const QByteArray &rawData) override
    {
        int crcFail = 0;
        return im19_detail::scan(rawData, 'm', skipCrc(), crcFail);
    }

    QString csvHeader() const override
    {
        return QStringLiteral(
            "时间(hhmmss.ss),时间(s),"
            "加计X(g),加计Y(g),加计Z(g),"
            "陀螺X(rad/s),陀螺Y(rad/s),陀螺Z(rad/s),"
            "校验和(收),校验和(算),校验结果\n");
    }

    QString formatFrame(const ParsedFrame &f) const override
    {
        const auto &v = f.im19Mems;
        return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11\n")
            .arg(v.time, 0, 'f', 2)
            .arg(v.timeSec, 0, 'f', 3)
            .arg(v.accel[0], 0, 'f', 6).arg(v.accel[1], 0, 'f', 6).arg(v.accel[2], 0, 'f', 6)
            .arg(v.gyro[0], 0, 'f', 6).arg(v.gyro[1], 0, 'f', 6).arg(v.gyro[2], 0, 'f', 6)
            .arg(v.checksum, 4, 16, QLatin1Char('0'))
            .arg(v.checksumCalc, 4, 16, QLatin1Char('0'))
            .arg(f.crcOk ? QStringLiteral("OK") : QStringLiteral("错"));
    }
};

// ════════════════════════════════════════════════════════════════
//  GNSS 原始数据
// ════════════════════════════════════════════════════════════════
class Im19GnssParser : public ParserBase
{
public:
    QString buttonLabel() const override { return QStringLiteral("IM19-GNSS原始(g)"); }
    QString category() const override { return QStringLiteral("IM19 惯性模组"); }
    ProtocolID protocolId() const override { return ProtocolID::Im19_Gnss; }

    QList<ParsedFrame> parse(const QByteArray &rawData) override
    {
        int crcFail = 0;
        return im19_detail::scan(rawData, 'g', skipCrc(), crcFail);
    }

    QString csvHeader() const override
    {
        return QStringLiteral(
            "时间(hhmmss.ss),时间(s),纬度(deg),经度(deg),高度(m),"
            "北向速度(m/s),东向速度(m/s),向下速度(m/s),"
            "北向方差,东向方差,高程方差,"
            "双天线航向(deg),HRMS,VRMS,HDOP,VDOP,"
            "卫星数,解状态,差分龄期,校验和(收),校验和(算),校验结果\n");
    }

    QString formatFrame(const ParsedFrame &f) const override
    {
        const auto &v = f.im19Gnss;
        return QStringLiteral(
            "%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22\n")
            .arg(v.time, 0, 'f', 2)
            .arg(v.timeSec, 0, 'f', 3)
            .arg(v.lat, 0, 'f', 9)
            .arg(v.lon, 0, 'f', 9)
            .arg(v.hgt, 0, 'f', 3)
            .arg(v.vel[0], 0, 'f', 3).arg(v.vel[1], 0, 'f', 3).arg(v.vel[2], 0, 'f', 3)
            .arg(v.posVar[0], 0, 'f', 4).arg(v.posVar[1], 0, 'f', 4).arg(v.posVar[2], 0, 'f', 4)
            .arg(v.heading, 0, 'f', 3)
            .arg(v.hrms, 0, 'f', 3).arg(v.vrms, 0, 'f', 3)
            .arg(v.hdop, 0, 'f', 2).arg(v.vdop, 0, 'f', 2)
            .arg(v.satNum).arg(v.solStat).arg(v.diffAge)
            .arg(v.checksum, 4, 16, QLatin1Char('0'))
            .arg(v.checksumCalc, 4, 16, QLatin1Char('0'))
            .arg(f.crcOk ? QStringLiteral("OK") : QStringLiteral("错"));
    }
};

// ════════════════════════════════════════════════════════════════
//  NAVI 惯性定位结果（二进制）
// ════════════════════════════════════════════════════════════════
class Im19NaviParser : public ParserBase
{
public:
    QString buttonLabel() const override { return QStringLiteral("IM19-NAVI定位(n)"); }
    QString category() const override { return QStringLiteral("IM19 惯性模组"); }
    ProtocolID protocolId() const override { return ProtocolID::Im19_Navi; }

    QList<ParsedFrame> parse(const QByteArray &rawData) override
    {
        int crcFail = 0;
        return im19_detail::scan(rawData, 'n', skipCrc(), crcFail);
    }

    QString csvHeader() const override
    {
        return QStringLiteral(
            "时间(hhmmss.ss),时间(s),纬度(deg),经度(deg),高程(m),"
            "北向速度(m/s),东向速度(m/s),向下速度(m/s),"
            "横滚(deg),俯仰(deg),航向(deg),定位精度,"
            "加计零偏X,加计零偏Y,加计零偏Z,"
            "陀螺零偏X,陀螺零偏Y,陀螺零偏Z,"
            "温度(C),状态字,校验和(收),校验和(算),校验结果\n");
    }

    QString formatFrame(const ParsedFrame &f) const override
    {
        const auto &v = f.im19Navi;
        return QStringLiteral(
            "%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,"
            "%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23\n")
            .arg(v.time, 0, 'f', 2)
            .arg(v.timeSec, 0, 'f', 3)
            .arg(v.lat, 0, 'f', 9)
            .arg(v.lon, 0, 'f', 9)
            .arg(v.hgt, 0, 'f', 3)
            .arg(v.vel[0], 0, 'f', 3).arg(v.vel[1], 0, 'f', 3).arg(v.vel[2], 0, 'f', 3)
            .arg(v.roll * im19_detail::kRadToDeg, 0, 'f', 4)
            .arg(v.pitch * im19_detail::kRadToDeg, 0, 'f', 4)
            .arg(v.yaw * im19_detail::kRadToDeg, 0, 'f', 4)
            .arg(v.posAcc, 0, 'f', 3)
            .arg(v.accBias[0], 0, 'f', 6).arg(v.accBias[1], 0, 'f', 6).arg(v.accBias[2], 0, 'f', 6)
            .arg(v.gyroBias[0], 0, 'f', 8).arg(v.gyroBias[1], 0, 'f', 8).arg(v.gyroBias[2], 0, 'f', 8)
            .arg(v.temperature, 0, 'f', 2)
            .arg(im19StatusText(v.status))
            .arg(v.checksum, 4, 16, QLatin1Char('0'))
            .arg(v.checksumCalc, 4, 16, QLatin1Char('0'))
            .arg(f.crcOk ? QStringLiteral("OK") : QStringLiteral("错"));
    }
};

// ════════════════════════════════════════════════════════════════
//  NAVI 字符格式（$GPFMI）
// ════════════════════════════════════════════════════════════════
class Im19NascParser : public ParserBase
{
public:
    QString buttonLabel() const override { return QStringLiteral("IM19-NAVI字符($GPFMI)"); }
    QString category() const override { return QStringLiteral("IM19 惯性模组"); }
    ProtocolID protocolId() const override { return ProtocolID::Im19_Nasc; }

    QList<ParsedFrame> parse(const QByteArray &rawData) override
    {
        int crcFail = 0;
        return im19_detail::scanNasc(rawData, skipCrc(), crcFail);
    }

    QString csvHeader() const override
    {
        return QStringLiteral(
            "Utc,时间(s),纬度(deg),经度(deg),高程(m),"
            "横滚(deg),俯仰(deg),航向(deg),HRms,卫星数,差分龄期,导航模式(NavMod),状态字,"
            "校验(收),校验(算),校验结果\n");
    }

    QString formatFrame(const ParsedFrame &f) const override
    {
        const auto &v = f.im19Nasc;
        return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16\n")
            .arg(QString::fromLatin1(v.utc))
            .arg(v.timeSec, 0, 'f', 3)
            .arg(v.lat, 0, 'f', 9)
            .arg(v.lon, 0, 'f', 9)
            .arg(v.hgt, 0, 'f', 3)
            .arg(v.roll * im19_detail::kRadToDeg, 0, 'f', 4)
            .arg(v.pitch * im19_detail::kRadToDeg, 0, 'f', 4)
            .arg(v.yaw * im19_detail::kRadToDeg, 0, 'f', 4)
            .arg(v.hrms, 0, 'f', 3)
            .arg(v.satNum).arg(v.diffAge).arg(v.navMode)
            .arg(im19_detail::statusTextFromField(QByteArray(v.status)))
            .arg(int(v.checksum), 2, 16, QLatin1Char('0'))
            .arg(int(v.checksumCalc), 2, 16, QLatin1Char('0'))
            .arg(f.crcOk ? QStringLiteral("OK") : QStringLiteral("错"));
    }
};

#endif // IM19PARSER_H
