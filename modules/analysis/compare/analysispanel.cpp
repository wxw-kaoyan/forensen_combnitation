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
#include <QDoubleSpinBox>
#include <QTextStream>
#include <QRegularExpression>
#include <QPainter>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <cmath>
#include <algorithm>
#include <limits>
#include <QtMath>

namespace Analysis {

// ════════════════════════════════════════════════════════════════
//  通用工具（分隔符 / 切分 / 时间列识别 / 数值格式化 / 精确时间匹配）
// ════════════════════════════════════════════════════════════════
static QChar detectDelimiter(const QString &line)
{
    int comma = line.count(',');
    int tab   = line.count('\t');
    int semi  = line.count(';');
    if (tab > 0 && tab >= comma && tab >= semi) return '\t';
    if (comma > 0 && comma >= semi) return ',';
    if (semi > 0) return ';';
    return ' ';  // 默认按空白分割
}

static QStringList splitFields(const QString &line, QChar delim)
{
    if (delim == ' ' || delim == '\t') {
        static const QRegularExpression ws(QStringLiteral("\\s+"));
        return line.trimmed().split(ws, Qt::SkipEmptyParts);
    }
    return line.split(delim);
}

// ── 时间列识别结果 ──
struct TimeDetect {
    int  timeColumn    = -1;   // 秒/毫秒列
    int  timeDayColumn = -1;   // 天数列（天内秒模式；-1 = 周内秒模式）
    bool milliseconds  = false;// true = 毫秒，false = 秒
};

// 通用时间列识别：支持 周内秒 / 周内毫秒 / 天数+天内秒 等格式
static TimeDetect detectTime(const QStringList &header)
{
    TimeDetect r;

    // 1) 毫秒列（优先）
    for (int i = 0; i < header.size(); ++i) {
        const QString h = header[i].toLower();
        if (h.contains(QStringLiteral("毫秒")) || h.contains(QStringLiteral("ms"))) {
            r.timeColumn = i;
            r.milliseconds = true;
            break;
        }
    }
    // 2) 秒列（周内秒 / 天内秒 / itow / time / sod）
    if (r.timeColumn < 0) {
        for (int i = 0; i < header.size(); ++i) {
            const QString h = header[i].toLower();
            if (h.contains(QStringLiteral("秒")) || h.contains(QStringLiteral("itow")) ||
                h.contains(QStringLiteral("time")) || h.contains(QStringLiteral("sod"))) {
                r.timeColumn = i;
                break;
            }
        }
    }
    // 3) 默认第 0 列（未识别到任何时间列时）
    if (r.timeColumn < 0)
        r.timeColumn = 0;

    // 4) 天内秒模式：时间列名含"天内"或"sod"，需配合天数列
    if (r.timeColumn < header.size()) {
        const QString th = header[r.timeColumn].toLower();
        if (th.contains(QStringLiteral("天内")) || th.contains(QStringLiteral("sod"))) {
            for (int i = 0; i < header.size(); ++i) {
                if (i == r.timeColumn) continue;
                const QString h = header[i].toLower();
                if ((h.contains(QStringLiteral("天")) || h.contains(QStringLiteral("day"))) &&
                    !h.contains(QStringLiteral("秒")) && !h.contains(QStringLiteral("毫秒"))) {
                    r.timeDayColumn = i;
                    break;
                }
            }
        }
    }
    return r;
}

// 识别前向速度列与速度方向列（用于派生 VelN/VelE）。无则对应返回 -1。
static void detectSpeedColumns(const QStringList &header, int *speedCol, int *dirCol)
{
    *speedCol = -1;
    *dirCol   = -1;
    for (int i = 0; i < header.size(); ++i) {
        const QString h = header[i].toLower();
        if (*speedCol < 0 && h.contains(QStringLiteral("速度")) &&
            (h.contains(QStringLiteral("前向")) || h.contains(QStringLiteral("m/s")) ||
             h.contains(QStringLiteral("speed"))))
            *speedCol = i;
        if (*dirCol < 0 && h.contains(QStringLiteral("方向")) &&
            (h.contains(QStringLiteral("速度")) || h.contains(QStringLiteral("dir"))))
            *dirCol = i;
    }
}

static QString fmtNum(double v)
{
    if (!std::isfinite(v)) return QStringLiteral("-");
    return QString::number(v, 'g', 8);
}

// 时间戳精确对齐：两个升序数组完全相等才配对（带 1e-6 秒容差抗浮点噪声）
struct MatchedPair { int aIdx; int bIdx; };

static QVector<MatchedPair> exactMatchTimes(const QVector<double> &a,
                                            const QVector<double> &b)
{
    constexpr double kEps = 1e-6;
    QVector<MatchedPair> res;
    int i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        const double d = a[i] - b[j];
        if (std::abs(d) <= kEps) {
            res.append({i, j});
            ++i; ++j;
        } else if (d < 0) {
            ++i;
        } else {
            ++j;
        }
    }
    return res;
}

// ════════════════════════════════════════════════════════════════
//  静态辅助：真值字段名/单位/取值，时间换算，文件读取
// ════════════════════════════════════════════════════════════════
QString AnalysisPanel::truthFieldName(TruthField f)
{
    switch (f) {
        case TruthField::Lat:     return QStringLiteral("纬度");
        case TruthField::Lon:     return QStringLiteral("经度");
        case TruthField::Height:  return QStringLiteral("椭球高");
        case TruthField::Roll:    return QStringLiteral("横滚(Roll)");
        case TruthField::Pitch:   return QStringLiteral("俯仰(Pitch)");
        case TruthField::Heading: return QStringLiteral("航向(Heading)");
        case TruthField::VelN:    return QStringLiteral("北向速度(VelN)");
        case TruthField::VelE:    return QStringLiteral("东向速度(VelE)");
        case TruthField::VelU:    return QStringLiteral("天向速度(VelU)");
    }
    return QString();
}

