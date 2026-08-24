#include "analysispanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QTableWidget>
#include <QCheckBox>
#include <QProgressBar>
#include <QHeaderView>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QGuiApplication>
#include <QApplication>
#include <QTableWidgetItem>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

#include <cmath>
#include <algorithm>
#include <limits>

namespace Analysis {

// ── 从表格取某行某列的数值 ──
static double cellDouble(const TextFileTable &t, int row, int col, bool *ok)
{
    if (row < 0 || row >= t.rows.size() || col < 0 || col >= t.header.size()) {
        if (ok) *ok = false;
        return 0.0;
    }
    return t.rows[row].value(col).trimmed().toDouble(ok);
}

// ── 重算时间列数值（用户切换时间列后调用） ──
static void recomputeTimes(TextFileTable &t)
{
    t.times.resize(t.rows.size());
    for (int i = 0; i < t.rows.size(); ++i) {
        bool ok = false;
        const double v = t.rows[i].value(t.timeColumn).trimmed().toDouble(&ok);
        t.times[i] = ok ? v : 0.0;
    }
}

// ── 数值格式化（有效数字） ──
static QString fmtNum(double v)
{
    if (!std::isfinite(v)) return QStringLiteral("-");
    return QString::number(v, 'g', 8);
}

// ── 时间戳精确对齐：只有完全相等的时间戳才配对 ──
// 两个时间数组需按升序排列（降采样/newcode 输出天然有序）。
struct MatchedPair { int truthIdx; int cmpIdx; };

static QVector<MatchedPair> exactMatchTimes(const QVector<double> &a,
                                            const QVector<double> &b)
{
    QVector<MatchedPair> res;
    int i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] == b[j]) {
            res.append({i, j});
            ++i; ++j;
        } else if (a[i] < b[j]) {
            ++i;
        } else {
            ++j;
        }
    }
    return res;
}

