#ifndef ALLANWORKER_H
#define ALLANWORKER_H

#include <QObject>
#include <QString>
#include <QVector>

#include "modules/normalfunction/allan/allananalysis.h"

// ════════════════════════════════════════════════════════════════
//  Allan 方差计算后台 Worker
//
//  调用方式：
//    1. Panel 设置 params 成员
//    2. QMetaObject::invokeMethod(worker, &AllanWorker::computeQueued, Qt::QueuedConnection)
//       零参数 slot 避免了 Q_ARG 注册元类型的麻烦
//  所有结果通过 signal 传回 UI 线程。
// ════════════════════════════════════════════════════════════════
class AllanWorker : public QObject
{
    Q_OBJECT

public:
    explicit AllanWorker(QObject *parent = nullptr);

    // ── 由 Panel 设置，随后 computeQueued() 会读取 ──
    QString          paramsCsvPath;
    QVector<int>     paramsColumns;    // 要读的列索引 (0-based)
    int              paramsSkipHead = 0;
    double           paramsFs       = 100.0;

public slots:
    /** 零参数入口 —— Panel 用 invokeMethod(QueuedConnection) 调到后台线程 */
    void computeQueued();

signals:
    void progress(int percent);
    void resultReady(const AllanAnalysis::MultiResult &mr,
                     const QVector<AllanAnalysis::AccelParams> &acc,
                     const QVector<AllanAnalysis::GyroParams> &gyr);
    void error(const QString &msg);

private:
    /** 简单 CSV 读：逗号/分号/制表符分隔，空行跳过，坏值跳过 */
    bool loadCsvCols(const QString &path,
                     const QVector<int> &cols,
                     int skipHead,
                     QVector<QVector<double>> &out,
                     int &badLines);
};

#endif // ALLANWORKER_H
