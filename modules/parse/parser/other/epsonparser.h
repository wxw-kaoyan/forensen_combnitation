#ifndef EPSONPARSER_H
#define EPSONPARSER_H

#include "modules/parse/parser/parserbase.h"
#include <QHash>
#include <cstring>

// ════════════════════════════════════════════════════════════════
//  爱普生 IMU 串口协议解析
//  依据《爱普生协议.docx》固件源码，并用实测数据（M-G 系列 .dat）校准：
//
//  固定 38 字节帧：
//    [0]      SYNC1 = 0x80
//    [1]      SYNC2 = 0x01
//    [2..33]  数据区 32B（小端 int16）：
//             [0..1]   计数器/时间戳（实测每帧 +625，u16 回绕）
//             [2..3]   状态字（实测恒为 3）
//             [4..9]   陀螺 X/Y/Z ÷64        (deg/s)
//             [10..15] 加表 X/Y/Z ÷1000      (g)
//             [16..17] 俯仰 pitch ÷100       (deg)
//             [18..19] 横滚 roll  ÷100       (deg)
//             [20..21] 航向 yaw   ÷100       (deg)
//             [22..23] 温度 ÷100             (°C)
//             [24..31] 四元数 W/X/Y/Z ÷30000
//    [34..35] 校验和 = 数据区 32B 累加和(uint16)，**低字节在前**
//    [36..37] END1 = 0x0D, END2 = 0x0A
//
//  注：文档固件把 status 当温度、且 pitch/roll/yaw 顺序与实测不符，
//      此处以实测为准（航向才可能出现 ±180°，横滚不可能）。
// ════════════════════════════════════════════════════════════════

namespace epson_detail {

constexpr int      kFrameLen = 38;   // 帧总长
constexpr int      kDataLen  = 32;   // 数据区（含计数器+状态）
constexpr uint8_t  kSync1    = 0x80;
constexpr uint8_t  kSync2    = 0x01;
constexpr uint8_t  kEnd1     = 0x0D;
constexpr uint8_t  kEnd2     = 0x0A;

// 计数器时基：实测每帧 +625，对应 100Hz 输出的 10ms，
// 即 1 tick = 16µs（625 tick = 10ms，65536 tick ≈ 1.0486s 回绕）
constexpr double   kCounterTickUs = 16.0;

inline uint16_t checksum32(const uint8_t *p)
{
    uint16_t s = 0;
    for (int i = 0; i < kDataLen; ++i)
        s = static_cast<uint16_t>(s + p[i]);
    return s;
}

inline uint16_t recvChecksum(const uint8_t *frame)  // 低字节在前
{
    return static_cast<uint16_t>(frame[34] | (frame[35] << 8));
}

inline EpsonImuData decode(const uint8_t *d)  // d 指向数据区 32B
{
    EpsonImuData v;
    std::memset(&v, 0, sizeof(v));
    auto u16 = [](const uint8_t *p) -> uint16_t {
        return static_cast<uint16_t>(p[0] | (p[1] << 8));
    };
    auto i16 = [&u16](const uint8_t *p) -> int16_t {
        return static_cast<int16_t>(u16(p));
    };

    v.counter     = u16(d);
    v.status      = i16(d + 2);
    v.gyro[0]     = i16(d + 4)  / 64.0f;
    v.gyro[1]     = i16(d + 6)  / 64.0f;
    v.gyro[2]     = i16(d + 8)  / 64.0f;
    v.accel[0]    = i16(d + 10) / 1000.0f;
    v.accel[1]    = i16(d + 12) / 1000.0f;
    v.accel[2]    = i16(d + 14) / 1000.0f;
    v.pitch       = i16(d + 16) / 100.0f;
    v.roll        = i16(d + 18) / 100.0f;
    v.yaw         = i16(d + 20) / 100.0f;
    v.temperature = i16(d + 22) / 100.0f;
    v.quat[0]     = i16(d + 24) / 30000.0f;
    v.quat[1]     = i16(d + 26) / 30000.0f;
    v.quat[2]     = i16(d + 28) / 30000.0f;
    v.quat[3]     = i16(d + 30) / 30000.0f;
    return v;
}

inline ParsedFrame makeFrame(const QByteArray &raw, int pos)
{
    const uint8_t *d = reinterpret_cast<const uint8_t *>(raw.constData()) + pos;
    ParsedFrame f;
    std::memset(&f, 0, sizeof(f));
    f.protocol      = ProtocolID::Epson_Imu;
    f.frameId       = (static_cast<uint16_t>(kSync1) << 8) | kSync2;
    f.payloadLength = kFrameLen;
    f.crc32         = recvChecksum(d);
    f.crc32Expected = checksum32(d + 2);
    f.crcOk         = (f.crc32 == f.crc32Expected);
    f.offsetInFile  = pos;
    f.epsonImu      = decode(d + 2);
    f.epsonImu.checksum     = f.crc32;
    f.epsonImu.checksumCalc = f.crc32Expected;
    return f;
}

// 主路径：同步字 + 帧尾 + 累加和校验，命中后按 38B 步进
inline QList<ParsedFrame> scanSync(const QByteArray &raw, bool skipCrc, int &crcFail)
{
    QList<ParsedFrame> out;
    crcFail = 0;
    const uint8_t *d = reinterpret_cast<const uint8_t *>(raw.constData());
    const int n = raw.size();
    int i = 0;
    while (i + kFrameLen <= n) {
        if (d[i] != kSync1 || d[i + 1] != kSync2
            || d[i + 36] != kEnd1 || d[i + 37] != kEnd2) {
            ++i;
            continue;
        }
        ParsedFrame f = makeFrame(raw, i);
        if (!f.crcOk && !skipCrc) {
            ++crcFail;      // 校验失败：丢弃该帧，前移 1 字节重新同步
            ++i;
            continue;
        }
        out.append(f);
        i += kFrameLen;     // 连续流：下一帧紧跟 38B
    }
    return out;
}

} // namespace epson_detail

