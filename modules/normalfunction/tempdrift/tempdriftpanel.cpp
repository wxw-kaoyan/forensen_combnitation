#include "tempdriftpanel.h"
#include "tempdriftworker.h"

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QPen>
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

#include <algorithm>

namespace {
const char *kAxisNames[6] = {"加计X", "加计Y", "加计Z", "陀螺X", "陀螺Y", "陀螺Z"};
}

TempDriftPanel::TempDriftPanel(QWidget *parent, QThread *workerThread)
    : QDialog(parent), m_thread(workerThread)
{
    setWindowTitle(QStringLiteral("拉温（温漂）分析"));
    resize(1180, 780);

    qRegisterMetaType<TempDriftAnalysis::Result>("TempDriftAnalysis::Result");

    auto *root = new QVBoxLayout(this);

    // ════════ 输入参数 ════════
    auto *paramBox  = new QGroupBox(QStringLiteral("输入参数"), this);
    auto *paramGrid = new QVBoxLayout(paramBox);

    auto *fileRow = new QHBoxLayout();
    m_fileEdit = new QLineEdit(this);
    m_fileEdit->setPlaceholderText(QStringLiteral("选择含 6 轴 + 温度列的 CSV / TXT"));
    m_browseBtn = new QPushButton(QStringLiteral("浏览..."), this);
    connect(m_browseBtn, &QPushButton::clicked, this, &TempDriftPanel::onBrowse);
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

    // 列选择：6 轴 + 温度
    auto *colRow = new QHBoxLayout();
    static const char *kColNames[7] = {"AX", "AY", "AZ", "GX", "GY", "GZ", "T"};
    for (int i = 0; i < 7; ++i) {
        auto *lb = new QLabel(QString::fromLatin1(kColNames[i]), this);
        lb->setStyleSheet(QStringLiteral("color:#555; font-weight:bold;"));
        colRow->addWidget(lb);
        m_colCombo[i] = new QComboBox(this);
        m_colCombo[i]->setFixedWidth(64);
        m_colCombo[i]->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        m_colCombo[i]->setMinimumContentsLength(2);
        m_colCombo[i]->setToolTip(i < 6
            ? QStringLiteral("该轴对应的列号（从 1 开始）")
            : QStringLiteral("温度列（单位 ℃）"));
        colRow->addWidget(m_colCombo[i]);
        colRow->addSpacing(8);
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

    m_rampSpin = new QDoubleSpinBox(this);
    m_rampSpin->setRange(0.01, 50.0);
    m_rampSpin->setValue(0.5);
    m_rampSpin->setDecimals(2);
    m_rampSpin->setSuffix(QStringLiteral(" ℃/min"));
    m_rampSpin->setPrefix(QStringLiteral("保温阈值 "));

    ctrlRow->addWidget(m_skipSpin);
    ctrlRow->addSpacing(12);
    ctrlRow->addWidget(m_fsSpin);
    ctrlRow->addSpacing(12);
    ctrlRow->addWidget(m_rampSpin);

    m_computeBtn = new QPushButton(QStringLiteral("开始计算"), this);
    m_computeBtn->setEnabled(false);
    connect(m_computeBtn, &QPushButton::clicked, this, &TempDriftPanel::onCompute);

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
    m_table = new QTableWidget(6, 6, this);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("轴"),
        QStringLiteral("峰峰值"),
        QStringLiteral("标准差"),
        QStringLiteral("保温段峰峰值"),
        QStringLiteral("保温段标准差"),
        QStringLiteral("温漂斜率 (/℃)"),
    });
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    for (int a = 0; a < 6; ++a) {
        const QString unit = (a < 3) ? QStringLiteral("mg") : QStringLiteral("deg/s");
        m_table->setItem(a, 0, new QTableWidgetItem(
            QStringLiteral("%1 (%2)").arg(QString::fromUtf8(kAxisNames[a]), unit)));
        for (int c = 1; c < 6; ++c)
            m_table->setItem(a, c, new QTableWidgetItem(QStringLiteral("--")));
    }
    m_table->resizeColumnsToContents();
    root->addWidget(m_table, 2);

    // ════════ 日志 ════════
    auto *logBox = new QGroupBox(QStringLiteral("数据摘要 / 保温平台"), this);
    auto *logLay = new QVBoxLayout(logBox);
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setStyleSheet(
        QStringLiteral("QPlainTextEdit { font-family: Consolas, Menlo, monospace; font-size: 12px; }"));
    logLay->addWidget(m_log);
    root->addWidget(logBox, 3);

    // ════════ 后台 Worker ════════
    if (m_thread) {
        m_worker = new TempDriftWorker();
        m_worker->moveToThread(m_thread);

        connect(m_worker, &TempDriftWorker::progress, this, &TempDriftPanel::onProgress,
                Qt::QueuedConnection);
        connect(m_worker, &TempDriftWorker::error, this, &TempDriftPanel::onError,
                Qt::QueuedConnection);
        connect(m_worker, &TempDriftWorker::resultReady,
                this, &TempDriftPanel::onResultReadyWith, Qt::QueuedConnection);
    }
}

