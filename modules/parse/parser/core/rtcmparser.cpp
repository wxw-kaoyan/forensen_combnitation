#include "modules/parse/parser/core/rtcmparser.h"
#include <QTextStream>
#include <QMap>
#include <QDebug>

// ════════════════════════════════════════════════════════════════
//  CRC-24Q 查找表
//  多项式: 0x1864CFB (x^24 + x^23 + x^18 + x^17 + x^14 + x^11 + x^10 + x^7 + x^6 + x^5 + x^4 + x^3 + x + 1)
//  初始值: 0x000000
// ════════════════════════════════════════════════════════════════

static const uint32_t crc24qTable[256] = {
    0x00000000,0x864CFB00,0x8AD50D00,0x0C99F600,0x93E9E100,0x15A51A00,0x193CEC00,0x9F701700,
    0xA1E15900,0x27ADA200,0x2B345400,0xAD78AF00,0x3208B800,0xB4444300,0xB8DDB500,0x3E914E00,
    0xC5A93300,0x43E5C800,0x4F7C3E00,0xC930C500,0x5640D200,0xD00C2900,0xDC95DF00,0x5AD92400,
    0x64486A00,0xE2049100,0xEE9D6700,0x68D19C00,0xF7A18B00,0x71ED7000,0x7D748600,0xFB387D00,
    0x0DA3E700,0x8BEF1C00,0x8776EA00,0x013A1100,0x9E4A0600,0x1806FD00,0x149F0B00,0x92D3F000,
    0xAC42BE00,0x2A0E4500,0x2697B300,0xA0DB4800,0x3FAB5F00,0xB9E7A400,0xB57E5200,0x3332A900,
    0xC80AD400,0x4E462F00,0x42DFD900,0xC4932200,0x5BE33500,0xDDAFCE00,0xD1363800,0x577AC300,
    0x69EB8D00,0xEFA77600,0xE33E8000,0x65727B00,0xFA026C00,0x7C4E9700,0x70D76100,0xF69B9A00,
    0x1B47CE00,0x9D0B3500,0x9192C300,0x17DE3800,0x88AE2F00,0x0EE2D400,0x027B2200,0x8437D900,
    0xBAA69700,0x3CEA6C00,0x30739A00,0xB63F6100,0x294F7600,0xAF038D00,0xA39A7B00,0x25D68000,
    0xDEEEFD00,0x58A20600,0x543BF000,0xD2770B00,0x4D071C00,0xCB4BE700,0xC7D21100,0x419EEA00,
    0x7F0FA400,0xF9435F00,0xF5DAA900,0x73965200,0xECE64500,0x6AAABE00,0x66334800,0xE07FB300,
    0x16E42900,0x90A8D200,0x9C312400,0x1A7DDF00,0x850DC800,0x03413300,0x0FD8C500,0x89943E00,
    0xB7057000,0x31498B00,0x3DD07D00,0xBB9C8600,0x24EC9100,0xA2A06A00,0xAE399C00,0x28756700,
    0xD34D1A00,0x5501E100,0x59981700,0xDFD4EC00,0x40A4FB00,0xC6E80000,0xCA71F600,0x4C3D0D00,
    0x72AC4300,0xF4E0B800,0xF8794E00,0x7E35B500,0xE145A200,0x67095900,0x6B90AF00,0xEDDC5400,
    0x368F9C00,0xB0C36700,0xBC5A9100,0x3A166A00,0xA5667D00,0x232A8600,0x2FB37000,0xA9FF8B00,
    0x976EC500,0x11223E00,0x1DBBC800,0x9BF73300,0x04872400,0x82CBDF00,0x8E522900,0x081ED200,
    0xF326AF00,0x756A5400,0x79F3A200,0xFFBF5900,0x60CF4E00,0xE683B500,0xEA1A4300,0x6C56B800,
    0x52C7F600,0xD48B0D00,0xD812FB00,0x5E5E0000,0xC12E1700,0x4762EC00,0x4BFB1A00,0xCDB7E100,
    0x3B2C7B00,0xBD608000,0xB1F97600,0x37B58D00,0xA8C59A00,0x2E896100,0x22109700,0xA45C6C00,
    0x9ACD2200,0x1C81D900,0x10182F00,0x9654D400,0x0924C300,0x8F683800,0x83F1CE00,0x05BD3500,
    0xFE854800,0x78C9B300,0x74504500,0xF21CBE00,0x6D6CA900,0xEB205200,0xE7B9A400,0x61F55F00,
    0x5F641100,0xD928EA00,0xD5B11C00,0x53FDE700,0xCC8DF000,0x4AC10B00,0x4658FD00,0xC0140600,
    0x2DC85200,0xAB84A900,0xA71D5F00,0x2151A400,0xBE21B300,0x386D4800,0x34F4BE00,0xB2B84500,
    0x8C290B00,0x0A65F000,0x06FC0600,0x80B0FD00,0x1FC0EA00,0x998C1100,0x9515E700,0x13591C00,
    0xE8616100,0x6E2D9A00,0x62B46C00,0xE4F89700,0x7B888000,0xFDC47B00,0xF15D8D00,0x77117600,
    0x49803800,0xCFCCC300,0xC3553500,0x4519CE00,0xDA69D900,0x5C252200,0x50BCD400,0xD6F02F00,
    0x206BB500,0xA6274E00,0xAABEB800,0x2CF24300,0xB3825400,0x35CEAF00,0x39575900,0xBF1BA200,
    0x818AEC00,0x07C61700,0x0B5FE100,0x8D131A00,0x12630D00,0x942FF600,0x98B60000,0x1EFAFB00,
    0xE5C28600,0x638E7D00,0x6F178B00,0xE95B7000,0x762B6700,0xF0679C00,0xFCFE6A00,0x7AB29100,
    0x4423DF00,0xC26F2400,0xCEF6D200,0x48BA2900,0xD7CA3E00,0x5186C500,0x5D1F3300,0xDB53C800
};

