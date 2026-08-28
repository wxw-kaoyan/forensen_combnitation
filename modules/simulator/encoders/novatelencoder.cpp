#include "modules/simulator/encoders/novatelencoder.h"

// NovAtel 32-bit CRC（初值 0，无最终取反；与 rtklib rtk_crc32 一致）
uint32_t NovatelEncoder::crc32(const QByteArray &data)
{
    uint32_t crc = 0;
    for (char c : data) {
        crc ^= static_cast<uint8_t>(c);
        for (int i = 0; i < 8; ++i)
            crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
    }
    return crc;
}

void NovatelEncoder::appendU16(QByteArray &b, uint16_t v)
{
    b.append(static_cast<char>(v & 0xFF));
    b.append(static_cast<char>((v >> 8) & 0xFF));
}

void NovatelEncoder::appendU32(QByteArray &b, uint32_t v)
{
    b.append(static_cast<char>(v & 0xFF));
    b.append(static_cast<char>((v >> 8) & 0xFF));
    b.append(static_cast<char>((v >> 16) & 0xFF));
    b.append(static_cast<char>((v >> 24) & 0xFF));
}

QByteArray NovatelEncoder::pack(uint16_t msgId, const QByteArray &payload, const SimClock &clock)
{
    QByteArray hdr;
    hdr.append(static_cast<char>(0xAA));
    hdr.append(static_cast<char>(0x44));
    hdr.append(static_cast<char>(0x12));   // binary 同步头
    hdr.append(static_cast<char>(28));     // header 长度
    appendU16(hdr, msgId);                 // message ID
    hdr.append(static_cast<char>(0));      // message type = 0 (binary)
    hdr.append(static_cast<char>(32));     // port address（模拟用 COM2）
    appendU16(hdr, static_cast<uint16_t>(payload.size())); // message length
    appendU16(hdr, 0);                     // sequence
    hdr.append(static_cast<char>(0));      // idle time
    hdr.append(static_cast<char>(160));    // time status = FINESTEERING
    appendU16(hdr, clock.gpsWeek);
    appendU32(hdr, clock.gpsWeekMs);
    appendU32(hdr, 0);                     // receiver status
    appendU16(hdr, 0);                     // reserved
    appendU16(hdr, 0);                     // receiver software version

    QByteArray frame = hdr + payload;
    const uint32_t crc = crc32(frame);

    QByteArray out = frame;
    appendU32(out, crc);
    return out;
}