QString AnalysisPanel::truthFieldUnit(TruthField f)
{
    switch (f) {
        case TruthField::Lat:
        case TruthField::Lon:
        case TruthField::Roll:
        case TruthField::Pitch:
        case TruthField::Heading: return QStringLiteral("deg");
        case TruthField::Height:  return QStringLiteral("m");
        case TruthField::VelN:
        case TruthField::VelE:
        case TruthField::VelU:    return QStringLiteral("m/s");
    }
    return QString();
}

QVector<double> AnalysisPanel::truthFieldValues(const TruthSummary &s, TruthField f)
{
    QVector<double> v;
    v.reserve(s.samples.size());
    for (const TruthSample &sp : s.samples) {
        switch (f) {
            case TruthField::Lat:     v.append(sp.lat); break;
            case TruthField::Lon:     v.append(sp.lon); break;
            case TruthField::Height:  v.append(sp.h); break;
            case TruthField::Roll:    v.append(sp.roll); break;
            case TruthField::Pitch:   v.append(sp.pitch); break;
            case TruthField::Heading: v.append(sp.heading); break;
            case TruthField::VelN:    v.append(sp.velN); break;
            case TruthField::VelE:    v.append(sp.velE); break;
            case TruthField::VelU:    v.append(sp.velU); break;
        }
    }
    return v;
}

double AnalysisPanel::toGpsTime(double raw, TimeStandard std, double leap)
{
    return (std == TimeStandard::Utc) ? (raw + leap) : raw;
}

// 「天数 + 天内秒」合成 GPS 周内秒：day/sod 为同一基准（GPS 或 UTC 由 std 指定）
// UTC 时加闰秒（GPS 领先 UTC leap 秒），最后折到一周（0~604800）内。
double AnalysisPanel::daySodToGpsWeekSec(double day, double sod, TimeStandard std, double leap)
{
    double absSec = day * 86400.0 + sod;
    if (std == TimeStandard::Utc)
        absSec += leap;   // GPS = UTC + 闰秒
    return std::fmod(absSec, 604800.0);
}

bool AnalysisPanel::readCmpHeader(const QString &path, QStringList *header, int *dataStart, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("无法打开文件");
        return false;
    }
    QTextStream ts(&f);

    // 流式定位表头行（不 readAll，避免 100MB+ 大文件整体读入内存卡机）
    // 对比文件为 CSV/TXT：首个非空行即表头
    QString headerLine;
    int headerIdx = -1;          // 表头行号（0-based）
    int lineNo = 0;
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        if (!line.trimmed().isEmpty()) {
            headerLine = line;
            headerIdx = lineNo;
            break;
        }
        ++lineNo;
    }

    if (headerIdx < 0) { if (err) *err = QStringLiteral("空文件"); return false; }

    const QChar delim = detectDelimiter(headerLine);
    const QStringList hdr = splitFields(headerLine, delim);
    if (hdr.isEmpty()) { if (err) *err = QStringLiteral("无法识别表头"); return false; }

    // 数据起始行：表头下一行；若是单位行（首字段以 '(' 开头）再跳一行
    int ds = headerIdx + 1;
    if (!ts.atEnd()) {
        const QString nextLine = ts.readLine();
        const QStringList unitFields = splitFields(nextLine, delim);
        if (!unitFields.isEmpty() && unitFields.first().startsWith('('))
            ++ds;
    }
    f.close();

    if (header) *header = hdr;
    if (dataStart) *dataStart = ds;
    if (err) err->clear();
    return true;
}

bool AnalysisPanel::readCmpColumn(const QString &path, int dataStart, int col,
                                  QVector<double> *out, QString *err)
{
    if (!out || col < 0) return false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("无法打开文件");
        return false;
    }
    QTextStream ts(&f);
    out->clear();

    QChar delim('\0');
    int lineNo = 0;
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (lineNo < dataStart) { ++lineNo; continue; }
        ++lineNo;
        if (line.isEmpty()) continue;
        if (delim == '\0') delim = detectDelimiter(line);   // 用首个数据行推断分隔符
        const QStringList cols = splitFields(line, delim);
        if (col < cols.size()) {
            bool ok = false;
            const double v = cols[col].toDouble(&ok);
            // 无效值（N/A、空、非数字）记 NaN，统计时跳过，避免被当成 0 拉大误差
            out->append(ok ? v : std::numeric_limits<double>::quiet_NaN());
        } else {
            out->append(std::numeric_limits<double>::quiet_NaN());
        }
    }
    f.close();
    if (err) err->clear();
    return true;
}

