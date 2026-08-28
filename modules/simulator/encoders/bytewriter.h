#ifndef BYTEWRITER_H
#define BYTEWRITER_H

#include <QByteArray>
#include <cstdint>
#include <cstring>

// 小端字节写入辅助（NovAtel / AA55 payload 打包用）
namespace ByteWriter {

inline void appendU8(QByteArray &b, uint8_t v)
{
    b.append(static_cast<char>(v));
}

inline void appendU16(QByteArray &b, uint16_t v)
{
    b.append(static_cast<char>(v & 0xFF));
    b.append(static_cast<char>((v >> 8) & 0xFF));
}

inline void appendU32(QByteArray &b, uint32_t v)
{
    b.append(static_cast<char>(v & 0xFF));
    b.append(static_cast<char>((v >> 8) & 0xFF));
    b.append(static_cast<char>((v >> 16) & 0xFF));
    b.append(static_cast<char>((v >> 24) & 0xFF));
}

inline void appendI32(QByteArray &b, int32_t v)
{
    appendU32(b, static_cast<uint32_t>(v));
}

inline void appendFloat(QByteArray &b, float v)
{
    uint32_t u;
    std::memcpy(&u, &v, 4);
    appendU32(b, u);
}

inline void appendDouble(QByteArray &b, double v)
{
    uint64_t u;
    std::memcpy(&u, &v, 8);
    b.append(static_cast<char>(u & 0xFF));
    b.append(static_cast<char>((u >> 8) & 0xFF));
    b.append(static_cast<char>((u >> 16) & 0xFF));
    b.append(static_cast<char>((u >> 24) & 0xFF));
    b.append(static_cast<char>((u >> 32) & 0xFF));
    b.append(static_cast<char>((u >> 40) & 0xFF));
    b.append(static_cast<char>((u >> 48) & 0xFF));
    b.append(static_cast<char>((u >> 56) & 0xFF));
}

} // namespace ByteWriter

#endif // BYTEWRITER_H