// CRC-24Q 计算 (输入: 从 preamble 开始到 payload 结束的全部字节)
uint32_t computeRtcmCrc24Q(const uint8_t *data, int len)
{
    uint32_t crc = 0;
    for (int i = 0; i < len; ++i) {
        crc = ((crc << 8) & 0xFFFFFF) ^ crc24qTable[(crc >> 16) ^ data[i]];
    }
    return crc;
}

// ════════════════════════════════════════════════════════════════
//  帧查找
// ════════════════════════════════════════════════════════════════

int RtcmParser::findNextFrame(const QByteArray &data, int startPos)
{
    const int size = data.size();
    for (int i = startPos; i < size; ++i) {
        if (static_cast<uint8_t>(data[i]) == kRtcmPreamble) {
            return i;
        }
    }
    return -1;
}

bool RtcmParser::isFrameComplete(const uint8_t *p, int availableBytes)
{
    if (availableBytes < kRtcmHeaderSize) return false;
    uint16_t payloadLen = extractPayloadLength(p);
    int totalLen = kRtcmHeaderSize + payloadLen + kRtcmCrcSize;
    return availableBytes >= totalLen;
}

// ════════════════════════════════════════════════════════════════
//  Header 解析
// ════════════════════════════════════════════════════════════════

uint16_t RtcmParser::extractPayloadLength(const uint8_t *header)
{
    // header[0] = preamble (0xD3)
    // header[1..2] = 6-bit reserved + 10-bit message length (big-endian)
    // 低 2 bit of header[1] 是高 2 bit of 长度, header[2] 是低 8 bit
    return (static_cast<uint16_t>(header[1] & 0x03) << 8) | header[2];
}

uint16_t RtcmParser::extractMessageType(const uint8_t *payload)
{
    // payload 前 12 位是消息类型号 (big-endian)
    return (static_cast<uint16_t>(payload[0]) << 4) | (payload[1] >> 4);
}

// ════════════════════════════════════════════════════════════════
//  帧解析
// ════════════════════════════════════════════════════════════════

RtcmFrame RtcmParser::parseOneFrame(const uint8_t *p, int /*availableBytes*/, int fileOffset)
{
    RtcmFrame frame;
    frame.offsetInFile = fileOffset;

    uint16_t payloadLen = extractPayloadLength(p);
    frame.payloadLength  = payloadLen;

    const uint8_t *payload = p + kRtcmHeaderSize;

    frame.messageType = extractMessageType(payload);

    // CRC-24Q: 从 preamble 开始计算，长度 = header(3) + payload，共 3+payloadLen 字节
    int crcInputLen = kRtcmHeaderSize + payloadLen;
    frame.crcExpected = computeRtcmCrc24Q(p, crcInputLen);

    // 收到的 CRC: payload 后的 3 字节 (big-endian)
    const uint8_t *crcPtr = p + kRtcmHeaderSize + payloadLen;
    frame.crcReceived = (static_cast<uint32_t>(crcPtr[0]) << 16) |
                        (static_cast<uint32_t>(crcPtr[1]) << 8)  |
                         static_cast<uint32_t>(crcPtr[2]);

    frame.crcOk = (frame.crcReceived == frame.crcExpected);

    return frame;
}

