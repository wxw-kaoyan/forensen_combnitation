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
    StreamFramer() = default;

    // 喂入一段新数据，返回本次解析出的完整帧。
    // skipCrc = true 时不做 CRC 校验（与离线解析默认行为一致）
    QList<ParsedFrame> feed(const QByteArray &data, bool skipCrc = true);

    // 清空内部缓冲与统计（断开连接/切换模式时调用）
    void reset();

    // 诊断统计：找到的完整帧数 / 因 CRC 被丢弃的帧数
    qint64 framesFound() const { return m_found; }
    qint64 framesCrcRejected() const { return m_crcRejected; }

private:
    QByteArray m_buffer;
    qint64 m_found = 0;
    qint64 m_crcRejected = 0;
};

#endif // STREAMFRAMER_H
