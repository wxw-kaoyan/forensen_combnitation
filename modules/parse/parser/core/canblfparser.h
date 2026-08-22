#ifndef CANBLFPARSER_H
#define CANBLFPARSER_H

#include "modules/parse/parser/core/protocoldefs.h"
#include <QByteArray>
#include <QList>

// ════════════════════════════════════════════════════════════════
//  BLF 文件解析器 —— 提取 CAN 0x1CEBFF9A 帧 → 解码为 CanRtkData
//  使用 Qt qUncompress 处理 zlib 压缩容器
// ════════════════════════════════════════════════════════════════

class CanBlfParser
{
public:
    CanBlfParser() = default;

    QList<ParsedFrame> parseBlfFile(const QByteArray &rawData,
                                     int &totalCanFrames,
                                     int &validGroups);

    static CanRtkData decodeRtkData(const uint8_t data[40]);

private:
    static constexpr uint32_t TARGET_CAN_ID_RAW = 0x9CEBFF9A;
    static constexpr int BLF_HEADER_SIZE = 144;
    static constexpr int FRAMES_PER_GROUP = 5;

    struct CanFrame { uint8_t data[8]; };

    QList<CanFrame> scanCanFrames(const QByteArray &decompressed);
    QList<QByteArray> groupCanFrames(const QList<CanFrame> &frames, int &validGroupCount);
};

#endif // CANBLFPARSER_H
