#ifndef TXCANPARSER_H
#define TXCANPARSER_H

#include "modules/parse/parser/parserbase.h"
#include <QByteArray>
#include <QHash>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <cstring>

// ════════════════════════════════════════════════════════════════
//  同星 (TOSUN) CAN 原始数据解析（《通讯协议》1.4 节 表9~表15）
//  自动识别两种 CAN 文本日志行格式：
//
//  ① TRC（PCAN-View，$COLUMNS=N,O,T,I,d,l,D，时间列为 ms）：
//      2  3.805 DT 19FFCCC4 Rx 8 92 00 43 FF B5 F7 77 F7
//      └序号  └偏移ms └类型 └帧ID  └方向 └长度 └数据字节
//     类型列: DT=数据帧 FB=错误帧 FD=CANFD帧；扩展帧无 x 后缀
//
//  ② ASC（Vector CANalyzer）：
//      47.364275 1  19FFCDC4x       Rx   d 8 00 00 01 00 FF FF 00 00
//      └时间(s)   └通道 └帧ID(x=扩展) └方向 └d └长度 └数据字节
//
//  七组扩展帧（小端 int16/int32）：
//    表9  姿态    横滚/俯仰/航向/航迹      deg*100
//    表10 陀螺    gyro x/y/z + 预留        deg/s*52.0127
//    表11 加表    accel x/y/z + 状态位      g*3276.8
//    表12 周内秒 + 天线安装偏差角 + 速度方向
//    表13 纬度/经度                        deg/1e7 (int32)
//    表14 高程/周内秒                      1e-7 (int32)
//    表15 北/东/地速度 + 地速              cm/s
//
//  ⚠ 29 位扩展帧 ID 整体不固定（不同设备/固件都会变），因此不按 ID 分帧。
//  改为：按 ID 分组 → 用帧内容特征判定该组属于哪张表 → 解码。
//  判定依据（多帧统计，顺序即优先级）：
//    1) 第 1~4 字节 int32 单调递增      → 表12（周内秒在头部）
//    2) 第 5~8 字节 int32 单调递增      → 表14（周内秒在尾部）
//    3) 两个 int32 均为大地坐标量级      → 表13（经纬度）
//    4) 三轴 int16 模长≈3276（1g）      → 表11（加表）
//    5) 第 7~8 字节恒 0 且前三轴有抖动   → 表10（陀螺，预留位恒 0）
//    6) |地速-√(N²+E²)| 一致            → 表15（速度）
//    7) 其余                            → 表9（姿态）
//  判定为 Unknown 的组再用 ID 结构兜底。
//
//  一个按钮聚合七组：界面显示陀螺主轴，导出 7 个 CSV。
// ════════════════════════════════════════════════════════════════
class TxCanParser : public ParserBase
{
public:
    QString buttonLabel() const override { return QStringLiteral("同星CAN-IMU"); }
    QString category() const override { return QStringLiteral("CAN RTK"); }
    ProtocolID protocolId() const override { return ProtocolID::Tx_Gyro; }

    QList<ParsedFrame> parse(const QByteArray &rawData) override
    {
        QList<Rec> recs;
        const int n = rawData.size();
        int pos = 0;

        while (pos < n) {
            int nl = rawData.indexOf('\n', pos);
            const int lineEnd = (nl < 0) ? n : nl;
            QByteArray line = rawData.mid(pos, lineEnd - pos);
            pos = (nl < 0) ? n : nl + 1;

            if (line.endsWith('\r')) line.chop(1);
            if (line.isEmpty()) continue;

            const QStringList tok = QString::fromLatin1(line.simplified())
                                        .split(QLatin1Char(' '));
            Rec r;
            if (!parseLine(tok, r)) continue;
            recs.append(r);
        }

        if (recs.isEmpty()) return {};

        // 29 位 ID 整体不固定 → 先按 ID 分组，再用帧内容判定每组属于哪张表
        QHash<uint32_t, QList<Rec>> groups;
        for (const auto &r : recs)
            groups[r.id].append(r);

        QHash<uint32_t, ProtocolID> typeMap;
        for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
            ProtocolID t = classifyByContent(it.value());
            if (t == ProtocolID::Unknown)
                t = classifyById(it.key());
            typeMap.insert(it.key(), t);
        }

