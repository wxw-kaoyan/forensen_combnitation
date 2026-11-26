#include "modules/commtool/streamframer.h"
#include "modules/parse/parser/core/frameparser.h"
#include "modules/parse/parser/other/epsonparser.h"
#include "modules/parse/parser/other/ahrsaa5parser.h"
#include "modules/parse/parser/other/novatelparser.h"
#include "modules/parse/parser/other/im19parser.h"

#include <cstring>

// 单帧最大长度保护：当前最大 payload 约 208B（614E-B KF），
// 头(6) + payload + CRC(4) 远小于 4096。超限视为误同步，跳过一个字节。
static constexpr int kMaxFrameLen = 4096;

StreamFramer::FramingMode StreamFramer::modeForProtocol(ProtocolID pid)
{
    switch (pid) {
    case ProtocolID::Epson_Imu:        return FramingMode::Epson;
    case ProtocolID::Ahrs_Aa5:         return FramingMode::AhrsAa5;
    case ProtocolID::Novatel_BestPos:
    case ProtocolID::Novatel_PsrVel:   return FramingMode::Novatel;
    case ProtocolID::Im19_Mems:
    case ProtocolID::Im19_Gnss:
    case ProtocolID::Im19_Navi:
    case ProtocolID::Im19_Nasc:        return FramingMode::Im19;
    default:                           return FramingMode::Aa55;
    }
}

void StreamFramer::setFramingMode(FramingMode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    reset();
}

void StreamFramer::reset()
{
    m_buffer.clear();
    m_found = 0;
    m_crcRejected = 0;
    m_epsonPrev = 0;
    m_epsonTicks = 0;
    m_epsonHasPrev = false;
}

QList<ParsedFrame> StreamFramer::feed(const QByteArray &data, bool skipCrc)
{
    switch (m_mode) {
    case FramingMode::Epson:    return feedEpson(data, skipCrc);
    case FramingMode::AhrsAa5:  return feedAhrsAa5(data, skipCrc);
    case FramingMode::Novatel:  return feedNovatel(data, skipCrc);
    case FramingMode::Im19:     return feedIm19(data, skipCrc);
    default:                    return feedAa55(data, skipCrc);
    }
}

// ════════════════════════════════════════════════════════════════
//  AA55 标准帧
// ════════════════════════════════════════════════════════════════
QList<ParsedFrame> StreamFramer::feedAa55(const QByteArray &data, bool skipCrc)
{
    m_buffer.append(data);
    QList<ParsedFrame> frames;

    while (true) {
        const int pos = FrameParser::findNextFrame(m_buffer, 0);
        if (pos < 0) {
            // 无帧头：仅保留末尾 1 字节，可能是 AA 的一半
            if (m_buffer.size() > 1)
                m_buffer = m_buffer.right(1);
            break;
        }
        if (pos > 0)
            m_buffer.remove(0, pos); // 丢弃帧头前的噪声

        const uint8_t *p = reinterpret_cast<const uint8_t *>(m_buffer.constData());
        const int avail = m_buffer.size();

        if (!FrameParser::isFrameComplete(p, avail))
            break; // 半包，等待更多数据

        const uint16_t frameId   = static_cast<uint16_t>(p[2]) | (static_cast<uint16_t>(p[3]) << 8);
        const uint16_t payloadLen = static_cast<uint16_t>(p[4]) | (static_cast<uint16_t>(p[5]) << 8);
        const int total = kHeaderSize + payloadLen + kCrcSize;

        if (total > kMaxFrameLen) {
            m_buffer.remove(0, 1); // 误同步，跳过 AA，重新找帧头
            continue;
        }

        ++m_found;

        // CRC32 校验（覆盖帧头 + payload，与 FrameParser::parseAll 一致）
        // 离线解析默认跳过校验（crcCheckBox 默认不勾），实时保持一致，避免
        // 因 CRC 算法/初值差异导致"离线能解析、实时一个帧都出不来"
        bool crcOk = true;
        uint32_t crcExpected = 0;
        uint32_t crcReceived = 0;
        if (!skipCrc) {
            crcExpected = FrameParser::computeCrc32(p, kHeaderSize + payloadLen);
            crcReceived =
                static_cast<uint32_t>(p[total - 4]) |
                (static_cast<uint32_t>(p[total - 3]) << 8) |
                (static_cast<uint32_t>(p[total - 2]) << 16) |
                (static_cast<uint32_t>(p[total - 1]) << 24);
            crcOk = (crcReceived == crcExpected);
            if (!crcOk) {
                // 兼容：部分固件帧尾写 16 位字节累加和（高 16 位为 0）
                const uint32_t sum = FrameParser::computeSum16(p, kHeaderSize + payloadLen);
                if (crcReceived == sum) {
                    crcOk = true;
                    crcExpected = sum;
                }
            }
        }
        if (!crcOk)
            ++m_crcRejected;

        if (crcOk) {
            ProtocolID pid = resolveProtocolID(frameId, payloadLen);
            ParsedFrame frame = FrameParser::parsePayload(pid, p + kHeaderSize, payloadLen, 0);
            frame.protocol      = pid;
            frame.frameId       = frameId;
            frame.payloadLength = payloadLen;
            frame.crc32         = crcReceived;
            frame.crc32Expected = crcExpected;
            frame.crcOk         = crcOk;
            frame.offsetInFile  = 0;
            frames.append(frame);
        }

        m_buffer.remove(0, total);
    }

    return frames;
}

