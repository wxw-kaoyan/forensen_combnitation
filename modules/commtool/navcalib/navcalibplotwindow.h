#ifndef NAVCALIB_NAVCALIBPLOTWINDOW_H
#define NAVCALIB_NAVCALIBPLOTWINDOW_H

#include <QPointF>
#include <QVector>
#include <QWidget>

class QChart;
class QChartView;
class QLineSeries;
class QScatterSeries;
class QValueAxis;

// ════════════════════════════════════════════════════════════════
//  杆臂标定轨迹窗（非模态弹窗，实时刷新）
//    · 实际运动轨迹（蓝，按时间顺序连线 + 散点）
//    · 补偿后点云（绿，正常应收缩为一个点）
//  不叠加任何拟合圆/椭圆——直接看真实运动形状，判断是否稳定绕圆。
//  横轴东向 E、纵轴北向 N，两轴强制同跨度。
// ════════════════════════════════════════════════════════════════
class NavCalibPlotWindow : public QWidget
{
    Q_OBJECT
public:
    explicit NavCalibPlotWindow(QWidget *parent = nullptr);

    // 传空轨迹即清空；hasFit=false 时隐藏补偿点云
    void setData(const QVector<QPointF> &raw,
                 const QVector<QPointF> &projected,
                 bool hasFit);

    void clearData();

private:
    void applyEqualRange(double minX, double maxX, double minY, double maxY);

    QChart        *m_chart  = nullptr;
    QChartView    *m_view   = nullptr;
    QScatterSeries *m_raw   = nullptr;
    QScatterSeries *m_proj  = nullptr;
    QLineSeries   *m_path   = nullptr;   // 实际运动轨迹（按采样顺序连线）
    QValueAxis    *m_xAxis  = nullptr;
    QValueAxis    *m_yAxis  = nullptr;
};

#endif // NAVCALIB_NAVCALIBPLOTWINDOW_H
