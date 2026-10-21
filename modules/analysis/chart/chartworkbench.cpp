#include "chartworkbench.h"
#include "chartwindow.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QListWidget>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>

#include <cmath>

namespace Analysis {

namespace {
// 曲线默认配色（按勾选顺序循环）
const QColor kCurveColors[] = {
    QColor(220, 38, 38),   // 红
    QColor(37, 99, 235),   // 蓝
    QColor(22, 163, 74),   // 绿
    QColor(217, 119, 6),   // 橙
    QColor(147, 51, 234),  // 紫
    QColor(8, 145, 178),   // 青
    QColor(219, 39, 119),  // 洋红
    QColor(120, 85, 45),   // 棕
    QColor(75, 85, 99),    // 灰
    QColor(16, 185, 129),  // 深绿
};
constexpr int kCurveColorCount = int(sizeof(kCurveColors) / sizeof(kCurveColors[0]));

// 在表头里按关键词优先级猜列（用于自动选中经度/纬度列）
int guessColumn(const QStringList &header, const QStringList &keys)
{
    for (const QString &k : keys)
        for (int i = 0; i < header.size(); ++i)
            if (header[i].toLower().contains(k))
                return i;
    return -1;
}

// 判断两列是否是经纬度（度）：经度 |lon|<=180、纬度 |lat|<=90，且不是全 0
// 用于区分「经纬度」与「已换算好的平面坐标(m)」，后者不再二次换算
bool looksLikeLonLatDegrees(const QVector<double> &lon, const QVector<double> &lat)
{
    const int n = qMin(lon.size(), lat.size());
    bool anyNonZero = false;
    for (int i = 0; i < n; ++i) {
        if (std::isnan(lon[i]) || std::isnan(lat[i])) continue;
        if (std::abs(lon[i]) > 180.0 || std::abs(lat[i]) > 90.0) return false;
        if (lon[i] != 0.0 || lat[i] != 0.0) anyNonZero = true;
    }
    return anyNonZero;
}

// 经纬度(度) → 以首个有效点为原点的局部「东-北」平面坐标(m)
// 等距圆柱近似：跨几百米~几公里的轨迹误差在厘米级，绘图足够
void lonLatToEnuMeters(const QVector<double> &lon, const QVector<double> &lat,
                       QVector<double> *east, QVector<double> *north)
{
    constexpr double kEarthRadius = 6378137.0;          // WGS-84 长半轴
    constexpr double kDeg2Rad     = 0.017453292519943295;

    int origin = 0;
    for (int i = 0; i < qMin(lon.size(), lat.size()); ++i) {
        if (lon[i] != 0.0 || lat[i] != 0.0) { origin = i; break; }
    }

    const double lon0     = lon.value(origin);
    const double lat0     = lat.value(origin);
    const double cosLat0  = std::cos(lat0 * kDeg2Rad);
    const double mPerDeg  = kEarthRadius * kDeg2Rad;    // 每度纬度对应米数

    east->resize(lon.size());
    north->resize(lon.size());
    for (int i = 0; i < lon.size(); ++i) {
        (*east)[i]  = (lon[i] - lon0) * mPerDeg * cosLat0;
        (*north)[i] = (lat.value(i) - lat0) * mPerDeg;
    }
}
} // namespace

ChartWorkbench::ChartWorkbench(QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(QStringLiteral("通用绘图"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(720, 560);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    // ── 文件选择行 ──
    auto *fileRow = new QHBoxLayout();
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(QStringLiteral("选择 CSV 文件（数据解析导出）"));
    m_browseBtn = new QPushButton(QStringLiteral("浏览..."), this);
    fileRow->addWidget(m_pathEdit, 1);
    fileRow->addWidget(m_browseBtn);
    layout->addLayout(fileRow);
    connect(m_browseBtn, &QPushButton::clicked, this, &ChartWorkbench::onBrowse);

    // ── 时间列选择 ──
    auto *timeRow = new QHBoxLayout();
    timeRow->addWidget(new QLabel(QStringLiteral("时间列（X 轴）:"), this));
    m_timeCombo = new QComboBox(this);
    m_timeCombo->setMinimumWidth(260);
    timeRow->addWidget(m_timeCombo);
    timeRow->addStretch(1);
    layout->addLayout(timeRow);

    // ── 轨迹：经度列(X) + 纬度列(Y) ──
    auto *trajRow = new QHBoxLayout();
    trajRow->addWidget(new QLabel(QStringLiteral("轨迹  经度列:"), this));
    m_lonCombo = new QComboBox(this);
    m_lonCombo->setMinimumWidth(150);
    trajRow->addWidget(m_lonCombo);
    trajRow->addWidget(new QLabel(QStringLiteral("纬度列:"), this));
    m_latCombo = new QComboBox(this);
    m_latCombo->setMinimumWidth(150);
    trajRow->addWidget(m_latCombo);
    m_trajBtn = new QPushButton(QStringLiteral("生成轨迹"), this);
    trajRow->addWidget(m_trajBtn);
    trajRow->addStretch(1);
    layout->addLayout(trajRow);
    connect(m_trajBtn, &QPushButton::clicked, this, &ChartWorkbench::onGenerateTrajectory);

    // ── 数据列选择（勾选多列，各画一条曲线） ──
    layout->addWidget(new QLabel(QStringLiteral("数据列（Y 轴，可多选，各生成一条曲线）:"), this));
    m_colList = new QListWidget(this);
    m_colList->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(m_colList, 1);

    auto *selRow = new QHBoxLayout();
    m_selAllBtn   = new QPushButton(QStringLiteral("全选"), this);
    m_deselAllBtn = new QPushButton(QStringLiteral("全不选"), this);
    selRow->addWidget(m_selAllBtn);
    selRow->addWidget(m_deselAllBtn);
    selRow->addStretch(1);
    layout->addLayout(selRow);
    connect(m_selAllBtn,   &QPushButton::clicked, this, &ChartWorkbench::onSelectAll);
    connect(m_deselAllBtn, &QPushButton::clicked, this, &ChartWorkbench::onDeselectAll);

    // ── 生成窗口 + 状态 ──
    m_generateBtn = new QPushButton(QStringLiteral("生成窗口"), this);
    m_generateBtn->setFixedHeight(36);
    layout->addWidget(m_generateBtn);
    connect(m_generateBtn, &QPushButton::clicked, this, &ChartWorkbench::onGenerate);

    m_statusLabel = new QLabel(QStringLiteral("请选择文件"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#5b6472;"));
    layout->addWidget(m_statusLabel);
}

void ChartWorkbench::setBusy(bool busy, const QString &msg)
{
    m_browseBtn->setEnabled(!busy);
    m_generateBtn->setEnabled(!busy);
    m_trajBtn->setEnabled(!busy);
    m_lonCombo->setEnabled(!busy);
    m_latCombo->setEnabled(!busy);
    m_selAllBtn->setEnabled(!busy);
    m_deselAllBtn->setEnabled(!busy);
    if (!msg.isEmpty())
        m_statusLabel->setText(msg);
}

void ChartWorkbench::onBrowse()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 CSV 文件"), QString(),
        QStringLiteral("表格文件 (*.csv *.txt);;所有文件 (*.*)"));
    if (path.isEmpty()) return;

    m_pathEdit->setText(path);
    startLoad(path);
}

void ChartWorkbench::startLoad(const QString &path)
{
    if (m_loading) return;
    m_loading = true;
    setBusy(true, QStringLiteral("加载中..."));

    m_loadWatcher = new QFutureWatcher<TextFileTable>(this);
    connect(m_loadWatcher, &QFutureWatcher<TextFileTable>::finished,
            this, &ChartWorkbench::onLoadFinished);
    m_loadWatcher->setFuture(
        QtConcurrent::run([path]() { return loadTextFileTable(path, -1); }));
}

void ChartWorkbench::onLoadFinished()
{
    TextFileTable t = m_loadWatcher->result();
    m_loadWatcher->deleteLater();
    m_loadWatcher = nullptr;
    m_loading = false;

    if (!t.error.isEmpty()) {
        setBusy(false, QStringLiteral("加载失败：%1").arg(t.error));
        return;
    }

    m_table = t;   // 隐式共享，浅拷贝

    // 时间列
    m_timeCombo->clear();
    m_timeCombo->addItems(m_table.header);
    if (m_table.timeColumn >= 0 && m_table.timeColumn < m_table.header.size())
        m_timeCombo->setCurrentIndex(m_table.timeColumn);

    // 经/纬度列（按表头关键词自动选中）
    m_lonCombo->clear();
    m_lonCombo->addItems(m_table.header);
    m_latCombo->clear();
    m_latCombo->addItems(m_table.header);
    const int gLon = guessColumn(m_table.header, QStringList{QStringLiteral("经度"), QStringLiteral("lon"), QStringLiteral("lng")});
    const int gLat = guessColumn(m_table.header, QStringList{QStringLiteral("纬度"), QStringLiteral("lat")});
    if (gLon >= 0) m_lonCombo->setCurrentIndex(gLon);
    if (gLat >= 0) m_latCombo->setCurrentIndex(gLat);

    // 数据列（排除时间列默认不勾选，其余默认不勾选，由用户选择）
    m_colList->clear();
    for (int i = 0; i < m_table.header.size(); ++i) {
        auto *item = new QListWidgetItem(m_table.header[i], m_colList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setData(Qt::UserRole, i);
    }

    setBusy(false, QStringLiteral("已加载 %1 行 × %2 列").arg(m_table.rows.size()).arg(m_table.header.size()));
}

void ChartWorkbench::onSelectAll()
{
    for (int i = 0; i < m_colList->count(); ++i)
        m_colList->item(i)->setCheckState(Qt::Checked);
}

void ChartWorkbench::onDeselectAll()
{
    for (int i = 0; i < m_colList->count(); ++i)
        m_colList->item(i)->setCheckState(Qt::Unchecked);
}

QVector<double> ChartWorkbench::extractColumn(const QList<QStringList> &rows, int col)
{
    QVector<double> out;
    out.reserve(rows.size());
    for (const auto &r : rows) {
        if (col >= 0 && col < r.size()) {
            bool ok = false;
            const double v = r[col].toDouble(&ok);
            out.append(ok ? v : 0.0);
        }
    }
    return out;
}

void ChartWorkbench::onGenerate()
{
    if (m_extracting) return;
    if (m_table.header.isEmpty() || m_table.rows.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("请先加载文件"));
        return;
    }

    const int timeCol = m_timeCombo->currentIndex();
    if (timeCol < 0) {
        m_statusLabel->setText(QStringLiteral("请选择时间列"));
        return;
    }

    QList<int> checkedCols;
    for (int i = 0; i < m_colList->count(); ++i) {
        if (m_colList->item(i)->checkState() == Qt::Checked)
            checkedCols.append(m_colList->item(i)->data(Qt::UserRole).toInt());
    }
    if (checkedCols.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("请至少勾选一列数据"));
        return;
    }

    startExtract();
}

void ChartWorkbench::startExtract()
{
    m_extracting = true;
    setBusy(true, QStringLiteral("提取数据中..."));

    const QList<QStringList> rows = m_table.rows;    // 隐式共享浅拷贝，后台只读安全
    const QStringList header = m_table.header;
    const int timeCol = m_timeCombo->currentIndex();
    const QList<int> cols = [&]() {
        QList<int> c;
        for (int i = 0; i < m_colList->count(); ++i)
            if (m_colList->item(i)->checkState() == Qt::Checked)
                c.append(m_colList->item(i)->data(Qt::UserRole).toInt());
        return c;
    }();
    const QString baseName = QFileInfo(m_table.path).fileName();
    const QString xLabel = (timeCol >= 0 && timeCol < header.size()) ? header[timeCol] : QStringLiteral("时间");

    m_extractWatcher = new QFutureWatcher<ExtractResult>(this);
    connect(m_extractWatcher, &QFutureWatcher<ExtractResult>::finished,
            this, &ChartWorkbench::onGenerateFinished);
    m_extractWatcher->setFuture(
        QtConcurrent::run([rows, header, cols, timeCol, baseName, xLabel]() -> ExtractResult {
            ExtractResult r;
            r.xLabel = xLabel;
            r.x = extractColumn(rows, timeCol);
            QStringList names;
            for (int i = 0; i < cols.size(); ++i) {
                const int c = cols[i];
                ChartWindow::Curve curve;
                curve.name  = (c >= 0 && c < header.size()) ? header[c] : QStringLiteral("列%1").arg(c);
                curve.y     = extractColumn(rows, c);
                curve.color = kCurveColors[i % kCurveColorCount];
                r.curves.append(curve);
                names << curve.name;
            }
            r.title = QStringLiteral("%1 — %2").arg(baseName, names.join(QStringLiteral(", ")));
            return r;
        }));
}

// 生成轨迹图：X = 经度列，Y = 纬度列
void ChartWorkbench::onGenerateTrajectory()
{
    if (m_extracting) return;
    if (m_table.header.isEmpty() || m_table.rows.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("请先加载文件"));
        return;
    }
    const int lonCol = m_lonCombo->currentIndex();
    const int latCol = m_latCombo->currentIndex();
    if (lonCol < 0 || latCol < 0) {
        m_statusLabel->setText(QStringLiteral("请选择经度列和纬度列"));
        return;
    }
    if (lonCol == latCol) {
        m_statusLabel->setText(QStringLiteral("经度列和纬度列不能是同一列"));
        return;
    }
    startExtractTrajectory();
}

void ChartWorkbench::startExtractTrajectory()
{
    m_extracting = true;
    setBusy(true, QStringLiteral("提取轨迹中..."));

    const QList<QStringList> rows = m_table.rows;   // 隐式共享浅拷贝，后台只读安全
    const QStringList header = m_table.header;
    const int lonCol = m_lonCombo->currentIndex();
    const int latCol = m_latCombo->currentIndex();
    const QString baseName = QFileInfo(m_table.path).fileName();
    const QString xLabel = (lonCol >= 0 && lonCol < header.size()) ? header[lonCol] : QStringLiteral("经度");
    const QString yName  = (latCol >= 0 && latCol < header.size()) ? header[latCol] : QStringLiteral("纬度");

    m_extractWatcher = new QFutureWatcher<ExtractResult>(this);
    connect(m_extractWatcher, &QFutureWatcher<ExtractResult>::finished,
            this, &ChartWorkbench::onGenerateFinished);
    m_extractWatcher->setFuture(
        QtConcurrent::run([rows, lonCol, latCol, baseName, xLabel, yName]() -> ExtractResult {
            ExtractResult r;
            r.trajectory = true;

            const QVector<double> lonVals = extractColumn(rows, lonCol);
            const QVector<double> latVals = extractColumn(rows, latCol);

            ChartWindow::Curve curve;
            if (looksLikeLonLatDegrees(lonVals, latVals)) {
                // 经纬度（度）→ 以首点为原点的东/北平面坐标(m)，避免用 0.0001° 量级读数
                QVector<double> east, north;
                lonLatToEnuMeters(lonVals, latVals, &east, &north);
                r.xLabel    = QStringLiteral("东向(m)");
                curve.name  = QStringLiteral("北向(m)");
                curve.y     = north;
                r.x         = east;
                r.title     = QStringLiteral("%1 — 轨迹(东向 × 北向, 单位 m)").arg(baseName);
            } else {
                // 选中的列已是平面坐标(m)，直接画
                r.xLabel    = xLabel;
                curve.name  = yName;
                curve.y     = latVals;
                r.x         = lonVals;
                r.title     = QStringLiteral("%1 — 轨迹(%2 × %3)").arg(baseName, xLabel, yName);
            }

            curve.color = kCurveColors[0];
            r.curves.append(curve);
            return r;
        }));
}

void ChartWorkbench::onGenerateFinished()
{
    ExtractResult r = m_extractWatcher->result();
    m_extractWatcher->deleteLater();
    m_extractWatcher = nullptr;
    m_extracting = false;
    setBusy(false, QStringLiteral("就绪"));

    if (r.curves.isEmpty()) return;

    auto *win = new ChartWindow(r.title, r.xLabel, r.x, r.curves, r.trajectory);
    win->show();
}

} // namespace Analysis
