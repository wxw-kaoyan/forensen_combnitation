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
//    · 原始轨迹散点（蓝）
//    · 拟合圆（橙，圆心 + |杆臂| 半径）
//    · 补偿后点云（绿，正常应收缩为一个点）
//  横轴东向 E、纵轴北向 N，两轴强制同跨度，保证圆不会画成椭圆。
// ════════════════════════════════════════════════════════════════
class NavCalibPlotWindow : public QWidget
{
    Q_OBJECT
public:
    explicit NavCalibPlotWindow(QWidget *parent = nullptr);

    // 传空轨迹即清空；hasFit=false 时隐藏圆与补偿点云
    void setData(const QVector<QPointF> &raw,
                 const QVector<QPointF> &projected,
                 bool hasFit,
                 double centerN, double centerE,
                 double lx, double ly);

    void clearData();

private:
    void applyEqualRange(double minX, double maxX, double minY, double maxY);

    QChart        *m_chart  = nullptr;
    QChartView    *m_view   = nullptr;
    QScatterSeries *m_raw   = nullptr;
    QScatterSeries *m_proj  = nullptr;
    QLineSeries   *m_circle = nullptr;
    QValueAxis    *m_xAxis  = nullptr;
    QValueAxis    *m_yAxis  = nullptr;
};

#endif // NAVCALIB_NAVCALIBPLOTWINDOW_H