// ════════════════════════════════════════════════════════════════
//  爱普生 IMU（38B 定长：80 01 + 32B 数据 + 累加和 + 0D 0A）
// ════════════════════════════════════════════════════════════════
QList<ParsedFrame> StreamFramer::feedEpson(const QByteArray &data, bool skipCrc)
{
    m_buffer.append(data);
    QList<ParsedFrame> frames;

    int pos = 0;
    while (pos + 2 <= m_buffer.size()) {
        const uint8_t *b = reinterpret_cast<const uint8_t *>(m_buffer.constData());
        if (b[pos] != epson_detail::kSync1 || b[pos + 1] != epson_detail::kSync2) {
            ++pos;
            continue;
        }
        if (m_buffer.size() - pos < epson_detail::kFrameLen)
            break; // 半包
        const int end = pos + epson_detail::kFrameLen;
        if (b[end - 2] != epson_detail::kEnd1 || b[end - 1] != epson_detail::kEnd2) {
            ++pos;
            continue;
        }
        ParsedFrame f = epson_detail::makeFrame(m_buffer, pos);
        if (!f.crcOk && !skipCrc) {
            ++m_crcRejected;
            ++pos;
            continue;
        }
        // 计数器回绕累积 → 连续相对时间（与离线 EpsonParser::parse 一致）
        const uint16_t c = f.epsonImu.counter;
        if (m_epsonHasPrev)
            m_epsonTicks += static_cast<uint16_t>(c - m_epsonPrev);
        m_epsonPrev = c;
        m_epsonHasPrev = true;
        f.epsonImu.timeSec = m_epsonTicks * epson_detail::kCounterTickUs * 1e-6;
        ++m_found;
        frames.append(f);
        pos = end;
    }
    if (pos > 0)
        m_buffer.remove(0, pos);
    return frames;
}

// ════════════════════════════════════════════════════════════════
//  AHRS 标准协议（26B 定长：AA A5 + … + 校验 + 0D 0A）
// ════════════════════════════════════════════════════════════════
QList<ParsedFrame> StreamFramer::feedAhrsAa5(const QByteArray &data, bool skipCrc)
{
    m_buffer.append(data);
    QList<ParsedFrame> frames;

    int pos = 0;
    while (pos + 2 <= m_buffer.size()) {
        const uint8_t *b = reinterpret_cast<const uint8_t *>(m_buffer.constData());
        if (b[pos] != ahrs_aa5_detail::kSync1 || b[pos + 1] != ahrs_aa5_detail::kSync2) {
            ++pos;
            continue;
        }
        if (m_buffer.size() - pos < ahrs_aa5_detail::kFrameLen)
            break; // 半包
        const int end = pos + ahrs_aa5_detail::kFrameLen;
        if (b[end - 2] != ahrs_aa5_detail::kEnd1 || b[end - 1] != ahrs_aa5_detail::kEnd2) {
            ++pos;
            continue;
        }
        ParsedFrame f = ahrs_aa5_detail::makeFrame(m_buffer, pos);
        if (!f.crcOk && !skipCrc) {
            ++m_crcRejected;
            ++pos;
            continue;
        }
        ++m_found;
        frames.append(f);
        pos = end;
    }
    if (pos > 0)
        m_buffer.remove(0, pos);
    return frames;
}

