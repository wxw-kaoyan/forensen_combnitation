#ifndef ANALYSIS_CHART_CHARTWINDOW_H
#define ANALYSIS_CHART_CHARTWINDOW_H

#include <QWidget>
#include <QString>
#include <QVector>
#include <QColor>

class QChart;
class QChartView;
class QLineSeries;
class QValueAxis;
class QDoubleSpinBox;
class QPushButton;

namespace Analysis {

// ════════════════════════════════════════════════════════════════
//  通用曲线窗口
//  X 轴为时间（或任意数值列），Y 轴为若干条数据曲线（共享 X 轴）。
//  支持滚轮缩放 X 轴 / Ctrl+滚轮缩放 Y 轴 / 左键拖拽平移，
//  以及顶部起止时间输入过滤。
//  采样率 >10Hz 时默认抽稀到 10Hz 显示，可一键恢复真实采样率。
//  独立非模态窗口，可同时开多个用于对比。
// ════════════════════════════════════════════════════════════════
class ChartWindow : public QWidget
{
    Q_OBJECT
public:
    // 一条曲线（与 x 等长）
    struct Curve {
        QString         name;
        QVector<double> y;
        QColor          color;
    };

    ChartWindow(const QString &title,
                const QString &xLabel,
                const QVector<double> &x,
                const QVector<Curve> &curves,
                QWidget *parent = nullptr);

private slots:
    void onApplyRange();        // 按顶部起止时间输入设置 X 轴范围
    void onResetRange();        // 恢复全量范围
    void onToggleDownsample();  // 切换 10Hz 抽稀 / 真实采样率

private:
    void rebuildSeries();               // 按当前抽稀状态重建曲线
    double computeSampleHz() const;     // 自动推断时间单位后估算采样率

    QVector<double>     m_x;
    QVector<Curve>      m_curves;

    QChart             *m_chart  = nullptr;
    QChartView         *m_view   = nullptr;
    QValueAxis         *m_xAxis  = nullptr;
    QValueAxis         *m_yAxis  = nullptr;
    QVector<QLineSeries*> m_series;

    QDoubleSpinBox     *m_minSpin = nullptr;
    QDoubleSpinBox     *m_maxSpin = nullptr;
    QPushButton        *m_dsBtn   = nullptr;

    double  m_sourceHz     = 0.0;   // 推断出的真实采样率
    int     m_step         = 1;     // 抽稀步长（1 = 不抽稀）
    bool    m_downsampled  = false; // 当前是否抽稀显示
};

} // namespace Analysis

#endif // ANALYSIS_CHART_CHARTWINDOW_H