// ════════════════════════════════════════════════════════════════
//  面板
// ════════════════════════════════════════════════════════════════
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
    left->setMinimumWidth(420);
    left->setMaximumWidth(540);
    left->setFrameShape(QFrame::StyledPanel);
    auto *ll = new QVBoxLayout(left);
    ll->setSpacing(6);

    auto addSectionLabel = [&](const QString &s) {
        auto *lbl = new QLabel(s, left);
        lbl->setStyleSheet(QStringLiteral("font-size:13px;font-weight:700;color:#12263f;"));
        ll->addWidget(lbl);
    };

    // 真值文件
    addSectionLabel(QStringLiteral("真值文件（GINS 基准）"));
    auto *truthRow = new QHBoxLayout;
    m_truthPathEdit = new QLineEdit(left);
    m_truthPathEdit->setPlaceholderText(QStringLiteral("选择真值文件（自动识别列）"));
    auto *browseBtn = new QPushButton(QStringLiteral("浏览"), left);
    browseBtn->setFixedWidth(72);
    truthRow->addWidget(m_truthPathEdit, 1);
    truthRow->addWidget(browseBtn);
    ll->addLayout(truthRow);

    auto *truthOptForm = new QFormLayout;
    m_truthStandardCombo = new QComboBox(left);
    m_truthStandardCombo->addItem(QStringLiteral("GPS 周内秒"));
    m_truthStandardCombo->addItem(QStringLiteral("UTC 周内秒"));
    truthOptForm->addRow(QStringLiteral("时间标准"), m_truthStandardCombo);

    m_heightAnomalySpin = new QDoubleSpinBox(left);
    m_heightAnomalySpin->setRange(-100000.0, 100000.0);
    m_heightAnomalySpin->setDecimals(3);
    m_heightAnomalySpin->setValue(0.0);
    m_heightAnomalySpin->setSuffix(QStringLiteral(" m"));
    truthOptForm->addRow(QStringLiteral("高程异常"), m_heightAnomalySpin);
    ll->addLayout(truthOptForm);

    m_truthInfoLabel = new QLabel(QStringLiteral("未加载真值"), left);
    m_truthInfoLabel->setWordWrap(true);
    m_truthInfoLabel->setStyleSheet(QStringLiteral("color:#617487;font-size:12px;"));
    ll->addWidget(m_truthInfoLabel);

    auto *sep1 = new QFrame(left);
    sep1->setFrameShape(QFrame::HLine);
    ll->addWidget(sep1);

    // 对比文件
    addSectionLabel(QStringLiteral("对比文件"));
    m_cmpFileList = new QListWidget(left);
    m_cmpFileList->setMinimumHeight(70);
    ll->addWidget(m_cmpFileList, 1);

    auto *cmpBtnRow = new QHBoxLayout;
    auto *addCmpBtn = new QPushButton(QStringLiteral("添加"), left);
    auto *rmCmpBtn  = new QPushButton(QStringLiteral("移除"), left);
    cmpBtnRow->addWidget(addCmpBtn);
    cmpBtnRow->addWidget(rmCmpBtn);
    cmpBtnRow->addStretch();
    ll->addLayout(cmpBtnRow);

    auto *cmpOptForm = new QFormLayout;
    m_cmpTimeCombo = new QComboBox(left);
    m_cmpTimeCombo->setEnabled(false);
    cmpOptForm->addRow(QStringLiteral("时间列(秒)"), m_cmpTimeCombo);

    m_cmpDayCombo = new QComboBox(left);
    m_cmpDayCombo->setEnabled(false);
    cmpOptForm->addRow(QStringLiteral("天数列(可选)"), m_cmpDayCombo);

    m_cmpStandardCombo = new QComboBox(left);
    m_cmpStandardCombo->addItem(QStringLiteral("GPS 周内秒"));
    m_cmpStandardCombo->addItem(QStringLiteral("UTC 周内秒"));
    m_cmpStandardCombo->setEnabled(false);
    cmpOptForm->addRow(QStringLiteral("时间标准"), m_cmpStandardCombo);
    ll->addLayout(cmpOptForm);

    m_alignInfoLabel = new QLabel(left);
    m_alignInfoLabel->setWordWrap(true);
    m_alignInfoLabel->setStyleSheet(QStringLiteral("color:#617487;font-size:12px;"));
    ll->addWidget(m_alignInfoLabel);

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
    addSectionLabel(QStringLiteral("场景切段（手动截断时间，秒）"));
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
    m_sceneList->setMinimumHeight(56);
    ll->addWidget(m_sceneList, 1);

    // 闰秒（可配置，默认 18）
    auto *leapForm = new QFormLayout;
    m_leapSpin = new QDoubleSpinBox(left);
    m_leapSpin->setRange(0.0, 60.0);
    m_leapSpin->setDecimals(0);
    m_leapSpin->setValue(m_leapSeconds);
    m_leapSpin->setSuffix(QStringLiteral(" s"));
    leapForm->addRow(QStringLiteral("闰秒(GPS-UTC)"), m_leapSpin);
    ll->addLayout(leapForm);

    m_startBtn = new QPushButton(QStringLiteral("开始对比"), left);
    m_startBtn->setStyleSheet(QStringLiteral("font-weight:600;color:#fff;background-color:#2c7be5;padding:8px;"));
    ll->addWidget(m_startBtn);

    m_progressBar = new QProgressBar(left);
    m_progressBar->setRange(0, 0);
    m_progressBar->setVisible(false);
    ll->addWidget(m_progressBar);

    // ── 右侧：结果表 + 误差曲线 ──
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
    rl->addWidget(m_resultTable, 2);

    auto *errorTitle = new QLabel(QStringLiteral("误差曲线"), right);
    errorTitle->setStyleSheet(QStringLiteral("font-size:13px;font-weight:700;color:#12263f;"));
    rl->addWidget(errorTitle);

    m_errorChartView = new QChartView(right);
    m_errorChartView->setRenderHint(QPainter::Antialiasing, false);
    m_errorChartView->setMinimumHeight(200);
    rl->addWidget(m_errorChartView, 1);

    mainLayout->addWidget(left, 0);
    mainLayout->addWidget(right, 1);

    // ── 信号 ──
    connect(browseBtn, &QPushButton::clicked, this, &AnalysisPanel::onBrowseTruth);
    connect(m_truthStandardCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalysisPanel::onTruthStandardChanged);
    connect(m_heightAnomalySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double v) { m_truth.heightAnomaly = v; });
    connect(addCmpBtn, &QPushButton::clicked, this, &AnalysisPanel::onAddCmpFile);
    connect(rmCmpBtn,  &QPushButton::clicked, this, &AnalysisPanel::onRemoveCmpFile);
    connect(m_cmpFileList, &QListWidget::currentRowChanged,
            this, &AnalysisPanel::onCmpFileSelected);
    connect(m_cmpTimeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalysisPanel::onCmpTimeColChanged);
    connect(m_cmpDayCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalysisPanel::onCmpDayColChanged);
    connect(m_cmpStandardCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalysisPanel::onCmpStandardChanged);
    connect(addMetricBtn, &QPushButton::clicked, this, &AnalysisPanel::onAddMetric);
    connect(rmMetricBtn,  &QPushButton::clicked, this, &AnalysisPanel::onRemoveMetric);
    connect(addSceneBtn, &QPushButton::clicked, this, &AnalysisPanel::onAddScene);
    connect(rmSceneBtn,  &QPushButton::clicked, this, &AnalysisPanel::onRemoveScene);
    connect(m_startBtn, &QPushButton::clicked, this, &AnalysisPanel::onStartCompare);
    connect(m_leapSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double v) {
                m_leapSeconds = v;
                recomputeTruthTimeGps();
                for (int i = 0; i < m_cmpFiles.size(); ++i)
                    recomputeCmpTimeGps(i);
                refreshTruthInfo();
                updateAlignStatus();
            });

    // 忙碌时禁用
    m_busyWidgets = {
        browseBtn, m_truthStandardCombo, m_heightAnomalySpin, m_leapSpin,
        addCmpBtn, rmCmpBtn, m_cmpFileList, m_cmpTimeCombo, m_cmpDayCombo, m_cmpStandardCombo,
        addMetricBtn, rmMetricBtn, m_mapTable,
        m_sceneNameEdit, m_sceneStartEdit, m_sceneEndEdit,
        addSceneBtn, rmSceneBtn, m_sceneList, m_startBtn
    };

    // 指标名编辑回写
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
//  真值加载（复用 TruthReader 自动解析）
// ════════════════════════════════════════════════════════════════
void AnalysisPanel::onBrowseTruth()
{
    if (m_loading) return;
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择真值文件"), m_lastDir,
        QStringLiteral("文本文件 (*.csv *.txt *.out);;所有文件 (*)"));
    if (path.isEmpty()) return;
    m_lastDir = QFileInfo(path).absolutePath();
    startLoadTruth(path);
}

