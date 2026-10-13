#include "modules/parse/parser/core/frameparser.h"

// ==================== CRC32 (IEEE 802.3, init=1, 不含帧头) ====================

uint32_t FrameParser::computeCrc32(const uint8_t *data, int len)
{
    static const uint32_t crcTable[256] = {
        0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
        0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
        0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
        0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
        0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
        0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
        0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
        0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
        0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
        0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
        0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
        0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
        0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
        0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
        0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
        0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
        0xEDB88320,0x9ABFB3B6,0x03B6B20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,
        0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
        0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,
        0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
        0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
        0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
        0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,
        0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB30A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
        0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
        0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
        0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
        0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
        0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,
        0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
        0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,
        0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
    };

    uint32_t crc = 1;  // 初值=1
    for (int i = 0; i < len; ++i) {
        crc = crcTable[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

// ==================== 累加和校验 (8-bit) ====================

uint8_t FrameParser::computeChecksum8(const uint8_t *data, int len)
{
    uint8_t sum = 0;
    for (int i = 0; i < len; ++i)
        sum += data[i];
    return sum;
}

// ==================== 累加和校验 (16-bit，帧尾 4 字节兼容) ====================
// 部分固件不用 CRC32，而是在帧尾 4 字节写 [和低][和高][00][00]，
// 即对 AA55 + ID + len + payload 逐字节求和（低 16 位有效）。

uint32_t FrameParser::computeSum16(const uint8_t *data, int len)
{
    uint32_t sum = 0;
    for (int i = 0; i < len; ++i)
        sum += data[i];
    return sum & 0xFFFFu;
}

// ==================== 帧查找 ====================

int FrameParser::findNextFrame(const QByteArray &data, int startPos)
{
    const int size = data.size();
    for (int i = startPos; i < size - 1; ++i) {
        if (static_cast<uint8_t>(data[i])   == kFrameSync1 &&
            static_cast<uint8_t>(data[i+1]) == kFrameSync2) {
            return i;
        }
    }
    return -1;
}

bool FrameParser::isFrameComplete(const uint8_t *p, int availableBytes)
{
    if (availableBytes < kHeaderSize) return false;
    // 读取 payload 长度 (offset 4-5, little-endian)
    uint16_t payloadLen = static_cast<uint16_t>(p[4]) |
                          (static_cast<uint16_t>(p[5]) << 8);
    int totalLen = kHeaderSize + payloadLen + kCrcSize;
    return availableBytes >= totalLen;
}

// ==================== 批量解析 ====================

QList<ParsedFrame> FrameParser::parseAll(const QByteArray &data, bool skipCrc) const
{
    QList<ParsedFrame> results;
    const int size = data.size();
    int pos = 0;

    while (pos < size - 1) {
        int frameStart = findNextFrame(data, pos);
        if (frameStart < 0) break;

        const uint8_t *p = reinterpret_cast<const uint8_t*>(data.constData()) + frameStart;
        int remaining = size - frameStart;

        if (!isFrameComplete(p, remaining)) {
            pos = frameStart + 1;
            continue;
        }

        // 读取帧信息
        uint16_t frameId   = readU16LE(p + 2);
        uint16_t payloadLen = readU16LE(p + 4);
        const uint8_t *payload = p + kHeaderSize;
        int totalLen = kHeaderSize + payloadLen + kCrcSize;

        // CRC 校验
        uint32_t crcExpected = 0;
        uint32_t crcReceived = 0;
        bool crcOk = true;
        if (!skipCrc) {
            crcExpected = computeCrc32(p, static_cast<int>(kHeaderSize + payloadLen));
            crcReceived = readU32LE(p + kHeaderSize + payloadLen);
            crcOk = (crcReceived == crcExpected);
            if (!crcOk) {
                // 兼容：部分固件帧尾写 16 位字节累加和（高 16 位为 0）
                const uint32_t sum = computeSum16(p, static_cast<int>(kHeaderSize + payloadLen));
                if (crcReceived == sum) {
                    crcOk = true;
                    crcExpected = sum;
                }
            }
        }

        ProtocolID pid = resolveProtocolID(frameId, payloadLen);

        // 解析 payload
        ParsedFrame frame = parsePayload(pid, payload, payloadLen, frameStart);
        frame.protocol       = pid;
        frame.frameId        = frameId;
        frame.payloadLength  = payloadLen;
        frame.crc32          = crcReceived;
        frame.crc32Expected  = crcExpected;
        frame.crcOk          = crcOk;
        frame.offsetInFile   = frameStart;

        results.append(frame);

        pos = frameStart + totalLen;
    }

    return results;
}

ParsedFrame FrameParser::parsePayload(ProtocolID pid,
                                      const uint8_t *payload, int /*payloadLen*/,
                                      int fileOffset)
{
    ParsedFrame f;
    Q_UNUSED(fileOffset);
    std::memset(&f, 0, sizeof(f));  // 清零 union

    switch (pid) {
        case ProtocolID::AHRS_Basic:    parseAhRsBasic(payload, f.ahrsBasic);       break;
        case ProtocolID::AHRS_Extended: parseAhRsExtended(payload, f.ahrsExtended); break;
        case ProtocolID::Quaternion:    parseQuaternion(payload, f.quat);           break;
        case ProtocolID::AG_Data:       parseAGData(payload, f.agData);             break;
        case ProtocolID::FusionResult:  parseFusionResult(payload, f.fusion);       break;
        case ProtocolID::TiltMeasure:   parseTiltMeasure(payload, f.tilt);          break;
        case ProtocolID::Laser:         parseLaser(payload, f.laser);               break;
        case ProtocolID::NanfangLaser:  parseNanfangLaser(payload, f.nanfangLaser); break;
        case ProtocolID::ZhdLaser:      parseZhdLaser(payload, f.zhdLaser);         break;
        case ProtocolID::Attitude:      parseAttitude(payload, f.attitude);         break;
        case ProtocolID::Quat614P:      parseQuat614P(payload, f.quat614p);         break;
        case ProtocolID::Debug_IMU:     parseDebugImu(payload, f.debugImu);         break;
        case ProtocolID::Debug_GPS:     parseDebugGps(payload, f.debugGps);         break;
        case ProtocolID::Debug_DRTK:    parseDebugDrtk(payload, f.debugDrtk);       break;
        case ProtocolID::Debug_OD:      parseDebugOd(payload, f.debugOd);           break;
        case ProtocolID::Debug_KF:      parseDebugKf(payload, f.debugKf);           break;
        case ProtocolID::Debug_Param:   parseDebugParam(payload, f.debugParam);     break;
        case ProtocolID::Eb614_IMU:     parseEb614Imu(payload, f.eb614Imu);         break;
        case ProtocolID::Eb614_GPS:     parseEb614Gps(payload, f.eb614Gps);         break;
        case ProtocolID::Eb614_KF:      parseEb614Kf(payload, f.eb614Kf);           break;
        default: break;
    }
    return f;
}

// ==================== Payload 解析 ====================

void FrameParser::parseAhRsBasic(const uint8_t *p, AhRsBasic &out)
{
    out.timer = readU32LE(p + 0);
    out.pitch = readFloatLE(p + 4);
    out.roll  = readFloatLE(p + 8);
    out.yaw   = readFloatLE(p + 12);
    out.ax    = readFloatLE(p + 16);
    out.ay    = readFloatLE(p + 20);
    out.az    = readFloatLE(p + 24);
    out.gx    = readFloatLE(p + 28);
    out.gy    = readFloatLE(p + 32);
    out.gz    = readFloatLE(p + 36);
    out.temp  = readFloatLE(p + 40);
}

void FrameParser::parseAhRsExtended(const uint8_t *p, AhRsExtended &out)
{
    parseAhRsBasic(p, out.basic);
    out.flag   = p[44];
    out.status = readU32LE(p + 45);
}

void FrameParser::parseQuaternion(const uint8_t *p, QuaternionData &out)
{
    out.timer = readU32LE(p + 0);
    out.qw    = readFloatLE(p + 4);
    out.qx    = readFloatLE(p + 8);
    out.qy    = readFloatLE(p + 12);
    out.qz    = readFloatLE(p + 16);
    out.ax    = readFloatLE(p + 20);
    out.ay    = readFloatLE(p + 24);
    out.az    = readFloatLE(p + 28);
    out.gx    = readFloatLE(p + 32);
    out.gy    = readFloatLE(p + 36);
    out.gz    = readFloatLE(p + 40);
    out.temp  = readFloatLE(p + 44);
    out.flag  = p[48];
    out.status = readU32LE(p + 49);
}

void FrameParser::parseAGData(const uint8_t *p, AGData &out)
{
    out.gpsTime   = readU32LE(p + 0);
    out.roll      = readFloatLE(p + 4);
    out.pitch     = readFloatLE(p + 8);
    out.yaw       = readFloatLE(p + 12);
    out.reserved  = readFloatLE(p + 16);
    out.gyroX     = readFloatLE(p + 20);
    out.gyroY     = readFloatLE(p + 24);
    out.gyroZ     = readFloatLE(p + 28);
    out.accelX    = readFloatLE(p + 32);
    out.accelY    = readFloatLE(p + 36);
    out.accelZ    = readFloatLE(p + 40);
    out.rtkStatus = p[44];
    out.attValid  = p[45];
    out.statusFlag = readU32LE(p + 46);
}

void FrameParser::parseFusionResult(const uint8_t *p, FusionResult &out)
{
    out.gpsWeekSecond = readU32LE(p + 0);
    out.gpsWeek       = readU16LE(p + 4);
    out.lat           = readI32LE(p + 6);
    out.lon           = readI32LE(p + 10);
    out.alt           = readI32LE(p + 14);
    out.velN          = readFloatLE(p + 18);
    out.velE          = readFloatLE(p + 22);
    out.velD          = readFloatLE(p + 26);
    out.roll          = readFloatLE(p + 30);
    out.pitch         = readFloatLE(p + 34);
    out.yaw           = readFloatLE(p + 38);
    out.heading       = readFloatLE(p + 42);
    out.wheelAngle    = readFloatLE(p + 46);
    out.accelX        = readFloatLE(p + 50);
    out.accelY        = readFloatLE(p + 54);
    out.accelZ        = readFloatLE(p + 58);
    out.gyroX         = readFloatLE(p + 62);
    out.gyroY         = readFloatLE(p + 66);
    out.gyroZ         = readFloatLE(p + 70);
    out.temp          = readFloatLE(p + 74);
    out.rtkStatus     = p[78];
    out.numSatellites = p[79];
    out.diffAge       = p[80];
    out.dualAntStatus = p[81];
    out.posStd        = readU16LE(p + 82);
    out.statusFlag    = readU16LE(p + 84);
    out.reserved1     = readU32LE(p + 86);
    out.reserved2     = readU32LE(p + 90);
}

void FrameParser::parseTiltMeasure(const uint8_t *p, TiltMeasure &out)
{
    out.lat       = readDoubleLE(p + 0);
    out.lon       = readDoubleLE(p + 8);
    out.elev      = readDoubleLE(p + 16);
    out.laserBias[0] = readI16LE(p + 24);
    out.laserBias[1] = readI16LE(p + 26);
    out.laserDist = readU16LE(p + 28);
    out.sampleCnt = p[30];
    out.coordFlag = p[31];
    out.temp      = readI16LE(p + 32);
    out.rollStd   = p[34];
    out.pitchStd  = p[35];
    out.tiltRoll  = readFloatLE(p + 36);
    out.tiltPitch = readFloatLE(p + 40);
    out.gyroRaw[0] = readFloatLE(p + 44);
    out.gyroRaw[1] = readFloatLE(p + 48);
    out.gyroRaw[2] = readFloatLE(p + 52);
    out.accelRaw[0] = readFloatLE(p + 56);
    out.accelRaw[1] = readFloatLE(p + 60);
    out.accelRaw[2] = readFloatLE(p + 64);
    out.timestamp = readU32LE(p + 68);
    out.sysStatus = readU32LE(p + 72);
    out.poleHeight = readU16LE(p + 76);
    out.solutionRate = readFloatLE(p + 78);
    out.gnssFix    = p[82];
    out.gnssSatCnt = p[83];
    out.rtkAge     = p[84];
    out.poleBias[0] = readI16LE(p + 85);
    out.poleBias[1] = readI16LE(p + 87);
    out.azimuth   = readFloatLE(p + 89);
    out.rollDeg   = readFloatLE(p + 93);
    out.pitchDeg  = readFloatLE(p + 97);
    out.yawDeg    = readFloatLE(p + 101);
}

void FrameParser::parseLaser(const uint8_t *p, LaserData &out)
{
    out.laserDist    = readFloatLE(p + 0);
    out.laserBias[0] = readFloatLE(p + 4);
    out.laserBias[1] = readFloatLE(p + 8);
    out.targetLat    = readDoubleLE(p + 12);
    out.targetLon    = readDoubleLE(p + 20);
    out.targetElev   = readDoubleLE(p + 28);
    out.poleLat      = readDoubleLE(p + 36);
    out.poleLon      = readDoubleLE(p + 44);
    out.poleElev     = readDoubleLE(p + 52);
    out.solutionType = p[60];
    out.timestamp    = readU32LE(p + 61);
    out.latStd       = p[65];
    out.lonStd       = p[66];
    out.elevStd      = p[67];
    out.tiltAngle    = readFloatLE(p + 72);
    out.checksum     = readU32LE(p + 76);
}

void FrameParser::parseNanfangLaser(const uint8_t *p, NanfangLaserData &out)
{
    out.timestamp       = readU32LE(p + 0);
    out.laserDist       = readFloatLE(p + 4);
    out.roll            = readFloatLE(p + 8);
    out.pitch           = readFloatLE(p + 12);
    out.yaw             = readFloatLE(p + 16);
    out.emitLat         = readDoubleLE(p + 20);
    out.emitLon         = readDoubleLE(p + 28);
    out.emitAlt         = readDoubleLE(p + 36);
    out.centerLat       = readDoubleLE(p + 44);
    out.centerLon       = readDoubleLE(p + 52);
    out.centerAlt       = readDoubleLE(p + 60);
    out.faultCode       = p[68];
    out.precisionFactor = readFloatLE(p + 69);
}

void FrameParser::parseZhdLaser(const uint8_t *p, ZhdLaserData &out)
{
    out.timestamp       = readU32LE(p + 0);
    out.laserDist       = readFloatLE(p + 4);
    out.roll            = readFloatLE(p + 8);
    out.pitch           = readFloatLE(p + 12);
    out.yaw             = readFloatLE(p + 16);
    out.laserLat        = readDoubleLE(p + 20);
    out.laserLon        = readDoubleLE(p + 28);
    out.laserAlt        = readDoubleLE(p + 36);
    out.imuLat          = readDoubleLE(p + 44);
    out.imuLon          = readDoubleLE(p + 52);
    out.imuAlt          = readDoubleLE(p + 60);
    out.laserLatStd     = p[68];
    out.laserLonStd     = p[69];
    out.laserAltStd     = p[70];
    out.rollStd         = p[71];
    out.pitchStd        = p[72];
    out.yawStd          = p[73];
    out.imuLatStd       = p[74];
    out.imuLonStd       = p[75];
    out.imuAltStd       = p[76];
    out.laserError      = p[77];
    out.precisionFactor = readFloatLE(p + 78);
}

void FrameParser::parseAttitude(const uint8_t *p, AttitudeData &out)
{
    out.timestamp    = readU32LE(p + 0);
    out.roll         = readFloatLE(p + 4);
    out.pitch        = readFloatLE(p + 8);
    out.yaw          = readFloatLE(p + 12);
    out.rollStd      = readFloatLE(p + 16);
    out.pitchStd     = readFloatLE(p + 20);
    out.yawStd       = readFloatLE(p + 24);
    out.reserved[0]  = readFloatLE(p + 28);
    out.reserved[1]  = readFloatLE(p + 32);
    out.checksum     = readU32LE(p + 36);
}

void FrameParser::parseQuat614P(const uint8_t *p, Quat614PData &out)
{
    out.gpsWeekMs    = readU32LE(p + 0);
    out.insLat       = readDoubleLE(p + 4);
    out.insLon       = readDoubleLE(p + 12);
    out.insAlt       = readFloatLE(p + 20);
    out.quatW        = readFloatLE(p + 24);
    out.quatX        = readFloatLE(p + 28);
    out.quatY        = readFloatLE(p + 32);
    out.quatZ        = readFloatLE(p + 36);
    out.imuPrecision = readFloatLE(p + 40);
    out.systemStatus = readU16LE(p + 44);
}

// ==================== DEBUG 调试数据流 (0x51~0x56) ====================

void FrameParser::parseDebugImu(const uint8_t *p, DebugImuData &out)
{
    out.time_gps_ms = readU32LE(p + 0);
    out.week_num    = readU16LE(p + 4);
    for (int i = 0; i < 3; ++i) out.gyro[i]  = readFloatLE(p + 6  + i * 4);
    for (int i = 0; i < 3; ++i) out.accel[i] = readFloatLE(p + 18 + i * 4);
    out.temperature = readFloatLE(p + 30);
    out.status      = readU32LE(p + 34);
    for (int i = 0; i < 3; ++i) out.att_ahrs[i] = readFloatLE(p + 38 + i * 4);
    for (int i = 0; i < 3; ++i) out.att[i]      = readFloatLE(p + 50 + i * 4);
    for (int i = 0; i < 3; ++i) out.vel[i]      = readFloatLE(p + 62 + i * 4);
    for (int i = 0; i < 3; ++i) out.pos[i]      = readI32LE(p + 74 + i * 4);
}

void FrameParser::parseDebugGps(const uint8_t *p, DebugGpsData &out)
{
    out.itow         = readU32LE(p + 0);
    out.week_num     = readU16LE(p + 4);
    out.pps_time_us  = readU32LE(p + 6);
    out.revc_time_ms = readU32LE(p + 10);
    out.fixtype      = p[14];
    out.numSV        = p[15];
    out.multiSVs     = p[16];
    out.lat          = readI32LE(p + 17);
    out.lon          = readI32LE(p + 21);
    out.alt          = readI32LE(p + 25);
    out.vn           = readFloatLE(p + 29);
    out.ve           = readFloatLE(p + 33);
    out.vd           = readFloatLE(p + 37);
    out.gspeed       = readFloatLE(p + 41);
    out.headMot      = readFloatLE(p + 45);
    out.p_hacc       = readI16LE(p + 49);
    out.p_vacc       = readI16LE(p + 51);
    out.v_hacc       = readI16LE(p + 53);
    out.v_vacc       = readI16LE(p + 55);
    out.diff_age     = readU16LE(p + 57);
}

void FrameParser::parseDebugDrtk(const uint8_t *p, DebugDrtkData &out)
{
    out.itow        = readU32LE(p + 0);
    out.fixtype     = p[4];
    out.heading     = readFloatLE(p + 5);
    out.heading_std = readFloatLE(p + 9);
    out.pitch       = readFloatLE(p + 13);
    out.pitch_std   = readFloatLE(p + 17);
    out.length      = readU16LE(p + 21);
    out.svnum       = p[23];
}

void FrameParser::parseDebugOd(const uint8_t *p, DebugOdData &out)
{
    out.time_gps_ms = readU32LE(p + 0);
    out.vel_fl_cm_s = readI16LE(p + 4);
    out.vel_fr_cm_s = readI16LE(p + 6);
    out.vel_bl_cm_s = readI16LE(p + 8);
    out.vel_br_cm_s = readI16LE(p + 10);
    out.gear        = p[12];
    out.steer_angle = readI16LE(p + 13);
}

void FrameParser::parseDebugKf(const uint8_t *p, DebugKfData &out)
{
    for (int i = 0; i < 18; ++i) out.pk[i]       = readFloatLE(p + 0   + i * 4);
    for (int i = 0; i < 6;  ++i) out.imu_bias[i] = readFloatLE(p + 72  + i * 4);
    for (int i = 0; i < 6;  ++i) out.innov[i]    = readFloatLE(p + 96  + i * 4);
    out.od_k        = readU16LE(p + 120);
    out.inst_pitch  = readI16LE(p + 122);
    out.inst_yaw    = readI16LE(p + 124);
    out.time_consume = readFloatLE(p + 126);
    out.task_period = readU32LE(p + 130);
    out.cpu_usage   = readU32LE(p + 134);
    for (int i = 0; i < 3; ++i) out.RK_p[i] = readU16LE(p + 138 + i * 2);
    for (int i = 0; i < 3; ++i) out.Rk_v[i] = readU16LE(p + 144 + i * 2);
    for (int i = 0; i < 3; ++i) out.Rk_b[i] = readU16LE(p + 150 + i * 2);
    for (int i = 0; i < 4; ++i) out.rev[i]  = readU32LE(p + 156 + i * 4);
}

void FrameParser::parseDebugParam(const uint8_t *p, DebugParamData &out)
{
    out.SETTING                  = readFloatLE(p + 0);
    out.GPS_HACC_FIX             = readFloatLE(p + 4);
    out.GPS_HACC_SINGLE          = readFloatLE(p + 8);
    out.GPS_SACC                 = readFloatLE(p + 12);
    out.GPS_HEADING_ACC          = readFloatLE(p + 16);
    out.GYRO_STD                 = readFloatLE(p + 20);
    out.ACCEL_STD                = readFloatLE(p + 24);
    out.GYRO_ZUPT_VALUE          = readFloatLE(p + 28);
    out.MEAS_CONSTRAIN_AHRS      = readFloatLE(p + 32);
    out.ALIGN_VEL                = readFloatLE(p + 36);
    out.PARAM_ANTENNA_INST_ANGLE = readFloatLE(p + 40);
    out.PARAM_ANTENNA_MODIFY_ANGLE = readFloatLE(p + 44);
    out.ANTENNA_LENGTH           = readFloatLE(p + 48);
    out.PARAM_OD_K               = readFloatLE(p + 52);
    out.PARAM_INST_ROLL          = readFloatLE(p + 56);
    out.PARAM_INST_PITCH         = readFloatLE(p + 60);
    out.PARAM_INST_YAW           = readFloatLE(p + 64);
    out.PARAM_GPS_POS_X          = readFloatLE(p + 68);
    out.PARAM_GPS_POS_Y          = readFloatLE(p + 72);
    out.PARAM_GPS_POS_Z          = readFloatLE(p + 76);
    out.PARAM_OD_POS_X           = readFloatLE(p + 80);
    out.PARAM_OD_POS_Y           = readFloatLE(p + 84);
    out.PARAM_OD_POS_Z           = readFloatLE(p + 88);
    out.ODR                      = readFloatLE(p + 92);
    for (int i = 0; i < 10; ++i) out.rev[i] = readFloatLE(p + 96 + i * 4);
    out.VERSION                  = readFloatLE(p + 136);
}

// ==================== 614E-B 调试数据流 (0x0101/0x0111/0x0131) ====================

void FrameParser::parseEb614Imu(const uint8_t *p, Eb614ImuData &out)
{
    out.time_gps_ms = readU32LE(p + 0);
    out.week_num    = readU16LE(p + 4);
    for (int i = 0; i < 3; ++i) out.gyro[i]  = readFloatLE(p + 6  + i * 4);
    for (int i = 0; i < 3; ++i) out.accel[i] = readFloatLE(p + 18 + i * 4);
    out.temperature = readFloatLE(p + 30);
    out.temp_vel    = readI16LE(p + 34);
    out.imu_type    = p[36];
    out.rtk_type    = p[37];
    out.zupt        = p[38];
    for (int i = 0; i < 3; ++i) out.qekf_att[i] = readI16LE(p + 39 + i * 2);
    for (int i = 0; i < 3; ++i) out.kf_att[i]   = readI16LE(p + 45 + i * 2);
}

void FrameParser::parseEb614Gps(const uint8_t *p, Eb614GpsData &out)
{
    out.itow_pos     = readU32LE(p + 0);
    out.week_num     = readU16LE(p + 4);
    out.pps_time_us  = readU32LE(p + 6);
    out.revc_time_ms = readU32LE(p + 10);
    out.fixtype      = p[14];
    out.numSV        = p[15];
    out.multiSVs     = p[16];
    out.lat          = readDoubleLE(p + 17);
    out.lon          = readDoubleLE(p + 25);
    out.alt          = readI32LE(p + 33);
    out.p_hacc       = readU16LE(p + 37);
    out.p_vacc       = readU16LE(p + 39);
    out.diff_age     = readU16LE(p + 41);
    out.itow_vel     = readU32LE(p + 43);
    out.vn           = readFloatLE(p + 47);
    out.ve           = readFloatLE(p + 51);
    out.vd           = readFloatLE(p + 55);
    out.gspeed       = readFloatLE(p + 59);
    out.headMot      = readFloatLE(p + 63);
    out.v_hacc       = readU16LE(p + 67);
    out.v_vacc       = readU16LE(p + 69);
}

void FrameParser::parseEb614Kf(const uint8_t *p, Eb614KfData &out)
{
    for (int i = 0; i < 21; ++i) out.pk[i]           = readFloatLE(p + 0   + i * 4);
    for (int i = 0; i < 6;  ++i) out.innov[i]        = readFloatLE(p + 84  + i * 4);
    for (int i = 0; i < 3;  ++i) out.att[i]          = readFloatLE(p + 108 + i * 4);
    for (int i = 0; i < 3;  ++i) out.vel[i]          = readFloatLE(p + 120 + i * 4);
    for (int i = 0; i < 3;  ++i) out.pos[i]          = readDoubleLE(p + 132 + i * 8);
    for (int i = 0; i < 3;  ++i) out.gyro_bias[i]    = readFloatLE(p + 156 + i * 4);
    for (int i = 0; i < 3;  ++i) out.acceel_bias[i]  = readFloatLE(p + 168 + i * 4);
    for (int i = 0; i < 3;  ++i) out.gyro_scale[i]   = readFloatLE(p + 180 + i * 4);
    for (int i = 0; i < 3;  ++i) out.est_lever_arm[i]= readFloatLE(p + 192 + i * 4);
    out.itow = readU32LE(p + 204);
}

// ==================== 字节序读工具 (小端) ====================

float FrameParser::readFloatLE(const uint8_t *p)
{
    float v;
    std::memcpy(&v, p, 4);
    return v;
}

double FrameParser::readDoubleLE(const uint8_t *p)
{
    double v;
    std::memcpy(&v, p, 8);
    return v;
}

uint16_t FrameParser::readU16LE(const uint8_t *p)
{
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t FrameParser::readU32LE(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

int32_t FrameParser::readI32LE(const uint8_t *p)
{
    return static_cast<int32_t>(readU32LE(p));
}

int16_t FrameParser::readI16LE(const uint8_t *p)
{
    return static_cast<int16_t>(readU16LE(p));
}