QList<RtcmFrame> RtcmParser::parseAll(const QByteArray &data, bool skipCrc) const
{
    qDebug() << "[RTCM parseAll] start, data size:" << data.size() << "skipCrc:" << skipCrc;
    QList<RtcmFrame> results;
    const int size = data.size();
    int pos = 0;

    static const int kMaxFrames = 50000;

    while (pos < size) {
        int frameStart = findNextFrame(data, pos);
        if (frameStart < 0) break;

        const uint8_t *p = reinterpret_cast<const uint8_t*>(data.constData()) + frameStart;
        int remaining = size - frameStart;

        if (!isFrameComplete(p, remaining)) {
            pos = frameStart + 1;
            continue;
        }

        uint16_t payloadLen = extractPayloadLength(p);
        int totalLen = kRtcmHeaderSize + payloadLen + kRtcmCrcSize;

        RtcmFrame frame = parseOneFrame(p, remaining, frameStart);
        if (skipCrc)
            frame.crcOk = true;
        results.append(frame);

        pos = frameStart + totalLen;

        if (results.size() > kMaxFrames) {
            results.clear();
            break;
        }
    }

    qDebug() << "[RTCM parseAll] done, frames:" << results.size();
    return results;
}

// ════════════════════════════════════════════════════════════════
//  格式化输出
// ════════════════════════════════════════════════════════════════

QString RtcmParser::formatFrame(const RtcmFrame &f)
{
    QString header = QStringLiteral("─ [RTCM 偏移:%1]  %2  MsgType=%3  Payload=%4B  CRC=%5")
                         .arg(f.offsetInFile)
                         .arg(rtcmMessageName(f.messageType))
                         .arg(f.messageType)
                         .arg(f.payloadLength)
                         .arg(f.crcOk ? QStringLiteral("✓") : QStringLiteral("✗错"));

    QString detail = QStringLiteral("  CRC收到:0x%1  CRC计算:0x%2")
                         .arg(f.crcReceived, 6, 16, QLatin1Char('0'))
                         .arg(f.crcExpected, 6, 16, QLatin1Char('0'));

    return header + QStringLiteral("\n") + detail + QStringLiteral("\n");
}

QString RtcmParser::formatSummary(const QList<RtcmFrame> &frames)
{
    QString out;
    QTextStream s(&out);

    // 统计每种消息类型
    struct CountInfo { int total = 0; int crcErr = 0; };
    QMap<uint16_t, CountInfo> counts;
    CountInfo totalStat;

    for (const auto &f : frames) {
        counts[f.messageType].total++;
        if (!f.crcOk) { counts[f.messageType].crcErr++; totalStat.crcErr++; }
        totalStat.total++;
    }

    s << QStringLiteral("════════════════════════════════════════════\n");
    s << QStringLiteral("  RTCM 3.x 解析统计: 共 %1 帧, CRC错误 %2 帧\n")
         .arg(totalStat.total).arg(totalStat.crcErr);
    s << QStringLiteral("────────────────────────────────────────────\n");
    s << QStringLiteral("  %-4s  %-24s  %6s  %6s\n")
         .arg(QStringLiteral("类型"), QStringLiteral("消息名称"), QStringLiteral("帧数"), QStringLiteral("CRC错误"));
    s << QStringLiteral("────────────────────────────────────────────\n");

    for (auto it = counts.begin(); it != counts.end(); ++it) {
        s << QStringLiteral("  %-4u  %-24s  %6d  %6d\n")
             .arg(it.key())
             .arg(rtcmMessageName(it.key()))
             .arg(it.value().total)
             .arg(it.value().crcErr);
    }

    s << QStringLiteral("════════════════════════════════════════════\n");
    s.flush();
    return out;
}
