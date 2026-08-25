#include "imureader.h"

#include <QFile>
#include <QTextStream>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

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

} // namespace

bool ImuReader::analyze(const QString &file, ImuSummary *out, QString *err)
{
    if (!out) return false;

    QFile f(file);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("无法打开文件：%1").arg(file);
        return false;
    }

    QStringList lines;
    QTextStream ts(&f);
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (!line.isEmpty())
            lines << line;
    }
    f.close();

    if (lines.isEmpty()) {
        if (err) *err = QStringLiteral("文件为空");
        return false;
    }

    const QChar delim = detectDelimiter(lines.first());
    const QStringList header = splitLine(lines.first(), delim);

    // 识别列
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

    // 读原始样本
    struct Raw { double t, ax, ay, az, gx, gy, gz; };
    QVector<Raw> raw;
    raw.reserve(lines.size() - 1);

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

        Raw r;
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
    // 加速度：模值中位数判断（静止段占多数时接近重力）
    //   m/s² 静止 ≈ 9.8；g 静止 ≈ 1；cm/s² 静止 ≈ 980
    QVector<double> accMagRaw, gyrMagRaw;
    accMagRaw.reserve(raw.size());
    gyrMagRaw.reserve(raw.size());
    for (const Raw &r : raw) {
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

    // ── 换算 + 填充样本（t 为相对首样本秒） ──
    const double t0 = raw.first().t;
    ImuSummary &s = *out;
    s = ImuSummary();
    s.accelUnitText = accText;
    s.gyroUnitText = gyrText;
    s.firstTime = t0;
    s.lastTime = raw.last().t;
    s.durationSec = s.lastTime - s.firstTime;

    s.samples.reserve(raw.size());
    s.accelMag.reserve(raw.size());
    s.gyroMag.reserve(raw.size());

    QVector<double> dtList;
    dtList.reserve(raw.size() - 1);

    for (int i = 0; i < raw.size(); ++i) {
        const Raw &r = raw[i];
        ImuSample smp;
        smp.t  = r.t - t0;
        smp.ax = r.ax * accScale;
        smp.ay = r.ay * accScale;
        smp.az = r.az * accScale;
        smp.gx = r.gx * gyrScale;
        smp.gy = r.gy * gyrScale;
        smp.gz = r.gz * gyrScale;
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

    // ── 采样率 + 丢包 ──
    const double dtMed = median(dtList);
    if (dtMed > 0.0) {
        s.sampleRate = 1.0 / dtMed;
        const double tol = dtMed * 1.5;   // 间隔容差
        for (double dt : dtList) {
            if (dt > tol) {
                const int dropped = qMax(0, qRound(dt / dtMed) - 1);
                s.gapCount += dropped;
                s.totalGapSec += dt - dtMed;
            }
        }
    }

    return true;
}

} // namespace Analysis