AnalysisPanel::AnalysisPanel(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

AnalysisPanel::~AnalysisPanel() = default;

void AnalysisPanel::buildUi()
{
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // ── 左侧：真值文件 + 对比文件 + 映射 + 场景 ──
    auto *left = new QFrame(this);
    left->setMinimumWidth(400);
    left->setMaximumWidth(500);
    left->setFrameShape(QFrame::StyledPanel);
    auto *ll = new QVBoxLayout(left);
    ll->setSpacing(6);

    auto addSectionLabel = [&](const QString &s) {
        auto *lbl = new QLabel(s, left);
        lbl->setStyleSheet(QStringLiteral("font-size:13px;font-weight:700;color:#12263f;"));
        ll->addWidget(lbl);
    };

    // 真值文件
    addSectionLabel(QStringLiteral("真值文件（基准）"));
    auto *truthRow = new QHBoxLayout;
    m_truthPathEdit = new QLineEdit(left);
    m_truthPathEdit->setPlaceholderText(QStringLiteral("选择真值 CSV/TXT 文件"));
    auto *browseBtn = new QPushButton(QStringLiteral("浏览"), left);
    browseBtn->setFixedWidth(72);
    truthRow->addWidget(m_truthPathEdit, 1);
    truthRow->addWidget(browseBtn);
    ll->addLayout(truthRow);

    auto *truthTimeRow = new QFormLayout;
    m_truthTimeCombo = new QComboBox(left);
    m_truthTimeCombo->setEnabled(false);
    truthTimeRow->addRow(QStringLiteral("时间列"), m_truthTimeCombo);
    ll->addLayout(truthTimeRow);

    auto *sep1 = new QFrame(left);
    sep1->setFrameShape(QFrame::HLine);
    ll->addWidget(sep1);

    // 对比文件
    addSectionLabel(QStringLiteral("对比文件"));
    m_cmpFileList = new QListWidget(left);
    m_cmpFileList->setMinimumHeight(80);
    ll->addWidget(m_cmpFileList, 1);

    auto *cmpBtnRow = new QHBoxLayout;
    auto *addCmpBtn = new QPushButton(QStringLiteral("添加"), left);
    auto *rmCmpBtn  = new QPushButton(QStringLiteral("移除"), left);
    cmpBtnRow->addWidget(addCmpBtn);
    cmpBtnRow->addWidget(rmCmpBtn);
    cmpBtnRow->addStretch();
    ll->addLayout(cmpBtnRow);

    auto *cmpTimeRow = new QFormLayout;
    m_cmpTimeCombo = new QComboBox(left);
    m_cmpTimeCombo->setEnabled(false);
    cmpTimeRow->addRow(QStringLiteral("时间列"), m_cmpTimeCombo);
    ll->addLayout(cmpTimeRow);

    auto *sep2 = new QFrame(left);
    sep2->setFrameShape(QFrame::HLine);
    ll->addWidget(sep2);

    // 对比项映射
    addSectionLabel(QStringLiteral("对比项映射"));
    m_mapTable = new QTableWidget(left);
    m_mapTable->setMinimumHeight(120);
    ll->addWidget(m_mapTable, 2);

    auto *mapBtnRow = new QHBoxLayout;
    auto *addMetricBtn = new QPushButton(QStringLiteral("添加对比项"), left);
    auto *rmMetricBtn  = new QPushButton(QStringLiteral("删除对比项"), left);
    mapBtnRow->addWidget(addMetricBtn);
    mapBtnRow->addWidget(rmMetricBtn);
    mapBtnRow->addStretch();
    ll->addLayout(mapBtnRow);

    auto *sep3 = new QFrame(left);
    sep3->setFrameShape(QFrame::HLine);
    ll->addWidget(sep3);

    // 场景切段
    addSectionLabel(QStringLiteral("场景切段（手动截断时间）"));
    auto *sceneForm = new QFormLayout;
    m_sceneNameEdit = new QLineEdit(left);
    m_sceneNameEdit->setPlaceholderText(QStringLiteral("可选"));
    m_sceneStartEdit = new QLineEdit(left);
    m_sceneStartEdit->setPlaceholderText(QStringLiteral("起始(秒)"));
    m_sceneEndEdit = new QLineEdit(left);
    m_sceneEndEdit->setPlaceholderText(QStringLiteral("结束(秒)"));
    sceneForm->addRow(QStringLiteral("场景名"), m_sceneNameEdit);
    sceneForm->addRow(QStringLiteral("起始(秒)"), m_sceneStartEdit);
    sceneForm->addRow(QStringLiteral("结束(秒)"), m_sceneEndEdit);
    ll->addLayout(sceneForm);

    auto *sceneBtnRow = new QHBoxLayout;
    auto *addSceneBtn = new QPushButton(QStringLiteral("添加场景"), left);
    auto *rmSceneBtn  = new QPushButton(QStringLiteral("删除场景"), left);
    sceneBtnRow->addWidget(addSceneBtn);
    sceneBtnRow->addWidget(rmSceneBtn);
    sceneBtnRow->addStretch();
    ll->addLayout(sceneBtnRow);

    m_sceneList = new QListWidget(left);
    m_sceneList->setMinimumHeight(64);
    ll->addWidget(m_sceneList, 1);

    m_startBtn = new QPushButton(QStringLiteral("开始对比"), left);
    m_startBtn->setStyleSheet(QStringLiteral("font-weight:600;color:#fff;background-color:#2c7be5;padding:8px;"));
    ll->addWidget(m_startBtn);

    m_progressBar = new QProgressBar(left);
    m_progressBar->setRange(0, 0);
    m_progressBar->setVisible(false);
    ll->addWidget(m_progressBar);

    // ── 右侧：结果表 ──
    auto *right = new QFrame(this);
    right->setFrameShape(QFrame::StyledPanel);
    auto *rl = new QVBoxLayout(right);
    rl->setSpacing(6);

    auto *resultTitle = new QLabel(QStringLiteral("对比结果（RMS / 2σ）"), right);
    resultTitle->setStyleSheet(QStringLiteral("font-size:13px;font-weight:700;color:#12263f;"));
    rl->addWidget(resultTitle);

    m_resultTable = new QTableWidget(right);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setAlternatingRowColors(true);
    rl->addWidget(m_resultTable, 1);

    mainLayout->addWidget(left, 0);
    mainLayout->addWidget(right, 1);

    // ── 信号 ──
    connect(browseBtn, &QPushButton::clicked, this, &AnalysisPanel::onBrowseTruth);
    connect(m_truthTimeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalysisPanel::onTruthTimeColChanged);
    connect(addCmpBtn, &QPushButton::clicked, this, &AnalysisPanel::onAddCmpFile);
    connect(rmCmpBtn,  &QPushButton::clicked, this, &AnalysisPanel::onRemoveCmpFile);
    connect(m_cmpFileList, &QListWidget::currentRowChanged,
            this, &AnalysisPanel::onCmpFileSelected);
    connect(m_cmpTimeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalysisPanel::onCmpTimeColChanged);
    connect(addMetricBtn, &QPushButton::clicked, this, &AnalysisPanel::onAddMetric);
    connect(rmMetricBtn,  &QPushButton::clicked, this, &AnalysisPanel::onRemoveMetric);
    connect(addSceneBtn, &QPushButton::clicked, this, &AnalysisPanel::onAddScene);
    connect(rmSceneBtn,  &QPushButton::clicked, this, &AnalysisPanel::onRemoveScene);
    connect(m_startBtn, &QPushButton::clicked, this, &AnalysisPanel::onStartCompare);

    // 忙碌（加载/计算）时要禁用的控件
    // 注意：时间列下拉框的状态由数据是否存在决定，不在 busy 里统一控制
    m_busyWidgets = {
        browseBtn, addCmpBtn, rmCmpBtn,
        m_cmpFileList,
        addMetricBtn, rmMetricBtn, m_mapTable,
        m_sceneNameEdit, m_sceneStartEdit, m_sceneEndEdit,
        addSceneBtn, rmSceneBtn, m_sceneList,
        m_startBtn
    };

    // 指标名编辑（表格级信号只连一次）
    connect(m_mapTable, &QTableWidget::itemChanged, this,
            [this](QTableWidgetItem *it) {
                if (!it || it->column() != 0) return;
                const int r = it->row();
                if (r < 0 || r >= m_mappings.size()) return;
                m_mappings[r].name = it->text();
            });

    rebuildMappingTable();
    rebuildResultTable();
}

// ════════════════════════════════════════════════════════════════
//  文件加载
// ════════════════════════════════════════════════════════════════
void AnalysisPanel::onBrowseTruth()
{
    if (m_loading) return;
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择真值文件"), m_lastDir,
        QStringLiteral("文本文件 (*.csv *.txt);;所有文件 (*)"));
    if (path.isEmpty()) return;
    m_lastDir = QFileInfo(path).absolutePath();
    startLoadTruth(path);
}

