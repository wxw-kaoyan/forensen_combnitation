#ifndef A3PARSER_H
#define A3PARSER_H

#include "modules/parse/parser/parserbase.h"
#include <QByteArray>
#include <cstring>

// ════════════════════════════════════════════════════════════════
//  A3 IMU 异常测试协议解析
//  CANFD 扩展帧, ID = 0x272, 数据长度 64 字节, 小端, 500Hz
//
//  帧字段（按 byte 偏移）：
//    byte0       Cycle (uint8)
//    byte1-2     GPSWeek (uint16 LE)
//    byte3-6     GPSWeekMs (uint32 LE)
//    byte7-10    GYR_X (float LE, deg/s)
//    byte11-14   GYR_Y (float LE, deg/s)
//    byte15-18   GYR_Z (float LE, deg/s)
//    byte19-22   ACC_X (float LE, m/s²)
//    byte23-26   ACC_Y (float LE, m/s²)
//    byte27-30   ACC_Z (float LE, m/s²)
//    byte31-32   Temp (int16 LE, ×0.01, °C)
//    byte33-36   Q0 (float LE)
//    byte37-40   Q1 (float LE)
//    byte41-44   Q2 (float LE)
//    byte45-48   Q3 (float LE)
//    byte49      Run Status (uint8, bit0~bit5)
//    byte50-59   10 监控标志位 (10×uint8)
//    byte60-63   CRC (uint32 LE)
//
//  输入：文本 CAN 日志（每行带 "0x272" 与 "x| XX XX..." 64 字节 hex）
// ════════════════════════════════════════════════════════════════

class A3Parser : public ParserBase
{
public:
    QString buttonLabel() const override { return QStringLiteral("A3协议解析"); }
    QString category() const override { return QStringLiteral("CAN RTK"); }
    ProtocolID protocolId() const override { return ProtocolID::A3_Imu; }

    QList<ParsedFrame> parse(const QByteArray &rawData) override
    {
        QList<ParsedFrame> out;
        // 行内查找标识 "0x272"（不区分大小写）
        // 与 "x|" 之间任意字符（如 ",64,"）
        const char *base = rawData.constData();
        const int n = rawData.size();
        int pos = 0;
        const QByteArray idMark = "0x272";
        const QByteArray dataMark = "x|";
        while (pos < n) {
            int idPos = rawData.indexOf(idMark, pos);
            if (idPos < 0) break;
            // 行尾：找下一个换行
            int nl = rawData.indexOf('\n', idPos);
            int lineEnd = (nl < 0) ? n : nl;
            // 行内找 "x|"
            int xBar = rawData.indexOf(dataMark, idPos);
            if (xBar < 0 || xBar >= lineEnd) { pos = lineEnd + 1; continue; }

            // 从 xBar+2 开始解析 64 个 hex 字节
            uint8_t data[64] = {0};
            int byteIdx = 0;
            int p = xBar + 2;
            while (byteIdx < 64 && p < lineEnd) {
                int hi = hexVal(rawData.at(p));
                if (hi < 0) { ++p; continue; }
                if (p + 1 >= lineEnd) break;
                int lo = hexVal(rawData.at(p + 1));
                if (lo < 0) { pos = lineEnd + 1; break; }
                data[byteIdx++] = static_cast<uint8_t>((hi << 4) | lo);
                p += 2;
            }
            if (byteIdx == 64) {
                ParsedFrame f;
                std::memset(&f, 0, sizeof(f));
                f.protocol = ProtocolID::A3_Imu;
                f.frameId = 0x272;
                f.payloadLength = 64;
                f.crcOk = true;
                f.offsetInFile = out.size();
                f.a3Imu = decodeA3(data);
                out.append(f);
            }
            pos = lineEnd + 1;
        }
        return out;
    }

    QString csvHeader() const override
    {
        return QStringLiteral(
            "Cycle,GPS周,GPS周内毫秒(ms),"
            "陀螺X(deg/s),陀螺Y(deg/s),陀螺Z(deg/s),"
            "加表X(m/s²),加表Y(m/s²),加表Z(m/s²),"
            "温度(C),Q0,Q1,Q2,Q3,RunStatus,"
            "陀螺硬件失效计数,加计硬件失效计数,陀螺超量程计数,"
            "协方差奇异重置计数,协方差对角线负数计数,航向方差下溢计数,"
            "EKF健康状态位寄存器,姿态步突跳超限计数,CAN报文+秒脉冲同步,系统异常状态寄存器,"
            "CRC\n");
    }

    QString formatFrame(const ParsedFrame &f) const override
    {
        const auto &d = f.a3Imu;
        return QStringLiteral(
            "%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,"
            "%16,%17,%18,%19,%20,%21,%22,%23,%24,%25,%26\n")
            .arg(d.cycle).arg(d.gpsWeek).arg(d.gpsWeekMs)
            .arg(d.gyro[0], 0, 'f', 5).arg(d.gyro[1], 0, 'f', 5).arg(d.gyro[2], 0, 'f', 5)
            .arg(d.accel[0], 0, 'f', 5).arg(d.accel[1], 0, 'f', 5).arg(d.accel[2], 0, 'f', 5)
            .arg(d.temperature, 0, 'f', 2)
            .arg(d.quat[0], 0, 'f', 5).arg(d.quat[1], 0, 'f', 5)
            .arg(d.quat[2], 0, 'f', 5).arg(d.quat[3], 0, 'f', 5)
            .arg(d.runStatus, 0, 16)
            .arg(d.flags[0]).arg(d.flags[1]).arg(d.flags[2]).arg(d.flags[3])
            .arg(d.flags[4]).arg(d.flags[5]).arg(d.flags[6]).arg(d.flags[7])
            .arg(d.flags[8]).arg(d.flags[9])
            .arg(d.crc, 0, 16);
    }

private:
    static int hexVal(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    static A3ImuData decodeA3(const uint8_t data[64])
    {
        A3ImuData d;
        std::memset(&d, 0, sizeof(d));
        // 小端读取辅助
        auto u16 = [](const uint8_t *p) -> uint16_t {
            return static_cast<uint16_t>(p[0] | (p[1] << 8));
        };
        auto u32 = [](const uint8_t *p) -> uint32_t {
            return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
                 | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
        };
        auto f32 = [](const uint8_t *p) -> float {
            uint32_t v = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
            float f = 0.0f;
            std::memcpy(&f, &v, sizeof(f));
            return f;
        };

        d.cycle       = data[0];
        d.gpsWeek     = u16(data + 1);
        d.gpsWeekMs   = u32(data + 3);
        d.gyro[0]     = f32(data + 7);
        d.gyro[1]     = f32(data + 11);
        d.gyro[2]     = f32(data + 15);
        d.accel[0]    = f32(data + 19);
        d.accel[1]    = f32(data + 23);
        d.accel[2]    = f32(data + 27);
        // Temp: int16 LE × 0.01
        int16_t tempRaw = static_cast<int16_t>(u16(data + 31));
        d.temperature = static_cast<float>(tempRaw) * 0.01f;
        d.quat[0]     = f32(data + 33);
        d.quat[1]     = f32(data + 37);
        d.quat[2]     = f32(data + 41);
        d.quat[3]     = f32(data + 45);
        d.runStatus   = data[49];
        for (int i = 0; i < 10; ++i) d.flags[i] = data[50 + i];
        d.crc         = u32(data + 60);
        return d;
    }
};

#endif // A3PARSER_H