void AnalysisPanel::startLoadTruth(const QString &path)
{
    m_pendingTruthPath = path;
    setBusy(true);
    auto *watcher = new QFutureWatcher<TruthSummary>(this);
    m_watcher = watcher;
    connect(watcher, &QFutureWatcher<TruthSummary>::finished,
            this, &AnalysisPanel::onTruthLoadFinished);
    watcher->setFuture(QtConcurrent::run([path]() -> TruthSummary {
        TruthSummary s;
        QString err;
        if (!TruthReader::analyze(path, &s, &err))
            s.totalSamples = -1;   // 用负数标记失败
        return s;
    }));
}

void AnalysisPanel::onTruthLoadFinished()
{
    auto *watcher = static_cast<QFutureWatcher<TruthSummary>*>(m_watcher);
    TruthSummary s = watcher->result();
    watcher->deleteLater();
    m_watcher = nullptr;
    setBusy(false);

    if (s.totalSamples < 0 || s.samples.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("加载失败"),
                             QStringLiteral("无法解析真值文件（未识别到经纬度等列）。"));
        return;
    }

    m_truth = TruthSource();
    m_truth.path = m_pendingTruthPath;
    m_truth.summary = s;
    m_truth.timeStandard = static_cast<TimeStandard>(m_truthStandardCombo->currentIndex());
    m_truth.heightAnomaly = m_heightAnomalySpin->value();
    recomputeTruthTimeGps();

    m_truthPathEdit->setText(m_truth.path);
    refreshTruthInfo();
    updateAlignStatus();
    rebuildMappingTable();
}

void AnalysisPanel::recomputeTruthTimeGps()
{
    if (m_truth.summary.samples.isEmpty()) {
        m_truth.timeGps.clear();
        return;
    }
    const TruthSummary &s = m_truth.summary;
    m_truth.timeGps.resize(s.samples.size());
    for (int i = 0; i < s.samples.size(); ++i)
        m_truth.timeGps[i] = toGpsTime(s.firstTime + s.samples[i].t, m_truth.timeStandard, m_leapSeconds);
}

void AnalysisPanel::refreshTruthInfo()
{
    if (m_truth.summary.samples.isEmpty()) {
        m_truthInfoLabel->setText(QStringLiteral("未加载真值"));
        return;
    }
    const TruthSummary &s = m_truth.summary;
    m_truthInfoLabel->setText(
        QStringLiteral("样本 %1，时长 %2s，采样率 %3Hz；"
                       "时间范围 %4 ~ %5（GPS 周内秒）")
            .arg(s.totalSamples)
            .arg(s.durationSec, 0, 'f', 1)
            .arg(s.sampleRate, 0, 'f', 2)
            .arg(m_truth.timeGps.isEmpty() ? 0.0 : m_truth.timeGps.first(), 0, 'f', 3)
            .arg(m_truth.timeGps.isEmpty() ? 0.0 : m_truth.timeGps.last(), 0, 'f', 3));
}

void AnalysisPanel::onTruthStandardChanged(int idx)
{
    if (m_truth.summary.samples.isEmpty()) return;
    m_truth.timeStandard = static_cast<TimeStandard>(idx);
    recomputeTruthTimeGps();
    refreshTruthInfo();
    updateAlignStatus();
}

