#include "imureader.h"

#include <QFile>
#include <QTextStream>
#include <QtGlobal>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

namespace Analysis {

namespace {

QString normalizeHeader(const QString &s)
{
    QString r = s.toLower();
    r.remove('_');
    r.remove(' ');
    r.remove('(');
    r.remove(')');
    r.remove('.');
    r.remove('[');
    r.remove(']');
    return r;
}

QChar detectDelimiter(const QString &line)
{
    if (line.contains(',')) return ',';
    if (line.contains('\t')) return '\t';
    if (line.contains(';')) return ';';
    return ' ';
}

QStringList splitLine(const QString &line, QChar delim)
{
    if (delim == ' ')
        return line.split(' ', Qt::SkipEmptyParts);
    QStringList parts = line.split(delim);
    for (auto &p : parts)
        p = p.trimmed();
    return parts;
}

int findTimeColumn(const QStringList &header)
{
    for (int i = 0; i < header.size(); ++i) {
        const QString h = normalizeHeader(header[i]);
        if (h.contains("time") || h.contains("timestamp") || h == "t" ||
            h == "sec" || h.contains("tsec") || h.contains("second"))
            return i;
    }
    return 0;   // 默认第一列
}

int findAccelColumn(const QStringList &header, QChar axis)
{
    for (int i = 0; i < header.size(); ++i) {
        const QString h = normalizeHeader(header[i]);
        if (h == QString("a") + axis)
            return i;
        if ((h.contains("acc") || h.contains("accel")) && h.contains(axis))
            return i;
    }
    return -1;
}

int findGyroColumn(const QStringList &header, QChar axis)
{
    for (int i = 0; i < header.size(); ++i) {
        const QString h = normalizeHeader(header[i]);
        if (h == QString("g") + axis || h == QString("w") + axis)
            return i;
        if ((h.contains("gyro") || h.contains("gyr") ||
             h.contains("omega") || h.contains("angvel")) && h.contains(axis))
            return i;
    }
    return -1;
}

double median(QVector<double> v)
{
    if (v.isEmpty()) return 0.0;
    std::sort(v.begin(), v.end());
    const int n = v.size();
    return (n % 2) ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

double percentile(QVector<double> v, double p)
{
    if (v.isEmpty()) return 0.0;
    std::sort(v.begin(), v.end());
    int idx = static_cast<int>(p * (v.size() - 1));
    idx = qBound(0, idx, v.size() - 1);
    return v[idx];
}

void accumulate(AxisStat &st, double v)
{
    if (st.count == 0) {
        st.min = v;
        st.max = v;
    } else {
        st.min = qMin(st.min, v);
        st.max = qMax(st.max, v);
    }
    st.sum += v;
    st.count++;
}

constexpr double kPi = 3.14159265358979323846;

// ── 统一中间表示（已换算为 m/s² / rad/s） ──
struct RawImu { double t, ax, ay, az, gx, gy, gz; };

// 由已换算的样本序列生成摘要：采样率、丢数、回退、各轴统计、模值曲线
void finalizeSummary(QVector<RawImu> &raw, ImuSummary *out)
{
    ImuSummary &s = *out;
    const double t0 = raw.first().t;
    s.firstTime = t0;
    s.lastTime  = raw.last().t;
    s.durationSec = s.lastTime - s.firstTime;

    s.samples.reserve(raw.size());
    s.accelMag.reserve(raw.size());
    s.gyroMag.reserve(raw.size());
    QVector<double> dtList;
    dtList.reserve(raw.size());

    for (int i = 0; i < raw.size(); ++i) {
        const RawImu &r = raw[i];
        ImuSample smp;
        smp.t  = r.t - t0;
        smp.ax = r.ax; smp.ay = r.ay; smp.az = r.az;
        smp.gx = r.gx; smp.gy = r.gy; smp.gz = r.gz;
        s.samples.append(smp);

        const double am = std::sqrt(smp.ax * smp.ax + smp.ay * smp.ay + smp.az * smp.az);
        const double gm = std::sqrt(smp.gx * smp.gx + smp.gy * smp.gy + smp.gz * smp.gz);
        s.accelMag.append({smp.t, am});
        s.gyroMag.append({smp.t, gm});

        accumulate(s.accX, smp.ax);
        accumulate(s.accY, smp.ay);
        accumulate(s.accZ, smp.az);
        accumulate(s.gyrX, smp.gx);
        accumulate(s.gyrY, smp.gy);
        accumulate(s.gyrZ, smp.gz);

        if (i > 0)
            dtList << (raw[i].t - raw[i - 1].t);
    }
    s.totalSamples = s.samples.size();

    // ── 采样率 / 丢数 / 回退 ──
    const double dtMed = median(dtList);
    if (dtMed > 0.0) {
        s.sampleRate = 1.0 / dtMed;
        const double tol = dtMed * 1.5;   // 间隔容差
        for (double dt : dtList) {
            if (dt < -1e-9) {
                // 时间倒退（回退）
                s.rollbackCount++;
                s.totalRollbackSec += -dt;
                if (-dt > s.maxRollbackSec) s.maxRollbackSec = -dt;
            } else if (dt > tol) {
                // 丢数
                s.gapEvents++;
                s.gapCount += qMax(0, qRound(dt / dtMed) - 1);
                s.totalGapSec += dt - dtMed;
            }
        }
    }
}

// ════════════════════════════════════════════════════════════════
//  CSV / 文本格式
// ════════════════════════════════════════════════════════════════
bool parseCsvImpl(const QByteArray &data, ImuSummary *out, QString *err)
{
    QStringList lines;
    {
        QTextStream ts(data);
        while (!ts.atEnd()) {
            const QString line = ts.readLine().trimmed();
            if (!line.isEmpty())
                lines << line;
        }
    }
    if (lines.isEmpty()) {
        if (err) *err = QStringLiteral("文件为空");
        return false;
    }

    const QChar delim = detectDelimiter(lines.first());
    const QStringList header = splitLine(lines.first(), delim);

    const int tCol  = findTimeColumn(header);
    const int axCol = findAccelColumn(header, 'x');
    const int ayCol = findAccelColumn(header, 'y');
    const int azCol = findAccelColumn(header, 'z');
    const int gxCol = findGyroColumn(header, 'x');
    const int gyCol = findGyroColumn(header, 'y');
    const int gzCol = findGyroColumn(header, 'z');

    if (axCol < 0 || ayCol < 0 || azCol < 0 ||
        gxCol < 0 || gyCol < 0 || gzCol < 0) {
        if (err) {
            QStringList miss;
            if (axCol < 0) miss << QStringLiteral("加速度X");
            if (ayCol < 0) miss << QStringLiteral("加速度Y");
            if (azCol < 0) miss << QStringLiteral("加速度Z");
            if (gxCol < 0) miss << QStringLiteral("陀螺X");
            if (gyCol < 0) miss << QStringLiteral("陀螺Y");
            if (gzCol < 0) miss << QStringLiteral("陀螺Z");
            *err = QStringLiteral("未识别到列：%1（表头：%2）")
                       .arg(miss.join(QStringLiteral("、")))
                       .arg(header.join(QStringLiteral(", ")));
        }
        return false;
    }

    struct CsvRaw { double t, ax, ay, az, gx, gy, gz; };
    QVector<CsvRaw> raw;
    raw.reserve(lines.size());

    const int maxCol = qMax(tCol, qMax(qMax(axCol, ayCol),
                        qMax(azCol, qMax(gxCol, qMax(gyCol, gzCol)))));

    for (int i = 1; i < lines.size(); ++i) {
        const QStringList parts = splitLine(lines[i], delim);
        if (parts.size() <= maxCol)
            continue;

        bool ok = true;
        auto num = [&](int col) -> double {
            double v = parts[col].toDouble(&ok);
            return ok ? v : 0.0;
        };

        CsvRaw r;
        r.t  = num(tCol);
        r.ax = num(axCol);
        r.ay = num(ayCol);
        r.az = num(azCol);
        r.gx = num(gxCol);
        r.gy = num(gyCol);
        r.gz = num(gzCol);
        if (!ok)
            continue;   // 跳过非数值行
        raw.append(r);
    }

    if (raw.isEmpty()) {
        if (err) *err = QStringLiteral("未解析到有效数据行");
        return false;
    }

    // ── 单位识别 ──
    QVector<double> accMagRaw, gyrMagRaw;
    accMagRaw.reserve(raw.size());
    gyrMagRaw.reserve(raw.size());
    for (const CsvRaw &r : raw) {
        accMagRaw << std::sqrt(r.ax * r.ax + r.ay * r.ay + r.az * r.az);
        gyrMagRaw << std::sqrt(r.gx * r.gx + r.gy * r.gy + r.gz * r.gz);
    }

    const double accMed = median(accMagRaw);
    double accScale = 1.0;
    QString accText = QStringLiteral("m/s²");
    if (accMed > 100.0) {
        accScale = 0.01;      // cm/s² → m/s²
        accText = QStringLiteral("cm/s²（已换算为 m/s²）");
    } else if (accMed > 0.0 && accMed < 2.0) {
        accScale = 9.80665;   // g → m/s²
        accText = QStringLiteral("g（已换算为 m/s²）");
    }

    const double gyrQ90 = percentile(gyrMagRaw, 0.90);
    double gyrScale = 1.0;
    QString gyrText = QStringLiteral("rad/s");
    if (gyrQ90 > 5.0) {
        gyrScale = kPi / 180.0;   // °/s → rad/s
        gyrText = QStringLiteral("°/s（已换算为 rad/s）");
    }

    QVector<RawImu> conv;
    conv.reserve(raw.size());
    for (const CsvRaw &r : raw) {
        RawImu c;
        c.t  = r.t;
        c.ax = r.ax * accScale; c.ay = r.ay * accScale; c.az = r.az * accScale;
        c.gx = r.gx * gyrScale; c.gy = r.gy * gyrScale; c.gz = r.gz * gyrScale;
        conv.append(c);
    }

    ImuSummary &s = *out;
    s = ImuSummary();
    s.format     = ImuFormat::Csv;
    s.formatText = QStringLiteral("文本表格（CSV/TXT）");
    s.accelUnitText = accText;
    s.gyroUnitText  = gyrText;

    finalizeSummary(conv, out);
    return true;
}

// ════════════════════════════════════════════════════════════════
//  RAWIMUA（诺瓦泰二进制）
//    帧结构：AA 44 13 | 0x28 | MsgID 0x05B6 | ... | 数据 24B | CRC32(4B)
//    整帧 56 字节，头部 28 字节，CRC 覆盖前 52 字节
//    CRC32：poly 0xEDB88320，初值 0，无最终取反（非标准 CRC32）
//    时间：Byte8..11 = GPS 周内毫秒(u32 LE)，Byte14..15 = GPS 周(u16 LE)
// ════════════════════════════════════════════════════════════════
constexpr int kRawFrameLen  = 56;
constexpr int kRawDataOff   = 28;
constexpr double kWeekSec   = 604800.0;

// 加速度比例因子：优先由静止段（模值中位数 ≈ 1g）自动标定，
// 标定不可用时退回该默认值（m/s²/LSB）。
constexpr double kRawAccelScaleFallback = 3.0517578125e-5;   // 2^-15
// 角速度比例因子（rad/s/LSB）。不同 IMU 型号取值不同，后续可按实物核对。
constexpr double kRawGyroScale = 9.5367431640625e-7;         // 2^-20

const quint32 *crcTableRaw()
{
    static const std::array<quint32, 256> s = [] {
        std::array<quint32, 256> a{};
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1u) ? ((c >> 1) ^ 0xEDB88320u) : (c >> 1);
            a[i] = c;
        }
        return a;
    }();
    return s.data();
}

inline quint32 crc32Raw(const quint8 *p, int n, quint32 crc = 0)
{
    const quint32 *t = crcTableRaw();
    while (n--)
        crc = ((crc >> 8) & 0x00FFFFFFu) ^ t[(crc ^ *p++) & 0xFFu];
    return crc;
}

inline qint32 i32le(const quint8 *p)
{
    return qint32(quint32(p[0]) | (quint32(p[1]) << 8) |
                  (quint32(p[2]) << 16) | (quint32(p[3]) << 24));
}
inline quint32 u32le(const quint8 *p)
{
    return quint32(p[0]) | (quint32(p[1]) << 8) |
           (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
}
inline quint16 u16le(const quint8 *p)
{
    return quint16(quint16(p[0]) | (quint16(p[1]) << 8));
}

bool parseRawImuaImpl(const QByteArray &data, bool skipCrc, ImuSummary *out, QString *err)
{
    const quint8 *d = reinterpret_cast<const quint8 *>(data.constData());
    const int n = data.size();
    if (n < kRawFrameLen) {
        if (err) *err = QStringLiteral("文件太小，不足一帧（56 字节）");
        return false;
    }

    int crcFail = 0;
    // 帧遍历：checkCrc 控制是否做 CRC 校验；回调收到帧首指针。
    // 采用两次遍历（先标定比例因子、再产出样本），避免缓存整份中间数据。
    auto forEachFrame = [&](bool checkCrc, const std::function<void(const quint8 *)> &fn) {
        int i = 0;
        while (i + kRawFrameLen <= n) {
            if (!(d[i] == 0xAA && d[i + 1] == 0x44 && d[i + 2] == 0x13)) { ++i; continue; }
            // 头校验：头部长度 + MsgID(0x05B6)
            if (!(d[i + 3] == 0x28 && d[i + 4] == 0xB6 && d[i + 5] == 0x05)) { ++i; continue; }
            if (checkCrc && !skipCrc && crc32Raw(d + i, 52) != u32le(d + i + 52)) {
                ++crcFail; ++i;      // 逐字节前移重新同步
                continue;
            }
            fn(d + i);
            i += kRawFrameLen;
        }
    };

    // 第一遍：统计加速度模值，用于比例因子自动标定
    QVector<double> accMag;
    accMag.reserve(n / kRawFrameLen + 1);
    forEachFrame(true, [&](const quint8 *f) {
        const quint8 *p = f + kRawDataOff;
        const double z = i32le(p), y = i32le(p + 4), x = i32le(p + 8);
        accMag << std::sqrt(x * x + y * y + z * z);
    });

    if (accMag.isEmpty()) {
        if (err) {
            *err = QStringLiteral("未找到有效 RAWIMUA 帧（同步头/MsgID 不符，CRC 失败 %1 次）")
                       .arg(crcFail);
        }
        return false;
    }

    // 静止段占多数时，加速度模值中位数 ≈ 1g，据此反推比例因子
    const double med = median(accMag);
    double accScale = kRawAccelScaleFallback;
    QString accText = QStringLiteral("m/s²（比例因子按默认值换算）");
    if (med > 1.0) {
        accScale = 9.80665 / med;
        accText = QStringLiteral("m/s²（比例因子由静止段自动标定）");
    }

    // 第二遍：产出换算后的样本（CRC 已校验过，不再重复）
    QVector<RawImu> conv;
    conv.reserve(accMag.size());
    forEachFrame(false, [&](const quint8 *f) {
        RawImu c;
        c.t  = double(u16le(f + 14)) * kWeekSec + double(u32le(f + 8)) / 1000.0;
        const quint8 *p = f + kRawDataOff;
        c.az = double(i32le(p))      * accScale;
        c.ay = double(i32le(p + 4))  * accScale;
        c.ax = double(i32le(p + 8))  * accScale;
        c.gz = double(i32le(p + 12)) * kRawGyroScale;
        c.gy = double(i32le(p + 16)) * kRawGyroScale;
        c.gx = double(i32le(p + 20)) * kRawGyroScale;
        conv.append(c);
    });

    ImuSummary &s = *out;
    s = ImuSummary();
    s.format     = ImuFormat::RawImua;
    s.formatText = QStringLiteral("诺瓦泰 RAWIMUA（二进制，%1 帧%2）")
                       .arg(conv.size())
                       .arg(crcFail > 0 ? QStringLiteral("，CRC 失败 %1 次").arg(crcFail)
                                        : QString());
    s.accelUnitText = accText;
    s.gyroUnitText  = QStringLiteral("rad/s（比例因子 2^-20，可按实物核对）");

    finalizeSummary(conv, out);
    return true;
}

bool looksLikeRawImua(const QByteArray &data)
{
    const int lim = qMin(data.size(), 4096);
    for (int i = 0; i + 2 < lim; ++i) {
        if (static_cast<quint8>(data[i])     == 0xAA &&
            static_cast<quint8>(data[i + 1]) == 0x44 &&
            static_cast<quint8>(data[i + 2]) == 0x13)
            return true;
    }
    return false;
}

} // namespace

bool ImuReader::analyze(const QString &file, ImuSummary *out, QString *err, bool skipCrc)
{
    if (!out) return false;

    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("无法打开文件：%1").arg(file);
        return false;
    }
    const QByteArray data = f.readAll();
    f.close();

    if (data.isEmpty()) {
        if (err) *err = QStringLiteral("文件为空");
        return false;
    }

    if (looksLikeRawImua(data))
        return parseRawImuaImpl(data, skipCrc, out, err);
    return parseCsvImpl(data, out, err);
}

} // namespace Analysis
