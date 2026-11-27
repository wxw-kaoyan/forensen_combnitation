#include "allanworker.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

AllanWorker::AllanWorker(QObject *parent) : QObject(parent) {}

bool AllanWorker::loadCsvCols(const QString &path,
                              const QVector<int> &cols,
                              int skipHead,
                              QVector<QVector<double>> &out,
                              int &badLines)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit error(QStringLiteral("无法打开文件: %1").arg(path));
        return false;
    }

    QTextStream ts(&f);
    for (int i = 0; i < skipHead && !ts.atEnd(); ++i) ts.readLine();

    out.resize(cols.size());
    badLines = 0;
    const int totalApprox = 100000;  // 每 10 万行给一次进度提示

    int lineIdx = 0;
    while (!ts.atEnd()) {
        QString line = ts.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList parts = line.split(
            QRegularExpression(QStringLiteral("[,;\\t]+")), Qt::SkipEmptyParts);
        if (parts.size() < cols.size()) { ++badLines; continue; }

        bool ok = true;
        QVector<double> row(cols.size());
        for (int j = 0; j < cols.size(); ++j) {
            if (cols[j] < 0 || cols[j] >= parts.size()) { ok = false; break; }
            row[j] = parts[cols[j]].toDouble(&ok);
            if (!ok) break;
        }
        if (!ok) { ++badLines; continue; }

        for (int j = 0; j < cols.size(); ++j)
            out[j].append(row[j]);

        if (++lineIdx % totalApprox == 0) {
            QMetaObject::invokeMethod(this, [this, lineIdx]() {
                emit progress(std::min(25, lineIdx / totalApprox * 5));
            }, Qt::QueuedConnection);
        }
    }

    if (out.isEmpty() || out.first().size() < 100) {
        emit error(QStringLiteral("有效数据不足 100 行（当前 %1）").arg(out.isEmpty() ? 0 : out.first().size()));
        return false;
    }
    return true;
}

void AllanWorker::computeQueued()
{
    const QString &csvPath   = paramsCsvPath;
    const auto    &columns   = paramsColumns;
    const int      skipHead  = paramsSkipHead;
    const double   fs        = paramsFs;

    emit progress(0);

    QVector<QVector<double>> cols;
    int bad = 0;
    if (!loadCsvCols(csvPath, columns, skipHead, cols, bad)) return;

    emit progress(30);

    // 等长检查
    int N = cols.first().size();
    for (int c = 1; c < cols.size(); ++c) {
        if (cols[c].size() != N) {
            emit error(QStringLiteral("第 %1 列长度不匹配").arg(c));
            return;
        }
    }

    emit progress(50);
    auto mr = AllanAnalysis::compute6(cols, fs);
    emit progress(75);

    auto acc = AllanAnalysis::extractAccel3(mr, cols, fs);
    auto gyr = AllanAnalysis::extractGyro3(mr, cols, fs);
    emit progress(100);

    emit resultReady(mr, acc, gyr);
}
