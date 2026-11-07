#ifndef AHRSA5PARSER_H
#define AHRSA5PARSER_H

#include "modules/parse/parser/parserbase.h"
#include <QDebug>
#include <QHash>
#include <cstring>

// ════════════════════════════════════════════════════════════════
//  AHRS 标准协议（《IMU规格标准》V1.x）— 26 字节定长帧
//
//  [0..1]   0xAA 0xA5            包头
//  [2]      Index                帧序号，每帧 +1（u8 回绕）
//  [3..4]   Roll   int16 LE ÷100 (°, ±180)
//  [5..6]   Pitch  int16 LE ÷100 (°, ±90)
//  [7..8]   Yaw    int16 LE ÷100 (°, ±180)
//  [9..10]  RateX  int16 LE ÷100 (deg/s)   ← 严格按协议文档
//  [11..12] RateY  int16 LE ÷100 (deg/s)
//  [13..14] RateZ  int16 LE ÷100 (deg/s)
//  [15..16] AccX   int16 LE ÷1000 (g)       ← 严格按协议文档
//  [17..18] AccY   int16 LE ÷1000 (g)
//  [19..20] AccZ   int16 LE ÷1000 (g)
//
// 解析器只做按协议字段名取值，不做任何轴映射或符号反转
//  [21..22] ChecksumA uint16 LE = (int16)Yaw + (int16)RateZ
//  [23]     ChecksumB uint8     = Index + Pitch + Roll + RateX + RateY
//                                 + AccX + AccY + AccZ 各字节和
//  [24..25] 0x0D 0x0A            包尾
// ════════════════════════════════════════════════════════════════

namespace ahrs_aa5_detail {

constexpr int     kFrameLen = 26;
constexpr uint8_t kSync1    = 0xAA;
constexpr uint8_t kSync2    = 0xA5;
constexpr uint8_t kEnd1     = 0x0D;
constexpr uint8_t kEnd2     = 0x0A;

inline int16_t i16(const uint8_t *p)
{
    return static_cast<int16_t>(p[0] | (p[1] << 8));
}

inline uint16_t calcChecksumA(const uint8_t *p)
{
    return static_cast<uint16_t>((static_cast<int32_t>(i16(p + 7))
                                  + static_cast<int32_t>(i16(p + 13))) & 0xFFFF);
}

inline uint8_t calcChecksumB(const uint8_t *p)
{
    // Index + Roll + Pitch + RateX + RateY + AccX + AccY + AccZ
    // 注意：Yaw 与 RateZ 已计入 ChecksumA，此处不含
    uint32_t s = p[2];
    for (int o = 3; o <= 6; o += 2)  s += p[o] + p[o + 1];   // Roll, Pitch
    for (int o = 9; o <= 11; o += 2) s += p[o] + p[o + 1];   // RateX, RateY
    for (int o = 15; o <= 19; o += 2) s += p[o] + p[o + 1];  // AccX, AccY, AccZ
    return static_cast<uint8_t>(s & 0xFF);
}

// 累计全局序号计数器（每次 scan 开始时重置）
static uint32_t s_globalCounter = 0;

inline ParsedFrame makeFrame(const QByteArray &raw, int pos)
{
    const uint8_t *d = reinterpret_cast<const uint8_t *>(raw.constData()) + pos;
    ParsedFrame f;
    std::memset(&f, 0, sizeof(f));
    f.protocol      = ProtocolID::Ahrs_Aa5;
    f.frameId       = (static_cast<uint16_t>(kSync1) << 8) | kSync2;
    f.payloadLength = kFrameLen;
    f.offsetInFile  = pos;

    AhrsAa5Data &v = f.ahrsAa5;
    v.index       = d[2];
    v.globalIndex = ++s_globalCounter;
    v.roll        = i16(d + 3)  / 100.0f;
    v.pitch       = i16(d + 5)  / 100.0f;
    v.yaw         = i16(d + 7)  / 100.0f;
    /*qDebug("[AHRS AA5] idx=%3u  roll=%7.2f  pitch=%7.2f  yaw=%7.2f",
           v.index, v.roll, v.pitch, v.yaw);*/
    // 严格按协议文档字段布局解析，不做任何轴映射或符号反转
    v.rate[0]     =  i16(d + 9)  / 100.0f;   // RateX
    v.rate[1]     =  i16(d + 11) / 100.0f;   // RateY
    v.rate[2]     =  i16(d + 13) / 100.0f;   // RateZ
    v.accel[0]    =  i16(d + 15) / 1000.0f;  // AccX
    v.accel[1]    =  i16(d + 17) / 1000.0f;  // AccY
    v.accel[2]    =  i16(d + 19) / 1000.0f;  // AccZ 

    v.checksumA     = static_cast<uint16_t>(d[21] | (d[22] << 8));
    v.checksumACalc = calcChecksumA(d);
    v.checksumB     = d[23];
    v.checksumBCalc = calcChecksumB(d);

    f.crc32         = v.checksumA;
    f.crc32Expected = v.checksumACalc;
    f.crcOk         = (v.checksumA == v.checksumACalc) && (v.checksumB == v.checksumBCalc);
    return f;
}

inline QList<ParsedFrame> scan(const QByteArray &raw, bool skipCrc, int &crcFail)
{
    QList<ParsedFrame> out;
    crcFail = 0;
    s_globalCounter = 0;  // 每次扫描重置累计序号
    const uint8_t *d = reinterpret_cast<const uint8_t *>(raw.constData());
    const int n = raw.size();
    int i = 0;
    while (i + kFrameLen <= n) {
        if (d[i] != kSync1 || d[i + 1] != kSync2
            || d[i + 24] != kEnd1 || d[i + 25] != kEnd2) {
            ++i;
            continue;
        }
        ParsedFrame f = makeFrame(raw, i);
        if (!f.crcOk && !skipCrc) {
            ++crcFail;
            ++i;
            continue;
        }
        out.append(f);
        i += kFrameLen;
    }
    return out;
}

} // namespace ahrs_aa5_detail

