#ifndef NOVATELENCODER_H
#define NOVATELENCODER_H

#include <QByteArray>
#include <cstdint>

#include "modules/simulator/simclock.h"

// NovAtel OEM 二进制日志编码器：AA 44 12 头 + 28 字节 header + payload + CRC32
class NovatelEncoder
{
public:
    static QByteArray pack(uint16_t msgId, const QByteArray &payload, const SimClock &clock);

private:
    static uint32_t crc32(const QByteArray &data);
    static void appendU16(QByteArray &b, uint16_t v);
    static void appendU32(QByteArray &b, uint32_t v);
};

#endif // NOVATELENCODER_H
