#ifndef CALIB612WORKER_H
#define CALIB612WORKER_H

#include <QObject>
#include <QString>
#include <QVector>

#include "modules/normalfunction/calib612/calib612analysis.h"

// ════════════════════════════════════════════════════════════════
//  6+12 位置标定后台 Worker
//
//  调用方式与 AllanWorker 一致：
//    Panel 填 params* → invokeMethod(computeQueued, QueuedConnection)
//    结果通过信号回 UI 线程
// ════════════════════════════════════════════════════════════════
class Calib612Worker : public QObject
{
    Q_OBJECT

public:
    explicit Calib612Worker(QObject *parent = nullptr);

    // ── 由 Panel 设置 ──
    QString      paramsFilePath;
    QVector<int> paramsColumns;       // 6 个列索引 (0-based)：ax ay az gx gy gz
    int          paramsSkipHead = 0;
    double       paramsFs       = 100.0;
    double       paramsGyroRate = 100.0;

public slots:
    void computeQueued();

signals:
    void progress(int percent);
    void resultReady(const Calib612Analysis::Result &result);
    void error(const QString &msg);

private:
    bool loadColumns(const QString &path, const QVector<int> &cols, int skipHead,
                     QVector<QVector<double>> &out, int &badLines);
};

#endif // CALIB612WORKER_H
