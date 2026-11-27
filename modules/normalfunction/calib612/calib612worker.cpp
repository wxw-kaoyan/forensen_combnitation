#include "calib612worker.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

#include <algorithm>

Calib612Worker::Calib612Worker(QObject *parent) : QObject(parent) {}

bool Calib612Worker::loadColumns(const QString &path, const QVector<int> &cols, int skipHead,
                                 QVector<QVector<double>> &out, int &badLines)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit error(QStringLiteral("无法打开文件: %1").arg(path));
        return false;
    }

    QTextStream ts(&f);
    for (int i = 0; i < skipHead && !ts.atEnd(); ++i)
        ts.readLine();

    out.resize(cols.size());
    badLines = 0;

    const int maxCol = *std::max_element(cols.begin(), cols.end());
    QChar sep;
    bool sepDecided = false;
    int lineIdx = 0;

    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (line.isEmpty())
            continue;

        if (!sepDecided) {
            if (line.contains(QLatin1Char(',')))      sep = QLatin1Char(',');
            else if (line.contains(QLatin1Char(';'))) sep = QLatin1Char(';');
            else if (line.contains(QLatin1Char('\t'))) sep = QLatin1Char('\t');
            sepDecided = true;
        }

        const QStringList parts = sep.isNull()
            ? line.split(QRegularExpression(QStringLiteral("[\\s]+")), Qt::SkipEmptyParts)
            : line.split(sep, Qt::SkipEmptyParts);

        if (parts.size() <= maxCol) { ++badLines; continue; }

        bool ok = true;
        double vals[6] = {0, 0, 0, 0, 0, 0};
        for (int j = 0; j < cols.size(); ++j) {
            const int c = cols[j];
            if (c < 0 || c >= parts.size()) { ok = false; break; }
            vals[j] = parts[c].toDouble(&ok);
            if (!ok) break;
        }
        if (!ok) { ++badLines; continue; }

        for (int j = 0; j < cols.size(); ++j)
            out[j].append(vals[j]);

        if (++lineIdx % 100000 == 0)
            emit progress(std::min(40, lineIdx / 100000 * 5));
    }

    if (out.isEmpty() || out.first().size() < 200) {
        emit error(QStringLiteral("有效数据不足 200 行（当前 %1 行）")
                       .arg(out.isEmpty() ? 0 : out.first().size()));
        return false;
    }
    return true;
}

void Calib612Worker::computeQueued()
{
    emit progress(0);

    QVector<QVector<double>> cols;
    int bad = 0;
    if (!loadColumns(paramsFilePath, paramsColumns, paramsSkipHead, cols, bad))
        return;

    emit progress(45);

    const Calib612Analysis::Result r =
        Calib612Analysis::compute(cols, paramsFs, paramsGyroRate, 10.0);

    emit progress(100);
    emit resultReady(r);
}
