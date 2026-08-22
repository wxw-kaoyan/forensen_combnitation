#ifndef FRAMEPARSER_H
#define FRAMEPARSER_H

#include "modules/parse/parser/core/protocoldefs.h"
#include <QByteArray>
#include <QList>
#include <cstring>

// ════════════════════════════════════════════════════════════════
//  通用帧解析器：同步 → CRC32校验 → 解帧 → 填充结构体
// ════════════════════════════════════════════════════════════════
class FrameParser
{
public:
    FrameParser() = default;

    // 从 QByteArray 中解析所有帧，返回解析结果列表
    // skipCrc=true 时跳过 CRC 校验，帧尾 4 字节作为普通数据
    QList<ParsedFrame> parseAll(const QByteArray &data, bool skipCrc = false) const;

    // 从二进制数据流中查找下一帧头位置
    // 返回 >=0 的偏移，-1 表示未找到
    static int findNextFrame(const QByteArray &data, int startPos = 0);

    // 校验一帧数据是否完整（帧头+ID+len+payload+CRC）
    static bool isFrameComplete(const uint8_t *p, int availableBytes);

    // 计算 CRC32 (初始值=1, 多项式=0xEDB88320, 含帧头 AA 55 + ID + Length + Payload)
    static uint32_t computeCrc32(const uint8_t *data, int len);

    // 累加和校验 (8-bit, 所有字节求和取低 8 位)
    static uint8_t computeChecksum8(const uint8_t *data, int len);

    // 从 payload 解析为具体结构体
    static ParsedFrame parsePayload(ProtocolID pid,
                                   const uint8_t *payload, int payloadLen,
                                   int fileOffset);

private:
    // 各协议 payload 解析函数
    static void parseAhRsBasic(const uint8_t *p, AhRsBasic &out);
    static void parseAhRsExtended(const uint8_t *p, AhRsExtended &out);
    static void parseQuaternion(const uint8_t *p, QuaternionData &out);
    static void parseAGData(const uint8_t *p, AGData &out);
    static void parseFusionResult(const uint8_t *p, FusionResult &out);
    static void parseTiltMeasure(const uint8_t *p, TiltMeasure &out);
    static void parseLaser(const uint8_t *p, LaserData &out);
    static void parseAttitude(const uint8_t *p, AttitudeData &out);
    static void parseQuat614P(const uint8_t *p, Quat614PData &out);
    static void parseDebugImu(const uint8_t *p, DebugImuData &out);
    static void parseDebugGps(const uint8_t *p, DebugGpsData &out);
    static void parseDebugDrtk(const uint8_t *p, DebugDrtkData &out);
    static void parseDebugOd(const uint8_t *p, DebugOdData &out);
    static void parseDebugKf(const uint8_t *p, DebugKfData &out);
    static void parseDebugParam(const uint8_t *p, DebugParamData &out);
    static void parseEb614Imu(const uint8_t *p, Eb614ImuData &out);
    static void parseEb614Gps(const uint8_t *p, Eb614GpsData &out);
    static void parseEb614Kf(const uint8_t *p, Eb614KfData &out);

    static float readFloatLE(const uint8_t *p);
    static double readDoubleLE(const uint8_t *p);
    static uint16_t readU16LE(const uint8_t *p);
    static uint32_t readU32LE(const uint8_t *p);
    static int32_t readI32LE(const uint8_t *p);
    static int16_t readI16LE(const uint8_t *p);
};

#endif // FRAMEPARSER_H