TempDriftPanel::~TempDriftPanel() = default;

void TempDriftPanel::onBrowse()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择拉温数据文件"), QString(),
        QStringLiteral("CSV / 文本 (*.csv *.txt);;所有文件 (*)"));
    if (path.isEmpty()) return;

    m_fileEdit->setText(path);
    m_computeBtn->setEnabled(true);
    refreshColumns(path);
}

void TempDriftPanel::refreshColumns(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream ts(&f);
    QStringList previewLines;
    int maxCol = 0;

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

    if (!previewLines.isEmpty()) {
        const QStringList first = splitLine(previewLines.first());
        bool ok = false;
        if (!first.isEmpty()) first.first().toDouble(&ok);
        m_skipSpin->setValue(ok ? 0 : 1);
    }

    QStringList header;
    if (!previewLines.isEmpty())
        header = splitLine(previewLines.first());

    for (int i = 0; i < 7; ++i) {
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
        {QStringLiteral("温度"), QStringLiteral("temp"), QStringLiteral("temperature"), QStringLiteral("temperature_c")},
    };

    for (int i = 0; i < 7; ++i) {
        int def = matchAxis(axisKeys[i]);
        if (def < 0 || def >= maxCol)
            def = qMin(i, maxCol - 1);
        if (def >= 0 && def < m_colCombo[i]->count())
            m_colCombo[i]->setCurrentIndex(def);
    }
}

void TempDriftPanel::onCompute()
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

    m_worker->paramsFilePath  = path;
    m_worker->paramsSkipHead  = m_skipSpin->value();
    m_worker->paramsFs        = m_fsSpin->value();
    m_worker->paramsRampThreshold = m_rampSpin->value();
    m_worker->paramsColumns.resize(7);
    for (int i = 0; i < 7; ++i)
        m_worker->paramsColumns[i] = m_colCombo[i]->currentData().toInt();

    QMetaObject::invokeMethod(m_worker, &TempDriftWorker::computeQueued,
                              Qt::QueuedConnection);
}

void TempDriftPanel::onProgress(int percent)
{
    m_progress->setValue(percent);
}

void TempDriftPanel::onError(const QString &msg)
{
    m_statusLbl->setText(msg);
    QMessageBox::warning(this, QStringLiteral("计算失败"), msg);
    m_computeBtn->setEnabled(true);
}

void TempDriftPanel::onResultReadyWith(const TempDriftAnalysis::Result &r)
{
    m_last = r;
    m_hasResult = r.ok;
    m_computeBtn->setEnabled(true);
    m_progress->setValue(100);

    if (!r.ok) {
        m_statusLbl->setText(QStringLiteral("计算失败，详见日志"));
        fillLog(r);
        return;
    }

    m_statusLbl->setText(QStringLiteral("完成 — %1 点 → 平滑 %2 点，保温平台 %3 段")
                             .arg(r.totalSamples).arg(r.outSamples).arg(r.soakSegments.size()));
    fillTable(r);
    fillLog(r);
    showChartWindow();
}

void TempDriftPanel::fillTable(const TempDriftAnalysis::Result &r)
{
    auto text = [](double v, int prec) { return QString::number(v, 'f', prec); };

    for (int a = 0; a < 6; ++a) {
        const auto &m = r.metrics[a];
        m_table->item(a, 1)->setText(text(m.ppAll, 4));
        m_table->item(a, 2)->setText(text(m.stdAll, 4));
        if (m.ok) {
            m_table->item(a, 3)->setText(text(m.ppSoak, 4));
            m_table->item(a, 4)->setText(text(m.stdSoak, 4));
            m_table->item(a, 5)->setText(text(m.slopePerC, 5));
        } else {
            for (int c = 3; c <= 5; ++c)
                m_table->item(a, c)->setText(QStringLiteral("--"));
        }
    }
    m_table->resizeColumnsToContents();
}