void AnalysisPanel::startLoadTruth(const QString &path)
{
    setBusy(true);

    auto *watcher = new QFutureWatcher<TextFileTable>(this);
    m_watcher = watcher;
    connect(watcher, &QFutureWatcher<TextFileTable>::finished,
            this, &AnalysisPanel::onTruthLoadFinished);
    watcher->setFuture(QtConcurrent::run([path]() {
        return loadTextFileTable(path);
    }));
}

void AnalysisPanel::onTruthLoadFinished()
{
    auto *watcher = static_cast<QFutureWatcher<TextFileTable>*>(m_watcher);
    TextFileTable t = watcher->result();
    watcher->deleteLater();
    m_watcher = nullptr;
    setBusy(false);

    if (!t.error.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("加载失败"),
                             QStringLiteral("%1").arg(t.error));
        return;
    }

    m_truth = t;
    // 加载真值是「更新」语义：保留已有对比项/对比文件/场景，只重置越界的真值列
    for (MetricMapping &m : m_mappings)
        if (m.truthCol >= t.header.size()) m.truthCol = -1;
    m_truthPathEdit->setText(t.path);
    refreshTruthTimeCombo();
    rebuildMappingTable();
    rebuildResultTable();
}

void AnalysisPanel::setBusy(bool busy)
{
    m_loading = busy;
    for (QWidget *w : m_busyWidgets)
        if (w) w->setEnabled(!busy);
    m_progressBar->setVisible(busy);
}

