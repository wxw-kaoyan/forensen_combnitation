#include "allanpanel.h"
#include "allananalysis.h"
#include "allanworker.h"

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QLogValueAxis>
#include <QtCharts/QValueAxis>

#include <QChart>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QRegularExpression>
#include <QThread>
#include <QMetaObject>
#include <QTextStream>
#include <QFontDatabase>

#include <cmath>

AllanPanel::AllanPanel(QWidget *parent, QThread *workerThread)
    : QDialog(parent), m_thread(workerThread)
{
    setWindowTitle(QStringLiteral("Allan 方差分析"));
    resize(1280, 760);

    auto *root = new QVBoxLayout(this);

    // ════════ 参数区 ════════
    auto *paramBox = new QGroupBox(QStringLiteral("输入参数"), this);
    auto *paramGrid = new QVBoxLayout(paramBox);

    // 第一行: 文件
    auto *fileRow = new QHBoxLayout();
    m_fileEdit = new QLineEdit(this);
    m_fileEdit->setPlaceholderText(QStringLiteral("选择 CSV 文件"));
    m_browseBtn = new QPushButton(QStringLiteral("浏览..."), this);
    connect(m_browseBtn, &QPushButton::clicked, this, &AllanPanel::onBrowse);
    fileRow->addWidget(new QLabel(QStringLiteral("文件:"), this));
    fileRow->addWidget(m_fileEdit, 1);
    fileRow->addWidget(m_browseBtn);
    paramGrid->addLayout(fileRow);

    // 第二行: 预览（只读 QLabel，显示前 5 行截断）
    m_previewLbl = new QLabel(QStringLiteral("（选择文件后会显示前 5 行）"), this);
    m_previewLbl->setWordWrap(true);
    m_previewLbl->setStyleSheet(
        QStringLiteral("QLabel { background: #f5f5f5; color: #333; "
                       "font-family: Consolas, Menlo, monospace; padding: 6px; "
                       "border: 1px solid #ddd; border-radius: 4px; }"));
    m_previewLbl->setFixedHeight(110);
    paramGrid->addWidget(m_previewLbl);

    // 第三行: 列选择（AX AY AZ GX GY GZ），ComboBox 只显示列号 1/2/3...
    // 原实现把 skip/fs/6 组下拉全塞一行，1280 宽也放不下，下拉被压窄到看不见；
    // 现在拆成独立一行，且下拉里只放列号，表头名放进 tooltip。
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
        m_colCombo[i]->setToolTip(QStringLiteral("选择该轴对应的 CSV 列号（从 1 开始）"));
        colRow->addWidget(m_colCombo[i]);
        colRow->addSpacing(10);
    }
    colRow->addStretch(1);
    paramGrid->addLayout(colRow);

    // 第四行: 跳过表头 + 采样率 + 计算按钮
    auto *ctrlRow = new QHBoxLayout();

    m_skipSpin = new QSpinBox(this);
    m_skipSpin->setRange(0, 50);
    m_skipSpin->setValue(1);
    m_skipSpin->setPrefix(QStringLiteral("跳过表头 "));
    m_skipSpin->setSuffix(QStringLiteral(" 行"));

    m_fsSpin = new QDoubleSpinBox(this);
    m_fsSpin->setRange(0.1, 10000.0);
    m_fsSpin->setValue(100.0);
    m_fsSpin->setDecimals(1);
    m_fsSpin->setSuffix(QStringLiteral(" Hz"));
    m_fsSpin->setPrefix(QStringLiteral("采样率 "));

    ctrlRow->addWidget(m_skipSpin);
    ctrlRow->addSpacing(12);
    ctrlRow->addWidget(m_fsSpin);

    m_computeBtn = new QPushButton(QStringLiteral("开始计算"), this);
    m_computeBtn->setEnabled(false);
    connect(m_computeBtn, &QPushButton::clicked, this, &AllanPanel::onCompute);

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

    // ════════ 下半区: 参数表（曲线改成计算完成后弹独立窗口） ════════
    // 表: 9 行 × 7 列（第 0 列参数名, 1-3 加计, 4-6 陀螺）
    m_table = new QTableWidget(9, 7, this);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("参数"),
        QStringLiteral("加计X"), QStringLiteral("加计Y"), QStringLiteral("加计Z"),
        QStringLiteral("陀螺X"), QStringLiteral("陀螺Y"), QStringLiteral("陀螺Z")
    });
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QStringList rowLabels = {
        QStringLiteral("随机游走 RW"),
        QStringLiteral("偏置不稳定性 @10s"),
        QStringLiteral("偏置不稳定性 @60s"),
        QStringLiteral("偏置不稳定性 @200s"),
        QStringLiteral("静态偏置稳定性"),
        QStringLiteral("────────────"),
        QStringLiteral("有效数据行数 N"),
        QStringLiteral("τ 点数"),
        QStringLiteral("采样率 fs (Hz)"),
    };
    for (int r = 0; r < rowLabels.size(); ++r) {
        auto *it = new QTableWidgetItem(rowLabels[r]);
        m_table->setVerticalHeaderItem(r, it);
    }
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setStretchLastSection(true);

    root->addWidget(m_table, 1);

    // ════════ 后台 Worker ════════
    if (m_thread) {
        m_worker = new AllanWorker();   // 构造在当前线程 (UI 线程)，之后 moveToThread
        m_worker->moveToThread(m_thread);

        connect(m_worker, &AllanWorker::progress, this, &AllanPanel::onProgress,
                Qt::QueuedConnection);
        connect(m_worker, &AllanWorker::error, this, &AllanPanel::onError,
                Qt::QueuedConnection);
        connect(m_worker, &AllanWorker::resultReady,
                this, &AllanPanel::onResultReadyWith, Qt::QueuedConnection);
    }
}

