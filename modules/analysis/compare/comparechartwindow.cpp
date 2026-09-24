#include "comparechartwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPainter>
#include <QPen>
#include <QColor>

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <cmath>

namespace Analysis {

CompareChartWindow::CompareChartWindow(const QString &title,
                                       const QString &yUnit,
                                       const QVector<double> &timeUtc,
                                       const QVector<double> &error,
                                       QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_timeUtc(timeUtc)
    , m_error(error)
{
    setWindowTitle(title);
    setWindowModality(Qt::WindowModal);
    resize(960, 560);

    auto *chart = new QChart();
    chart->setTitle(title);
    chart->setAnimationOptions(QChart::NoAnimation);

    m_series = new QLineSeries(chart);
    m_series->setName(QStringLiteral("误差（对比-真值）"));
    m_series->setColor(QColor(239, 68, 68));      // 红
    m_series->setPen(QPen(QColor(239, 68, 68), 1.5));

    chart->addSeries(m_series);
    chart->createDefaultAxes();

    m_xAxis = qobject_cast<QValueAxis *>(chart->axes(Qt::Horizontal).value(0));
    m_yAxis = qobject_cast<QValueAxis *>(chart->axes(Qt::Vertical).value(0));
    if (m_yAxis)
        m_yAxis->setTitleText(yUnit);

    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    m_view = new QChartView(chart, this);
    m_view->setRenderHint(QPainter::Antialiasing, true);

    // 横轴时间单位切换：默认 UTC 天内秒，可选天内毫秒
    m_unitCombo = new QComboBox(this);
    m_unitCombo->addItem(QStringLiteral("UTC 天内秒"));
    m_unitCombo->addItem(QStringLiteral("UTC 天内毫秒"));
    m_unitCombo->setCurrentIndex(0);
    connect(m_unitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CompareChartWindow::onUnitChanged);

    auto *unitRow = new QHBoxLayout();
    unitRow->addWidget(new QLabel(QStringLiteral("横轴时间"), this));
    unitRow->addWidget(m_unitCombo);
    unitRow->addStretch(1);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);
    lay->addLayout(unitRow);
    lay->addWidget(m_view);

    rebuildSeries();
}

void CompareChartWindow::onUnitChanged(int idx)
{
    m_millis = (idx == 1);
    rebuildSeries();
}

double CompareChartWindow::toTimeValue(double utcAbsSec) const
{
    const double daySec = 86400.0;
    if (m_millis) {
        double ms = std::fmod(utcAbsSec * 1000.0, daySec * 1000.0);
        if (ms < 0.0) ms += daySec * 1000.0;
        return ms;
    }
    double s = std::fmod(utcAbsSec, daySec);
    if (s < 0.0) s += daySec;
    return s;
}

void CompareChartWindow::rebuildSeries()
{
    m_series->clear();

    const int n = qMin(m_timeUtc.size(), m_error.size());
    double xmin = 0.0, xmax = 0.0, ymin = 0.0, ymax = 0.0;
    bool first = true;
    for (int i = 0; i < n; ++i) {
        const double x = toTimeValue(m_timeUtc[i]);
        const double y = m_error[i];
        m_series->append(x, y);
        if (first) {
            xmin = xmax = x;
            ymin = ymax = y;
            first = false;
        } else {
            xmin = qMin(xmin, x);
            xmax = qMax(xmax, x);
            ymin = qMin(ymin, y);
            ymax = qMax(ymax, y);
        }
    }

    if (m_xAxis) {
        m_xAxis->setTitleText(m_millis ? QStringLiteral("时间（UTC 天内毫秒）")
                                       : QStringLiteral("时间（UTC 天内秒）"));
        if (!first)
            m_xAxis->setRange(xmin, xmax);
    }
    if (m_yAxis && !first) {
        const double pad = (ymax - ymin) * 0.05 + 1e-9;
        m_yAxis->setRange(ymin - pad, ymax + pad);
    }
}

} // namespace Analysis
