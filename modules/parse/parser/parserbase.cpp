#include "modules/parse/parser/parserbase.h"
#include <QTextStream>
#include <algorithm>

// ════════════════════════════════════════════════════════════════
//  丢数统计 — 从「帧内时间戳」判断是否有帧丢失，适用于所有带时间戳的协议
//    各协议的时间字段不同，这里统一换算成毫秒：
//      · 周内毫秒类 → 跨周时补 604800000 ms
//      · µs 计数器类（AHRS/四元数）→ 2^32 µs 回绕时补 4294967 ms
//      · 秒类（CAN ASC/TRC 行首时间、CAN-RTK 天内秒）→ ×1000
//    正常间隔取相邻间隔的中位数（自适应，不写死帧率），
//    dt > 1.5×间隔 判为丢数，丢帧数 = round(dt/间隔) - 1
// ════════════════════════════════════════════════════════════════
static bool frameTimeMs(const ParsedFrame &f, qint64 &ms, bool &usTimer)
{
    usTimer = false;
    switch (f.protocol) {
    case ProtocolID::AHRS_Basic:      ms = qint64(f.ahrsBasic.timer) / 1000;   usTimer = true; return true;
    case ProtocolID::AHRS_Extended:   ms = qint64(f.ahrsExtended.basic.timer) / 1000; usTimer = true; return true;
    case ProtocolID::Quaternion:      ms = qint64(f.quat.timer) / 1000;        usTimer = true; return true;
    case ProtocolID::AG_Data:         ms = f.agData.gpsTime;                   return true;
    case ProtocolID::FusionResult:    ms = f.fusion.gpsWeekSecond;             return true;
    case ProtocolID::Nav_Append:      ms = f.navAppend.gpsWeekMs;              return true;
    case ProtocolID::TiltMeasure:     ms = f.tilt.timestamp;                   return true;
    case ProtocolID::Laser:           ms = f.laser.timestamp;                  return true;
    case ProtocolID::NanfangLaser:    ms = f.nanfangLaser.timestamp;          return true;
    case ProtocolID::ZhdLaser:        ms = f.zhdLaser.timestamp;               return true;
    case ProtocolID::Attitude:        ms = f.attitude.timestamp;               return true;
    case ProtocolID::Quat614P:        ms = f.quat614p.gpsWeekMs;               return true;
    case ProtocolID::Debug_IMU:       ms = f.debugImu.time_gps_ms;             return true;
    case ProtocolID::Debug_GPS:       ms = f.debugGps.itow;                    return true;
    case ProtocolID::Debug_DRTK:      ms = f.debugDrtk.itow;                   return true;
    case ProtocolID::Debug_OD:        ms = f.debugOd.time_gps_ms;              return true;
    case ProtocolID::Eb614_IMU:       ms = f.eb614Imu.time_gps_ms;             return true;
    case ProtocolID::Eb614_GPS:       ms = f.eb614Gps.itow_pos;                return true;
    case ProtocolID::Eb614_KF:        ms = f.eb614Kf.itow;                     return true;
    case ProtocolID::Can_RTK:         ms = qint64(f.canRtk.secondsOfDay * 1000.0);  return true;
    case ProtocolID::LsNav_Imu:       ms = f.lsNavImu.timeGpsMs;               return true;
    case ProtocolID::LsNav_Gps:       ms = f.lsNavGps.itow;                    return true;
    case ProtocolID::A3_Imu:          ms = f.a3Imu.gpsWeekMs;                  return true;
    case ProtocolID::Novatel_BestPos: ms = f.novatelBestPos.gpsWeekMs;         return true;
    case ProtocolID::Novatel_PsrVel:  ms = f.novatelPsrVel.gpsWeekMs;          return true;
    case ProtocolID::Tx_Att:          ms = qint64(f.txAtt.ascTime * 1000.0);   return true;
    case ProtocolID::Tx_Gyro:         ms = qint64(f.txGyro.ascTime * 1000.0);  return true;
    case ProtocolID::Tx_Accel:        ms = qint64(f.txAccel.ascTime * 1000.0); return true;
    case ProtocolID::Tx_Sec:          ms = qint64(f.txSec.ascTime * 1000.0);   return true;
    case ProtocolID::Tx_Pos:          ms = qint64(f.txPos.ascTime * 1000.0);   return true;
    case ProtocolID::Tx_Height:       ms = qint64(f.txHeight.ascTime * 1000.0); return true;
    case ProtocolID::Tx_Vel:          ms = qint64(f.txVel.ascTime * 1000.0);   return true;
    default: return false;
    }
}