AllanPanel::~AllanPanel() = default;

// ───────────────── 浏览文件 ─────────────────
void AllanPanel::onBrowse()
{
    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 CSV 文件"), QString(),
        QStringLiteral("CSV / 文本 (*.csv *.txt);;所有文件 (*)"));
    if (path.isEmpty()) return;
    m_fileEdit->setText(path);
    m_computeBtn->setEnabled(true);
    refreshColumnCombos(path);
}

// ───────────────── 预览 + 刷新列 ComboBox ─────────────────
void AllanPanel::refreshColumnCombos(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream ts(&f);
    QStringList previewLines;
    int maxCol = 0;
    // 读前 10 行，跳过空行
    for (int i = 0; i < 10 && !ts.atEnd(); ++i) {
        QString line = ts.readLine();
        if (line.trimmed().isEmpty()) { --i; continue; }
        previewLines.append(line);
        auto parts = line.split(QRegularExpression(QStringLiteral("[,;\\t]+")));
        if (parts.size() > maxCol) maxCol = parts.size();
    }

    // 预览（截断长度）
    QString preview;
    for (const auto &l : previewLines)
        preview += l.left(180) + QStringLiteral("\n");
    m_previewLbl->setText(preview.isEmpty()
                          ? QStringLiteral("（文件为空）")
                          : preview.left(900) + (preview.size() > 900 ? QStringLiteral("...") : QString()));

    // 表头（第一个非空行）用于 tooltip + 自动匹配
    QStringList header;
    if (!previewLines.isEmpty())
        header = previewLines.first().split(QRegularExpression(QStringLiteral("[,;\\t]+")));

    // 填列选择：显示 1-based 列号，data 存 0-based 索引，表头名放 tooltip
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

    // 按表头关键字自动匹配 6 轴（匹配不到再退回按顺序默认）
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
            def = qMin(i, maxCol - 1);              // 回退：按列顺序
        if (def >= 0 && def < m_colCombo[i]->count())
            m_colCombo[i]->setCurrentIndex(def);    // item 顺序 = 列号顺序，index 即 0-based 列
    }
}

// ───────────────── 触发后台计算 ─────────────────
void AllanPanel::onCompute()
{
    if (!m_worker || !m_thread) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("后台线程未就绪"));
        return;
    }

    QString path = m_fileEdit->text().trimmed();
    if (path.isEmpty()) return;

    QVector<int> cols(6);
    for (int i = 0; i < 6; ++i)
        cols[i] = m_colCombo[i]->currentData().toInt();

    // 禁用按钮，显示进度
    m_computeBtn->setEnabled(false);
    m_progress->setValue(0);
    m_statusLbl->setText(QStringLiteral("计算中..."));

    // 设置 Worker 的 params，然后零参数 invokeMethod 调到后台线程
    m_worker->paramsCsvPath  = path;
    m_worker->paramsSkipHead = m_skipSpin->value();
    m_worker->paramsFs       = m_fsSpin->value();
    m_worker->paramsColumns.resize(6);
    for (int i = 0; i < 6; ++i)
        m_worker->paramsColumns[i] = m_colCombo[i]->currentData().toInt();

    QMetaObject::invokeMethod(m_worker, &AllanWorker::computeQueued,
                              Qt::QueuedConnection);
}

