#include "truthreader.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QtGlobal>
#include <algorithm>

namespace Analysis {

namespace {

QString norm(const QString &s)
{
    QString r = s.toLower();
    r.remove('-');
    r.remove('_');
    r.remove(' ');
    return r;
}

int findColumn(const QStringList &header, const QStringList &keys)
{
    for (int i = 0; i < header.size(); ++i) {
        const QString h = norm(header[i]);
        for (const QString &k : keys) {
            if (h == k)
                return i;
        }
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

} // namespace

bool TruthReader::analyze(const QString &file, TruthSummary *out, QString *err)
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

    // 定位列名表头行：同时含 Latitude 与 Longitude（头部元信息再怎么变都不受影响）
    int headerIdx = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].contains(QStringLiteral("Latitude")) &&
            lines[i].contains(QStringLiteral("Longitude"))) {
            headerIdx = i;
            break;
        }
    }
    if (headerIdx < 0) {
        if (err) *err = QStringLiteral("未找到表头（Latitude/Longitude）");
        return false;
    }

    const QStringList header = lines[headerIdx].split(' ', Qt::SkipEmptyParts);

    int tCol = findColumn(header, {QStringLiteral("gpstime")});
    if (tCol < 0)
        tCol = findColumn(header, {QStringLiteral("utctime")});

    const int latCol   = findColumn(header, {QStringLiteral("latitude"), QStringLiteral("lat")});
    const int lonCol   = findColumn(header, {QStringLiteral("longitude"), QStringLiteral("lon")});
    const int hCol     = findColumn(header, {QStringLiteral("hell"), QStringLiteral("hgt"), QStringLiteral("height")});
    const int vnCol    = findColumn(header, {QStringLiteral("veln")});
    const int veCol    = findColumn(header, {QStringLiteral("vele")});
    const int vuCol    = findColumn(header, {QStringLiteral("velu")});
    const int rollCol  = findColumn(header, {QStringLiteral("roll")});
    const int pitchCol = findColumn(header, {QStringLiteral("pitch")});
    const int headCol  = findColumn(header, {QStringLiteral("heading")});
    const int satCol   = findColumn(header, {QStringLiteral("satuse")});

    qDebug() << "[TruthReader] headerIdx" << headerIdx << "header" << header;
    qDebug() << "[TruthReader] cols: lat" << latCol << "lon" << lonCol
             << "h" << hCol << "sat" << satCol << "time" << tCol;

    if (latCol < 0 || lonCol < 0) {
        if (err) *err = QStringLiteral("未识别到经纬度列");
        return false;
    }

    // 数据从表头行的下一行（单位行）之后开始
    const int dataStart = headerIdx + 2;

    int maxCol = qMax(tCol, qMax(latCol, qMax(lonCol, hCol)));
    maxCol = qMax(maxCol, qMax(vnCol, qMax(veCol, qMax(vuCol,
                 qMax(rollCol, qMax(pitchCol, qMax(headCol, satCol)))))));

    struct Raw { TruthSample s; double rawT = 0; };
    QVector<Raw> tmp;
    tmp.reserve(lines.size() - dataStart);

    for (int i = dataStart; i < lines.size(); ++i) {
        const QStringList parts = lines[i].split(' ', Qt::SkipEmptyParts);
        if (parts.size() <= maxCol)
            continue;

        bool ok = true;
        auto num = [&](int col) -> double {
            if (col < 0) return 0.0;
            double v = parts[col].toDouble(&ok);
            return ok ? v : 0.0;
        };

        Raw r;
        r.s.lat     = num(latCol);
        r.s.lon     = num(lonCol);
        r.s.h       = num(hCol);
        r.s.velN    = num(vnCol);
        r.s.velE    = num(veCol);
        r.s.velU    = num(vuCol);
        r.s.roll    = num(rollCol);
        r.s.pitch   = num(pitchCol);
        r.s.heading = num(headCol);
        r.s.satUse  = static_cast<int>(num(satCol));

        if (tCol >= 0) {
            double v = parts[tCol].toDouble(&ok);
            if (ok) r.rawT = v;
        }
        if (!ok)
            continue;
        tmp.append(r);
    }

    if (tmp.isEmpty()) {
        if (err) *err = QStringLiteral("未解析到有效数据行");
        return false;
    }

    const bool hasTime = (tCol >= 0);
    const double t0 = hasTime ? tmp.first().rawT : 0.0;

    TruthSummary &s = *out;
    s = TruthSummary();
    s.samples.reserve(tmp.size());
    s.firstTime = t0;

    QVector<double> dtList;
    dtList.reserve(tmp.size() - 1);

    for (int i = 0; i < tmp.size(); ++i) {
        TruthSample smp = tmp[i].s;
        smp.t = hasTime ? (tmp[i].rawT - t0) : static_cast<double>(i);
        s.samples.append(smp);

        if (i > 0)
            dtList << (s.samples[i].t - s.samples[i - 1].t);

        if (i == 0) {
            s.minLat = s.maxLat = smp.lat;
            s.minLon = s.maxLon = smp.lon;
            s.minH = s.maxH = smp.h;
        } else {
            s.minLat = qMin(s.minLat, smp.lat);
            s.maxLat = qMax(s.maxLat, smp.lat);
            s.minLon = qMin(s.minLon, smp.lon);
            s.maxLon = qMax(s.maxLon, smp.lon);
            s.minH = qMin(s.minH, smp.h);
            s.maxH = qMax(s.maxH, smp.h);
        }
    }

    s.totalSamples = s.samples.size();
    s.lastTime = t0 + s.samples.last().t;
    s.durationSec = s.samples.last().t;

    const double dtMed = median(dtList);
    if (dtMed > 0.0)
        s.sampleRate = 1.0 / dtMed;

    qDebug() << "[TruthReader] totalSamples" << s.totalSamples
             << "lat" << s.minLat << s.maxLat
             << "lon" << s.minLon << s.maxLon
             << "h" << s.minH << s.maxH;
    if (!s.samples.isEmpty()) {
        const auto &p = s.samples.first();
        qDebug() << "[TruthReader] first sample t/lat/lon/sat:" << p.t << p.lat << p.lon << p.satUse;
    }

    return true;
}

} // namespace Analysis
