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
class QScrollBar;

namespace Analysis {

// ════════════════════════════════════════════════════════════════
//  通用曲线窗口
//  X 轴为时间（或任意数值列），Y 轴为若干条数据曲线（共享 X 轴）。
//  触摸板：左右滑缩放 X 轴 / 上下滑缩放 Y 轴；鼠标：滚轮缩放 X 轴 / Ctrl+滚轮缩放 Y 轴；
//  左键拖拽平移 X/Y 轴，以及顶部起止时间输入过滤。
//  底部横向移动条 / 右侧纵向移动条：平移 X/Y 轴（滑块长度 = 可见区间占比）。
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

    // trajectory = true 时按"经纬度轨迹"处理：X 为经度、Y 为纬度，不做采样率抽稀
    ChartWindow(const QString &title,
                const QString &xLabel,
                const QVector<double> &x,
                const QVector<Curve> &curves,
                bool trajectory = false,
                QWidget *parent = nullptr);

private slots:
    void onApplyRange();        // 按顶部起止时间输入设置 X 轴范围
    void onResetRange();        // 恢复全量范围
    void onToggleDownsample();  // 切换 10Hz 抽稀 / 真实采样率
    void onXScroll(int value);  // 底部横向移动条 → 平移 X 轴
    void onYScroll(int value);  // 右侧纵向移动条 → 平移 Y 轴

private:
    void rebuildSeries();               // 按当前抽稀状态重建曲线
    double computeSampleHz() const;     // 自动推断时间单位后估算采样率
    void syncScrollbars();              // 轴范围变化后同步两个移动条

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

    QScrollBar         *m_hScroll = nullptr;
    QScrollBar         *m_vScroll = nullptr;
    bool    m_syncing  = false;     // 防止移动条 ↔ 轴范围 递归触发
    double  m_xFullMin = 0.0;       // X 全量范围
    double  m_xFullMax = 1.0;
    double  m_yFullMin = 0.0;       // Y 全量范围（所有曲线）
    double  m_yFullMax = 1.0;

    double  m_sourceHz     = 0.0;   // 推断出的真实采样率
    int     m_step         = 1;     // 抽稀步长（1 = 不抽稀）
    bool    m_downsampled  = false; // 当前是否抽稀显示
};

} // namespace Analysis

#endif // ANALYSIS_CHART_CHARTWINDOW_H
