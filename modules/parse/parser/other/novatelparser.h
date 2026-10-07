#ifndef NOVATELPARSER_H
#define NOVATELPARSER_H

#include "modules/parse/parser/parserbase.h"
#include <cstring>

// ════════════════════════════════════════════════════════════════
//  诺瓦泰 NovAtel OEM 二进制解析（同步头 AA 44 12）
//  帧结构: 28B 头 + payload + CRC32(4B 小端)
//  CRC32: 多项式 0xEDB88320，初值 0，不取反（与 rtklib rtk_crc32 一致）
//  支持: BESTPOSB(MsgID=42, 72B) / PSRVELB(MsgID=100, 44B)
//  同一文件里混有 NMEA 文本，扫描时自动跳过
// ════════════════════════════════════════════════════════════════

namespace novatel_detail {

inline uint16_t u16le(const uint8_t *p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}
inline uint32_t u32le(const uint8_t *p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
inline int32_t i32le(const uint8_t *p) { return static_cast<int32_t>(u32le(p)); }
inline double f32le(const uint8_t *p) {
    const uint32_t v = u32le(p);
    float f = 0.0f;
    std::memcpy(&f, &v, sizeof(f));
    return static_cast<double>(f);
}
inline double f64le(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i) v = (v << 8) | p[i];
    double d = 0.0;
    std::memcpy(&d, &v, sizeof(d));
    return d;
}
inline uint32_t crc32(const uint8_t *data, int len) {
    uint32_t crc = 0;
    for (int i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b)
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
    }
    return crc;
}

struct RawFrame {
    uint16_t   gpsWeek = 0;
    uint32_t   gpsWeekMs = 0;
    QByteArray payload;
};

// 扫描指定 MsgID 的帧；非目标帧按长度整体跳过
inline QList<RawFrame> scan(const QByteArray &data, int msgId, bool skipCrc, int &crcFail)
{
    QList<RawFrame> out;
    crcFail = 0;
    if (data.isEmpty()) return out;
    const uint8_t *d = reinterpret_cast<const uint8_t *>(data.constData());
    const int n = data.size();
    constexpr int kHdr = 28;
    int i = 0;
    while (i + kHdr + 4 <= n) {
        if (!(d[i] == 0xAA && d[i + 1] == 0x44 && d[i + 2] == 0x12 && d[i + 3] == kHdr)) {
            ++i; continue;
        }
        const int len = u16le(d + i + 8);
        // 头部可信性检查：msgType 必须为 0（二进制），长度不得超限
        if (d[i + 6] != 0 || len > 8192 || i + kHdr + len + 4 > n) { ++i; continue; }
        const int frameLen = kHdr + len + 4;
        if (u16le(d + i + 4) != msgId) { i += frameLen; continue; }
        if (!skipCrc && crc32(d + i, kHdr + len) != u32le(d + i + kHdr + len)) {
            ++crcFail; ++i; continue;   // 逐字节前移重新同步
        }
        RawFrame f;
        f.gpsWeek   = u16le(d + i + 14);
        f.gpsWeekMs = u32le(d + i + 16);
        f.payload   = QByteArray(reinterpret_cast<const char *>(d + i + kHdr), len);
        out.append(f);
        i += frameLen;
    }
    return out;
}

} // namespace novatel_detail

// ── BESTPOSB 解码 (payload 72B) ──
inline NovatelBestPosData decodeNovatelBestPos(const novatel_detail::RawFrame &f)
{
    NovatelBestPosData d;
    std::memset(&d, 0, sizeof(d));
    d.gpsWeek = f.gpsWeek;  d.gpsWeekMs = f.gpsWeekMs;
    if (f.payload.size() < 72) return d;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.payload.constData());
    d.solStatus  = novatel_detail::i32le(p);
    d.posType    = novatel_detail::i32le(p + 4);
    d.lat        = novatel_detail::f64le(p + 8);
    d.lon        = novatel_detail::f64le(p + 16);
    d.height     = novatel_detail::f64le(p + 24);
    d.undulation = novatel_detail::f32le(p + 32);
    d.datumId    = novatel_detail::i32le(p + 36);
    d.latStd     = novatel_detail::f32le(p + 40);
    d.lonStd     = novatel_detail::f32le(p + 44);
    d.heightStd  = novatel_detail::f32le(p + 48);
    d.diffAge    = novatel_detail::f32le(p + 56);
    d.solAge     = novatel_detail::f32le(p + 60);
    d.numSVs          = p[64];
    d.numSolnSVs      = p[65];
    d.numSolnL1SVs    = p[66];
    d.numSolnMultiSVs = p[67];
    return d;
}

// ── PSRVELB 解码 (payload 44B) ──
inline NovatelPsrVelData decodeNovatelPsrVel(const novatel_detail::RawFrame &f)
{
    NovatelPsrVelData d;
    std::memset(&d, 0, sizeof(d));
    d.gpsWeek = f.gpsWeek;  d.gpsWeekMs = f.gpsWeekMs;
    if (f.payload.size() < 40) return d;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.payload.constData());
    d.solStatus   = novatel_detail::i32le(p);
    d.velType     = novatel_detail::i32le(p + 4);
    d.latency     = novatel_detail::f32le(p + 8);
    d.diffAge     = novatel_detail::f32le(p + 12);
    d.horSpeed    = novatel_detail::f64le(p + 16);
    d.trackGround = novatel_detail::f64le(p + 24);
    d.verSpeed    = novatel_detail::f64le(p + 32);
    return d;
}

