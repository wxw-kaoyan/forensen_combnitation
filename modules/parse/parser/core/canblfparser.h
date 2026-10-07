#ifndef CANBLFPARSER_H
#define CANBLFPARSER_H

#include "modules/parse/parser/core/protocoldefs.h"
#include <QByteArray>
#include <QList>

// ════════════════════════════════════════════════════════════════
//  CAN 日志解析器 —— 支持两种存储：
//    1) BLF 二进制 (LOGG 头 + zlib 压缩的 LOBJ 容器)
//    2) 文本日志 (每行: 时间,时间,0xID,XX XX XX ...)
//  自动识别；提取 CAN 帧 → 解码为各协议结构体
//  支持: 0x1CEBFF9A(联试_CAN981) / 0x1CFF9B9A(NAVDATA_联适) / 0x1FFFFF9A(IMULOG_联适)
// ════════════════════════════════════════════════════════════════

class CanBlfParser
{
public:
    CanBlfParser() = default;

    // 联试_CAN981 (0x1CEBFF9A, 5×8B 组一历元)
    QList<ParsedFrame> parseBlfFile(const QByteArray &rawData,
                                     int &totalCanFrames,
                                     int &validGroups);

    // NAVDATA_联适 (0x1CFF9B9A, 每帧即一条报文，按包号分为 IMU/GNSS/参数 三组)
    QList<ParsedFrame> parseLsNav(const QByteArray &rawData,
                                   int &totalCanFrames,
                                   int &validGroups);

    // IMULOG_联适 (0x1FFFFF9A, 6×8B 组一条内部状态记录)
    QList<ParsedFrame> parseLsImuLog(const QByteArray &rawData,
                                      int &totalCanFrames,
                                      int &validGroups);

    static CanRtkData decodeRtkData(const uint8_t data[40]);

private:
    static constexpr uint32_t TARGET_CAN_ID_RAW    = 0x9CEBFF9A;  // 0x1CEBFF9A (BLF 带扩展标志)
    static constexpr uint32_t LS_NAV_CAN_ID_RAW    = 0x9CFF9B9A;  // 0x1CFF9B9A
    static constexpr uint32_t LS_IMULOG_CAN_ID_RAW = 0x9FFFFF9A;  // 0x1FFFFF9A
    static constexpr uint32_t TARGET_CAN_ID        = 0x1CEBFF9A;  // 文本日志里的裸 ID
    static constexpr uint32_t LS_NAV_CAN_ID        = 0x1CFF9B9A;
    static constexpr uint32_t LS_IMULOG_CAN_ID     = 0x1FFFFF9A;
    static constexpr int BLF_HEADER_SIZE = 144;
    static constexpr int FRAMES_PER_GROUP = 5;

    struct CanFrame { uint8_t data[8]; };

    // 收集目标 CAN 帧：自动识别 BLF / 文本日志
    QList<CanFrame> collectFrames(const QByteArray &rawData, uint32_t blfRawId,
                                  uint32_t plainId) const;

    // 收集指定 CAN ID 的全部帧（自动遍历 zlib 容器）
    QList<CanFrame> collectCanFrames(const QByteArray &rawData, uint32_t targetRawId) const;

    // 文本 CAN 日志：每行 "时间,时间,0xID,b1 b2 ... b8"
    static QList<CanFrame> collectTextLogFrames(const QByteArray &rawData, uint32_t plainId);

    // 按首字节包号分组（凑齐 framesPerGroup 帧输出一组）
    static QList<QByteArray> groupCanFrames(const QList<CanFrame> &frames, int framesPerGroup,
                                            int &validGroupCount);

    static QList<CanFrame> scanCanFrames(const QByteArray &decompressed, uint32_t targetRawId);

    static LsNavImuData   decodeLsNavImu(const CanFrame p[3]);
    static LsNavGpsData   decodeLsNavGps(const CanFrame p[8]);
    static LsNavParamData decodeLsNavParam(const CanFrame p[3]);
    static LsImuLogData   decodeLsImuLog(const uint8_t data[48]);
};

#endif // CANBLFPARSER_H
