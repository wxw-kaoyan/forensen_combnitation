#ifndef AA55ENCODER_H
#define AA55ENCODER_H

#include <QByteArray>
#include <cstdint>

// 本工程 AA55 帧编码器：AA 55 + ID + len + payload + CRC32(初值=1)
class Aa55Encoder
{
public:
    static QByteArray pack(uint16_t id, const QByteArray &payload);

private:
    static uint32_t crc32(const QByteArray &data);
    static void appendU16(QByteArray &b, uint16_t v);
    static void appendU32(QByteArray &b, uint32_t v);
};

#endif // AA55ENCODER_H
