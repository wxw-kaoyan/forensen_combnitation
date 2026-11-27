#ifndef TEMPDRIFTWORKER_H
#define TEMPDRIFTWORKER_H

#include <QObject>
#include <QString>
#include <QVector>

#include "modules/normalfunction/tempdrift/tempdriftanalysis.h"

// ════════════════════════════════════════════════════════════════
//  拉温分析后台 Worker（读文件 + 平滑/统计，全部在后台线程）
// ════════════════════════════════════════════════════════════════
class TempDriftWorker : public QObject
{
    Q_OBJECT

public:
    explicit TempDriftWorker(QObject *parent = nullptr);

    // ── 由 Panel 设置 ──
    QString      paramsFilePath;
    QVector<int> paramsColumns;          // 7 列：ax ay az gx gy gz temp
    int          paramsSkipHead = 0;
    double       paramsFs       = 100.0;
    double       paramsRampThreshold = 0.5;   // ℃/min

public slots:
    void computeQueued();

signals:
    void progress(int percent);
    void resultReady(const TempDriftAnalysis::Result &result);
    void error(const QString &msg);

private:
    bool loadColumns(const QString &path, const QVector<int> &cols, int skipHead,
                     QVector<QVector<double>> &out, int &badLines);
};

#endif // TEMPDRIFTWORKER_H
