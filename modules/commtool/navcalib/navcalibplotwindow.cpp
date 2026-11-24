#include "navcalibplotwindow.h"

#include <QPainter>
#include <QPen>
#include <QVBoxLayout>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLegend>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>

#include <algorithm>

NavCalibPlotWindow::NavCalibPlotWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("超达投影估计轨迹"));
    resize(720, 620);

    m_raw = new QScatterSeries(this);
    m_raw->setName(QStringLiteral("实际轨迹点"));
    m_raw->setMarkerSize(3.0);
    m_raw->setColor(QColor("#2196f3"));

    // 按采样顺序把点连成线：直接呈现真实运动形状（圆/螺旋/乱走）
    m_path = new QLineSeries(this);
    m_path->setName(QStringLiteral("实际运动轨迹"));
    QPen pathPen(QColor("#1565c0"));
    pathPen.setWidth(1);
    pathPen.setColor(QColor(21, 101, 192, 140));
    m_path->setPen(pathPen);

    m_proj = new QScatterSeries(this);
    m_proj->setName(QStringLiteral("补偿后点云"));
    m_proj->setMarkerSize(4.0);
    m_proj->setColor(QColor("#43a047"));

    m_chart = new QChart;
    m_chart->addSeries(m_path);
    m_chart->addSeries(m_raw);
    m_chart->addSeries(m_proj);
    m_chart->setTitle(QStringLiteral("超达投影估计 · 实际运动轨迹（横轴东向 E，纵轴北向 N）"));
    m_chart->setAnimationOptions(QChart::NoAnimation);
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);

    m_xAxis = new QValueAxis;
    m_xAxis->setTitleText(QStringLiteral("东向 E (m)"));
    m_yAxis = new QValueAxis;
    m_yAxis->setTitleText(QStringLiteral("北向 N (m)"));
    m_chart->addAxis(m_xAxis, Qt::AlignBottom);
    m_chart->addAxis(m_yAxis, Qt::AlignLeft);

    m_raw->attachAxis(m_xAxis);
    m_raw->attachAxis(m_yAxis);
    m_path->attachAxis(m_xAxis);
    m_path->attachAxis(m_yAxis);
    m_proj->attachAxis(m_xAxis);
    m_proj->attachAxis(m_yAxis);

    m_view = new QChartView(m_chart, this);
    m_view->setRenderHint(QPainter::Antialiasing);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addWidget(m_view);

    m_xAxis->setRange(-1.0, 1.0);
    m_yAxis->setRange(-1.0, 1.0);
}

void NavCalibPlotWindow::setData(const QVector<QPointF> &raw,
                                 const QVector<QPointF> &projected,
                                 bool hasFit)
{
    if (raw.isEmpty() && projected.isEmpty()) {
        clearData();
        return;
    }

    // 实际运动轨迹：按采样顺序连线，不做任何拟合
    m_raw->replace(raw);
    m_path->replace(raw);

    if (hasFit) {
        m_proj->replace(projected);
        m_proj->setVisible(true);
    } else {
        m_proj->clear();
        m_proj->setVisible(false);
    }

    // 两轴同跨度
    double minX = 1e18, maxX = -1e18, minY = 1e18, maxY = -1e18;
    auto accumulate = [&](const QVector<QPointF> &pts) {
        for (const QPointF &p : pts) {
            minX = std::min(minX, p.x());
            maxX = std::max(maxX, p.x());
            minY = std::min(minY, p.y());
            maxY = std::max(maxY, p.y());
        }
    };
    accumulate(raw);
    // 补偿点云聚集在中心附近，不参与范围计算，
    // 否则会把视野压得过小、看不清真实运动形状

    if (minX > maxX) {
        m_xAxis->setRange(-1.0, 1.0);
        m_yAxis->setRange(-1.0, 1.0);
        return;
    }
    applyEqualRange(minX, maxX, minY, maxY);
}

void NavCalibPlotWindow::clearData()
{
    m_raw->clear();
    m_path->clear();
    m_proj->clear();
    m_proj->setVisible(false);
    m_xAxis->setRange(-1.0, 1.0);
    m_yAxis->setRange(-1.0, 1.0);
}

void NavCalibPlotWindow::applyEqualRange(double minX, double maxX,
                                         double minY, double maxY)
{
    const double spanX = std::max(maxX - minX, 0.2);
    const double spanY = std::max(maxY - minY, 0.2);
    const double span  = std::max(spanX, spanY) * 1.15;   // 留 15% 边距
    const double cx = (minX + maxX) * 0.5;
    const double cy = (minY + maxY) * 0.5;
    m_xAxis->setRange(cx - span * 0.5, cx + span * 0.5);
    m_yAxis->setRange(cy - span * 0.5, cy + span * 0.5);
}
