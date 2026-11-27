#include "calib612panel.h"
#include "calib612worker.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextCursor>
#include <QTextStream>
#include <QThread>
#include <QVBoxLayout>

namespace {

// 结果表行定义：label + 单位
struct RowDef { const char *label; const char *unit; };
const RowDef kRows[] = {
    {"加计标度误差 Sx",  "‰"},
    {"加计标度误差 Sy",  "‰"},
    {"加计标度误差 Sz",  "‰"},
    {"加计零偏 bx",      "mg"},
    {"加计零偏 by",      "mg"},
    {"加计零偏 bz",      "mg"},
    {"加计安装角 α",     "deg"},
    {"加计安装角 β",     "deg"},
    {"加计安装角 γ",     "deg"},
    {"加计最大非正交角", "deg"},
    {"陀螺标度误差 Sx",  "‰"},
    {"陀螺标度误差 Sy",  "‰"},
    {"陀螺标度误差 Sz",  "‰"},
    {"陀螺零偏 bx",      "deg/s"},
    {"陀螺零偏 by",      "deg/s"},
    {"陀螺零偏 bz",      "deg/s"},
    {"陀螺最大非正交角", "deg"},
    {"有效数据行数",     "行"},
    {"采样率",           "Hz"},
    {"每段点数",         "点"},
    {"检出段数（加计/陀螺）", ""},
};
constexpr int kRowCount = int(sizeof(kRows) / sizeof(kRows[0]));

} // namespace