class AhrsAa5Parser : public ParserBase
{
public:
    QString buttonLabel() const override { return QStringLiteral("AHRS(AA A5)"); }
    QString category() const override { return QStringLiteral("AHRS"); }
    ProtocolID protocolId() const override { return ProtocolID::Ahrs_Aa5; }

    QList<ParsedFrame> parse(const QByteArray &rawData) override
    {
        int crcFail = 0;
        return ahrs_aa5_detail::scan(rawData, skipCrc(), crcFail);
    }

    // 默认输出 + 序号连续性统计（丢帧检测）
    ParseResult parseToResult(const QByteArray &rawData,
                              const QString &filePath) override
    {
        ParseResult r = ParserBase::parseToResult(rawData, filePath);
        if (r.frames.size() < 2)
            return r;

        QHash<uint8_t, int> hist;
        for (int i = 1; i < r.frames.size(); ++i) {
            const uint8_t d = static_cast<uint8_t>(
                r.frames[i].ahrsAa5.index - r.frames[i - 1].ahrsAa5.index);
            ++hist[d];
        }
        uint8_t step = 0;
        int best = -1;
        for (auto it = hist.constBegin(); it != hist.constEnd(); ++it) {
            if (it.value() > best) { best = it.value(); step = it.key(); }
        }

        int missing = 0, breaks = 0, dup = 0;
        if (step >= 1) {
            for (int i = 1; i < r.frames.size(); ++i) {
                const uint8_t d = static_cast<uint8_t>(
                    r.frames[i].ahrsAa5.index - r.frames[i - 1].ahrsAa5.index);
                if (d == 0) { ++dup; continue; }
                const int n = d / step;     // 该间隔应有的帧数
                if (n > 1) { missing += n - 1; ++breaks; }
            }
        }

        int crcErr = 0;
        for (const auto &f : r.frames)
            if (!f.crcOk) ++crcErr;

        r.infoText = QStringLiteral("帧:%1 序号步长:%2 丢帧:%3 中断:%4 CRC错:%5")
                         .arg(r.frames.size()).arg(step).arg(missing)
                         .arg(breaks).arg(crcErr);
        return r;
    }

    QString csvHeader() const override
    {
        return QStringLiteral(
            "累计序号,帧序号,Roll(deg),Pitch(deg),Yaw(deg),"
            "RateX(deg/s),RateY(deg/s),RateZ(deg/s),"
            "AccX(g),AccY(g),AccZ(g),"
            "校验A(收),校验A(算),校验B(收),校验B(算),校验结果\n");
    }

    QString formatFrame(const ParsedFrame &f) const override
    {
        const auto &v = f.ahrsAa5;
        return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16\n")
            .arg(v.globalIndex)
            .arg(v.index)
            .arg(v.roll, 0, 'f', 2).arg(v.pitch, 0, 'f', 2).arg(v.yaw, 0, 'f', 2)
            .arg(v.rate[0], 0, 'f', 2).arg(v.rate[1], 0, 'f', 2).arg(v.rate[2], 0, 'f', 2)
            .arg(v.accel[0], 0, 'f', 3).arg(v.accel[1], 0, 'f', 3).arg(v.accel[2], 0, 'f', 3)
            .arg(v.checksumA, 4, 16, QChar('0'))
            .arg(v.checksumACalc, 4, 16, QChar('0'))
            .arg(v.checksumB, 2, 16, QChar('0'))
            .arg(v.checksumBCalc, 2, 16, QChar('0'))
            .arg(f.crcOk ? QStringLiteral("OK") : QStringLiteral("错"));
    }
};

#endif // AHRSA5PARSER_H
