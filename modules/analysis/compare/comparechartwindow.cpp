#include "comparechartwindow.h"

#include <QVBoxLayout>
#include <QPainter>
#include <QPen>
#include <QColor>

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <QtMath>

namespace Analysis {

CompareChartWindow::CompareChartWindow(const QString &title,
                                       const QString &yUnit,
                                       const QVector<double> &time,
                                       const QVector<double> &error,
                                       QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(title);
    setWindowModality(Qt::WindowModal);
    resize(960, 560);

    auto *chart = new QChart();
    chart->setTitle(title);
    chart->setAnimationOptions(QChart::NoAnimation);

    auto *errSeries = new QLineSeries(chart);
    errSeries->setName(QStringLiteral("误差（对比-真值）"));
    errSeries->setColor(QColor(239, 68, 68));      // 红
    errSeries->setPen(QPen(QColor(239, 68, 68), 1.5));

    const int n = qMin(time.size(), error.size());
    for (int i = 0; i < n; ++i)
        errSeries->append(time[i], error[i]);

    chart->addSeries(errSeries);
    chart->createDefaultAxes();

    if (auto *ax = qobject_cast<QValueAxis *>(chart->axes(Qt::Horizontal).value(0)))
        ax->setTitleText(QStringLiteral("时间（UTC 绝对秒）"));
    if (auto *ay = qobject_cast<QValueAxis *>(chart->axes(Qt::Vertical).value(0)))
        ay->setTitleText(yUnit);

    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    m_view = new QChartView(chart, this);
    m_view->setRenderHint(QPainter::Antialiasing, true);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_view);
}

} // namespace Analysis