void AnalysisPanel::setBusy(bool busy)
{
    m_loading = busy;
    for (QWidget *w : m_busyWidgets)
        if (w) w->setEnabled(!busy);
    m_progressBar->setVisible(busy);
}

// ════════════════════════════════════════════════════════════════
//  对比文件加载（只读表头 + 时间列，不缓存全量）
// ════════════════════════════════════════════════════════════════
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

    auto *watcher = new QFutureWatcher<QList<CmpSource>>(this);
    m_watcher = watcher;
    connect(watcher, &QFutureWatcher<QList<CmpSource>>::finished,
            this, &AnalysisPanel::onCmpLoadFinished);

    watcher->setFuture(QtConcurrent::run([](const QStringList &ps) -> QList<CmpSource> {
        QList<CmpSource> result;
        for (const QString &p : ps) {
            CmpSource c;
            c.path = p;
            QString err;
            if (!AnalysisPanel::readCmpHeader(p, &c.header, &c.dataStart, &err)) {
                c.header.clear();
                c.dataStart = -1;   // 标记失败
                continue;
            }
            const TimeDetect td = detectTime(c.header);
            c.timeColumn = td.timeColumn;
            c.timeDayColumn = td.timeDayColumn;
            c.milliseconds = td.milliseconds;
            detectSpeedColumns(c.header, &c.speedColumn, &c.dirColumn);

            // 时间列名含 UTC 时，默认按 UTC 基准（GPS = UTC + 闰秒）
            if (c.timeDayColumn >= 0 && c.timeDayColumn < c.header.size() &&
                c.header[c.timeDayColumn].toLower().contains(QStringLiteral("utc")))
                c.timeStandard = TimeStandard::Utc;
            else if (c.timeColumn >= 0 && c.timeColumn < c.header.size() &&
                     c.header[c.timeColumn].toLower().contains(QStringLiteral("utc")))
                c.timeStandard = TimeStandard::Utc;

            // 读取秒/毫秒列（仅时间列，保持轻量）；存原始值，换算交给 recomputeCmpTimeGps
            QVector<double> raw;
            if (AnalysisPanel::readCmpColumn(p, c.dataStart, c.timeColumn, &raw, &err))
                c.timeRaw = raw;
            else
                c.dataStart = -1;

            // 天内秒模式：读取天数列
            if (c.timeDayColumn >= 0) {
                QVector<double> dayRaw;
                if (AnalysisPanel::readCmpColumn(p, c.dataStart, c.timeDayColumn, &dayRaw, &err))
                    c.timeDayRaw = dayRaw;
                else
                    c.dataStart = -1;
            }
            result.append(c);
        }
        return result;
    }, paths));
}

void AnalysisPanel::onCmpLoadFinished()
{
    auto *watcher = static_cast<QFutureWatcher<QList<CmpSource>>*>(m_watcher);
    QList<CmpSource> loaded = watcher->result();
    watcher->deleteLater();
    m_watcher = nullptr;
    setBusy(false);

    int okCount = 0;
    for (CmpSource &c : loaded) {
        if (c.dataStart < 0 || c.header.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("加载失败"),
                                 QStringLiteral("无法读取表头：%1").arg(QFileInfo(c.path).fileName()));
            continue;
        }
        m_cmpFiles.append(c);
        for (MetricMapping &m : m_mappings)
            m.cmpCols.append(-1);
        ++okCount;
    }

    if (okCount == 0) {
        rebuildMappingTable();
        return;
    }

    // 统一换算时间列到 GPS 周内秒
    for (int i = 0; i < m_cmpFiles.size(); ++i)
        recomputeCmpTimeGps(i);

    refreshCmpFileList();
    rebuildMappingTable();
    updateAlignStatus();
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
    updateAlignStatus();
}

void AnalysisPanel::recomputeCmpTimeGps(int fileIdx)
{
    if (fileIdx < 0 || fileIdx >= m_cmpFiles.size()) return;
    CmpSource &c = m_cmpFiles[fileIdx];
    // 从原始时间列换算到 GPS 周内秒（幂等：每次都以 timeRaw/timeDayRaw 为基准，切换标准不会重复换算）
    c.timeGps.resize(c.timeRaw.size());
    for (int i = 0; i < c.timeRaw.size(); ++i) {
        const double sec = c.milliseconds ? c.timeRaw[i] / 1000.0 : c.timeRaw[i];   // 毫秒 → 秒
        if (c.timeDayColumn >= 0 && c.timeDayRaw.size() == c.timeRaw.size())
            c.timeGps[i] = daySodToGpsWeekSec(c.timeDayRaw[i], sec, c.timeStandard, m_leapSeconds);
        else
            c.timeGps[i] = toGpsTime(sec, c.timeStandard, m_leapSeconds);
    }
}

void AnalysisPanel::refreshCmpFileList()
{
    m_cmpFileList->clear();
    for (const auto &c : m_cmpFiles) {
        m_cmpFileList->addItem(QStringLiteral("%1  (%2 列)")
                                   .arg(QFileInfo(c.path).fileName())
                                   .arg(c.header.size()));
    }
    if (!m_cmpFiles.isEmpty() && m_cmpFileList->currentRow() < 0)
        m_cmpFileList->setCurrentRow(0);
}

void AnalysisPanel::onCmpFileSelected(int /*row*/)
{
    refreshCmpTimeCombo();
    refreshCmpStandardCombo();
}