Calib612Panel::Calib612Panel(QWidget *parent, QThread *workerThread)
    : QDialog(parent), m_thread(workerThread)
{
    setWindowTitle(QStringLiteral("6+12 位置标定"));
    resize(1180, 780);

    qRegisterMetaType<Calib612Analysis::Result>("Calib612Analysis::Result");

    auto *root = new QVBoxLayout(this);

    // ════════ 输入参数 ════════
    auto *paramBox  = new QGroupBox(QStringLiteral("输入参数"), this);
    auto *paramGrid = new QVBoxLayout(paramBox);

    auto *fileRow = new QHBoxLayout();
    m_fileEdit = new QLineEdit(this);
    m_fileEdit->setPlaceholderText(QStringLiteral("选择 IMU 六轴数据文件（CSV / TXT）"));
    m_browseBtn = new QPushButton(QStringLiteral("浏览..."), this);
    connect(m_browseBtn, &QPushButton::clicked, this, &Calib612Panel::onBrowse);
    fileRow->addWidget(new QLabel(QStringLiteral("文件:"), this));
    fileRow->addWidget(m_fileEdit, 1);
    fileRow->addWidget(m_browseBtn);
    paramGrid->addLayout(fileRow);

    m_previewLbl = new QLabel(QStringLiteral("（选择文件后会显示前 5 行）"), this);
    m_previewLbl->setWordWrap(true);
    m_previewLbl->setStyleSheet(
        QStringLiteral("QLabel { background: #f5f5f5; color: #333; "
                       "font-family: Consolas, Menlo, monospace; padding: 6px; "
                       "border: 1px solid #ddd; border-radius: 4px; }"));
    m_previewLbl->setFixedHeight(100);
    paramGrid->addWidget(m_previewLbl);

    auto *colRow = new QHBoxLayout();
    static const char *kAxisNames[6] = {"AX", "AY", "AZ", "GX", "GY", "GZ"};
    for (int i = 0; i < 6; ++i) {
        auto *lb = new QLabel(QString::fromLatin1(kAxisNames[i]), this);
        lb->setStyleSheet(QStringLiteral("color:#555; font-weight:bold;"));
        colRow->addWidget(lb);
        m_colCombo[i] = new QComboBox(this);
        m_colCombo[i]->setFixedWidth(64);
        m_colCombo[i]->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        m_colCombo[i]->setMinimumContentsLength(2);
        m_colCombo[i]->setToolTip(QStringLiteral("选择该轴对应的列号（从 1 开始）"));
        colRow->addWidget(m_colCombo[i]);
        colRow->addSpacing(10);
    }
    colRow->addStretch(1);
    paramGrid->addLayout(colRow);

    auto *ctrlRow = new QHBoxLayout();

    m_skipSpin = new QSpinBox(this);
    m_skipSpin->setRange(0, 50);
    m_skipSpin->setValue(0);
    m_skipSpin->setPrefix(QStringLiteral("跳过表头 "));
    m_skipSpin->setSuffix(QStringLiteral(" 行"));

    m_fsSpin = new QDoubleSpinBox(this);
    m_fsSpin->setRange(1.0, 10000.0);
    m_fsSpin->setValue(100.0);
    m_fsSpin->setDecimals(1);
    m_fsSpin->setSuffix(QStringLiteral(" Hz"));
    m_fsSpin->setPrefix(QStringLiteral("采样率 "));

    m_rateSpin = new QDoubleSpinBox(this);
    m_rateSpin->setRange(1.0, 1000.0);
    m_rateSpin->setValue(100.0);
    m_rateSpin->setDecimals(1);
    m_rateSpin->setSuffix(QStringLiteral(" °/s"));
    m_rateSpin->setPrefix(QStringLiteral("陀螺标定速率 "));

    ctrlRow->addWidget(m_skipSpin);
    ctrlRow->addSpacing(12);
    ctrlRow->addWidget(m_fsSpin);
    ctrlRow->addSpacing(12);
    ctrlRow->addWidget(m_rateSpin);

    m_computeBtn = new QPushButton(QStringLiteral("开始计算"), this);
    m_computeBtn->setEnabled(false);
    connect(m_computeBtn, &QPushButton::clicked, this, &Calib612Panel::onCompute);

    ctrlRow->addStretch(1);
    ctrlRow->addWidget(m_computeBtn);
    paramGrid->addLayout(ctrlRow);

    root->addWidget(paramBox);

    // ════════ 进度 + 状态 ════════
    auto *statusRow = new QHBoxLayout();
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setFixedWidth(200);
    m_statusLbl = new QLabel(QStringLiteral("就绪"), this);
    statusRow->addWidget(m_progress);
    statusRow->addWidget(m_statusLbl, 1);
    root->addLayout(statusRow);

    // ════════ 结果表 ════════
    m_table = new QTableWidget(kRowCount, 3, this);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("参数"), QStringLiteral("数值"), QStringLiteral("单位")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    for (int r = 0; r < kRowCount; ++r) {
        m_table->setItem(r, 0, new QTableWidgetItem(QString::fromUtf8(kRows[r].label)));
        m_table->setItem(r, 1, new QTableWidgetItem(QStringLiteral("--")));
        m_table->setItem(r, 2, new QTableWidgetItem(QString::fromUtf8(kRows[r].unit)));
    }
    m_table->resizeColumnsToContents();
    root->addWidget(m_table, 2);

    // ════════ 日志（检段明细 / 诊断） ════════
    auto *logBox = new QGroupBox(QStringLiteral("检段明细 / 诊断"), this);
    auto *logLay = new QVBoxLayout(logBox);
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setStyleSheet(
        QStringLiteral("QPlainTextEdit { font-family: Consolas, Menlo, monospace; "
                       "font-size: 12px; }"));
    logLay->addWidget(m_log);
    root->addWidget(logBox, 3);

    // ════════ 后台 Worker ════════
    if (m_thread) {
        m_worker = new Calib612Worker();   // 先建在 UI 线程，再 move 到后台
        m_worker->moveToThread(m_thread);

        connect(m_worker, &Calib612Worker::progress, this, &Calib612Panel::onProgress,
                Qt::QueuedConnection);
        connect(m_worker, &Calib612Worker::error, this, &Calib612Panel::onError,
                Qt::QueuedConnection);
        connect(m_worker, &Calib612Worker::resultReady,
                this, &Calib612Panel::onResultReadyWith, Qt::QueuedConnection);
    }
}

Calib612Panel::~Calib612Panel() = default;

// ───────────────── 浏览文件 ─────────────────
void Calib612Panel::onBrowse()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 IMU 六轴数据文件"), QString(),
        QStringLiteral("CSV / 文本 (*.csv *.txt);;所有文件 (*)"));
    if (path.isEmpty()) return;

    m_fileEdit->setText(path);
    m_computeBtn->setEnabled(true);
    refreshColumns(path);
}

