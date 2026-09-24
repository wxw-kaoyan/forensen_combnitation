#include "chartwindow.h"

#include <QDoubleSpinBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <limits>
#include <algorithm>

#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QChartView>

namespace Analysis {

namespace {

// 可缩放/平移视图：滚轮缩放 X 轴，Ctrl+滚轮缩放 Y 轴，左键拖拽平移 X 轴
class ChartZoomView : public QChartView
{
public:
    explicit ChartZoomView(QChart *chart, QValueAxis *xAxis, QValueAxis *yAxis, QWidget *parent = nullptr)
        : QChartView(chart, parent), m_xAxis(xAxis), m_yAxis(yAxis)
    {
        setRenderHint(QPainter::Antialiasing, false);
    }

protected:
    void mousePressEvent(QMouseEvent *ev) override
    {
        if (ev->button() == Qt::LeftButton) {
            m_panning = true;
            m_lastPos = ev->pos();
        }
        QChartView::mousePressEvent(ev);
    }

    void mouseMoveEvent(QMouseEvent *ev) override
    {
        if (m_panning && (ev->buttons() & Qt::LeftButton) && m_xAxis) {
            const QRectF pa = chart()->plotArea();
            if (pa.width() > 1.0) {
                const double span = m_xAxis->max() - m_xAxis->min();
                const double dx = (ev->pos().x() - m_lastPos.x()) / pa.width() * span;
                m_xAxis->setRange(m_xAxis->min() - dx, m_xAxis->max() - dx);
            }
            m_lastPos = ev->pos();
            ev->accept();
            return;
        }
        QChartView::mouseMoveEvent(ev);
    }

    void mouseReleaseEvent(QMouseEvent *ev) override
    {
        m_panning = false;
        QChartView::mouseReleaseEvent(ev);
    }