        QList<ParsedFrame> out;
        out.reserve(recs.size());
        for (const auto &r : recs) {
            ParsedFrame f;
            std::memset(&f, 0, sizeof(f));
            f.frameId       = static_cast<uint16_t>(r.id & 0xFFFF);
            f.payloadLength = 8;
            f.crcOk         = true;
            f.offsetInFile  = out.size();

            switch (typeMap.value(r.id, ProtocolID::Unknown)) {
            case ProtocolID::Tx_Att:    f.txAtt    = decodeAtt(r.d, r.ts);    break;
            case ProtocolID::Tx_Gyro:   f.txGyro   = decodeGyro(r.d, r.ts);   break;
            case ProtocolID::Tx_Accel:  f.txAccel  = decodeAccel(r.d, r.ts);  break;
            case ProtocolID::Tx_Sec:    f.txSec    = decodeSec(r.d, r.ts);    break;
            case ProtocolID::Tx_Pos:    f.txPos    = decodePos(r.d, r.ts);    break;
            case ProtocolID::Tx_Height: f.txHeight = decodeHeight(r.d, r.ts); break;
            case ProtocolID::Tx_Vel:    f.txVel    = decodeVel(r.d, r.ts);    break;
            default: continue;
            }
            f.protocol = typeMap.value(r.id);
            out.append(f);
        }
        return out;
    }

    QString csvHeader() const override { return csvHeaderFor(ProtocolID::Tx_Gyro); }

    QString formatFrame(const ParsedFrame &f) const override
    {
        return formatFrameFor(f.protocol, f);
    }

    // ── 三组各自的 CSV 表头 ──
    static QString csvHeaderFor(ProtocolID pid)
    {
        switch (pid) {
        case ProtocolID::Tx_Att:
            return QStringLiteral(
                "时间(s),帧ID,横滚角(deg),俯仰角(deg),航向角(deg),航迹角(deg),"
                "raw_roll,raw_pitch,raw_yaw,raw_track\n");
        case ProtocolID::Tx_Gyro:
            return QStringLiteral(
                "时间(s),帧ID,gyro_x(deg/s),gyro_y(deg/s),gyro_z(deg/s),"
                "预留,raw_x,raw_y,raw_z\n");
        case ProtocolID::Tx_Accel:
            return QStringLiteral(
                "时间(s),帧ID,acc_x(g),acc_y(g),acc_z(g),状态位,"
                "RTK板卡有效,PPS有效,前轮转角有效,前轮陀螺有效,RTK固定解,RTK定向固定解,航向已初始化,"
                "raw_x,raw_y,raw_z\n");
        case ProtocolID::Tx_Sec:
            return QStringLiteral(
                "时间(s),帧ID,周内秒(ms),估计天线安装偏差角(raw),速度方向,预留\n");
        case ProtocolID::Tx_Pos:
            return QStringLiteral(
                "时间(s),帧ID,经度(deg),纬度(deg),lon_raw,lat_raw\n");
        case ProtocolID::Tx_Height:
            return QStringLiteral(
                "时间(s),帧ID,椭球高(m),周内秒(ms),height_raw\n");
        case ProtocolID::Tx_Vel:
            return QStringLiteral(
                "时间(s),帧ID,北向速度(m/s),东向速度(m/s),地向速度(m/s),地速(m/s),"
                "raw_n,raw_e,raw_d,raw_g\n");
        default:
            return {};
        }
    }

    static QString formatFrameFor(ProtocolID pid, const ParsedFrame &f)
    {
        switch (pid) {
        case ProtocolID::Tx_Att: {
            const auto &d = f.txAtt;
            return QStringLiteral("%1,0x%2,%3,%4,%5,%6,%7,%8,%9,%10\n")
                .arg(d.ascTime, 0, 'f', 6)
                .arg(f.frameId, 4, 16, QLatin1Char('0'))
                .arg(d.roll, 0, 'f', 3).arg(d.pitch, 0, 'f', 3)
                .arg(d.yaw, 0, 'f', 3).arg(d.track, 0, 'f', 3)
                .arg(d.raw[0]).arg(d.raw[1]).arg(d.raw[2]).arg(d.raw[3]);
        }
        case ProtocolID::Tx_Gyro: {
            const auto &d = f.txGyro;
            return QStringLiteral("%1,0x%2,%3,%4,%5,%6,%7,%8,%9\n")
                .arg(d.ascTime, 0, 'f', 6)
                .arg(f.frameId, 4, 16, QLatin1Char('0'))
                .arg(d.gyro[0], 0, 'f', 4).arg(d.gyro[1], 0, 'f', 4).arg(d.gyro[2], 0, 'f', 4)
                .arg(d.reserved)
                .arg(d.raw[0]).arg(d.raw[1]).arg(d.raw[2]);
        }
        case ProtocolID::Tx_Accel: {
            const auto &d = f.txAccel;
            return QStringLiteral("%1,0x%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16\n")
                .arg(d.ascTime, 0, 'f', 6)
                .arg(f.frameId, 4, 16, QLatin1Char('0'))
                .arg(d.accel[0], 0, 'f', 4).arg(d.accel[1], 0, 'f', 4).arg(d.accel[2], 0, 'f', 4)
                .arg(d.status)
                .arg((d.status >> 0) & 1).arg((d.status >> 1) & 1)
                .arg((d.status >> 2) & 1).arg((d.status >> 3) & 1)
                .arg((d.status >> 4) & 1).arg((d.status >> 5) & 1)
                .arg((d.status >> 6) & 1)
                .arg(d.raw[0]).arg(d.raw[1]).arg(d.raw[2]);
        }
        case ProtocolID::Tx_Sec: {
            const auto &d = f.txSec;
            return QStringLiteral("%1,0x%2,%3,%4,%5,%6\n")
                .arg(d.ascTime, 0, 'f', 6)
                .arg(f.frameId, 4, 16, QLatin1Char('0'))
                .arg(d.weekMs).arg(d.antBiasRaw)
                .arg(d.velDirection).arg(d.reserved);
        }
        case ProtocolID::Tx_Pos: {
            const auto &d = f.txPos;
            return QStringLiteral("%1,0x%2,%3,%4,%5,%6\n")
                .arg(d.ascTime, 0, 'f', 6)
                .arg(f.frameId, 4, 16, QLatin1Char('0'))
                .arg(d.lon, 0, 'f', 8).arg(d.lat, 0, 'f', 8)
                .arg(d.lonRaw).arg(d.latRaw);
        }
        case ProtocolID::Tx_Height: {
            const auto &d = f.txHeight;
            return QStringLiteral("%1,0x%2,%3,%4,%5\n")
                .arg(d.ascTime, 0, 'f', 6)
                .arg(f.frameId, 4, 16, QLatin1Char('0'))
                .arg(d.height, 0, 'f', 4).arg(d.weekMs).arg(d.heightRaw);
        }
        case ProtocolID::Tx_Vel: {
            const auto &d = f.txVel;
            return QStringLiteral("%1,0x%2,%3,%4,%5,%6,%7,%8,%9,%10\n")
                .arg(d.ascTime, 0, 'f', 6)
                .arg(f.frameId, 4, 16, QLatin1Char('0'))
                .arg(d.vn, 0, 'f', 3).arg(d.ve, 0, 'f', 3)
                .arg(d.vd, 0, 'f', 3).arg(d.gspeed, 0, 'f', 3)
                .arg(d.raw[0]).arg(d.raw[1]).arg(d.raw[2]).arg(d.raw[3]);
        }
        default:
            return {};
        }
    }