void AnalysisPanel::refreshTruthTimeCombo()
{
    m_truthTimeCombo->clear();
    m_truthTimeCombo->addItems(m_truth.header);
    m_truthTimeCombo->setEnabled(!m_truth.header.isEmpty());
    if (m_truth.timeColumn >= 0 && m_truth.timeColumn < m_truth.header.size())
        m_truthTimeCombo->setCurrentIndex(m_truth.timeColumn);
}

void AnalysisPanel::onTruthTimeColChanged(int idx)
{
    if (idx < 0 || idx >= m_truth.header.size()) return;
    m_truth.timeColumn = idx;
    recomputeTimes(m_truth);
}

void AnalysisPanel::onAddCmpFile()
{
    if (m_loading) return;
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择对比文件"), m_lastDir,
        QStringLiteral("文本文件 (*.csv *.txt);;所有文件 (*)"));
    if (paths.isEmpty()) return;
    m_lastDir = QFileInfo(paths.first()).absolutePath();
    startLoadCmpFiles(paths);
}

void AnalysisPanel::startLoadCmpFiles(const QStringList &paths)
{
    setBusy(true);

    auto *watcher = new QFutureWatcher<QList<TextFileTable>>(this);
    m_watcher = watcher;
    connect(watcher, &QFutureWatcher<QList<TextFileTable>>::finished,
            this, &AnalysisPanel::onCmpLoadFinished);
    watcher->setFuture(QtConcurrent::run([](const QStringList &ps) {
        QList<TextFileTable> tables;
        for (const QString &p : ps)
            tables.append(loadTextFileTable(p));
        return tables;
    }, paths));
}

void AnalysisPanel::onCmpLoadFinished()
{
    auto *watcher = static_cast<QFutureWatcher<QList<TextFileTable>>*>(m_watcher);
    QList<TextFileTable> tables = watcher->result();
    watcher->deleteLater();
    m_watcher = nullptr;
    setBusy(false);

    int okCount = 0, failCount = 0;
    QStringList failMsgs;
    for (const TextFileTable &t : tables) {
        if (!t.error.isEmpty()) {
            ++failCount;
            failMsgs << QStringLiteral("  %1: %2").arg(QFileInfo(t.path).fileName(), t.error);
            continue;
        }
        m_cmpFiles.append(t);
        for (MetricMapping &m : m_mappings)
            m.cmpCols.append(-1);
        ++okCount;
    }

    if (failCount > 0) {
        QMessageBox::warning(this, QStringLiteral("部分文件加载失败"),
                             QStringLiteral("成功 %1 个，失败 %2 个:\n%3")
                                 .arg(okCount).arg(failCount).arg(failMsgs.join('\n')));
    }

    refreshCmpFileList();
    rebuildMappingTable();
    rebuildResultTable();
}

void AnalysisPanel::onRemoveCmpFile()
{
    const int row = m_cmpFileList->currentRow();
    if (row < 0 || row >= m_cmpFiles.size()) return;
    m_cmpFiles.removeAt(row);
    for (MetricMapping &m : m_mappings)
        if (row < m.cmpCols.size()) m.cmpCols.removeAt(row);
    refreshCmpFileList();
    rebuildMappingTable();
    rebuildResultTable();
}

void AnalysisPanel::refreshCmpFileList()
{
    m_cmpFileList->clear();
    for (const auto &t : m_cmpFiles) {
        m_cmpFileList->addItem(QStringLiteral("%1  (%2行 × %3列)")
                                   .arg(QFileInfo(t.path).fileName())
                                   .arg(t.rows.size())
                                   .arg(t.header.size()));
    }
    if (!m_cmpFiles.isEmpty() && m_cmpFileList->currentRow() < 0)
        m_cmpFileList->setCurrentRow(0);
}