    void wheelEvent(QWheelEvent *ev) override
    {
        if (ev->angleDelta().y() == 0) { QChartView::wheelEvent(ev); return; }
        const double factor = ev->angleDelta().y() > 0 ? 0.85 : 1.15;

        QValueAxis *axis = (ev->modifiers() & Qt::ControlModifier) ? m_yAxis : m_xAxis;
        if (!axis) axis = m_xAxis;
        if (!axis) return;

        const double lo = axis->min();
        const double hi = axis->max();
        const double center = (lo + hi) / 2.0;
        const double half = (hi - lo) * factor / 2.0;
        axis->setRange(center - half, center + half);
        ev->accept();
    }

private:
    QValueAxis *m_xAxis = nullptr;
    QValueAxis *m_yAxis = nullptr;
    bool  m_panning = false;
    QPoint m_lastPos;
};

} // namespace

ChartWindow::ChartWindow(const QString &title,
                         const QString &xLabel,
                         const QVector<double> &x,
                         const QVector<Curve> &curves,
                         QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_x(x)
    , m_curves(curves)
{
    setWindowTitle(title);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(1000, 620);

    m_chart = new QChart();
    m_chart->setTitle(title);
    m_chart->setAnimationOptions(QChart::NoAnimation);

    m_xAxis = new QValueAxis();
    m_xAxis->setTitleText(xLabel);
    m_yAxis = new QValueAxis();
    m_yAxis->setLabelFormat(QStringLiteral("%.4g"));

    m_chart->addAxis(m_xAxis, Qt::AlignBottom);
    m_chart->addAxis(m_yAxis, Qt::AlignLeft);
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);

    // 采样率检测：>10Hz 默认抽稀到 10Hz
    m_sourceHz = computeSampleHz();
    if (m_sourceHz > 10.0) {
        m_step = qMax(1, qRound(m_sourceHz / 10.0));
        m_downsampled = true;
    }

    rebuildSeries();

    // 初始 X 范围 = 数据全量范围
    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    for (double v : m_x) { lo = qMin(lo, v); hi = qMax(hi, v); }
    if (!(lo <= hi)) { lo = 0; hi = 1; }
    m_xAxis->setRange(lo, hi);

    m_view = new ChartZoomView(m_chart, m_xAxis, m_yAxis, this);

    // ── 顶部工具栏：起止时间输入 + 降采样切换 ──
    auto *toolbar = new QHBoxLayout();
    toolbar->setSpacing(8);
    toolbar->addWidget(new QLabel(QStringLiteral("起:"), this));

    m_minSpin = new QDoubleSpinBox(this);
    m_minSpin->setRange(-1e15, 1e15);
    m_minSpin->setDecimals(6);
    m_minSpin->setValue(lo);
    m_minSpin->setSingleStep(1.0);
    toolbar->addWidget(m_minSpin);

    toolbar->addWidget(new QLabel(QStringLiteral("止:"), this));

    m_maxSpin = new QDoubleSpinBox(this);
    m_maxSpin->setRange(-1e15, 1e15);
    m_maxSpin->setDecimals(6);
    m_maxSpin->setValue(hi);
    m_maxSpin->setSingleStep(1.0);
    toolbar->addWidget(m_maxSpin);

    auto *applyBtn = new QPushButton(QStringLiteral("应用"), this);
    auto *resetBtn = new QPushButton(QStringLiteral("重置"), this);
    toolbar->addWidget(applyBtn);
    toolbar->addWidget(resetBtn);

    if (m_sourceHz > 10.0) {
        m_dsBtn = new QPushButton(QStringLiteral("原始采样率"), this);
        m_dsBtn->setToolTip(QStringLiteral("当前按 10Hz 抽稀显示，点击恢复真实采样率（约 %1Hz）")
                                .arg(m_sourceHz, 0, 'f', 1));
        toolbar->addWidget(m_dsBtn);
        connect(m_dsBtn, &QPushButton::clicked, this, &ChartWindow::onToggleDownsample);
    }
    toolbar->addStretch(1);

    connect(applyBtn, &QPushButton::clicked, this, &ChartWindow::onApplyRange);
    connect(resetBtn, &QPushButton::clicked, this, &ChartWindow::onResetRange);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addLayout(toolbar);
    layout->addWidget(m_view, 1);
}

double ChartWindow::computeSampleHz() const
{
    const int n = m_x.size();
    if (n < 2) return 0.0;

    QVector<double> gaps;
    gaps.reserve(qMin(n - 1, 2000));
    const int maxSamples = qMin(n - 1, 2000);
    for (int i = 1; i <= maxSamples; ++i) {
        const double d = m_x[i] - m_x[i - 1];
        if (d > 0) gaps.append(d);
    }
    if (gaps.isEmpty()) return 0.0;

    std::sort(gaps.begin(), gaps.end());
    const double median = gaps[gaps.size() / 2];
    if (median <= 0) return 0.0;

    // 推断时间单位：间隔 >= 1 视为毫秒，否则视为秒
    return (median >= 1.0) ? (1000.0 / median) : (1.0 / median);
}

void ChartWindow::rebuildSeries()
{
    for (auto *s : m_series) {
        m_chart->removeSeries(s);
        delete s;
    }
    m_series.clear();

    const int step = m_downsampled ? m_step : 1;

    for (const auto &c : m_curves) {
        auto *series = new QLineSeries();
        series->setName(c.name);
        series->setColor(c.color);
        series->setPen(QPen(c.color, 1.2));

        const int n = qMin(m_x.size(), c.y.size());
        for (int i = 0; i < n; i += step)
            series->append(m_x[i], c.y[i]);

        m_chart->addSeries(series);
        series->attachAxis(m_xAxis);
        series->attachAxis(m_yAxis);
        m_series.append(series);
    }
}

void ChartWindow::onToggleDownsample()
{
    m_downsampled = !m_downsampled;
    rebuildSeries();
    if (m_dsBtn)
        m_dsBtn->setText(m_downsampled ? QStringLiteral("原始采样率")
                                       : QStringLiteral("10Hz 抽稀"));
    onResetRange();
}

void ChartWindow::onApplyRange()
{
    if (!m_xAxis || !m_minSpin || !m_maxSpin) return;
    const double lo = m_minSpin->value();
    const double hi = m_maxSpin->value();
    if (lo >= hi) return;
    m_xAxis->setRange(lo, hi);
}

void ChartWindow::onResetRange()
{
    if (!m_xAxis) return;
    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    for (double v : m_x) { lo = qMin(lo, v); hi = qMax(hi, v); }
    if (!(lo <= hi)) { lo = 0; hi = 1; }
    m_xAxis->setRange(lo, hi);
    if (m_minSpin) m_minSpin->setValue(lo);
    if (m_maxSpin) m_maxSpin->setValue(hi);
}

} // namespace Analysis
