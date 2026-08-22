#include "modules/parse/parser/core/canblfparser.h"
#include <cstring>

// ==================== 辅助 ====================

static inline uint32_t readU32LE(const uint8_t *p) {
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

// ==================== zlib 解压 (Qt 内置) ====================
//
// BLF 中的 zlib 数据: [zlib header:2][deflate][Adler32:4]
// qUncompress 需要:  [uncomp_len:4 BE][zlib data 完整]
// qCompress 输出也是 zlib 格式，所以只需在前面补 4B 预估长度即可

static QByteArray decompressZlib(const QByteArray &zlibData)
{
    if (zlibData.size() < 6) return {};
    // 在前面补 4B 预估解压后长度（qUncompress 按实际大小裁剪）
    QByteArray qtInput;
    qtInput.resize(4);
    quint32 estimated = static_cast<quint32>(zlibData.size()) * 20;
    qtInput[0] = static_cast<char>((estimated >> 24) & 0xFF);
    qtInput[1] = static_cast<char>((estimated >> 16) & 0xFF);
    qtInput[2] = static_cast<char>((estimated >> 8)  & 0xFF);
    qtInput[3] = static_cast<char>( estimated        & 0xFF);
    qtInput.append(zlibData);   // 完整 zlib 数据，不砍头尾
    return qUncompress(qtInput);
}

// ==================== 扫描解压后的 LOBJ 对象 ====================

QList<CanBlfParser::CanFrame> CanBlfParser::scanCanFrames(const QByteArray &decompressed)
{
    QList<CanFrame> result;
    const uint8_t *data = reinterpret_cast<const uint8_t *>(decompressed.constData());
    int size = decompressed.size();
    int offset = 0;

    while (offset + 48 <= size) {
        if (data[offset] != 'L' || data[offset+1] != 'O' ||
            data[offset+2] != 'B' || data[offset+3] != 'J') {
            ++offset; continue;
        }
        uint16_t hdrSize = (static_cast<uint16_t>(data[offset+4]))
                         | (static_cast<uint16_t>(data[offset+5]) << 8);
        uint32_t objSize = readU32LE(data + offset + 8);
        if (objSize < hdrSize || offset + static_cast<int>(objSize) > size) {
            ++offset; continue;
        }
        int payloadOff = offset + hdrSize;
        int payloadLen = static_cast<int>(objSize) - hdrSize;
        if (payloadLen >= 16) {
            uint32_t canId = readU32LE(data + payloadOff + 4);
            if (canId == TARGET_CAN_ID_RAW) {
                CanFrame frame;
                std::memcpy(frame.data, data + payloadOff + 8, 8);
                result.append(frame);
            }
        }
        offset += static_cast<int>(objSize);
    }
    return result;
}

// ==================== 按帧序号分组 ====================
// 重复帧号直接覆盖旧帧（不丢失已有的其他帧），凑齐5帧即输出一组

QList<QByteArray> CanBlfParser::groupCanFrames(const QList<CanFrame> &frames, int &validGroupCount)
{
    QList<QByteArray> groups;
    validGroupCount = 0;
    CanFrame currentGroup[5];
    bool hasFrame[5] = {false, false, false, false, false};
    int collectedCount = 0;

    for (const auto &frame : frames) {
        int idx = frame.data[0];
        if (idx < 1 || idx > 5) continue;
        int slot = idx - 1;
        if (!hasFrame[slot]) ++collectedCount;
        std::memcpy(currentGroup[slot].data, frame.data, 8);
        hasFrame[slot] = true;
        if (collectedCount == FRAMES_PER_GROUP) {
            QByteArray merged(40, '\0');
            for (int i = 0; i < 5; ++i)
                std::memcpy(merged.data() + i * 8, currentGroup[i].data, 8);
            groups.append(merged); ++validGroupCount;
            std::memset(hasFrame, 0, sizeof(hasFrame));
            collectedCount = 0;
        }
    }
    return groups;
}

// ==================== 解码 RTK ====================

CanRtkData CanBlfParser::decodeRtkData(const uint8_t data[40])
{
    CanRtkData d;
    std::memset(&d, 0, sizeof(d));
    d.frameIdx1 = data[0]; d.frameIdx2 = data[8]; d.frameIdx3 = data[16];
    d.frameIdx4 = data[24]; d.frameIdx5 = data[32];

    d.utcDays = (static_cast<uint16_t>(data[1]) << 8) | data[2];
    uint32_t secRaw = (static_cast<uint32_t>(data[3]) << 24)
                    | (static_cast<uint32_t>(data[4]) << 16)
                    | (static_cast<uint32_t>(data[5]) << 8) | data[6];
    d.secondsOfDay = secRaw / 10000.0;
    d.fixType = data[7]; d.diffAge = data[9]; d.numSatellites = data[23];

    d.latRaw = (static_cast<uint64_t>(data[10]) << 40) | (static_cast<uint64_t>(data[11]) << 32)
             | (static_cast<uint64_t>(data[12]) << 24) | (static_cast<uint64_t>(data[13]) << 16)
             | (static_cast<uint64_t>(data[14]) << 8)  | data[15];
    d.latitude = d.latRaw / 1e11 - 90.0;

    d.lonRaw = (static_cast<uint64_t>(data[17]) << 40) | (static_cast<uint64_t>(data[18]) << 32)
             | (static_cast<uint64_t>(data[19]) << 24) | (static_cast<uint64_t>(data[20]) << 16)
             | (static_cast<uint64_t>(data[21]) << 8)  | data[22];
    d.longitude = d.lonRaw / 1e11 - 180.0;

    d.altRaw = (static_cast<uint32_t>(data[25]) << 24) | (static_cast<uint32_t>(data[26]) << 16)
             | (static_cast<uint32_t>(data[27]) << 8)  | data[28];
    d.altitude = d.altRaw / 10000.0 - 1000.0;

    d.velRaw = (static_cast<uint16_t>(data[29]) << 8) | data[30];
    d.forwardSpeed = d.velRaw / 100.0 - 320.0;

    d.sdRaw = (static_cast<uint16_t>(data[31]) << 8) | data[33];
    d.speedDirection = (d.sdRaw != 0xFFFF) ? (d.sdRaw / 100.0) : 0.0;

    d.rollRaw = (static_cast<uint16_t>(data[34]) << 8) | data[35];
    d.rollValid = (d.rollRaw != 0xFFFF);
    d.roll = d.rollValid ? (d.rollRaw / 100.0 - 180.0) : 0.0;

    d.pitchRaw = (static_cast<uint16_t>(data[36]) << 8) | data[37];
    d.pitchValid = (d.pitchRaw != 0xFFFF);
    d.pitch = d.pitchValid ? (d.pitchRaw / 100.0 - 180.0) : 0.0;

    d.yawRaw = (static_cast<uint16_t>(data[38]) << 8) | data[39];
    d.yawValid = (d.yawRaw != 0xFFFF);
    d.yaw = d.yawValid ? (d.yawRaw / 100.0) : 0.0;

    return d;
}

// ==================== 主解析入口 ====================

QList<ParsedFrame> CanBlfParser::parseBlfFile(const QByteArray &rawData,
                                              int &totalCanFrames,
                                              int &validGroups)
{
    QList<ParsedFrame> result;
    totalCanFrames = 0; validGroups = 0;
    if (rawData.size() < BLF_HEADER_SIZE + 32) return result;

    const uint8_t *data = reinterpret_cast<const uint8_t *>(rawData.constData());
    int offset = BLF_HEADER_SIZE;

    // 第一步：遍历所有 zlib 容器，收集全部 CAN 帧
    QList<CanFrame> allCanFrames;
    while (offset + 32 <= rawData.size()) {
        if (data[offset] != 'L' || data[offset+1] != 'O' ||
            data[offset+2] != 'B' || data[offset+3] != 'J') {
            ++offset; continue;
        }
        uint16_t hdrSize = (static_cast<uint16_t>(data[offset+4]))
                         | (static_cast<uint16_t>(data[offset+5]) << 8);
        uint32_t objSize = readU32LE(data + offset + 8);
        uint32_t objType = readU32LE(data + offset + 12);
        if (objSize < hdrSize || offset + static_cast<int>(objSize) > rawData.size())
            break;

        if (objType == 0x0A && objSize > hdrSize + 16) {
            int zlibStart = offset + hdrSize + 16;
            int zlibLen = static_cast<int>(objSize) - hdrSize - 16;
            if (zlibStart + zlibLen <= rawData.size()) {
                QByteArray zlibBlock(reinterpret_cast<const char *>(data + zlibStart), zlibLen);
                QByteArray decompressed = decompressZlib(zlibBlock);
                if (!decompressed.isEmpty()) {
                    allCanFrames.append(scanCanFrames(decompressed));
                }
            }
        }
        offset += static_cast<int>(objSize);
    }

    // 第二步：把所有帧汇合后一次性分组（避免跨容器边界丢帧）
    totalCanFrames = allCanFrames.size();
    QList<QByteArray> groups = groupCanFrames(allCanFrames, validGroups);
    for (int i = 0; i < groups.size(); ++i) {
        ParsedFrame f;
        std::memset(&f, 0, sizeof(f));
        f.protocol = ProtocolID::Can_RTK;
        f.frameId = 0xCD01; f.payloadLength = 40;
        f.crcOk = true; f.offsetInFile = result.size();
        f.canRtk = decodeRtkData(
            reinterpret_cast<const uint8_t *>(groups[i].constData()));
        result.append(f);
    }
    return result;
}