// 生成丢数统计块（block 进显示区，brief 进状态栏）；无可统计时间戳时输出为空
static void gapSummary(const QList<ParsedFrame> &frames, QString &block, QString &brief)
{
    const int n = frames.size();
    if (n < 2)
        return;

    QList<qint64> ts;
    ts.reserve(n);
    bool usTimer = false;
    for (int i = 0; i < n; ++i) {
        qint64 t = 0;
        bool us = false;
        if (!frameTimeMs(frames[i], t, us))
            return;                 // 该协议没有可用时间戳
        usTimer = us;
        ts.append(t);
    }

    // 回绕修正，保证时间单调
    int backCount = 0;
    for (int i = 1; i < n; ++i) {
        const qint64 dt = ts[i] - ts[i - 1];
        if (dt < 0) {
            if (usTimer && -dt < 4500000)
                ts[i] += 4294967;              // µs 计数器回绕（2^32 µs）
            else if (-dt > 300000000)
                ts[i] += 604800000;            // GPS 周回绕
            else
                ++backCount;                   // 时间戳回退（非回绕）
        }
    }

    QList<qint64> dts;
    dts.reserve(n - 1);
    for (int i = 1; i < n; ++i)
        dts.append(ts[i] - ts[i - 1]);
    std::sort(dts.begin(), dts.end());

    const qint64 interval = dts[dts.size() / 2];   // 中位间隔
    qint64 maxGap = 0, totalLost = 0;
    int abnormal = 0;
    for (int i = 1; i < n; ++i) {
        const qint64 dt = ts[i] - ts[i - 1];
        if (dt > maxGap) maxGap = dt;
        if (interval > 0 && dt * 2 > interval * 3) {
            ++abnormal;
            totalLost += (dt + interval / 2) / interval - 1;
        }
    }

    const qint64 span = ts.last() - ts.first();
    const double hz = span > 0 ? 1000.0 * (n - 1) / double(span) : 0.0;

    const QString verdict = (totalLost == 0 && abnormal == 0)
            ? QStringLiteral("无丢数，数据完整")
            : QStringLiteral("存在丢数");

    brief = QStringLiteral("丢帧:%1 间隔%2ms 平均%3Hz %4")
                .arg(totalLost).arg(interval)
                .arg(hz, 0, 'f', 3).arg(verdict);

    block = QStringLiteral(
        "────────────────────────────────────────\n"
        "丢数统计（按 %1 帧内时间戳判定）\n"
        "  总帧数     : %2\n"
        "  时间跨度   : %3 s\n"
        "  平均帧率   : %4 Hz\n"
        "  正常间隔   : %5 ms（中位数）\n"
        "  最大间隔   : %6 ms\n"
        "  异常间隔   : %7 处\n"
        "  时间回退   : %8 次\n"
        "  丢帧总数   : %9\n"
        "  结论       : %10\n"
        "────────────────────────────────────────\n")
            .arg(ParserBase::protocolNameStatic(frames.first().protocol))
            .arg(n)
            .arg(span / 1000.0, 0, 'f', 1)
            .arg(hz, 0, 'f', 3)
            .arg(interval)
            .arg(maxGap)
            .arg(abnormal)
            .arg(backCount)
            .arg(totalLost)
            .arg(verdict);
}

// ════════════════════════════════════════════════════════════════
//  默认统一入口：走「帧解析」路径（标准帧 / CAN / DEBUG 流）
//  外部脚本（凯芯）与深解码（RTCM）子类重写此方法
// ════════════════════════════════════════════════════════════════

// 输出区只保留前 maxFrames 帧的文本行（表头等额外行由 extraHeadLines 指定），
// 完整文本仍留在 fullText 里供「导出」使用。
// 几十万帧一次性灌进 QTextEdit 会在 GUI 线程建几十万个文本块而卡死界面。
static QString firstFramesOnly(const QString &text, int totalFrames,
                               int extraHeadLines, int maxFrames)
{
    if (totalFrames <= maxFrames)
        return text;

    const int keepLines = extraHeadLines + maxFrames;
    qsizetype pos = 0;
    for (int i = 0; i < keepLines; ++i) {
        const qsizetype nl = text.indexOf(QLatin1Char('\n'), pos);
        if (nl < 0)
            return text;              // 行数不足，不截断
        pos = nl + 1;
    }
    return text.left(pos)
         + QStringLiteral("……（仅显示前 %1 帧，共 %2 帧；完整数据请用「导出」保存）……\n")
               .arg(maxFrames).arg(totalFrames);
}