// ════════════════════════════════════════════════════════════════
//  诺瓦泰 OEM（AA 44 12 1C + 28B 头 + payload + CRC32）
//  同一条流里可能混有其它报文（MsgID 不同）与 NMEA 文本，
//  只把 BESTPOSB(42) / PSRVELB(100) 转成 ParsedFrame，其余按长度跳过。
// ════════════════════════════════════════════════════════════════
QList<ParsedFrame> StreamFramer::feedNovatel(const QByteArray &data, bool skipCrc)
{
    m_buffer.append(data);
    QList<ParsedFrame> frames;

    constexpr int kHdr = 28;
    int pos = 0;
    while (pos + 4 <= m_buffer.size()) {
        const uint8_t *b = reinterpret_cast<const uint8_t *>(m_buffer.constData());
        if (!(b[pos] == 0xAA && b[pos + 1] == 0x44 && b[pos + 2] == 0x12
              && b[pos + 3] == kHdr)) {
            ++pos;
            continue;
        }
        if (m_buffer.size() - pos < kHdr)
            break; // 头不完整，等待后续数据
        const int len = static_cast<int>(novatel_detail::u16le(b + pos + 8));
        if (b[pos + 6] != 0 || len > 8192) { // msgType 必须为二进制、长度不超限
            ++pos;
            continue;
        }
        if (m_buffer.size() - pos < kHdr + len + 4)
            break; // 半包
        const int frameLen = kHdr + len + 4;

        if (!skipCrc
            && novatel_detail::crc32(b + pos, kHdr + len)
                   != novatel_detail::u32le(b + pos + kHdr + len)) {
            ++m_crcRejected;
            pos += frameLen;
            continue;
        }
        ++m_found;

        const uint16_t msgId = novatel_detail::u16le(b + pos + 4);
        if ((msgId == 42 && len >= 72) || (msgId == 100 && len >= 40)) {
            novatel_detail::RawFrame rf;
            rf.gpsWeek   = novatel_detail::u16le(b + pos + 14);
            rf.gpsWeekMs = novatel_detail::u32le(b + pos + 16);
            rf.payload   = m_buffer.mid(pos + kHdr, len);

            ParsedFrame f;
            std::memset(&f, 0, sizeof(f));
            f.frameId       = msgId;
            f.payloadLength = static_cast<uint16_t>(len);
            f.crcOk         = true;
            f.offsetInFile   = 0;
            if (msgId == 42) {
                f.protocol = ProtocolID::Novatel_BestPos;
                f.novatelBestPos = decodeNovatelBestPos(rf);
            } else {
                f.protocol = ProtocolID::Novatel_PsrVel;
                f.novatelPsrVel = decodeNovatelPsrVel(rf);
            }
            frames.append(f);
        }
        pos += frameLen;
    }
    if (pos > 0)
        m_buffer.remove(0, pos);
    return frames;
}

// ════════════════════════════════════════════════════════════════
//  IM19 惯性模组（'fmi' + 类型字符，定长 52/95/100B，累加和校验）
//  MEMS / GNSS / NAVI 常在同一串口混发，按类型字符取帧长分别切出；
//  若开了字符输出，流里还会有 $GPFMI 文本行，一并按行切出。
//  面板只显示当前选中的协议，其余帧由 submitFrame 过滤掉，仅计数。
// ════════════════════════════════════════════════════════════════
QList<ParsedFrame> StreamFramer::feedIm19(const QByteArray &data, bool skipCrc)
{
    m_buffer.append(data);
    QList<ParsedFrame> frames;

    int pos = 0;
    while (pos < m_buffer.size()) {
        const uint8_t *b = reinterpret_cast<const uint8_t *>(m_buffer.constData());
        const int avail = m_buffer.size() - pos;

        // ── $GPFMI 文本行 ──
        if (b[pos] == '$') {
            const int lineEnd = m_buffer.indexOf('\n', pos);
            if (lineEnd < 0) {
                if (avail > 512) {   // 异常长行，当作噪声丢掉一个字节
                    ++pos;
                    continue;
                }
                break;               // 半行，等后续数据
            }
            const QByteArray line = m_buffer.mid(pos, lineEnd - pos + 1);
            pos = lineEnd + 1;
            if (!line.startsWith("$GPFMI"))
                continue;            // 其它 NMEA 语句直接跳过

            int crcFail = 0;
            const QList<ParsedFrame> fs = im19_detail::scanNasc(line, skipCrc, crcFail);
            m_crcRejected += crcFail;
            if (fs.isEmpty())
                continue;
            ++m_found;
            frames.append(fs);
            continue;
        }

        // ── 二进制帧 ──
        if (avail < 4)
            break;                   // 帧头都不够，等后续
        if (b[pos] != im19_detail::kH0 || b[pos + 1] != im19_detail::kH1
            || b[pos + 2] != im19_detail::kH2) {
            ++pos;
            continue;
        }
        const int len = im19_detail::frameLenFor(b[pos + 3]);
        if (len == 0) {              // 未知类型字符，重新同步
            ++pos;
            continue;
        }
        if (avail < len)
            break;                   // 半包
        if (b[pos + len - 2] != im19_detail::kTail0
            || b[pos + len - 1] != im19_detail::kTail1) {
            ++pos;                   // 帧尾不符，误同步
            continue;
        }

        ParsedFrame f = im19_detail::makeFrame(m_buffer, pos, b[pos + 3]);
        if (!f.crcOk && !skipCrc) {
            ++m_crcRejected;
            ++pos;
            continue;
        }
        ++m_found;
        frames.append(f);
        pos += len;
    }

    if (pos > 0)
        m_buffer.remove(0, pos);
    return frames;
}
