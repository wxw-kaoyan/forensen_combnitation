#ifndef ANALYSIS_COMPARE_COMPARECHARTWINDOW_H
#define ANALYSIS_COMPARE_COMPARECHARTWINDOW_H

#include <QWidget>
#include <QString>
#include <QVector>

QT_BEGIN_NAMESPACE
class QChartView;
class QLineSeries;
class QValueAxis;
class QComboBox;
QT_END_NAMESPACE

namespace Analysis {

// ════════════════════════════════════════════════════════════════
//  单个对比项差值图窗口
//  以独立非模态窗口展示「误差曲线（对比值 - 真值）」，避免主界面臃肿，
//  且不阻塞主 UI 线程。
//
//  横轴时间单位可切换：UTC 天内秒（默认）/ UTC 天内毫秒。
//  内部统一接收 UTC 绝对秒（Unix 纪元），显示时换算到「天内」。
// ════════════════════════════════════════════════════════════════
class CompareChartWindow : public QWidget
{
    Q_OBJECT
public:
    CompareChartWindow(const QString &title,
                       const QString &yUnit,
                       const QVector<double> &timeUtc,   // UTC 绝对秒（Unix 纪元）
                       const QVector<double> &error,
                       QWidget *parent = nullptr);

private slots:
    void onUnitChanged(int idx);

private:
    void rebuildSeries();
    double toTimeValue(double utcAbsSec) const;

    QChartView  *m_view    = nullptr;
    QLineSeries *m_series  = nullptr;
    QValueAxis  *m_xAxis   = nullptr;
    QValueAxis  *m_yAxis   = nullptr;
    QComboBox   *m_unitCombo = nullptr;

    QVector<double> m_timeUtc;   // UTC 绝对秒
    QVector<double> m_error;
    bool m_millis = false;       // false = 天内秒（默认），true = 天内毫秒
};

} // namespace Analysis

#endif // ANALYSIS_COMPARE_COMPARECHARTWINDOW_H
