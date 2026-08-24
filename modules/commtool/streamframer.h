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

    // 喂入一段新数据，返回本次解析出的完整帧（CRC 校验通过）
    QList<ParsedFrame> feed(const QByteArray &data);

    // 清空内部缓冲（断开连接/清空时调用）
    void reset();

private:
    QByteArray m_buffer;
};

#endif // STREAMFRAMER_H