// ── 解析器：最佳位置 BESTPOSB(42) ──
class NovatelBestPosParser : public ParserBase
{
public:
    QString buttonLabel() const override { return QStringLiteral("BESTPOS_诺瓦泰"); }
    QString category() const override { return QStringLiteral("诺瓦泰"); }
    ProtocolID protocolId() const override { return ProtocolID::Novatel_BestPos; }

    QList<ParsedFrame> parse(const QByteArray &rawData) override
    {
        int crcFail = 0;
        const QList<novatel_detail::RawFrame> fs =
            novatel_detail::scan(rawData, 42, m_skipCrc, crcFail);
        QList<ParsedFrame> out;
        out.reserve(fs.size());
        for (const auto &f : fs) {
            ParsedFrame p;
            std::memset(&p, 0, sizeof(p));
            p.protocol = ProtocolID::Novatel_BestPos;
            p.frameId = 42;
            p.payloadLength = static_cast<uint16_t>(f.payload.size());
            p.crcOk = true;
            p.offsetInFile = out.size();
            p.novatelBestPos = decodeNovatelBestPos(f);
            out.append(p);
        }
        return out;
    }

    QString csvHeader() const override
    {
        return QStringLiteral(
            "GPS周,GPS周内毫秒(ms),解状态,解状态名,定位类型,定位类型名,"
            "纬度(deg),经度(deg),椭球高(m),高程异常(m),基准,"
            "纬度标准差(m),经度标准差(m),高程标准差(m),"
            "差分龄期(s),解龄期(s),卫星数,参与解算星数,L1星数,多频星数\n");
    }

    QString formatFrame(const ParsedFrame &f) const override
    {
        const auto &d = f.novatelBestPos;
        return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20\n")
            .arg(d.gpsWeek).arg(d.gpsWeekMs)
            .arg(d.solStatus).arg(novatelSolStatusName(d.solStatus))
            .arg(d.posType).arg(novatelPosTypeName(d.posType))
            .arg(d.lat, 0, 'f', 9).arg(d.lon, 0, 'f', 9)
            .arg(d.height, 0, 'f', 4).arg(d.undulation, 0, 'f', 3).arg(d.datumId)
            .arg(d.latStd, 0, 'f', 4).arg(d.lonStd, 0, 'f', 4).arg(d.heightStd, 0, 'f', 4)
            .arg(d.diffAge, 0, 'f', 1).arg(d.solAge, 0, 'f', 1)
            .arg(d.numSVs).arg(d.numSolnSVs).arg(d.numSolnL1SVs).arg(d.numSolnMultiSVs);
    }
};

// ── 解析器：伪距速度 PSRVELB(100) ──
class NovatelPsrVelParser : public ParserBase
{
public:
    QString buttonLabel() const override { return QStringLiteral("PSRVEL_诺瓦泰"); }
    QString category() const override { return QStringLiteral("诺瓦泰"); }
    ProtocolID protocolId() const override { return ProtocolID::Novatel_PsrVel; }

    QList<ParsedFrame> parse(const QByteArray &rawData) override
    {
        int crcFail = 0;
        const QList<novatel_detail::RawFrame> fs =
            novatel_detail::scan(rawData, 100, m_skipCrc, crcFail);
        QList<ParsedFrame> out;
        out.reserve(fs.size());
        for (const auto &f : fs) {
            ParsedFrame p;
            std::memset(&p, 0, sizeof(p));
            p.protocol = ProtocolID::Novatel_PsrVel;
            p.frameId = 100;
            p.payloadLength = static_cast<uint16_t>(f.payload.size());
            p.crcOk = true;
            p.offsetInFile = out.size();
            p.novatelPsrVel = decodeNovatelPsrVel(f);
            out.append(p);
        }
        return out;
    }

    QString csvHeader() const override
    {
        return QStringLiteral(
            "GPS周,GPS周内毫秒(ms),解状态,解状态名,速度类型,速度类型名,"
            "延迟(s),差分龄期(s),水平速度(m/s),地面航向(deg),垂直速度(m/s)\n");
    }

    QString formatFrame(const ParsedFrame &f) const override
    {
        const auto &d = f.novatelPsrVel;
        return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11\n")
            .arg(d.gpsWeek).arg(d.gpsWeekMs)
            .arg(d.solStatus).arg(novatelSolStatusName(d.solStatus))
            .arg(d.velType).arg(novatelVelTypeName(d.velType))
            .arg(d.latency, 0, 'f', 3).arg(d.diffAge, 0, 'f', 1)
            .arg(d.horSpeed, 0, 'f', 5)
            .arg(d.trackGround, 0, 'f', 5)
            .arg(d.verSpeed, 0, 'f', 5);
    }
};

#endif // NOVATELPARSER_H
