#ifndef ANALYSIS_COMPARE_COMPARECHARTWINDOW_H
#define ANALYSIS_COMPARE_COMPARECHARTWINDOW_H

#include <QWidget>
#include <QString>
#include <QVector>

#include <QtCharts/QChartView>

namespace Analysis {

// ════════════════════════════════════════════════════════════════
//  单个对比项差值图窗口
//  以独立非模态窗口展示「误差曲线（对比值 - 真值）」，避免主界面臃肿，
//  且不阻塞主 UI 线程。
// ════════════════════════════════════════════════════════════════
class CompareChartWindow : public QWidget
{
    Q_OBJECT
public:
    CompareChartWindow(const QString &title,
                       const QString &yUnit,
                       const QVector<double> &time,
                       const QVector<double> &error,
                       QWidget *parent = nullptr);

private:
    QChartView *m_view = nullptr;
};

} // namespace Analysis

#endif // ANALYSIS_COMPARE_COMPARECHARTWINDOW_H
