#include "chartwindow.h"

#include <QDoubleSpinBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QScrollBar>
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

// 滚动条的整数刻度：把数据全量范围映射到 0..kScrollScale
constexpr int kScrollScale = 1000000;

// 可缩放/平移视图：
//   触摸板 左右滑 → 缩放 X 轴，上下滑 → 缩放 Y 轴
//   鼠标 滚轮 → 缩放 X 轴，Ctrl+滚轮 → 缩放 Y 轴
//   左键拖拽 → 平移 X/Y 轴
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
        if (m_panning && (ev->buttons() & Qt::LeftButton)) {
            const QRectF pa = chart()->plotArea();
            const QPoint d = ev->pos() - m_lastPos;
            if (m_xAxis && pa.width() > 1.0) {
                const double spanX = m_xAxis->max() - m_xAxis->min();
                const double dx = d.x() / pa.width() * spanX;
                m_xAxis->setRange(m_xAxis->min() - dx, m_xAxis->max() - dx);
            }
            if (m_yAxis && pa.height() > 1.0) {
                // Y 轴：鼠标下移对应视窗上移（内容跟随光标），故用 +dy
                const double spanY = m_yAxis->max() - m_yAxis->min();
                const double dy = d.y() / pa.height() * spanY;
                m_yAxis->setRange(m_yAxis->min() + dy, m_yAxis->max() + dy);
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
        int dx = ev->angleDelta().x();
        int dy = ev->angleDelta().y();

        // 部分触摸板/手绘板只上报 pixelDelta，回退使用
        bool fromPixel = false;
        if (dx == 0 && dy == 0) {
            dx = ev->pixelDelta().x();
            dy = ev->pixelDelta().y();
            fromPixel = true;
        }
        if (dx == 0 && dy == 0) { QChartView::wheelEvent(ev); return; }

        // 触摸板（平滑滚动）上报 pixelDelta；传统鼠标滚轮没有 pixelDelta
        const bool touchpad = fromPixel || !ev->pixelDelta().isNull();
        const bool ctrl = ev->modifiers().testFlag(Qt::ControlModifier);

        // 横向分量（触摸板左右滑）→ 缩放 X 轴
        if (dx != 0)
            zoomAxis(m_xAxis, dx);

        if (dy != 0) {
            // 触摸板：上下滑 → 缩放 Y 轴（Ctrl 时改缩放 X 轴）
            // 鼠标滚轮：纵向滚轮 → 缩放 X 轴（Ctrl 时改缩放 Y 轴）
            if (touchpad)
                zoomAxis(ctrl ? m_xAxis : m_yAxis, dy);
            else
                zoomAxis(ctrl ? m_yAxis : m_xAxis, dy);
        }
        ev->accept();
    }

private:
    // 以轴中心为基准缩放一档（delta > 0 放大，否则缩小）
    static void zoomAxis(QValueAxis *axis, int delta)
    {
        if (!axis || delta == 0) return;
        const double factor = delta > 0 ? 0.85 : 1.15;
        const double lo = axis->min();
        const double hi = axis->max();
        const double center = (lo + hi) / 2.0;
        const double half = (hi - lo) * factor / 2.0;
        axis->setRange(center - half, center + half);
    }

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
                         bool trajectory,
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

    // 采样率检测：>10Hz 默认抽稀到 10Hz（轨迹图不做抽稀，Y 也不是"值"而是纬度）
    if (!trajectory) {
        m_sourceHz = computeSampleHz();
        if (m_sourceHz > 10.0) {
            m_step = qMax(1, qRound(m_sourceHz / 10.0));
            m_downsampled = true;
        }
    }

    rebuildSeries();

    // 全量范围：X = 时间列范围；Y = 所有曲线的数据范围（滚动条据此映射）
    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    for (double v : m_x) { lo = qMin(lo, v); hi = qMax(hi, v); }
    if (!(lo <= hi)) { lo = 0; hi = 1; }
    m_xFullMin = lo; m_xFullMax = hi;

    m_yFullMin = std::numeric_limits<double>::max();
    m_yFullMax = std::numeric_limits<double>::lowest();
    for (const auto &c : m_curves)
        for (double v : c.y) { m_yFullMin = qMin(m_yFullMin, v); m_yFullMax = qMax(m_yFullMax, v); }
    if (!(m_yFullMin <= m_yFullMax)) { m_yFullMin = -1.0; m_yFullMax = 1.0; }

    m_xAxis->setRange(m_xFullMin, m_xFullMax);
    m_yAxis->setRange(m_yFullMin, m_yFullMax);

    m_view = new ChartZoomView(m_chart, m_xAxis, m_yAxis, this);

    // 底部/右侧移动条：左右、上下平移 X/Y 轴
    m_hScroll = new QScrollBar(Qt::Horizontal, this);
    m_vScroll = new QScrollBar(Qt::Vertical, this);
    m_hScroll->setRange(0, kScrollScale);
    m_vScroll->setRange(0, kScrollScale);
    connect(m_hScroll, &QScrollBar::valueChanged, this, &ChartWindow::onXScroll);
    connect(m_vScroll, &QScrollBar::valueChanged, this, &ChartWindow::onYScroll);
    connect(m_xAxis, &QValueAxis::rangeChanged, this, [this](qreal, qreal) { syncScrollbars(); });
    connect(m_yAxis, &QValueAxis::rangeChanged, this, [this](qreal, qreal) { syncScrollbars(); });
    syncScrollbars();

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

    // 图表 + 底部横向移动条 + 右侧纵向移动条
    auto *grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);
    grid->addWidget(m_view,    0, 0);
    grid->addWidget(m_vScroll, 0, 1);
    grid->addWidget(m_hScroll, 1, 0);
    grid->setRowStretch(0, 1);
    grid->setColumnStretch(0, 1);
    layout->addLayout(grid, 1);
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

    // Y 轴一并恢复为全部曲线的数据范围
    if (m_yAxis) {
        double ylo = std::numeric_limits<double>::max();
        double yhi = std::numeric_limits<double>::lowest();
        for (const auto &c : m_curves)
            for (double v : c.y) { ylo = qMin(ylo, v); yhi = qMax(yhi, v); }
        if (ylo <= yhi) m_yAxis->setRange(ylo, yhi);
    }
}