ParseResult ParserBase::parseToResult(const QByteArray &rawData,
                                      const QString &filePath)
{
    Q_UNUSED(filePath);

    // 解析结果输出区最多显示 200 帧
    constexpr int kMaxDisplayFrames = 200;

    ParseResult result;
    result.protocolId = protocolId();
    result.frames = parse(rawData);
    result.hasCsvHeader = !csvHeader().isEmpty();

    QString text;
    QTextStream s(&text);
    if (result.hasCsvHeader) {
        s << csvHeader();
        for (const auto &f : result.frames)
            s << formatFrame(f);
    } else {
        for (const auto &f : result.frames) {
            s << QStringLiteral("[#%1] %2\n")
                 .arg(f.offsetInFile)
                 .arg(formatFrame(f));
        }
    }
    s.flush();

    if (text.isEmpty())
        text = QStringLiteral("（无匹配帧）\n");

    int crcErr = 0;
    for (const auto &f : result.frames) {
        if (!f.crcOk)
            ++crcErr;
    }

    result.fullText = text;   // 完整内容（导出用）
    result.displayText = firstFramesOnly(text, result.frames.size(),
                                         result.hasCsvHeader ? 1 : 0,
                                         kMaxDisplayFrames);
    result.infoText = QStringLiteral("帧:%1 (CRC错:%2)")
                          .arg(result.frames.size()).arg(crcErr);

    // 解析完成后追加丢数统计：只进显示区，导出文本 fullText 保持纯净
    QString block, brief;
    gapSummary(result.frames, block, brief);
    if (!block.isEmpty())
        result.displayText += QLatin1Char('\n') + block;
    if (!brief.isEmpty())
        result.infoText += QStringLiteral(" | ") + brief;

    return result;
}

QString ParserBase::protocolNameStatic(ProtocolID pid)
{
    return QString::fromUtf8(protocolName(pid));
}

QString ParserBase::rtkStatusName(uint8_t s)
{
    switch (s) {
        // NMEA/组合导航融合帧(0x0166)及CAN-RTK 使用的定位质量码
        case 0: return QStringLiteral("未定位");
        case 1: return QStringLiteral("单点");
        case 2: return QStringLiteral("伪距差分");
        case 4: return QStringLiteral("固定解");
        case 5: return QStringLiteral("浮点解");
        // UM982 原始定位状态码，用于 NAV_APPEND(0x0167) 的 GNSS/双天线定位状态
        case 16: return QStringLiteral("单点定位");
        case 17: return QStringLiteral("伪距差分定位");
        case 34: return QStringLiteral("浮点解");
        case 50: return QStringLiteral("固定解");
        default: return QString::number(s);
    }
}

QString ParserBase::formatAttValid(uint8_t v)
{
    QStringList parts;
    if (v & 0x01) parts << QStringLiteral("姿态有效");
    if (v & 0x02) parts << QStringLiteral("RTK有效");
    if (v & 0x04) parts << QStringLiteral("已收敛");
    if (v & 0x08) parts << QStringLiteral("已初始化");

    if (parts.isEmpty()) return QStringLiteral("0x%1").arg(v, 0, 16);
    return QStringLiteral("0x%1[%2]").arg(v, 0, 16).arg(parts.join(','));
}

QString ParserBase::formatStatusFlags(uint32_t flags)
{
    QStringList parts;
    if (flags & 0x0001) parts << QStringLiteral("RTK有效");
    if (flags & 0x0002) parts << QStringLiteral("PPS有效");
    if (flags & 0x0004) parts << QStringLiteral("已初始化");
    if (flags & 0x0008) parts << QStringLiteral("前轮转角有效");
    if (flags & 0x0010) parts << QStringLiteral("已收敛");
    if (flags & 0x0020) parts << QStringLiteral("前轮陀螺有效");
    if (flags & 0x0040) parts << QStringLiteral("方向盘电机有效");

    uint8_t dir = (flags >> 7) & 0x03;
    if (dir == 1) parts << QStringLiteral("前进");
    else if (dir == 2) parts << QStringLiteral("后退");

    if (parts.isEmpty()) return QStringLiteral("0x%1").arg(flags, 0, 16);
    return QStringLiteral("0x%1[%2]").arg(flags, 0, 16).arg(parts.join(','));
}
