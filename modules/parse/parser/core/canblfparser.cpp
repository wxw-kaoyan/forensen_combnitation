#include "modules/parse/parser/core/canblfparser.h"
#include <QByteArrayView>
#include <cstring>

// ==================== 辅助 ====================

static inline uint32_t readU32LE(const uint8_t *p) {
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

// 大端读取（联适各帧多字节均为大端）
static inline uint16_t readU16BE(const uint8_t *p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}
static inline int16_t readI16BE(const uint8_t *p) {
    return static_cast<int16_t>(readU16BE(p));
}
static inline uint32_t readU32BE(const uint8_t *p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16)
         | (static_cast<uint32_t>(p[2]) << 8)  | static_cast<uint32_t>(p[3]);
}
static inline int32_t readI32BE(const uint8_t *p) {
    return static_cast<int32_t>(readU32BE(p));
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

QList<CanBlfParser::CanFrame> CanBlfParser::scanCanFrames(const QByteArray &decompressed,
                                                          uint32_t targetRawId)
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
            if (canId == targetRawId) {
                CanFrame frame;
                std::memcpy(frame.data, data + payloadOff + 8, 8);
                result.append(frame);
            }
        }
        offset += static_cast<int>(objSize);
    }
    return result;
}

// ==================== 遍历全部 zlib 容器，收集指定 ID 的 CAN 帧 ====================

QList<CanBlfParser::CanFrame> CanBlfParser::collectCanFrames(const QByteArray &rawData,
                                                             uint32_t targetRawId) const
{
    QList<CanFrame> all;
    const uint8_t *data = reinterpret_cast<const uint8_t *>(rawData.constData());
    int offset = BLF_HEADER_SIZE;

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
                if (!decompressed.isEmpty())
                    all.append(scanCanFrames(decompressed, targetRawId));
            }
        }
        offset += static_cast<int>(objSize);
    }
    return all;
}

// ==================== 文本 CAN 日志 ====================
// 每行: "09:16:40:892,09:16:40:154,0x1fffff9a,01 03 00 26 60 04 00 00"
// 第 3 个逗号字段为 CAN ID（含 0x 前缀），其后为数据字节（空格/逗号分隔均可）

QList<CanBlfParser::CanFrame> CanBlfParser::collectTextLogFrames(const QByteArray &rawData,
                                                                 uint32_t plainId)
{
    QList<CanFrame> result;
    const QByteArray idHex = QByteArray::number(static_cast<qulonglong>(plainId), 16); // 小写十六进制
    const char *base = rawData.constData();
    const int n = rawData.size();
    int pos = 0;

    while (pos < n) {
        int nl = rawData.indexOf('\n', pos);
        const int end = (nl < 0) ? n : nl;

        // 行内查找目标 ID —— 用 QByteArrayView 避免逐行分配内存
        const QByteArrayView lineView(base + pos, end - pos);
        if (lineView.indexOf(idHex) >= 0) {
            QByteArray line(base + pos, end - pos);
            if (line.endsWith('\r')) line.chop(1);
            const QList<QByteArray> parts = line.split(',');
            if (parts.size() >= 4) {
                QByteArray hexAll;
                for (int i = 3; i < parts.size(); ++i) { hexAll += parts[i]; hexAll += ' '; }
                const QList<QByteArray> toks = hexAll.simplified().split(' ');
                if (toks.size() >= 8) {
                    CanFrame f;
                    bool ok = true;
                    for (int i = 0; i < 8; ++i) {
                        const int v = toks[i].toInt(&ok, 16);
                        if (!ok) { ok = false; break; }
                        f.data[i] = static_cast<uint8_t>(v);
                    }
                    if (ok) result.append(f);
                }
            }
        }
        if (nl < 0) break;
        pos = nl + 1;
    }
    return result;
}

// ==================== 自动识别 BLF / 文本日志 ====================

QList<CanBlfParser::CanFrame> CanBlfParser::collectFrames(const QByteArray &rawData,
                                                          uint32_t blfRawId,
                                                          uint32_t plainId) const
{
    // BLF 文件头签名为 "LOGG"
    if (rawData.size() >= 4 && rawData[0] == 'L' && rawData[1] == 'O'
        && rawData[2] == 'G' && rawData[3] == 'G')
        return collectCanFrames(rawData, blfRawId);
    return collectTextLogFrames(rawData, plainId);
}

// ==================== 按帧序号分组 ====================
// 重复帧号直接覆盖旧帧（不丢失已有的其他帧），凑齐 framesPerGroup 帧即输出一组
// framesPerGroup 最多支持 8 帧（每帧 8 字节 → 最长 64 字节）