void TempDriftPanel::fillLog(const TempDriftAnalysis::Result &r)
{
    QString text;

    if (r.ok) {
        double tmin = r.temp.isEmpty() ? 0 : r.temp.first();
        double tmax = tmin;
        for (double v : r.temp) { tmin = std::min(tmin, v); tmax = std::max(tmax, v); }
        int soakCount = 0;
        for (auto v : r.soakMask) soakCount += int(v == 1);

        text += QStringLiteral("数据: %1 行, fs=%2 Hz, 平滑窗 %3 点 (%4 s), 步长 %5 点 (%6 s), 输出 %7 点\n")
                    .arg(r.totalSamples).arg(r.fs, 0, 'f', 1)
                    .arg(r.windowSamples).arg(r.windowSamples / r.fs, 0, 'f', 2)
                    .arg(r.stepSamples).arg(r.dtSec, 0, 'f', 2).arg(r.outSamples);
        text += QStringLiteral("温度: %1 ~ %2 ℃\n")
                    .arg(tmin, 0, 'f', 2).arg(tmax, 0, 'f', 2);
        text += QStringLiteral("保温判定: |dT/dt| < %1 ℃/min（30 s 窗），保温点 %2 / %3 (%4%)\n\n")
                    .arg(r.rampThreshold, 0, 'f', 2).arg(soakCount).arg(r.outSamples)
                    .arg(r.outSamples ? 100.0 * soakCount / r.outSamples : 0.0, 0, 'f', 1);

        text += QStringLiteral("【保温平台】%1 段\n").arg(r.soakSegments.size());
        for (int i = 0; i < r.soakSegments.size(); ++i) {
            const auto &s = r.soakSegments[i];
            text += QStringLiteral("  #%1  均值 %2 ℃   起始 %3 s   时长 %4 s\n")
                        .arg(i + 1)
                        .arg(s.meanTemp, 0, 'f', 2)
                        .arg(s.startSec, 0, 'f', 1)
                        .arg(s.durationSec, 0, 'f', 0);
        }
    }

    if (!r.notes.isEmpty()) {
        text += QStringLiteral("\n【诊断】\n");
        for (const QString &n : r.notes)
            text += QStringLiteral("  · %1\n").arg(n);
    }

    m_log->setPlainText(text);
    m_log->moveCursor(QTextCursor::Start);
}

// ───────────────── 值-温度 曲线（2×3） ─────────────────
void TempDriftPanel::showChartWindow()
{
    if (!m_hasResult || m_last.outSamples == 0) return;

    auto *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(QStringLiteral("拉温温度特性"));
    dlg->resize(1400, 820);

    auto *grid = new QGridLayout(dlg);
    grid->setContentsMargins(4, 4, 4, 4);
    grid->setSpacing(6);

    for (int a = 0; a < 6; ++a) {
        const bool accel = (a < 3);
        const QString unit = accel ? QStringLiteral("mg") : QStringLiteral("deg/s");

        auto *series = new QLineSeries();
        series->setPen(QPen(accel ? QColor(0, 90, 200) : QColor(200, 60, 20), 1.2));

        double xMin = 1e300, xMax = -1e300, yMin = 1e300, yMax = -1e300;
        const auto &vals = m_last.axes[a];
        for (int k = 0; k < m_last.outSamples; ++k) {
            const double x = m_last.temp[k];
            const double y = vals[k];
            if (!std::isfinite(x) || !std::isfinite(y)) continue;
            series->append(x, y);
            xMin = std::min(xMin, x); xMax = std::max(xMax, x);
            yMin = std::min(yMin, y); yMax = std::max(yMax, y);
        }
        if (series->count() == 0) { delete series; continue; }

        auto *chart = new QChart();
        chart->setTitle(QStringLiteral("%1 — 温度特性").arg(QString::fromUtf8(kAxisNames[a])));
        chart->legend()->setVisible(false);
        chart->addSeries(series);

        auto *axisX = new QValueAxis();
        axisX->setTitleText(QStringLiteral("温度 (℃)"));
        auto *axisY = new QValueAxis();
        axisY->setTitleText(unit);

        const double padX = (xMax > xMin) ? (xMax - xMin) * 0.05 : 1.0;
        const double padY = (yMax > yMin) ? (yMax - yMin) * 0.08 : 1.0;
        axisX->setRange(xMin - padX, xMax + padX);
        axisY->setRange(yMin - padY, yMax + padY);
        axisX->setLabelFormat(QStringLiteral("%.1f"));
        axisY->setLabelFormat(QStringLiteral("%.3f"));

        chart->addAxis(axisX, Qt::AlignBottom);
        chart->addAxis(axisY, Qt::AlignLeft);
        series->attachAxis(axisX);
        series->attachAxis(axisY);

        auto *view = new QChartView(chart, dlg);
        view->setRenderHint(QPainter::Antialiasing);
        grid->addWidget(view, a / 3, a % 3);
    }

    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}