void AnalysisPanel::onCmpFileSelected(int row)
{
    refreshCmpTimeCombo();
}

void AnalysisPanel::refreshCmpTimeCombo()
{
    m_cmpTimeCombo->clear();
    const int row = m_cmpFileList->currentRow();
    if (row < 0 || row >= m_cmpFiles.size()) {
        m_cmpTimeCombo->setEnabled(false);
        return;
    }
    m_cmpTimeCombo->addItems(m_cmpFiles[row].header);
    m_cmpTimeCombo->setEnabled(true);
    if (m_cmpFiles[row].timeColumn >= 0 && m_cmpFiles[row].timeColumn < m_cmpFiles[row].header.size())
        m_cmpTimeCombo->setCurrentIndex(m_cmpFiles[row].timeColumn);
}

void AnalysisPanel::onCmpTimeColChanged(int idx)
{
    const int row = m_cmpFileList->currentRow();
    if (row < 0 || row >= m_cmpFiles.size()) return;
    if (idx < 0 || idx >= m_cmpFiles[row].header.size()) return;
    m_cmpFiles[row].timeColumn = idx;
    recomputeTimes(m_cmpFiles[row]);
}

// ════════════════════════════════════════════════════════════════
//  对比项映射
// ════════════════════════════════════════════════════════════════
void AnalysisPanel::onAddMetric()
{
    MetricMapping m;
    m.cmpCols = QVector<int>(m_cmpFiles.size(), -1);
    m_mappings.append(m);
    rebuildMappingTable();
}

void AnalysisPanel::onRemoveMetric()
{
    const int row = m_mapTable->currentRow();
    if (row < 0 || row >= m_mappings.size()) return;
    m_mappings.removeAt(row);
    rebuildMappingTable();
}

static QComboBox *putCombo(QTableWidget *table, int row, int col,
                           const QStringList &items, int current)
{
    auto *combo = new QComboBox(table);
    combo->addItems(items);
    if (current >= 0 && current < items.size())
        combo->setCurrentIndex(current);
    table->setCellWidget(row, col, combo);
    return combo;
}

void AnalysisPanel::rebuildMappingTable()
{
    m_mapTable->blockSignals(true);

    QStringList headers{ QStringLiteral("指标名"), QStringLiteral("真值列"), QStringLiteral("扣均值") };
    for (const auto &t : m_cmpFiles)
        headers << QFileInfo(t.path).fileName();

    m_mapTable->clear();
    m_mapTable->setColumnCount(headers.size());
    m_mapTable->setHorizontalHeaderLabels(headers);
    m_mapTable->setRowCount(m_mappings.size());
    m_mapTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_mapTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    const QStringList truthCols = m_truth.header;

    for (int r = 0; r < m_mappings.size(); ++r) {
        MetricMapping &m = m_mappings[r];

        auto *nameItem = new QTableWidgetItem(m.name);
        m_mapTable->setItem(r, 0, nameItem);

        putCombo(m_mapTable, r, 1, truthCols, m.truthCol);

        auto *ck = new QCheckBox(m_mapTable);
        ck->setChecked(m.subtractMean);
        auto *ckWrap = new QWidget(m_mapTable);
        auto *ckLay = new QHBoxLayout(ckWrap);
        ckLay->addWidget(ck);
        ckLay->setAlignment(Qt::AlignCenter);
        ckLay->setContentsMargins(0, 0, 0, 0);
        m_mapTable->setCellWidget(r, 2, ckWrap);

        for (int f = 0; f < m_cmpFiles.size(); ++f) {
            const QStringList cols = m_cmpFiles[f].header;
            putCombo(m_mapTable, r, 3 + f, cols, m.cmpCols.value(f, -1));
        }
    }

    // 从单元格控件回写 m_mappings
    const int cmpCount = m_cmpFiles.size();
    for (int r = 0; r < m_mappings.size(); ++r) {
        const int rr = r;

        if (auto *tc = qobject_cast<QComboBox*>(m_mapTable->cellWidget(r, 1))) {
            connect(tc, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                    [this, rr](int idx) {
                        if (rr >= 0 && rr < m_mappings.size())
                            m_mappings[rr].truthCol = idx;
                    });
        }

        if (auto *wrap = qobject_cast<QWidget*>(m_mapTable->cellWidget(r, 2))) {
            if (auto *ck = wrap->findChild<QCheckBox*>()) {
                connect(ck, &QCheckBox::toggled, this, [this, rr](bool on) {
                    if (rr >= 0 && rr < m_mappings.size())
                        m_mappings[rr].subtractMean = on;
                });
            }
        }

        for (int f = 0; f < cmpCount; ++f) {
            const int ff = f;
            if (auto *cc = qobject_cast<QComboBox*>(m_mapTable->cellWidget(r, 3 + f))) {
                connect(cc, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                        [this, rr, ff](int idx) {
                            if (rr >= 0 && rr < m_mappings.size()) {
                                if (m_mappings[rr].cmpCols.size() <= ff)
                                    m_mappings[rr].cmpCols.resize(ff + 1);
                                m_mappings[rr].cmpCols[ff] = idx;
                            }
                        });
            }
        }
    }

    m_mapTable->blockSignals(false);
}