// ───────────────── 预览 + 列下拉 + 表头行数推断 ─────────────────
void Calib612Panel::refreshColumns(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream ts(&f);
    QStringList previewLines;
    int maxCol = 0;

    // 与 Worker 的列解析保持一致：优先常用分隔符，纯空白分隔也能识别
    auto splitLine = [](const QString &line) {
        if (line.contains(QLatin1Char(',')))       return line.split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (line.contains(QLatin1Char(';')))       return line.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        if (line.contains(QLatin1Char('\t')))      return line.split(QLatin1Char('\t'), Qt::SkipEmptyParts);
        return line.split(QRegularExpression(QStringLiteral("[\\s]+")), Qt::SkipEmptyParts);
    };

    for (int i = 0; i < 10 && !ts.atEnd(); ++i) {
        const QString line = ts.readLine();
        if (line.trimmed().isEmpty()) { --i; continue; }
        previewLines.append(line);
        const auto parts = splitLine(line);
        if (parts.size() > maxCol) maxCol = parts.size();
    }

    QString preview;
    for (const auto &l : previewLines)
        preview += l.left(180) + QStringLiteral("\n");
    m_previewLbl->setText(preview.isEmpty()
                          ? QStringLiteral("（文件为空）")
                          : preview.left(900) + (preview.size() > 900 ? QStringLiteral("...") : QString()));

    // 首行首列不是数字 → 视为表头，自动把「跳过表头」设为 1
    if (!previewLines.isEmpty()) {
        const QStringList first = splitLine(previewLines.first());
        bool ok = false;
        if (!first.isEmpty()) first.first().toDouble(&ok);
        m_skipSpin->setValue(ok ? 0 : 1);
    }

    QStringList header;
    if (!previewLines.isEmpty())
        header = splitLine(previewLines.first());

    for (int i = 0; i < 6; ++i) {
        m_colCombo[i]->clear();
        for (int c = 0; c < maxCol; ++c) {
            m_colCombo[i]->addItem(QString::number(c + 1), c);
            const QString hdr = (c < header.size()) ? header[c].trimmed() : QString();
            m_colCombo[i]->setItemData(
                c,
                hdr.isEmpty() ? QStringLiteral("第 %1 列").arg(c + 1)
                              : QStringLiteral("第 %1 列：%2").arg(c + 1).arg(hdr),
                Qt::ToolTipRole);
        }
    }

    auto matchAxis = [&header](const QStringList &keys) -> int {
        for (int c = 0; c < header.size(); ++c) {
            const QString h = header[c].trimmed().toLower();
            for (const QString &k : keys)
                if (h.contains(k)) return c;
        }
        return -1;
    };
    const QVector<QStringList> axisKeys = {
        {QStringLiteral("加计x"), QStringLiteral("accx"), QStringLiteral("acc_x"), QStringLiteral("ax")},
        {QStringLiteral("加计y"), QStringLiteral("accy"), QStringLiteral("acc_y"), QStringLiteral("ay")},
        {QStringLiteral("加计z"), QStringLiteral("accz"), QStringLiteral("acc_z"), QStringLiteral("az")},
        {QStringLiteral("陀螺x"), QStringLiteral("gyrx"), QStringLiteral("gyr_x"), QStringLiteral("gx")},
        {QStringLiteral("陀螺y"), QStringLiteral("gyry"), QStringLiteral("gyr_y"), QStringLiteral("gy")},
        {QStringLiteral("陀螺z"), QStringLiteral("gyrz"), QStringLiteral("gyr_z"), QStringLiteral("gz")},
    };

    for (int i = 0; i < 6; ++i) {
        int def = matchAxis(axisKeys[i]);
        if (def < 0 || def >= maxCol)
            def = qMin(i, maxCol - 1);
        if (def >= 0 && def < m_colCombo[i]->count())
            m_colCombo[i]->setCurrentIndex(def);
    }
}

// ───────────────── 触发计算 ─────────────────
void Calib612Panel::onCompute()
{
    if (!m_worker || !m_thread) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("后台线程未就绪"));
        return;
    }

    const QString path = m_fileEdit->text().trimmed();
    if (path.isEmpty()) return;

    m_computeBtn->setEnabled(false);
    m_progress->setValue(0);
    m_statusLbl->setText(QStringLiteral("计算中..."));
    m_log->clear();

    m_worker->paramsFilePath = path;
    m_worker->paramsSkipHead = m_skipSpin->value();
    m_worker->paramsFs       = m_fsSpin->value();
    m_worker->paramsGyroRate = m_rateSpin->value();
    m_worker->paramsColumns.resize(6);
    for (int i = 0; i < 6; ++i)
        m_worker->paramsColumns[i] = m_colCombo[i]->currentData().toInt();

    QMetaObject::invokeMethod(m_worker, &Calib612Worker::computeQueued,
                              Qt::QueuedConnection);
}

void Calib612Panel::onProgress(int percent)
{
    m_progress->setValue(percent);
}

void Calib612Panel::onError(const QString &msg)
{
    m_statusLbl->setText(msg);
    QMessageBox::warning(this, QStringLiteral("计算失败"), msg);
    m_computeBtn->setEnabled(true);
}