private:
    struct Rec { double ts; uint32_t id; uint8_t d[8]; };

    // ── 单行解析：自动识别 TRC / ASC 两种格式 ──
    static bool parseLine(const QStringList &tok, Rec &r)
    {
        if (tok.size() < 7) return false;

        // ① TRC (PCAN-View)：第3列 DT/FB/FD 为帧类型
        //    <序号> <时间ms> <DT|FB|FD> <ID> <Rx|Tx> <DLC> <数据...>
        if (tok[2] == QStringLiteral("DT") || tok[2] == QStringLiteral("FB")
            || tok[2] == QStringLiteral("FD")) {
            if (tok[4] != QStringLiteral("Rx") && tok[4] != QStringLiteral("Tx"))
                return false;
            bool okTs = false;
            const double tsMs = tok[1].toDouble(&okTs);      // 时间偏移 (ms)
            if (!okTs) return false;
            bool okId = false;
            const uint32_t canId = tok[3].toUInt(&okId, 16);
            // 本协议全部为 29 位扩展帧，标准帧(≤0x7FF)直接跳过
            if (!okId || canId <= 0x7FF) return false;
            bool okLen = false;
            const int dlc = tok[5].toInt(&okLen);
            if (!okLen || dlc != 8) return false;
            if (!readData(tok, r)) return false;
            r.ts = tsMs / 1000.0;                            // ms → s
            r.id = canId;
            return true;
        }

        // ② ASC (Vector CANalyzer)：第4列 Rx/Tx，第5列 d
        //    <时间s> <通道> <ID[x]> <Rx|Tx> d <DLC> <数据...>
        if (tok[3] != QStringLiteral("Rx") && tok[3] != QStringLiteral("Tx"))
            return false;
        if (tok[4] != QStringLiteral("d")) return false;      // 仅经典 CAN 数据帧
        QString idText = tok[2];
        const bool isExt = idText.endsWith(QLatin1Char('x'), Qt::CaseInsensitive);
        if (isExt) idText.chop(1);
        // 本协议全部为 29 位扩展帧，标准帧（无 x 后缀）直接跳过
        if (!isExt) return false;
        bool okId = false;
        const uint32_t canId = idText.toUInt(&okId, 16);
        if (!okId) return false;
        bool okLen = false;
        const int dlc = tok[5].toInt(&okLen);
        if (!okLen || dlc != 8) return false;
        if (!readData(tok, r)) return false;
        r.ts = tok[0].toDouble();
        r.id = canId;
        return true;
    }

    // 从第 6 列起读取 8 个 hex 数据字节
    static bool readData(const QStringList &tok, Rec &r)
    {
        for (int i = 0; i < 8; ++i) {
            bool ok = false;
            const int v = tok[6 + i].toInt(&ok, 16);
            if (!ok) return false;
            r.d[i] = static_cast<uint8_t>(v);
        }
        return true;
    }

    // ID 结构兜底（仅在内容判定失败时使用）
    static ProtocolID classifyById(uint32_t id)
    {
        if (isTxAttCanId(id))   return ProtocolID::Tx_Att;
        if (isTxGyroCanId(id))  return ProtocolID::Tx_Gyro;
        if (isTxAccelCanId(id)) return ProtocolID::Tx_Accel;
        if (isTxVelCanId(id))   return ProtocolID::Tx_Vel;
        if (isTxPosCanId(id))   return ProtocolID::Tx_Pos;
        return ProtocolID::Unknown;
    }

    static double median(QList<double> v)
    {
        if (v.isEmpty()) return 0.0;
        std::sort(v.begin(), v.end());
        return v.at(v.size() / 2);
    }

    static double stdDev(const QList<double> &v)
    {
        if (v.size() < 2) return 0.0;
        double mean = 0.0;
        for (double x : v) mean += x;
        mean /= v.size();
        double acc = 0.0;
        for (double x : v) acc += (x - mean) * (x - mean);
        return std::sqrt(acc / v.size());
    }

    // 某 4 字节 int32 是否为「GPS 周内毫秒」：
    // 单调递增，且 (末值-首值)/1000 与该组实际录制时长吻合（±40%）
    static bool looksLikeWeekMs(const QList<Rec> &g, const QList<int> &idx, int off)
    {
        const int m = idx.size();
        if (m < 4) return false;

        const double span = g.last().ts - g.first().ts;   // 秒
        if (span <= 1.0) return false;

        const double rangeSec =
            static_cast<double>(s32(g[idx.last()].d + off) - s32(g[idx.first()].d + off)) / 1000.0;
        if (rangeSec <= 0.0) return false;
        if (std::fabs(rangeSec - span) > 0.4 * span) return false;

        int good = 0;
        for (int k = 1; k < m; ++k) {
            const qint64 dlt = static_cast<qint64>(s32(g[idx[k]].d + off))
                             - s32(g[idx[k - 1]].d + off);
            if (dlt >= 0 && dlt <= 20000) ++good;
        }
        return good >= (m - 1) * 9 / 10;
    }

    // 依据帧内容判定该 ID 组属于哪张表（ID 整体不固定，不能靠 ID 分帧）
    static ProtocolID classifyByContent(const QList<Rec> &g)
    {
        const int n = g.size();
        if (n < 2) return ProtocolID::Unknown;

        // 均匀抽样，最多约 400 帧
        const int step = qMax(1, n / 400);
        QList<int> idx;
        for (int i = 0; i < n; i += step) idx.append(i);
        if (idx.last() != n - 1) idx.append(n - 1);
        const int m = idx.size();

        // 1) 头部 int32 = 周内秒 → 表12
        if (looksLikeWeekMs(g, idx, 0)) return ProtocolID::Tx_Sec;
        // 2) 尾部 int32 = 周内秒 → 表14
        if (looksLikeWeekMs(g, idx, 4)) return ProtocolID::Tx_Height;

        // 3) 三轴 int16 模长 ≈ 3276（1g）→ 表11 加表
        {
            QList<double> norms;
            norms.reserve(m);
            for (int k = 0; k < m; ++k) {
                const double x = s16(g[idx[k]].d);
                const double y = s16(g[idx[k]].d + 2);
                const double z = s16(g[idx[k]].d + 4);
                norms.append(std::sqrt(x * x + y * y + z * z));
            }
            const double med = median(norms);
            if (med > 1500.0 && med < 6000.0) return ProtocolID::Tx_Accel;
        }

        // 4) 预留位(第7~8字节)恒 0 且前三轴有抖动 → 表10 陀螺
        {
            int zeroTail = 0;
            QList<double> xs, ys, zs;
            xs.reserve(m); ys.reserve(m); zs.reserve(m);
            for (int k = 0; k < m; ++k) {
                const Rec &r = g[idx[k]];
                if (r.d[6] == 0 && r.d[7] == 0) ++zeroTail;
                xs.append(s16(r.d));
                ys.append(s16(r.d + 2));
                zs.append(s16(r.d + 4));
            }
            const double jitter = stdDev(xs) + stdDev(ys) + stdDev(zs);
            if (zeroTail >= m * 95 / 100 && jitter > 1.5)
                return ProtocolID::Tx_Gyro;
        }

        // 5) 地速 ≈ √(N²+E²) → 表15 速度
        {
            int good = 0;
            for (int k = 0; k < m; ++k) {
                const double vn = s16(g[idx[k]].d);
                const double ve = s16(g[idx[k]].d + 2);
                const double gs = s16(g[idx[k]].d + 6);
                const double planar = std::sqrt(vn * vn + ve * ve);
                const double tol = 0.15 * qMax(50.0, std::fabs(gs));
                if (std::fabs(gs - planar) <= tol) ++good;
            }
            if (good >= m * 4 / 5) return ProtocolID::Tx_Vel;
        }

        // 6) 两个 int32 均为大地坐标量级 → 表13 经纬度
        {
            int good = 0;
            for (int k = 0; k < m; ++k) {
                const qint64 a = qAbs(static_cast<qint64>(s32(g[idx[k]].d)));
                const qint64 b = qAbs(static_cast<qint64>(s32(g[idx[k]].d + 4)));
                if (a > 10000000LL && a < 1900000000LL
                    && b > 10000000LL && b < 1900000000LL) ++good;
            }
            if (good >= m * 9 / 10) return ProtocolID::Tx_Pos;
        }

        // 7) 兜底：姿态
        return ProtocolID::Tx_Att;
    }

    static int32_t s32(const uint8_t *p)
    {
        return static_cast<int32_t>(u32(p));
    }

    static int16_t s16(const uint8_t *p)
    {
        return static_cast<int16_t>(static_cast<uint16_t>(p[0])
                                    | (static_cast<uint16_t>(p[1]) << 8));
    }
    static uint16_t u16(const uint8_t *p)
    {
        return static_cast<uint16_t>(static_cast<uint16_t>(p[0])
                                     | (static_cast<uint16_t>(p[1]) << 8));
    }
    static uint32_t u32(const uint8_t *p)
    {
        return static_cast<uint32_t>(p[0])
             | (static_cast<uint32_t>(p[1]) << 8)
             | (static_cast<uint32_t>(p[2]) << 16)
             | (static_cast<uint32_t>(p[3]) << 24);
    }

    static TxAttData decodeAtt(const uint8_t d[8], double ts)
    {
        TxAttData v;
        std::memset(&v, 0, sizeof(v));
        v.ascTime = ts;
        for (int i = 0; i < 4; ++i) v.raw[i] = s16(d + i * 2);
        v.roll  = v.raw[0] / 100.0;
        v.pitch = v.raw[1] / 100.0;
        v.yaw   = v.raw[2] / 100.0;
        v.track = v.raw[3] / 100.0;
        return v;
    }

    static TxGyroData decodeGyro(const uint8_t d[8], double ts)
    {
        TxGyroData v;
        std::memset(&v, 0, sizeof(v));
        v.ascTime = ts;
        for (int i = 0; i < 3; ++i) {
            v.raw[i]  = s16(d + i * 2);
            v.gyro[i] = v.raw[i] / 52.0127;
        }
        v.reserved = s16(d + 6);
        return v;
    }

    static TxAccelData decodeAccel(const uint8_t d[8], double ts)
    {
        TxAccelData v;
        std::memset(&v, 0, sizeof(v));
        v.ascTime = ts;
        for (int i = 0; i < 3; ++i) {
            v.raw[i]   = s16(d + i * 2);
            v.accel[i] = v.raw[i] / 3276.8;
        }
        v.status = u16(d + 6);
        return v;
    }

    // 表12 周内秒 + 天线安装偏差角 + 速度方向
    static TxSecData decodeSec(const uint8_t d[8], double ts)
    {
        TxSecData v;
        std::memset(&v, 0, sizeof(v));
        v.ascTime    = ts;
        v.weekMs     = u32(d);
        v.antBiasRaw = s16(d + 4);
        v.velDirection = static_cast<int8_t>(d[6]);
        v.reserved   = d[7];
        return v;
    }

    // 表13 纬度 + 经度 (deg, int32/1e7)
    // 注：文档表头为「经度|纬度」，实测（31.26N/121.62E）为「纬度|经度」
    static TxPosData decodePos(const uint8_t d[8], double ts)
    {
        TxPosData v;
        std::memset(&v, 0, sizeof(v));
        v.ascTime = ts;
        v.latRaw  = static_cast<int32_t>(u32(d));
        v.lonRaw  = static_cast<int32_t>(u32(d + 4));
        v.lat     = v.latRaw / 1e7;
        v.lon     = v.lonRaw / 1e7;
        return v;
    }

    // 表14 高程 (椭球高, int32/1e7) + 周内秒
    static TxHeightData decodeHeight(const uint8_t d[8], double ts)
    {
        TxHeightData v;
        std::memset(&v, 0, sizeof(v));
        v.ascTime   = ts;
        v.heightRaw = static_cast<int32_t>(u32(d));
        v.weekMs    = u32(d + 4);
        v.height    = v.heightRaw / 1e7;
        return v;
    }

    // 表15 北/东/地速度 + 地速 (cm/s)
    static TxVelData decodeVel(const uint8_t d[8], double ts)
    {
        TxVelData v;
        std::memset(&v, 0, sizeof(v));
        v.ascTime = ts;
        for (int i = 0; i < 4; ++i) v.raw[i] = s16(d + i * 2);
        v.vn     = v.raw[0] / 100.0;
        v.ve     = v.raw[1] / 100.0;
        v.vd     = v.raw[2] / 100.0;
        v.gspeed = v.raw[3] / 100.0;
        return v;
    }
};

#endif // TXCANPARSER_H