void AllanPanel::onProgress(int percent)
{
    m_progress->setValue(percent);
}

void AllanPanel::onError(const QString &msg)
{
    m_statusLbl->setText(msg);
    QMessageBox::warning(this, QStringLiteral("计算失败"), msg);
    m_computeBtn->setEnabled(true);
}

void AllanPanel::onResultReady() { /* 占位 */ }

void AllanPanel::onResultReadyWith(
        const AllanAnalysis::MultiResult &mr,
        const QVector<AllanAnalysis::AccelParams> &acc,
        const QVector<AllanAnalysis::GyroParams> &gyr)
{
    m_lastResult = mr;
    m_lastAccel  = acc;
    m_lastGyro   = gyr;
    m_hasResult  = true;
    m_computeBtn->setEnabled(true);
    m_progress->setValue(100);
    m_statusLbl->setText(QStringLiteral("完成 — %1 轴 × %2 τ 点")
                             .arg(mr.axisCount).arg(mr.axes[0].tau.size()));

    // 填表格
    auto fill6 = [&](int row, double vA, double vB, double vC,
                     double vGx, double vGy, double vGz,
                     const QString &unitA, const QString &unitG) {
        auto set = [&](int c, double v, const QString &unit) {
            auto *it = new QTableWidgetItem(
                QStringLiteral("%1 %2").arg(v, 0, 'g', 5).arg(unit));
            m_table->setItem(row, c, it);
        };
        set(1, vA, unitA); set(2, vB, unitA); set(3, vC, unitA);
        set(4, vGx, unitG); set(5, vGy, unitG); set(6, vGz, unitG);
    };

    auto rwA = [](const AllanAnalysis::AccelParams &p) { return p.randomWalk; };
    auto rwG = [](const AllanAnalysis::GyroParams &p)   { return p.randomWalk; };
    auto biA = [](const AllanAnalysis::AccelParams &p)  { return p.biasInstability_10s; };
    auto bi60A = [](const AllanAnalysis::AccelParams &p){ return p.biasInstability_60s; };
    auto bi200A = [](const AllanAnalysis::AccelParams &p){return p.biasInstability_200s;};
    auto bstA = [](const AllanAnalysis::AccelParams &p) { return p.biasStabilityStatic; };
    auto biG  = [](const AllanAnalysis::GyroParams &p)   { return p.biasInstability_10s; };
    auto bi60G= [](const AllanAnalysis::GyroParams &p)  { return p.biasInstability_60s; };
    auto bi200G=[](const AllanAnalysis::GyroParams &p) { return p.biasInstability_200s;};
    auto bstG = [](const AllanAnalysis::GyroParams &p)  { return p.biasStabilityStatic; };

    if (mr.axisCount >= 6 && acc.size() >= 3 && gyr.size() >= 3) {
        fill6(0, rwA(acc[0]), rwA(acc[1]), rwA(acc[2]),
                 rwG(gyr[0]), rwG(gyr[1]), rwG(gyr[2]),
                 QStringLiteral("m/s/√h"), QStringLiteral("deg/√h"));
        fill6(1, biA(acc[0]), biA(acc[1]), biA(acc[2]),
                 biG(gyr[0]), biG(gyr[1]), biG(gyr[2]),
                 QStringLiteral("μg"), QStringLiteral("deg/h"));
        fill6(2, bi60A(acc[0]), bi60A(acc[1]), bi60A(acc[2]),
                 bi60G(gyr[0]), bi60G(gyr[1]), bi60G(gyr[2]),
                 QStringLiteral("μg"), QStringLiteral("deg/h"));
        fill6(3, bi200A(acc[0]), bi200A(acc[1]), bi200A(acc[2]),
                 bi200G(gyr[0]), bi200G(gyr[1]), bi200G(gyr[2]),
                 QStringLiteral("μg"), QStringLiteral("deg/h"));
        fill6(4, bstA(acc[0]), bstA(acc[1]), bstA(acc[2]),
                 bstG(gyr[0]), bstG(gyr[1]), bstG(gyr[2]),
                 QStringLiteral("μg"), QStringLiteral("deg/h"));
    }
    m_table->setItem(6, 0, new QTableWidgetItem(
                          QString::number(mr.totalLen)));
    m_table->setItem(7, 0, new QTableWidgetItem(
                          QString::number(mr.axes[0].tau.size())));
    m_table->setItem(8, 0, new QTableWidgetItem(
                          QString::number(m_fsSpin->value(), 'f', 1)));
    m_table->resizeColumnsToContents();

    // 画图：弹独立窗口
    showChartWindow();
}

