#ifndef RTCMPARSER_H
#define RTCMPARSER_H

#include "modules/parse/parser/core/protocoldefs.h"
#include <QByteArray>
#include <QList>

// ════════════════════════════════════════════════════════════════
//  RTCM 3.x 帧解析器
//  帧格式: 0xD3 + [6bit reserved + 10bit length] + data + CRC24Q(3B)
// ════════════════════════════════════════════════════════════════
class RtcmParser
{
public:
    RtcmParser() = default;

    // 从二进制数据中扫描所有 RTCM 帧
    // skipCrc=true: 跳过 CRC-24Q 校验（基站使用非标准 CRC 时）
    QList<RtcmFrame> parseAll(const QByteArray &data, bool skipCrc = false) const;

    // 查找下一个 RTCM 帧头 (0xD3)，返回偏移或 -1
    static int findNextFrame(const QByteArray &data, int startPos = 0);

    // 校验一帧是否完整
    static bool isFrameComplete(const uint8_t *p, int availableBytes);

    // 解析一条 RTCM 帧
    static RtcmFrame parseOneFrame(const uint8_t *p, int availableBytes, int fileOffset);

    // 从 payload 前 2 字节提取 12-bit 消息类型
    static uint16_t extractMessageType(const uint8_t *payload);

    // 解析 RTCM header 中的 10-bit 消息长度
    static uint16_t extractPayloadLength(const uint8_t *header);

    // 格式化单帧
    static QString formatFrame(const RtcmFrame &f);
    static QString formatSummary(const QList<RtcmFrame> &frames);
};

#endif // RTCMPARSER_H