// 底部横向移动条 → 平移 X 轴（保持当前可见跨度）
void ChartWindow::onXScroll(int value)
{
    if (m_syncing || !m_xAxis) return;
    const double total = m_xFullMax - m_xFullMin;
    if (total <= 0.0) return;
    const double visible = qMin(m_xAxis->max() - m_xAxis->min(), total);
    const double lo = m_xFullMin + (double(value) / kScrollScale) * total;
    m_syncing = true;
    m_xAxis->setRange(lo, lo + visible);
    m_syncing = false;
}

// 右侧纵向移动条 → 平移 Y 轴（保持当前可见跨度）
void ChartWindow::onYScroll(int value)
{
    if (m_syncing || !m_yAxis) return;
    const double total = m_yFullMax - m_yFullMin;
    if (total <= 0.0) return;
    const double visible = qMin(m_yAxis->max() - m_yAxis->min(), total);
    const double lo = m_yFullMin + (double(value) / kScrollScale) * total;
    m_syncing = true;
    m_yAxis->setRange(lo, lo + visible);
    m_syncing = false;
}

// 轴范围变化后同步两个滚动条：滑块长度 = 可见区间占全量的比例
void ChartWindow::syncScrollbars()
{
    if (m_syncing) return;
    m_syncing = true;

    auto apply = [](QScrollBar *sb, QValueAxis *axis, double fullMin, double fullMax) {
        if (!sb) return;
        const double total = fullMax - fullMin;
        if (!axis || total <= 0.0) { sb->setEnabled(false); return; }
        const double visible = qBound(0.0, axis->max() - axis->min(), total);
        int page = int(qRound(kScrollScale * visible / total));
        page = qBound(1, page, kScrollScale);
        sb->setPageStep(page);
        sb->setRange(0, kScrollScale);
        const int val = int(qRound(kScrollScale * (axis->min() - fullMin) / total));
        sb->setValue(qBound(0, val, kScrollScale - page));
        sb->setEnabled(visible < total * 0.999);
    };
    apply(m_hScroll, m_xAxis, m_xFullMin, m_xFullMax);
    apply(m_vScroll, m_yAxis, m_yFullMin, m_yFullMax);

    m_syncing = false;
}

} // namespace Analysis
