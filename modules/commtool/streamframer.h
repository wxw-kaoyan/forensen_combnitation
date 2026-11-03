#ifndef STREAMFRAMER_H
#define STREAMFRAMER_H

#include "modules/parse/parser/core/protocoldefs.h"
#include <QByteArray>
#include <QList>

// ════════════════════════════════════════════════════════════════
//  流式帧解析器 — 增量缓冲 + 帧同步 + 半包缓存 + CRC 校验
//  复用 FrameParser 的静态函数，处理串口/TCP 实时流中的粘包/半包。
//  与 FrameParser::parseAll 的离线批处理（会丢弃半包）不同，
//  本类保留未完成的半包，等待下一段数据到达后继续拼接。
// ════════════════════════════════════════════════════════════════
class StreamFramer
{
public:
    // 切帧格式：AA55 标准帧 + 三种非 AA55 串口协议
    enum class FramingMode {
        Aa55,      // AA 55 | ID | Len | payload | CRC32
        Epson,     // 80 01 | 32B 数据 | 累加和 | 0D 0A   (38B 定长)
        AhrsAa5,   // AA A5 | Index | 9×int16 | 校验 | 0D 0A (26B 定长)
        Novatel,   // AA 44 12 1C | ... | CRC32            (28B 头 + payload + 4)
    };

    StreamFramer() = default;

    // 喂入一段新数据，返回本次解析出的完整帧。
    // skipCrc = true 时不做 CRC 校验（与离线解析默认行为一致）
    QList<ParsedFrame> feed(const QByteArray &data, bool skipCrc = true);

    // 切换切帧格式（格式变化时清空缓冲，避免半包串格式）
    void setFramingMode(FramingMode mode);
    FramingMode framingMode() const { return m_mode; }

    // 协议 → 切帧格式（非 AA55 协议返回对应格式，其余一律 AA55）
    static FramingMode modeForProtocol(ProtocolID pid);

    // 清空内部缓冲与统计（断开连接/切换模式时调用）
    void reset();

    // 诊断统计：找到的完整帧数 / 因 CRC 被丢弃的帧数
    qint64 framesFound() const { return m_found; }
    qint64 framesCrcRejected() const { return m_crcRejected; }

private:
    QList<ParsedFrame> feedAa55(const QByteArray &data, bool skipCrc);
    QList<ParsedFrame> feedEpson(const QByteArray &data, bool skipCrc);
    QList<ParsedFrame> feedAhrsAa5(const QByteArray &data, bool skipCrc);
    QList<ParsedFrame> feedNovatel(const QByteArray &data, bool skipCrc);

    QByteArray m_buffer;
    FramingMode m_mode = FramingMode::Aa55;
    qint64 m_found = 0;
    qint64 m_crcRejected = 0;

    // 爱普生计数器回绕累积（16µs/tick）：离线在 EpsonParser::parse 里累积，
    // 实时走 makeFrame 不经过 parse，这里补上，否则实时"时间(s)"列恒为 0
    uint16_t m_epsonPrev = 0;
    uint32_t m_epsonTicks = 0;
    bool     m_epsonHasPrev = false;
};

#endif // STREAMFRAMER_H