QList<QByteArray> CanBlfParser::groupCanFrames(const QList<CanFrame> &frames, int framesPerGroup,
                                               int &validGroupCount)
{
    QList<QByteArray> groups;
    validGroupCount = 0;
    if (framesPerGroup < 1 || framesPerGroup > 8) return groups;

    CanFrame currentGroup[8];
    bool hasFrame[8] = {false};
    int collectedCount = 0;

    for (const auto &frame : frames) {
        int idx = frame.data[0];
        if (idx < 1 || idx > framesPerGroup) continue;
        int slot = idx - 1;
        if (!hasFrame[slot]) ++collectedCount;
        std::memcpy(currentGroup[slot].data, frame.data, 8);
        hasFrame[slot] = true;
        if (collectedCount == framesPerGroup) {
            QByteArray merged(framesPerGroup * 8, '\0');
            for (int i = 0; i < framesPerGroup; ++i)
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
    if (rawData.isEmpty()) return result;

    // 第一步：收集目标 CAN 帧（BLF 或文本日志，自动识别）
    QList<CanFrame> allCanFrames = collectFrames(rawData, TARGET_CAN_ID_RAW, TARGET_CAN_ID);

    // 第二步：把所有帧汇合后一次性分组（避免跨容器边界丢帧）
    totalCanFrames = allCanFrames.size();
    QList<QByteArray> groups = groupCanFrames(allCanFrames, FRAMES_PER_GROUP, validGroups);
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

// ==================== 联适 NAVDATA (0x1CFF9B9A) ====================
// 每帧即一条独立报文，按包号分为三组：
//   包1~3  IMU 原始 (100Hz) / 包4~11 GNSS+导航 (20Hz) / 包12~14 参数 (0.1Hz)

LsNavImuData CanBlfParser::decodeLsNavImu(const CanFrame p[3])
{
    LsNavImuData d;
    std::memset(&d, 0, sizeof(d));
    const uint8_t *b1 = p[0].data;   // 包1
    const uint8_t *b2 = p[1].data;   // 包2
    const uint8_t *b3 = p[2].data;   // 包3

    d.timeGpsMs = readU32BE(b1 + 1);
    const int16_t a0 = readI16BE(b1 + 5);
    const int16_t a1 = static_cast<int16_t>((static_cast<uint16_t>(b1[7]) << 8) | b2[1]);
    const int16_t a2 = readI16BE(b2 + 2);
    const int16_t g0 = readI16BE(b2 + 4);
    const int16_t g1 = readI16BE(b2 + 6);
    const int16_t g2 = readI16BE(b3 + 1);
    d.weekNumImu = readU16BE(b3 + 3);
    const int16_t temp = readI16BE(b3 + 5);
    d.kfInitDone = b3[7];

    d.accel[0] = a0 / 3276.8;  d.accel[1] = a1 / 3276.8;  d.accel[2] = a2 / 3276.8;
    d.gyro[0]  = g0 / 52.0127; d.gyro[1]  = g1 / 52.0127; d.gyro[2]  = g2 / 52.0127;
    d.temperature = temp / 10.0;
    return d;
}

LsNavGpsData CanBlfParser::decodeLsNavGps(const CanFrame p[8])
{
    LsNavGpsData d;
    std::memset(&d, 0, sizeof(d));
    const uint8_t *b4  = p[0].data;   // 包4
    const uint8_t *b5  = p[1].data;
    const uint8_t *b6  = p[2].data;
    const uint8_t *b7  = p[3].data;
    const uint8_t *b8  = p[4].data;
    const uint8_t *b9  = p[5].data;
    const uint8_t *b10 = p[6].data;
    const uint8_t *b11 = p[7].data;   // 包11

    d.itow       = readU32BE(b4 + 1);
    d.weekNumGps = readU16BE(b4 + 5);
    d.fixType    = b4[7];

    d.revcTimeMs = readU32BE(b5 + 1);
    d.numSV      = b5[5];
    d.multiSVs   = b5[6];

    const int32_t lat = static_cast<int32_t>(
        (static_cast<uint32_t>(b5[7]) << 24) | (static_cast<uint32_t>(b6[1]) << 16)
      | (static_cast<uint32_t>(b6[2]) << 8)  | static_cast<uint32_t>(b6[3]));
    const int32_t lon = readI32BE(b6 + 4);
    const int32_t alt = readI32BE(b7 + 1);
    const int32_t vn  = static_cast<int32_t>(
        (static_cast<uint32_t>(b7[5]) << 24) | (static_cast<uint32_t>(b7[6]) << 16)
      | (static_cast<uint32_t>(b7[7]) << 8)  | static_cast<uint32_t>(b8[1]));
    const int32_t ve  = readI32BE(b8 + 2);
    const int32_t vd  = static_cast<int32_t>(
        (static_cast<uint32_t>(b8[6]) << 24) | (static_cast<uint32_t>(b8[7]) << 16)
      | (static_cast<uint32_t>(b9[1]) << 8)  | static_cast<uint32_t>(b9[2]));
    const int32_t gsp = readI32BE(b9 + 3);
    const int32_t hd  = static_cast<int32_t>(
        (static_cast<uint32_t>(b9[7]) << 24) | (static_cast<uint32_t>(b10[1]) << 16)
      | (static_cast<uint32_t>(b10[2]) << 8) | static_cast<uint32_t>(b10[3]));

    d.lat = lat / 1e7;  d.lon = lon / 1e7;  d.alt = alt / 1000.0;
    d.vn  = vn / 1000.0; d.ve = ve / 1000.0; d.vd = vd / 1000.0;
    d.gspeed = gsp / 1000.0; d.headMot = hd / 1000.0;

    d.pHacc      = readI16BE(b10 + 4);
    d.pVacc      = readI16BE(b10 + 6);
    d.vHacc      = readI16BE(b11 + 1);
    d.llExtend   = readI16BE(b11 + 3);
    d.diffAge    = readU16BE(b11 + 5);
    d.kfIndexGet = b11[7];
    return d;
}

LsNavParamData CanBlfParser::decodeLsNavParam(const CanFrame p[3])
{
    LsNavParamData d;
    std::memset(&d, 0, sizeof(d));
    const uint8_t *b12 = p[0].data;   // 包12
    const uint8_t *b13 = p[1].data;
    const uint8_t *b14 = p[2].data;   // 包14

    d.gpsPosX = readI16BE(b12 + 1);
    d.gpsPosY = readI16BE(b12 + 3);
    d.gpsPosZ = readI16BE(b12 + 5);
    d.rtkFlag = b12[7];
    d.odPosX  = readI16BE(b13 + 1);
    d.odPosY  = readI16BE(b13 + 3);
    d.odPosZ  = readI16BE(b13 + 5);
    d.version = (static_cast<uint32_t>(b13[7]) << 24) | (static_cast<uint32_t>(b14[1]) << 16)
              | (static_cast<uint32_t>(b14[2]) << 8)  | static_cast<uint32_t>(b14[3]);
    d.resetReason = readU32BE(b14 + 4);
    return d;
}

QList<ParsedFrame> CanBlfParser::parseLsNav(const QByteArray &rawData,
                                            int &totalCanFrames,
                                            int &validGroups)
{
    QList<ParsedFrame> result;
    totalCanFrames = 0; validGroups = 0;
    if (rawData.isEmpty()) return result;

    const QList<CanFrame> frames = collectFrames(rawData, LS_NAV_CAN_ID_RAW, LS_NAV_CAN_ID);
    totalCanFrames = frames.size();

    CanFrame imuBuf[3];  bool imuHas[3] = {false, false, false};  int imuN = 0;
    CanFrame gpsBuf[8];  bool gpsHas[8] = {false};                int gpsN = 0;
    CanFrame parBuf[3];  bool parHas[3] = {false, false, false};  int parN = 0;

    auto emitFrame = [&result](ProtocolID pid, uint16_t frameId, uint16_t payLen) {
        ParsedFrame f;
        std::memset(&f, 0, sizeof(f));
        f.protocol = pid;
        f.frameId = frameId;
        f.payloadLength = payLen;
        f.crcOk = true;
        f.offsetInFile = result.size();
        return f;
    };

    for (const auto &fr : frames) {
        const int idx = fr.data[0];
        if (idx >= 1 && idx <= 3) {
            const int s = idx - 1;
            if (!imuHas[s]) { imuHas[s] = true; ++imuN; }
            imuBuf[s] = fr;
            if (imuN == 3) {
                ParsedFrame f = emitFrame(ProtocolID::LsNav_Imu, 0xCD03, 24);
                f.lsNavImu = decodeLsNavImu(imuBuf);
                result.append(f);
                std::memset(imuHas, 0, sizeof(imuHas)); imuN = 0;
                ++validGroups;
            }
        } else if (idx >= 4 && idx <= 11) {
            const int s = idx - 4;
            if (!gpsHas[s]) { gpsHas[s] = true; ++gpsN; }
            gpsBuf[s] = fr;
            if (gpsN == 8) {
                ParsedFrame f = emitFrame(ProtocolID::LsNav_Gps, 0xCD02, 64);
                f.lsNavGps = decodeLsNavGps(gpsBuf);
                result.append(f);
                std::memset(gpsHas, 0, sizeof(gpsHas)); gpsN = 0;
                ++validGroups;
            }
        } else if (idx >= 12 && idx <= 14) {
            const int s = idx - 12;
            if (!parHas[s]) { parHas[s] = true; ++parN; }
            parBuf[s] = fr;
            if (parN == 3) {
                ParsedFrame f = emitFrame(ProtocolID::LsNav_Param, 0xCD04, 24);
                f.lsNavParam = decodeLsNavParam(parBuf);
                result.append(f);
                std::memset(parHas, 0, sizeof(parHas)); parN = 0;
                ++validGroups;
            }
        }
    }
    return result;
}

// ==================== 联适 IMULOG (0x1FFFFF9A, 6×8B) ====================

LsImuLogData CanBlfParser::decodeLsImuLog(const uint8_t data[48])
{
    LsImuLogData d;
    std::memset(&d, 0, sizeof(d));
    const uint8_t *q1 = data;        // 包1
    const uint8_t *q2 = data + 8;
    const uint8_t *q3 = data + 16;
    const uint8_t *q4 = data + 24;
    const uint8_t *q5 = data + 32;
    const uint8_t *q6 = data + 40;   // 包6

    d.initStatus   = q1[1];
    d.filterSelect = q1[2];
    d.initHeading  = readU16BE(q1 + 3) / 100.0;
    d.deviceStatus = q1[6];
    d.betposGapCnt = q1[7];

    d.psrvelGapCnt = q2[1];
    d.zuptCnt      = (static_cast<uint32_t>(q2[3]) << 24) | (static_cast<uint32_t>(q2[4]) << 16)
                   | (static_cast<uint32_t>(q2[5]) << 8)  | static_cast<uint32_t>(q2[6]);
    d.staticFlag   = q2[7];

    d.straightCnt    = (static_cast<uint32_t>(q3[1]) << 24) | (static_cast<uint32_t>(q3[2]) << 16)
                     | (static_cast<uint32_t>(q3[3]) << 8)  | static_cast<uint32_t>(q3[4]);
    d.yawInstallBias = readI16BE(q3 + 5) / 100.0;
    d.betposCnt      = q3[7];

    d.gyroZBias = static_cast<double>(readI32BE(q4 + 1));
    d.delayMs   = readU16BE(q4 + 5);
    d.psrvelCnt = q4[7];

    d.imuStaticCnt = (static_cast<uint32_t>(q5[1]) << 24) | (static_cast<uint32_t>(q5[2]) << 16)
                   | (static_cast<uint32_t>(q5[3]) << 8)  | static_cast<uint32_t>(q5[4]);
    d.filterState  = q5[5];
    d.resetFlag    = q5[6];
    d.pitchInstallBias = static_cast<int16_t>(
        (static_cast<uint16_t>(q5[7]) << 8) | q6[1]) / 100.0;

    d.resN = readI16BE(q6 + 2) / 100.0;
    d.resE = readI16BE(q6 + 4) / 100.0;
    d.resU = readI16BE(q6 + 6) / 100.0;
    return d;
}

QList<ParsedFrame> CanBlfParser::parseLsImuLog(const QByteArray &rawData,
                                               int &totalCanFrames,
                                               int &validGroups)
{
    QList<ParsedFrame> result;
    totalCanFrames = 0; validGroups = 0;
    if (rawData.isEmpty()) return result;

    const QList<CanFrame> frames = collectFrames(rawData, LS_IMULOG_CAN_ID_RAW, LS_IMULOG_CAN_ID);
    totalCanFrames = frames.size();

    const QList<QByteArray> groups = groupCanFrames(frames, 6, validGroups);
    for (const auto &g : groups) {
        ParsedFrame f;
        std::memset(&f, 0, sizeof(f));
        f.protocol = ProtocolID::LsImuLog;
        f.frameId = 0xCD05; f.payloadLength = 48;
        f.crcOk = true; f.offsetInFile = result.size();
        f.lsImuLog = decodeLsImuLog(reinterpret_cast<const uint8_t *>(g.constData()));
        result.append(f);
    }
    return result;
}