void Calib612Panel::onResultReadyWith(const Calib612Analysis::Result &r)
{
    m_computeBtn->setEnabled(true);
    m_progress->setValue(100);

    int accelCount = 0, gyroCount = 0;
    for (const auto &s : r.accelSegments) { Q_UNUSED(s); ++accelCount; }
    for (const auto &s : r.gyroSegments)  { Q_UNUSED(s); ++gyroCount; }

    if (r.accelOk && r.gyroOk)
        m_statusLbl->setText(QStringLiteral("完成 — 加计 %1 段 / 陀螺 %2 段")
                                 .arg(accelCount).arg(gyroCount));
    else
        m_statusLbl->setText(QStringLiteral("完成（加计 %1 段 / 陀螺 %2 段，详见下方诊断）")
                                 .arg(accelCount).arg(gyroCount));

    fillTable(r);
    fillLog(r);
}

// ───────────────── 结果表 ─────────────────
void Calib612Panel::fillTable(const Calib612Analysis::Result &r)
{
    auto setVal = [this](int row, const QString &text) {
        m_table->item(row, 1)->setText(text);
    };
    auto num = [](double v, int prec) { return QString::number(v, 'f', prec); };
    const QString na = QStringLiteral("--");

    // 加计
    setVal(0, r.accelOk ? num(r.accelScale[0], 3) : na);
    setVal(1, r.accelOk ? num(r.accelScale[1], 3) : na);
    setVal(2, r.accelOk ? num(r.accelScale[2], 3) : na);
    setVal(3, r.accelOk ? num(r.accelBias[0], 3) : na);
    setVal(4, r.accelOk ? num(r.accelBias[1], 3) : na);
    setVal(5, r.accelOk ? num(r.accelBias[2], 3) : na);
    setVal(6, r.accelOk ? num(r.accelInstall[0], 4) : na);
    setVal(7, r.accelOk ? num(r.accelInstall[1], 4) : na);
    setVal(8, r.accelOk ? num(r.accelInstall[2], 4) : na);
    setVal(9, r.accelOk ? num(r.accelMaxNonOrtho, 4) : na);

    // 陀螺
    setVal(10, r.gyroOk ? num(r.gyroScale[0], 3) : na);
    setVal(11, r.gyroOk ? num(r.gyroScale[1], 3) : na);
    setVal(12, r.gyroOk ? num(r.gyroScale[2], 3) : na);
    setVal(13, r.gyroOk ? num(r.gyroBias[0], 5) : na);
    setVal(14, r.gyroOk ? num(r.gyroBias[1], 5) : na);
    setVal(15, r.gyroOk ? num(r.gyroBias[2], 5) : na);
    setVal(16, r.gyroOk ? num(r.gyroMaxNonOrtho, 4) : na);

    // 元信息
    setVal(17, QString::number(r.totalSamples));
    setVal(18, num(r.fs, 1));
    setVal(19, QString::number(r.segmentSamples));
    setVal(20, QStringLiteral("%1 / %2")
                   .arg(r.accelSegments.size()).arg(r.gyroSegments.size()));

    m_table->resizeColumnsToContents();
}

// ───────────────── 检段明细 ─────────────────
void Calib612Panel::fillLog(const Calib612Analysis::Result &r)
{
    QString text;

    if (!r.notes.isEmpty()) {
        text += QStringLiteral("【诊断】\n");
        for (const QString &n : r.notes)
            text += QStringLiteral("  · %1\n").arg(n);
        text += QStringLiteral("\n");
    }

    auto dump = [&text, &r](bool accel) {
        const auto &segs = accel ? r.accelSegments : r.gyroSegments;
        const QString title = accel ? QStringLiteral("加计（g）") : QStringLiteral("陀螺（deg/s）");
        const QString unit  = accel ? QStringLiteral("g") : QStringLiteral("deg/s");
        text += QStringLiteral("【%1】共 %2 段，每段 %3 点（%4 s）\n")
                    .arg(title).arg(segs.size()).arg(r.segmentSamples)
                    .arg(r.fs > 0 ? r.segmentSamples / r.fs : 0.0, 0, 'f', 1);
        for (const auto &s : segs) {
            text += QStringLiteral("  %1  起点 %2  (%3 s)   均值 %4, %5, %6 %7\n")
                        .arg(Calib612Analysis::dirLabel(s.dir))
                        .arg(s.startIndex)
                        .arg(r.fs > 0 ? s.startIndex / r.fs : 0.0, 0, 'f', 2)
                        .arg(s.mean[0], 0, 'f', 4)
                        .arg(s.mean[1], 0, 'f', 4)
                        .arg(s.mean[2], 0, 'f', 4)
                        .arg(unit);
        }
        text += QStringLiteral("\n");
    };
    dump(true);
    dump(false);

    m_log->setPlainText(text);
    m_log->moveCursor(QTextCursor::Start);
}