// ════════════════════════════════════════════════════════════════
//  场景切段
// ════════════════════════════════════════════════════════════════
void AnalysisPanel::onAddScene()
{
    bool ok1 = false, ok2 = false;
    const double startSec = m_sceneStartEdit->text().trimmed().toDouble(&ok1);
    const double endSec   = m_sceneEndEdit->text().trimmed().toDouble(&ok2);
    if (!ok1 || !ok2) {
        QMessageBox::warning(this, QStringLiteral("输入错误"),
                             QStringLiteral("请填写有效的起始/结束时间（秒）。"));
        return;
    }
    if (endSec <= startSec) {
        QMessageBox::warning(this, QStringLiteral("输入错误"),
                             QStringLiteral("结束时间必须大于起始时间。"));
        return;
    }

    SceneSegment seg;
    seg.name = m_sceneNameEdit->text().trimmed();
    if (seg.name.isEmpty())
        seg.name = QStringLiteral("场景%1").arg(m_scenes.size() + 1);
    seg.startSec = startSec;
    seg.endSec   = endSec;
    m_scenes.append(seg);
    refreshSceneList();
}

void AnalysisPanel::onRemoveScene()
{
    const int row = m_sceneList->currentRow();
    if (row < 0 || row >= m_scenes.size()) return;
    m_scenes.removeAt(row);
    refreshSceneList();
}

void AnalysisPanel::refreshSceneList()
{
    m_sceneList->clear();
    for (const auto &s : m_scenes) {
        QString name = s.name.isEmpty() ? QStringLiteral("场景%1").arg(m_sceneList->count() + 1) : s.name;
        m_sceneList->addItem(QStringLiteral("%1  [%2s - %3s]")
                                 .arg(name)
                                 .arg(s.startSec, 0, 'f', 3)
                                 .arg(s.endSec, 0, 'f', 3));
    }
}

