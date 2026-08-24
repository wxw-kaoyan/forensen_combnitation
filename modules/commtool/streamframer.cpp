#include "modules/commtool/streamframer.h"
#include "modules/parse/parser/core/frameparser.h"

// 单帧最大长度保护：当前最大 payload 约 208B（614E-B KF），
// 头(6) + payload + CRC(4) 远小于 4096。超限视为误同步，跳过一个字节。
static constexpr int kMaxFrameLen = 4096;

void StreamFramer::reset()
{
    m_buffer.clear();
}

QList<ParsedFrame> StreamFramer::feed(const QByteArray &data)
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

        // CRC32 校验（覆盖帧头 + payload，与 FrameParser::parseAll 一致）
        const uint32_t crcExpected = FrameParser::computeCrc32(p, kHeaderSize + payloadLen);
        const uint32_t crcReceived =
            static_cast<uint32_t>(p[total - 4]) |
            (static_cast<uint32_t>(p[total - 3]) << 8) |
            (static_cast<uint32_t>(p[total - 2]) << 16) |
            (static_cast<uint32_t>(p[total - 1]) << 24);
        const bool crcOk = (crcReceived == crcExpected);

        // 仅输出 CRC 校验通过的帧，过滤流中的噪声/误同步
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