// ───────────────── 独立窗口显示 log-log 曲线 ─────────────────
// 对应 MATLAB allan.m 生成的两张 figure：
//   图1 加计 Allan 标准差 [μg]
//   图2 陀螺 Allan 标准差 [deg/h]
void AllanPanel::showChartWindow()
{
    if (!m_hasResult || m_lastResult.axes.isEmpty()) return;

    struct PanelCfg {
        int    firstAxis;   // 0=加计 3=陀螺
        double coef;        // 原始单位 → 显示单位
        const char *title;
        const char *yTitle;
    };
    const PanelCfg panels[2] = {
        {0, 1e6,    "Accelerometer -- Allan variance", "Allan standard deviation [ug]"},
        {3, 3600.0, "Gyroscope -- Allan variance",     "Allan standard deviation [deg/h]"},
    };
    static const char *kAxisLabel[3] = {"x", "y", "z"};
    static const QColor kColors[3] = {QColor(0, 40, 200), QColor(0, 150, 0), QColor(210, 30, 30)};

    int shown = 0;
    for (const auto &cfg : panels) {
        auto *chart = new QChart();
        chart->setTitle(QString::fromLatin1(cfg.title));
        chart->legend()->setVisible(true);
        chart->legend()->setAlignment(Qt::AlignBottom);

        double xMin = 1e300, xMax = -1e300, yMin = 1e300, yMax = -1e300;
        bool hasAny = false;

        for (int k = 0; k < 3; ++k) {
            const int a = cfg.firstAxis + k;
            if (a >= m_lastResult.axisCount) break;

            auto *series = new QLineSeries();
            series->setName(QString::fromLatin1(kAxisLabel[k]));
            series->setPen(QPen(kColors[k], 1.8));
            series->setUseOpenGL(false);

            const auto &ax = m_lastResult.axes[a];
            for (int i = 0; i < ax.tau.size(); ++i) {
                if (ax.tau[i] <= 0 || ax.sigma[i] <= 0) continue;
                const double y = ax.sigma[i] * cfg.coef;
                if (!std::isfinite(y) || y <= 0) continue;
                series->append(ax.tau[i], y);
                xMin = qMin(xMin, ax.tau[i]); xMax = qMax(xMax, ax.tau[i]);
                yMin = qMin(yMin, y);         yMax = qMax(yMax, y);
                hasAny = true;
            }
            if (series->count() > 0)
                chart->addSeries(series);
            else
                delete series;
        }
        if (!hasAny) {
            delete chart;
            continue;
        }

        auto *axisX = new QLogValueAxis();
        axisX->setTitleText(QStringLiteral("Cluster time [s]"));
        axisX->setBase(10);
        axisX->setRange(xMin * 0.9, xMax * 1.1);
        axisX->setLabelFormat(QStringLiteral("%g"));

        auto *axisY = new QLogValueAxis();
        axisY->setTitleText(QString::fromLatin1(cfg.yTitle));
        axisY->setBase(10);
        axisY->setRange(yMin / 1.35, yMax * 1.35);
        axisY->setLabelFormat(QStringLiteral("%g"));

        chart->addAxis(axisX, Qt::AlignBottom);
        chart->addAxis(axisY, Qt::AlignLeft);
        for (auto *s : chart->series()) {
            s->attachAxis(axisX);
            s->attachAxis(axisY);
        }

        // 每张图各开一个独立窗口（对应 MATLAB 的两张 figure）
        auto *dlg = new QDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowTitle(QString::fromLatin1(cfg.title));
        dlg->resize(900, 620);

        auto *lay = new QVBoxLayout(dlg);
        lay->setContentsMargins(4, 4, 4, 4);
        auto *view = new QChartView(chart, dlg);
        view->setRenderHint(QPainter::Antialiasing);
        lay->addWidget(view);

        dlg->move(160 + shown * 70, 100 + shown * 70);   // 两窗错开，避免完全重叠
        dlg->show();
        dlg->raise();
        dlg->activateWindow();
        ++shown;
    }
}