// ════════════════════════════════════════════════════════════════
//  结果计算
// ════════════════════════════════════════════════════════════════
void AnalysisPanel::onStartCompare()
{
    if (m_truth.header.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先导入真值文件。"));
        return;
    }
    if (m_cmpFiles.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先导入至少一个对比文件。"));
        return;
    }
    if (m_mappings.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先添加对比项并指定列映射。"));
        return;
    }

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    m_progressBar->setVisible(true);
    QApplication::processEvents();

    // 有效场景：未定义场景时用全路段
    QList<SceneSegment> effScenes = m_scenes;
    if (effScenes.isEmpty()) {
        SceneSegment all;
        all.name = QStringLiteral("全路段");
        all.startSec = -std::numeric_limits<double>::max();
        all.endSec   =  std::numeric_limits<double>::max();
        effScenes.append(all);
    }

    // 每个对比文件先做一次时间戳精确对齐（复用索引配对）
    QVector<QVector<MatchedPair>> matchIdx(m_cmpFiles.size());
    for (int f = 0; f < m_cmpFiles.size(); ++f)
        matchIdx[f] = exactMatchTimes(m_truth.times, m_cmpFiles[f].times);

    m_resultTable->clear();
    m_resultTable->setColumnCount(7);
    m_resultTable->setHorizontalHeaderLabels({
        QStringLiteral("场景"), QStringLiteral("指标"), QStringLiteral("对比文件"),
        QStringLiteral("样本数"), QStringLiteral("均值偏置"),
        QStringLiteral("RMS"), QStringLiteral("2σ") });
    m_resultTable->setRowCount(0);
    m_resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    int outRow = 0;
    for (const MetricMapping &m : m_mappings) {
        if (m.truthCol < 0 || m.truthCol >= m_truth.header.size()) continue;
        const QString metricName = m.name.isEmpty()
            ? m_truth.header.value(m.truthCol) : m.name;

        for (int f = 0; f < m_cmpFiles.size(); ++f) {
            const int cmpCol = m.cmpCols.value(f, -1);
            if (cmpCol < 0 || cmpCol >= m_cmpFiles[f].header.size()) continue;

            const TextFileTable &cmp = m_cmpFiles[f];
            const QVector<MatchedPair> &pairs = matchIdx[f];

            for (const SceneSegment &sc : effScenes) {
                double sum = 0.0, sumSq = 0.0;
                int n = 0;
                for (const MatchedPair &p : pairs) {
                    const double t = m_truth.times[p.truthIdx];
                    if (t < sc.startSec || t > sc.endSec) continue;

                    bool ok1 = false, ok2 = false;
                    const double tv = cellDouble(m_truth, p.truthIdx, m.truthCol, &ok1);
                    const double cv = cellDouble(cmp, p.cmpIdx, cmpCol, &ok2);
                    if (!ok1 || !ok2) continue;

                    const double err = cv - tv;   // 误差 = 对比 - 真值
                    sum += err;
                    sumSq += err * err;
                    ++n;
                }

                if (n == 0) continue;

                const double mean  = sum / n;
                const double rmsBias = std::sqrt(sumSq / n);
                const double var = sumSq / n - mean * mean;
                const double sigma = var > 0.0 ? std::sqrt(var) : 0.0;
                const double rms = m.subtractMean ? sigma : rmsBias;

                m_resultTable->insertRow(outRow);
                m_resultTable->setItem(outRow, 0, new QTableWidgetItem(sc.name));
                m_resultTable->setItem(outRow, 1, new QTableWidgetItem(metricName));
                m_resultTable->setItem(outRow, 2, new QTableWidgetItem(QFileInfo(cmp.path).fileName()));
                m_resultTable->setItem(outRow, 3, new QTableWidgetItem(QString::number(n)));
                m_resultTable->setItem(outRow, 4, new QTableWidgetItem(fmtNum(mean)));
                m_resultTable->setItem(outRow, 5, new QTableWidgetItem(fmtNum(rms)));
                m_resultTable->setItem(outRow, 6, new QTableWidgetItem(fmtNum(2.0 * sigma)));
                ++outRow;
            }
        }
    }

    m_progressBar->setVisible(false);
    QGuiApplication::restoreOverrideCursor();

    if (outRow == 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("没有精确对齐的时间戳数据，请确认两边时间列单位一致且存在相同时间戳。"));
    }
}

void AnalysisPanel::rebuildResultTable()
{
    m_resultTable->clear();
    m_resultTable->setColumnCount(7);
    m_resultTable->setHorizontalHeaderLabels({
        QStringLiteral("场景"), QStringLiteral("指标"), QStringLiteral("对比文件"),
        QStringLiteral("样本数"), QStringLiteral("均值偏置"),
        QStringLiteral("RMS"), QStringLiteral("2σ") });
    m_resultTable->setRowCount(0);
    m_resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

} // namespace Analysis
