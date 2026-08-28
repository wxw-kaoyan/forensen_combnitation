#include "modules/simulator/encoders/aa55encoder.h"

// 与 FrameParser::computeCrc32 一致：IEEE 802.3，初值=1，最终不取反
uint32_t Aa55Encoder::crc32(const QByteArray &data)
{
    uint32_t crc = 1;
    for (char c : data) {
        crc ^= static_cast<uint8_t>(c);
        for (int i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (0xEDB88320u & (~(crc & 1u) + 1u));
    }
    return crc;
}

void Aa55Encoder::appendU16(QByteArray &b, uint16_t v)
{
    b.append(static_cast<char>(v & 0xFF));
    b.append(static_cast<char>((v >> 8) & 0xFF));
}

void Aa55Encoder::appendU32(QByteArray &b, uint32_t v)
{
    b.append(static_cast<char>(v & 0xFF));
    b.append(static_cast<char>((v >> 8) & 0xFF));
    b.append(static_cast<char>((v >> 16) & 0xFF));
    b.append(static_cast<char>((v >> 24) & 0xFF));
}

QByteArray Aa55Encoder::pack(uint16_t id, const QByteArray &payload)
{
    QByteArray frame;
    frame.append(static_cast<char>(0xAA));
    frame.append(static_cast<char>(0x55));
    appendU16(frame, id);
    appendU16(frame, static_cast<uint16_t>(payload.size()));
    frame.append(payload);

    const uint32_t crc = crc32(frame);
    appendU32(frame, crc);
    return frame;
}