void AnalysisPanel::refreshCmpTimeCombo()
{
    m_cmpTimeCombo->clear();
    m_cmpDayCombo->clear();
    const int row = m_cmpFileList->currentRow();
    if (row < 0 || row >= m_cmpFiles.size()) {
        m_cmpTimeCombo->setEnabled(false);
        m_cmpDayCombo->setEnabled(false);
        return;
    }
    m_cmpTimeCombo->addItems(m_cmpFiles[row].header);
    m_cmpTimeCombo->setEnabled(true);
    const int tc = m_cmpFiles[row].timeColumn;
    if (tc >= 0 && tc < m_cmpFiles[row].header.size())
        m_cmpTimeCombo->setCurrentIndex(tc);

    // 天数列下拉：第 0 项 = 无（单列时间），其余对应各列
    m_cmpDayCombo->addItem(QStringLiteral("（无 / 单列时间）"));
    m_cmpDayCombo->addItems(m_cmpFiles[row].header);
    m_cmpDayCombo->setEnabled(true);
    const int dc = m_cmpFiles[row].timeDayColumn;
    m_cmpDayCombo->setCurrentIndex((dc >= 0 && dc < m_cmpFiles[row].header.size()) ? dc + 1 : 0);
}

void AnalysisPanel::refreshCmpStandardCombo()
{
    const int row = m_cmpFileList->currentRow();
    if (row < 0 || row >= m_cmpFiles.size()) {
        m_cmpStandardCombo->setEnabled(false);
        return;
    }
    m_cmpStandardCombo->setEnabled(true);
    m_cmpStandardCombo->setCurrentIndex(static_cast<int>(m_cmpFiles[row].timeStandard));
}

void AnalysisPanel::onCmpTimeColChanged(int idx)
{
    const int row = m_cmpFileList->currentRow();
    if (row < 0 || row >= m_cmpFiles.size()) return;
    if (idx < 0 || idx >= m_cmpFiles[row].header.size()) return;
    m_cmpFiles[row].timeColumn = idx;
    // 根据列名判断时间单位（毫秒 / 秒）
    const QString h = m_cmpFiles[row].header[idx].toLower();
    m_cmpFiles[row].milliseconds = h.contains(QStringLiteral("毫秒"));
    // 重新读取该文件的时间列（存原始值，再按当前标准换算）
    QVector<double> raw;
    QString err;
    if (AnalysisPanel::readCmpColumn(m_cmpFiles[row].path, m_cmpFiles[row].dataStart, idx, &raw, &err)) {
        m_cmpFiles[row].timeRaw = raw;
        recomputeCmpTimeGps(row);
    }
    updateAlignStatus();
}

void AnalysisPanel::onCmpDayColChanged(int idx)
{
    const int row = m_cmpFileList->currentRow();
    if (row < 0 || row >= m_cmpFiles.size()) return;
    // idx 0 = 无（单列时间），idx>=1 对应 header[idx-1]
    m_cmpFiles[row].timeDayColumn =
        (idx >= 1 && idx - 1 < m_cmpFiles[row].header.size()) ? (idx - 1) : -1;

    if (m_cmpFiles[row].timeDayColumn >= 0) {
        QVector<double> dayRaw;
        QString err;
        if (AnalysisPanel::readCmpColumn(m_cmpFiles[row].path, m_cmpFiles[row].dataStart,
                                         m_cmpFiles[row].timeDayColumn, &dayRaw, &err))
            m_cmpFiles[row].timeDayRaw = dayRaw;
        else
            m_cmpFiles[row].timeDayRaw.clear();
    } else {
        m_cmpFiles[row].timeDayRaw.clear();
    }
    recomputeCmpTimeGps(row);
    updateAlignStatus();
}

void AnalysisPanel::onCmpStandardChanged(int idx)
{
    const int row = m_cmpFileList->currentRow();
    if (row < 0 || row >= m_cmpFiles.size()) return;
    m_cmpFiles[row].timeStandard = static_cast<TimeStandard>(idx);
    recomputeCmpTimeGps(row);
    updateAlignStatus();
}

// 显示当前真值 vs 选中对比文件的时间对齐情况
void AnalysisPanel::updateAlignStatus()
{
    if (m_truth.timeGps.isEmpty()) {
        m_alignInfoLabel->setText(QString());
        return;
    }
    const int row = m_cmpFileList->currentRow();
    if (row < 0 || row >= m_cmpFiles.size() || m_cmpFiles[row].timeGps.isEmpty()) {
        m_alignInfoLabel->setText(QString());
        return;
    }
    const int n = exactMatchTimes(m_truth.timeGps, m_cmpFiles[row].timeGps).size();
    m_alignInfoLabel->setText(
        QStringLiteral("时间对齐：匹配 %1 个时间戳（共 %2 / %3）")
            .arg(n)
            .arg(m_truth.timeGps.size())
            .arg(m_cmpFiles[row].timeGps.size()));
}