class EpsonParser : public ParserBase
{
public:
    QString buttonLabel() const override { return QStringLiteral("爱普生IMU"); }
    QString category() const override { return QStringLiteral("爱普生"); }
    ProtocolID protocolId() const override { return ProtocolID::Epson_Imu; }

    QList<ParsedFrame> parse(const QByteArray &rawData) override
    {
        int crcFail = 0;
        QList<ParsedFrame> frames = epson_detail::scanSync(rawData, skipCrc(), crcFail);

        // 计数器是 16 位时间戳（16µs/tick），按回绕累积成从 0 开始的相对时间
        uint32_t ticks = 0;
        uint16_t prev = 0;
        for (int i = 0; i < frames.size(); ++i) {
            const uint16_t c = frames[i].epsonImu.counter;
            if (i > 0)
                ticks += static_cast<uint16_t>(c - prev);
            prev = c;
            frames[i].epsonImu.timeSec =
                ticks * epson_detail::kCounterTickUs * 1e-6;
        }
        return frames;
    }

    // 在默认输出基础上追加计数器连续性统计（丢帧检测）
    ParseResult parseToResult(const QByteArray &rawData,
                              const QString &filePath) override
    {
        ParseResult r = ParserBase::parseToResult(rawData, filePath);
        if (r.frames.size() < 2)
            return r;

        // 统计相邻帧计数增量，取出现最多的作为正常步长
        QHash<uint16_t, int> hist;
        for (int i = 1; i < r.frames.size(); ++i) {
            const uint16_t d = static_cast<uint16_t>(
                r.frames[i].epsonImu.counter - r.frames[i - 1].epsonImu.counter);
            ++hist[d];
        }
        uint16_t step = 0;
        int best = -1;
        for (auto it = hist.constBegin(); it != hist.constEnd(); ++it) {
            if (it.value() > best) { best = it.value(); step = it.key(); }
        }

        int missing = 0, breaks = 0, dup = 0;
        if (step > 1) {
            for (int i = 1; i < r.frames.size(); ++i) {
                const uint16_t d = static_cast<uint16_t>(
                    r.frames[i].epsonImu.counter - r.frames[i - 1].epsonImu.counter);
                if (d == 0) { ++dup; continue; }
                const int n = d / step;          // 该间隔应有的帧数
                if (n <= 0) continue;
                const int miss = n - 1;
                if (miss > 0) { missing += miss; ++breaks; }
            }
        }

        int crcErr = 0;
        for (const auto &f : r.frames)
            if (!f.crcOk) ++crcErr;

        r.infoText = QStringLiteral("帧:%1 计数步长:%2(每帧%3ms) 推算帧率:%4Hz 丢帧:%5 中断:%6 重复:%7 CRC错:%8")
                         .arg(r.frames.size()).arg(step)
                         .arg(step * epson_detail::kCounterTickUs / 1000.0, 0, 'f', 2)
                         .arg(step > 0 ? 1e6 / (step * epson_detail::kCounterTickUs) : 0.0,
                              0, 'f', 2)
                         .arg(missing).arg(breaks).arg(dup).arg(crcErr);
        return r;
    }

    QString csvHeader() const override
    {
        return QStringLiteral(
            "计数器,状态,温度(C),"
            "陀螺X(deg/s),陀螺Y(deg/s),陀螺Z(deg/s),"
            "加表X(g),加表Y(g),加表Z(g),"
            "Pitch(deg),Yaw(deg),Roll(deg),"
            "QW,QX,QY,QZ,"
            "校验和(收),校验和(算),校验结果,时间(s)\n");
    }

    QString formatFrame(const ParsedFrame &f) const override
    {
        const auto &v = f.epsonImu;
        return QStringLiteral(
            "%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20\n")
            .arg(v.counter).arg(v.status)
            .arg(v.temperature, 0, 'f', 2)
            .arg(v.gyro[0], 0, 'f', 3).arg(v.gyro[1], 0, 'f', 3).arg(v.gyro[2], 0, 'f', 3)
            .arg(v.accel[0], 0, 'f', 4).arg(v.accel[1], 0, 'f', 4).arg(v.accel[2], 0, 'f', 4)
            .arg(v.pitch, 0, 'f', 2).arg(v.yaw, 0, 'f', 2).arg(v.roll, 0, 'f', 2)
            .arg(v.quat[0], 0, 'f', 5).arg(v.quat[1], 0, 'f', 5)
            .arg(v.quat[2], 0, 'f', 5).arg(v.quat[3], 0, 'f', 5)
            .arg(v.checksum, 4, 16, QChar('0'))
            .arg(v.checksumCalc, 4, 16, QChar('0'))
            .arg(f.crcOk ? QStringLiteral("OK") : QStringLiteral("错"))
            .arg(v.timeSec, 0, 'f', 4);
    }
};

#endif // EPSONPARSER_H