// ════════════════════════════════════════════════════════════════
//  对比项映射
// ════════════════════════════════════════════════════════════════
void AnalysisPanel::onAddMetric()
{
    MetricMapping m;
    m.field = TruthField::Lat;
    m.name = truthFieldName(m.field);
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

    QStringList headers{ QStringLiteral("指标名"), QStringLiteral("真值字段"), QStringLiteral("扣均值") };
    for (const auto &c : m_cmpFiles)
        headers << QFileInfo(c.path).fileName();

    m_mapTable->clear();
    m_mapTable->setColumnCount(headers.size());
    m_mapTable->setHorizontalHeaderLabels(headers);
    m_mapTable->setRowCount(m_mappings.size());
    m_mapTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_mapTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    // 真值字段下拉列表
    QStringList fieldNames;
    for (int i = 0; i <= static_cast<int>(TruthField::VelU); ++i)
        fieldNames << truthFieldName(static_cast<TruthField>(i));

    for (int r = 0; r < m_mappings.size(); ++r) {
        MetricMapping &m = m_mappings[r];

        auto *nameItem = new QTableWidgetItem(m.name);
        m_mapTable->setItem(r, 0, nameItem);

        putCombo(m_mapTable, r, 1, fieldNames, static_cast<int>(m.field));

        auto *ck = new QCheckBox(m_mapTable);
        ck->setChecked(m.subtractMean);
        auto *ckWrap = new QWidget(m_mapTable);
        auto *ckLay = new QHBoxLayout(ckWrap);
        ckLay->addWidget(ck);
        ckLay->setAlignment(Qt::AlignCenter);
        ckLay->setContentsMargins(0, 0, 0, 0);
        m_mapTable->setCellWidget(r, 2, ckWrap);

        for (int f = 0; f < m_cmpFiles.size(); ++f) {
            QStringList items = m_cmpFiles[f].header;
            // 若存在前向速度 + 速度方向列，追加两个派生项（索引 = header.size() / +1）
            if (m_cmpFiles[f].speedColumn >= 0 && m_cmpFiles[f].dirColumn >= 0) {
                items << QStringLiteral("北向速度(前向速度×cos方向)");
                items << QStringLiteral("东向速度(前向速度×sin方向)");
            }
            putCombo(m_mapTable, r, 3 + f, items, m.cmpCols.value(f, -1));
        }
    }

    // 从单元格控件回写 m_mappings
    const int cmpCount = m_cmpFiles.size();
    for (int r = 0; r < m_mappings.size(); ++r) {
        const int rr = r;

        if (auto *fc = qobject_cast<QComboBox*>(m_mapTable->cellWidget(r, 1))) {
            connect(fc, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                    [this, rr](int idx) {
                        if (rr >= 0 && rr < m_mappings.size()) {
                            m_mappings[rr].field = static_cast<TruthField>(idx);
                            if (m_mappings[rr].name.isEmpty() ||
                                m_mappings[rr].name == truthFieldName(static_cast<TruthField>((idx + 8) % 9)))
                                m_mappings[rr].name = truthFieldName(static_cast<TruthField>(idx));
                        }
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
//  结果计算（后台线程）
// ════════════════════════════════════════════════════════════════
void AnalysisPanel::onStartCompare()
{
    if (m_truth.summary.samples.isEmpty()) {
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

    setBusy(true);

    // 有效场景：分析当前选中的场景；未选中任何场景时用全路段
    QList<SceneSegment> effScenes;
    const int sceneRow = m_sceneList->currentRow();
    if (sceneRow >= 0 && sceneRow < m_scenes.size()) {
        effScenes.append(m_scenes[sceneRow]);
    } else {
        SceneSegment all;
        all.name = QStringLiteral("全路段");
        all.startSec = -std::numeric_limits<double>::max();
        all.endSec   =  std::numeric_limits<double>::max();
        effScenes.append(all);
    }

    // 捕获副本（QVector 隐式共享，拷贝代价低）
    const TruthSource truth = m_truth;
    const QList<CmpSource> cmp = m_cmpFiles;
    const QList<MetricMapping> mappings = m_mappings;
    const double leap = m_leapSeconds;

    auto *watcher = new QFutureWatcher<CompareOutcome>(this);
    m_watcher = watcher;
    connect(watcher, &QFutureWatcher<CompareOutcome>::finished,
            this, &AnalysisPanel::onCompareFinished);

    watcher->setFuture(QtConcurrent::run(
        [truth, cmp, mappings, effScenes, leap]() -> CompareOutcome {
            CompareOutcome out;

            // 每个对比文件先做一次时间精确对齐（复用索引配对）
            QVector<QVector<MatchedPair>> matchIdx(cmp.size());
            for (int f = 0; f < cmp.size(); ++f)
                matchIdx[f] = exactMatchTimes(truth.timeGps, cmp[f].timeGps);

            for (const MetricMapping &m : mappings) {
                // 真值字段取值
                const QVector<double> truthVals = truthFieldValues(truth.summary, m.field);
                // 应用高程异常（仅对高程/天向做偏置修正：真值 = 原始 + 异常）
                double anomaly = 0.0;
                if (m.field == TruthField::Height || m.field == TruthField::VelU)
                    anomaly = truth.heightAnomaly;

                const QString metricName = m.name.isEmpty() ? truthFieldName(m.field) : m.name;

                for (int f = 0; f < cmp.size(); ++f) {
                    const int cmpCol = m.cmpCols.value(f, -1);
                    const int hdrSize = cmp[f].header.size();
                    if (cmpCol < 0 || cmpCol >= hdrSize + 2) continue;

                    // 懒加载：只读该映射列（派生列则读速度/方向两列现场换算）
                    QVector<double> cmpVals;
                    QString err;
                    if (cmpCol == hdrSize || cmpCol == hdrSize + 1) {
                        // 派生列：北向速度 / 东向速度 = 前向速度 × cos/sin(方向)
                        const bool velN = (cmpCol == hdrSize);
                        QVector<double> spd, dir;
                        if (!readCmpColumn(cmp[f].path, cmp[f].dataStart, cmp[f].speedColumn, &spd, &err) ||
                            !readCmpColumn(cmp[f].path, cmp[f].dataStart, cmp[f].dirColumn, &dir, &err))
                            continue;
                        const int nv = qMin(spd.size(), dir.size());
                        cmpVals.resize(nv);
                        for (int i = 0; i < nv; ++i) {
                            const double rad = qDegreesToRadians(dir[i]);
                            cmpVals[i] = velN ? spd[i] * std::cos(rad)
                                              : spd[i] * std::sin(rad);
                        }
                    } else if (!readCmpColumn(cmp[f].path, cmp[f].dataStart, cmpCol, &cmpVals, &err)) {
                        continue;
                    }

                    const QVector<MatchedPair> &pairs = matchIdx[f];

                    // 误差曲线（全时段，一个对比项×文件一条）
                    ErrorSeries es;
                    es.label = QStringLiteral("%1 [%2]").arg(metricName, QFileInfo(cmp[f].path).fileName());

                    for (const SceneSegment &sc : effScenes) {
                        double sum = 0.0, sumSq = 0.0;
                        int n = 0;
                        for (const MatchedPair &p : pairs) {
                            const double t = truth.timeGps[p.aIdx];
                            if (t < sc.startSec || t > sc.endSec) continue;
                            if (p.bIdx >= cmpVals.size()) continue;

                            const double tv = truthVals[p.aIdx] + anomaly;   // 真值（含高程异常修正）
                            const double cv = cmpVals[p.bIdx];
                            if (!std::isfinite(tv) || !std::isfinite(cv)) continue;   // 跳过无效值（N/A 等）
                            const double e = cv - tv;   // 误差 = 对比 - 真值
                            sum += e;
                            sumSq += e * e;
                            ++n;
                            es.time.append(t);
                            es.error.append(e);
                        }

                        if (n == 0) continue;
                        const double mean  = sum / n;
                        const double rmsBias = std::sqrt(sumSq / n);
                        const double var = sumSq / n - mean * mean;
                        const double sigma = var > 0.0 ? std::sqrt(var) : 0.0;
                        const double rms = m.subtractMean ? sigma : rmsBias;

                        CompareRow row;
                        row.scene  = sc.name;
                        row.metric = metricName;
                        row.file   = QFileInfo(cmp[f].path).fileName();
                        row.n      = n;
                        row.mean   = mean;
                        row.rms    = rms;
                        row.twoSigma = 2.0 * sigma;
                        out.rows.append(row);
                    }

                    if (!es.time.isEmpty())
                        out.series.append(es);
                }
            }

            out.ok = true;
            if (out.rows.isEmpty())
                out.err = QStringLiteral("没有精确对齐的时间戳数据，请确认两边时间列单位一致且存在相同时间戳。");
            return out;
        }));
}

void AnalysisPanel::onCompareFinished()
{
    auto *watcher = static_cast<QFutureWatcher<CompareOutcome>*>(m_watcher);
    CompareOutcome out = watcher->result();
    watcher->deleteLater();
    m_watcher = nullptr;
    setBusy(false);

    if (!out.ok) {
        QMessageBox::warning(this, QStringLiteral("分析失败"), out.err);
        return;
    }
    if (out.rows.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), out.err);
        return;
    }

    // 结果表
    m_resultTable->clear();
    m_resultTable->setColumnCount(7);
    m_resultTable->setHorizontalHeaderLabels({
        QStringLiteral("场景"), QStringLiteral("指标"), QStringLiteral("对比文件"),
        QStringLiteral("样本数"), QStringLiteral("均值偏置"),
        QStringLiteral("RMS"), QStringLiteral("2σ") });
    m_resultTable->setRowCount(out.rows.size());
    m_resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    for (int r = 0; r < out.rows.size(); ++r) {
        const CompareRow &row = out.rows[r];
        m_resultTable->setItem(r, 0, new QTableWidgetItem(row.scene));
        m_resultTable->setItem(r, 1, new QTableWidgetItem(row.metric));
        m_resultTable->setItem(r, 2, new QTableWidgetItem(row.file));
        m_resultTable->setItem(r, 3, new QTableWidgetItem(QString::number(row.n)));
        m_resultTable->setItem(r, 4, new QTableWidgetItem(fmtNum(row.mean)));
        m_resultTable->setItem(r, 5, new QTableWidgetItem(fmtNum(row.rms)));
        m_resultTable->setItem(r, 6, new QTableWidgetItem(fmtNum(row.twoSigma)));
    }

    showErrorChart(out.series);
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
    clearErrorChart();
}

void AnalysisPanel::clearErrorChart()
{
    auto *empty = new QChart();
    empty->setTitle(QStringLiteral("误差曲线"));
    empty->legend()->setVisible(false);
    m_errorChartView->setChart(empty);
}

void AnalysisPanel::showErrorChart(const QVector<ErrorSeries> &series)
{
    auto *chart = new QChart();
    chart->setTitle(QStringLiteral("误差曲线"));
    chart->setAnimationOptions(QChart::NoAnimation);

    static const QList<QColor> kColors = {
        QColor(59, 130, 246), QColor(239, 68, 68), QColor(34, 197, 94),
        QColor(234, 88, 12),  QColor(168, 85, 247), QColor(20, 184, 166)
    };

    int ci = 0;
    for (const ErrorSeries &s : series) {
        auto *ls = new QLineSeries(chart);
        ls->setName(s.label);
        ls->setColor(kColors[ci % kColors.size()]);
        for (int i = 0; i < s.time.size(); ++i)
            ls->append(s.time[i], s.error[i]);
        chart->addSeries(ls);
        ++ci;
    }

    chart->createDefaultAxes();
    if (auto *ax = qobject_cast<QValueAxis*>(chart->axes(Qt::Horizontal).value(0)))
        ax->setTitleText(QStringLiteral("时间（GPS 周内秒）"));
    if (auto *ay = qobject_cast<QValueAxis*>(chart->axes(Qt::Vertical).value(0)))
        ay->setTitleText(QStringLiteral("误差"));

    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    m_errorChartView->setChart(chart);
}

} // namespace Analysis
