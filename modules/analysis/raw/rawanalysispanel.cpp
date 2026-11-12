#include "rawanalysispanel.h"
#include "rinexreader.h"
#include "../chart/chartworkbench.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QPainter>
#include <QSplitter>
#include <QStackedWidget>
#include <QComboBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QtCharts/QCategoryAxis>
#include <QtCharts/QScatterSeries>
#include <QtConcurrent/QtConcurrentRun>
#include <QCheckBox>
#include <QScrollBar>
#include <QTimer>
#include <QDebug>
#include <QtMath>
#include <limits>
#include <memory>

namespace Analysis {

RawAnalysisPanel::RawAnalysisPanel(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

void RawAnalysisPanel::onOpenChartWorkbench()
{
    auto *w = new ChartWorkbench(this);
    w->show();
}

void RawAnalysisPanel::buildUi()
{
    auto *mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->setChildrenCollapsible(false);

    // ═══════════════════════════════════════════════════════════
    //  左侧导航面板
    // ═══════════════════════════════════════════════════════════
    auto *leftPanel = new QWidget(mainSplitter);
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);
    leftLayout->setSpacing(8);

    // 数据类型 ComboBox
    m_typeCombo = new QComboBox(leftPanel);
    m_typeCombo->addItem(QStringLiteral("GNSS 原始观测值"));
    m_typeCombo->addItem(QStringLiteral("IMU 原始观测值"));
    m_typeCombo->addItem(QStringLiteral("真值（GINS）"));
    m_typeCombo->setFixedHeight(32);
    leftLayout->addWidget(m_typeCombo);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RawAnalysisPanel::onTypeChanged);

    // 树形导航
    m_tree = new QTreeWidget(leftPanel);
    m_tree->setHeaderHidden(true);
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tree->setIndentation(16);
    leftLayout->addWidget(m_tree);
    leftLayout->addStretch();

    connect(m_tree, &QTreeWidget::itemClicked, this, &RawAnalysisPanel::onTreeNodeClicked);

    mainSplitter->addWidget(leftPanel);

    // ═══════════════════════════════════════════════════════════
    //  右侧内容面板
    // ═══════════════════════════════════════════════════════════
    auto *rightPanel = new QWidget(mainSplitter);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(16, 16, 16, 16);
    rightLayout->setSpacing(12);

    // 文件选择行
    auto *fileRow = new QHBoxLayout();
    fileRow->setSpacing(8);
    m_pathEdit = new QLineEdit(rightPanel);
    m_pathEdit->setPlaceholderText(QStringLiteral("选择 RINEX 观测文件（.O / .26O / .obs）"));
    m_browseBtn = new QPushButton(QStringLiteral("浏览..."), rightPanel);
    m_analyzeBtn = new QPushButton(QStringLiteral("开始分析"), rightPanel);
    // 分析进行中按钮置灰（禁用态），避免重复触发；重新选文件后可再次分析新文件
    m_analyzeBtn->setStyleSheet(
        QStringLiteral("QPushButton:disabled { background:#b8c0ca; color:#5b6472; }"));
    auto *chartBtn = new QPushButton(QStringLiteral("通用绘图"), rightPanel);
    fileRow->addWidget(m_pathEdit, 1);
    fileRow->addWidget(m_browseBtn);
    fileRow->addWidget(m_analyzeBtn);
    fileRow->addWidget(chartBtn);
    rightLayout->addLayout(fileRow);

    connect(m_browseBtn, &QPushButton::clicked, this, &RawAnalysisPanel::onBrowse);
    connect(m_analyzeBtn, &QPushButton::clicked, this, &RawAnalysisPanel::onAnalyze);
    connect(chartBtn, &QPushButton::clicked, this, &RawAnalysisPanel::onOpenChartWorkbench);

    rightLayout->addStretch(0);

    // ── 检测报告页 ──
    m_reportLabel = new QLabel(QStringLiteral("尚未分析"), rightPanel);
    m_reportLabel->setWordWrap(true);
    m_reportLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_reportLabel->setStyleSheet(
        QStringLiteral("background:#f7f9fb;border:1px solid #dde4ec;border-radius:6px;"
                       "padding:14px;font-size:13px;color:#33475b;"));
    m_reportLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    // ── 卫星数曲线页：垂直 splitter（图表 + 占位），可拖拽调整图表高度 ──
    m_satPage = new QWidget(rightPanel);
    m_satSplitter = new QSplitter(Qt::Vertical, m_satPage);
    m_satSplitter->setChildrenCollapsible(false);
    {
        auto *layout = new QVBoxLayout(m_satPage);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_satSplitter);
    }

    // ── SNR 曲线页：垂直 splitter（图表 + 占位），可拖拽调整图表高度 ──
    m_snrPage = new QWidget(rightPanel);
    m_snrSplitter = new QSplitter(Qt::Vertical, m_snrPage);
    m_snrSplitter->setChildrenCollapsible(false);
    {
        auto *layout = new QVBoxLayout(m_snrPage);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_snrSplitter);
    }

    // ── 曲线页（IMU 加计 / 陀螺 / 模值 + 真值轨迹 / 速度 / 位置 / 姿态） ──
    auto makeChartPage = [&](QWidget **page, QSplitter **splitter) {
        *page = new QWidget(rightPanel);
        *splitter = new QSplitter(Qt::Vertical, *page);
        (*splitter)->setChildrenCollapsible(false);
        auto *layout = new QVBoxLayout(*page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(*splitter);
    };
    makeChartPage(&m_imuAccPage, &m_imuAccSplitter);
    makeChartPage(&m_imuGyrPage, &m_imuGyrSplitter);
    makeChartPage(&m_imuMagPage, &m_imuMagSplitter);
    makeChartPage(&m_truthTrajPage, &m_truthTrajSplitter);
    makeChartPage(&m_truthVelPage, &m_truthVelSplitter);
    makeChartPage(&m_truthPosPage, &m_truthPosSplitter);
    makeChartPage(&m_truthAttPage, &m_truthAttSplitter);
    makeChartPage(&m_truthSatPage, &m_truthSatSplitter);

    // ── 内容堆栈 ──
    m_contentStack = new QStackedWidget(rightPanel);
    m_contentStack->addWidget(m_reportLabel);                    // 0: 质量摘要
    m_contentStack->addWidget(m_satPage);                       // 1: 卫星数曲线
    m_contentStack->addWidget(m_snrPage);                       // 2: SNR曲线
    m_contentStack->addWidget(m_imuAccPage);                    // 3: 加计曲线
    m_contentStack->addWidget(m_imuGyrPage);                    // 4: 陀螺曲线
    m_contentStack->addWidget(m_imuMagPage);                    // 5: 模值曲线
    m_contentStack->addWidget(m_truthTrajPage);                 // 6: 轨迹
    m_contentStack->addWidget(m_truthVelPage);                  // 7: 速度
    m_contentStack->addWidget(m_truthPosPage);                  // 8: 位置高程
    m_contentStack->addWidget(m_truthAttPage);                  // 9: 姿态
    m_contentStack->addWidget(m_truthSatPage);                  // 10: 卫星数
    rightLayout->addWidget(m_contentStack, 1);

    // 左右分栏：左侧固定宽度（可拖动），右侧自适应
    mainSplitter->addWidget(rightPanel);
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setSizes({220, 820});

    auto *outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(mainSplitter);

    // 默认类型为 GNSS，构建树并选中质量摘要
    rebuildTree();
}

void RawAnalysisPanel::rebuildTree()
{
    m_tree->clear();

    QString rootName;
    QList<QPair<QString, int>> items;   // (显示名, 内容页索引)
    if (m_currentType == DataType::Imu) {
        rootName = QStringLiteral("IMU 原始观测值");
        items = {{QStringLiteral("质量摘要"), 0}, {QStringLiteral("加计曲线"), 3},
                 {QStringLiteral("陀螺曲线"), 4}, {QStringLiteral("模值曲线"), 5}};
    } else if (m_currentType == DataType::Truth) {
        rootName = QStringLiteral("真值（GINS）");
        items = {{QStringLiteral("质量摘要"), 0}, {QStringLiteral("轨迹"), 6},
                 {QStringLiteral("速度"), 7}, {QStringLiteral("位置高程"), 8},
                 {QStringLiteral("姿态"), 9}, {QStringLiteral("卫星数"), 10}};
    } else {
        rootName = QStringLiteral("GNSS 原始观测值");
        items = {{QStringLiteral("质量摘要"), 0}, {QStringLiteral("卫星数曲线"), 1},
                 {QStringLiteral("SNR 曲线"), 2}};
    }

    auto *root = new QTreeWidgetItem(m_tree, QStringList{rootName});
    for (const auto &it : items) {
        auto *item = new QTreeWidgetItem(root, QStringList{it.first});
        item->setData(0, Qt::UserRole, it.second);
    }
    root->setExpanded(true);
    m_tree->setCurrentItem(root->child(0));
    onTreeNodeClicked(root->child(0), 0);
}

void RawAnalysisPanel::onTypeChanged(int index)
{
    m_currentType = (index == 1) ? DataType::Imu
                  : (index == 2) ? DataType::Truth
                  : DataType::Gnss;
    resetResults();
    rebuildTree();

    if (m_currentType == DataType::Imu)
        m_pathEdit->setPlaceholderText(
            QStringLiteral("选择 IMU 数据文件（CSV 表格 或 诺瓦泰 RAWIMUA 二进制）"));
    else if (m_currentType == DataType::Truth)
        m_pathEdit->setPlaceholderText(
            QStringLiteral("选择真值文件（GINS 后处理输出 .txt）"));
    else
        m_pathEdit->setPlaceholderText(
            QStringLiteral("选择 RINEX 观测文件（.O / .26O / .obs）"));
    m_pathEdit->clear();
}

void RawAnalysisPanel::resetResults()
{
    stopTrajPlay();
    clearChartPage(m_satSplitter);
    clearChartPage(m_snrSplitter);
    clearChartPage(m_imuAccSplitter);
    clearChartPage(m_imuGyrSplitter);
    clearChartPage(m_imuMagSplitter);
    clearChartPage(m_truthTrajSplitter);
    clearChartPage(m_truthVelSplitter);
    clearChartPage(m_truthPosSplitter);
    clearChartPage(m_truthAttSplitter);
    clearChartPage(m_truthSatSplitter);
    m_outcome = AnalyzeOutcome();
    // 轨迹缓存与旧结果一并释放（swap 保证容量也归还，避免切换类型后内存叠加）
    QVector<QPointF>().swap(m_trajPath);
    QVector<double>().swap(m_trajTimes);
    m_reportLabel->setText(QStringLiteral("尚未分析"));
}

void RawAnalysisPanel::onBrowse()
{
    QString title, filter;
    if (m_currentType == DataType::Imu) {
        title = QStringLiteral("选择 IMU 数据文件");
        filter = QStringLiteral("IMU 数据文件 (*.csv *.txt *.dat *.log *.imu *.bin);;"
                                "诺瓦泰 RAWIMUA (*.dat *.log *.imu *.bin);;"
                                "文本表格 (*.csv *.txt);;所有文件 (*.*)");
    } else if (m_currentType == DataType::Truth) {
        title = QStringLiteral("选择真值文件");
        filter = QStringLiteral("真值文件 (*.txt *.out);;所有文件 (*.*)");
    } else {
        title = QStringLiteral("选择 RINEX 观测文件");
        filter = QStringLiteral("RINEX 观测文件 (*.O *.o *.obs *.rnx);;所有文件 (*.*)");
    }
    const QString path = QFileDialog::getOpenFileName(this, title, QString(), filter);
    if (!path.isEmpty())
        m_pathEdit->setText(path);
}

void RawAnalysisPanel::onAnalyze()
{
    const QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择数据文件"));
        return;
    }

    // 释放上一次分析占用的内存（旧图表与旧结果）
    resetResults();

    // 后台线程解析，避免阻塞 UI
    m_analyzeBtn->setEnabled(false);
    m_browseBtn->setEnabled(false);
    m_pathEdit->setEnabled(false);
    m_typeCombo->setEnabled(false);

    const DataType type = m_currentType;
    m_watcher = new QFutureWatcher<AnalyzeOutcome>(this);
    connect(m_watcher, &QFutureWatcher<AnalyzeOutcome>::finished,
            this, &RawAnalysisPanel::onAnalyzeFinished);

    QFuture<AnalyzeOutcome> future = QtConcurrent::run([path, type]() -> AnalyzeOutcome {
        AnalyzeOutcome r;
        r.dataType = type;
        if (type == DataType::Imu)
            r.ok = ImuReader::analyze(path, &r.imu, &r.err);
        else if (type == DataType::Truth)
            r.ok = TruthReader::analyze(path, &r.truth, &r.err);
        else
            r.ok = RinexReader::analyze(path, &r.summary, &r.err);
        return r;
    });
    m_watcher->setFuture(future);
}

void RawAnalysisPanel::onAnalyzeFinished()
{
    m_analyzeBtn->setEnabled(true);
    m_browseBtn->setEnabled(true);
    m_pathEdit->setEnabled(true);
    m_typeCombo->setEnabled(true);

    m_outcome = m_watcher->result();
    m_watcher->deleteLater();
    m_watcher = nullptr;

    if (!m_outcome.ok) {
        QMessageBox::warning(this, QStringLiteral("分析失败"), m_outcome.err);
        return;
    }

    // 一次性构建全部内容，切换树节点时直接显示已生成的内容
    if (m_outcome.dataType == DataType::Imu) {
        showImuReport(m_outcome.imu);
        buildImuAccChart(m_outcome.imu);
        buildImuGyrChart(m_outcome.imu);
        buildImuMagChart(m_outcome.imu);
    } else if (m_outcome.dataType == DataType::Truth) {
        qDebug() << "[Truth] build start, samples" << m_outcome.truth.samples.size();
        showTruthReport(m_outcome.truth);
        qDebug() << "[Truth] report done";
        buildTruthTrajChart(m_outcome.truth);
        qDebug() << "[Truth] traj done";
        buildTruthVelChart(m_outcome.truth);
        qDebug() << "[Truth] vel done";
        buildTruthPosChart(m_outcome.truth);
        qDebug() << "[Truth] pos done";
        buildTruthAttChart(m_outcome.truth);
        qDebug() << "[Truth] att done";
        buildTruthSatChart(m_outcome.truth);
        qDebug() << "[Truth] sat done";
    } else {
        showReport(m_outcome.summary);
        showSatelliteChart(m_outcome.summary);
        showSnrChart(m_outcome.summary);
    }
}

void RawAnalysisPanel::onTreeNodeClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item) return;
    const QVariant page = item->data(0, Qt::UserRole);
    if (!page.isValid())
        return;   // 根节点，无对应页面
    m_contentStack->setCurrentIndex(page.toInt());
}

// ═══════════════════════════════════════════════════════════════
//  检测报告
// ═══════════════════════════════════════════════════════════════
void RawAnalysisPanel::showReport(const RinexSummary &s)
{
    const int masks[] = {Sys::GPS, Sys::GLO, Sys::GAL, Sys::QZS, Sys::BDS};

    // ── 基础统计 ──
    QString report = QStringLiteral(
        "<p style='font-size:14px;font-weight:700;color:#12263f;margin-bottom:6px;'>"
        "观测文件质量检测报告</p>"
        "<table cellspacing='0' cellpadding='2' style='font-size:13px;color:#33475b;'>"
        "<tr><td style='color:#63768c;width:110px;'>观测记录总数</td>"
        " <td><b>%1</b></td></tr>"
        "<tr><td style='color:#63768c;'>历元总数</td>"
        " <td><b>%2</b></td></tr>"
        "<tr><td style='color:#63768c;'>时长</td>"
        " <td><b>%3 秒</b></td></tr>"
        "<tr><td style='color:#63768c;'>采样率</td>"
        " <td><b>%4 Hz</b></td></tr>"
        "<tr><td style='color:#63768c;'>丢历元数</td>"
        " <td><b>%5</b>（总间断 %6 秒）</td></tr>"
        "</table>")
        .arg(s.totalObsRecords)
        .arg(s.totalEpochs)
        .arg(s.durationSec, 0, 'f', 1)
        .arg(s.sampleRate, 0, 'f', 2)
        .arg(s.epochGapCount)
        .arg(s.totalGapSec, 0, 'f', 1);

    // ── 信噪比 SNR 汇总 ──
    QString snrRows;
    for (int m : masks) {
        if (!s.snrPerSysCode.contains(m)) continue;
        const auto &codeMap = s.snrPerSysCode.value(m);
        for (auto it = codeMap.constBegin(); it != codeMap.constEnd(); ++it) {
            const SnrStat &st = it.value();
            snrRows += QStringLiteral(
                           "<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
                           .arg(RinexSummary::sysName(m))
                           .arg(it.key())
                           .arg(st.min, 0, 'f', 1)
                           .arg(st.max, 0, 'f', 1)
                           .arg(st.mean(), 0, 'f', 1);
        }
    }

    if (!snrRows.isEmpty()) {
        report += QStringLiteral(
            "<p style='font-size:13px;font-weight:600;color:#12263f;margin:12px 0 4px;'>"
            "信噪比 SNR 汇总（dBHz）</p>"
            "<table cellspacing='0' cellpadding='2' style='font-size:13px;color:#33475b;'>"
            "<tr>"
            "<th style='text-align:left;color:#63768c;'>系统</th>"
            "<th style='text-align:left;color:#63768c;'>频点</th>"
            "<th style='text-align:left;color:#63768c;'>最小值</th>"
            "<th style='text-align:left;color:#63768c;'>最大值</th>"
            "<th style='text-align:left;color:#63768c;'>均值</th>"
            "</tr>%1</table>").arg(snrRows);
    }

    // ── 异常检测（SNR 偏低频点 + 异常时段定位 + 丢历元） ──
    const double SNR_LOW_THRESHOLD = 28.0;   // dBHz，低于此值视为偏低
    bool hasAnomaly = false;
    QStringList anomalyList;

    for (int m : masks) {
        if (!(s.systemMask & m)) continue;
        const auto &codeMap = s.snrPerSysCode.value(m);
        const auto &timeMap = s.snrTimePerSysCode.value(m);
        for (auto it = codeMap.constBegin(); it != codeMap.constEnd(); ++it) {
            const SnrStat &st = it.value();
            if (st.count == 0 || st.mean() >= SNR_LOW_THRESHOLD) continue;
            hasAnomaly = true;

            const QString code = it.key();

            // 定位 SNR 低于阈值的连续时段
            QString segs;
            if (timeMap.contains(code)) {
                const auto &ts = timeMap.value(code);
                double segStart = -1.0;
                QVector<QPair<double, double>> ranges;
                for (const auto &p : ts) {
                    if (p.second < SNR_LOW_THRESHOLD) {
                        if (segStart < 0) segStart = p.first;
                    } else if (segStart >= 0) {
                        ranges.append({segStart, p.first});
                        segStart = -1.0;
                    }
                }
                if (segStart >= 0) ranges.append({segStart, ts.last().first});

                QStringList segList;
                const int maxSeg = qMin(3, ranges.size());
                for (int k = 0; k < maxSeg; ++k) {
                    segList << QStringLiteral("%1~%2 秒")
                                   .arg(ranges[k].first - s.firstTime, 0, 'f', 1)
                                   .arg(ranges[k].second - s.firstTime, 0, 'f', 1);
                }
                if (!segList.isEmpty())
                    segs = segList.join(QStringLiteral("、"));
            }

            QString item = QStringLiteral("%1 %2 频段 SNR 偏低（均值 %3 dBHz）")
                               .arg(RinexSummary::sysName(m))
                               .arg(code)
                               .arg(st.mean(), 0, 'f', 1);
            if (!segs.isEmpty())
                item += QStringLiteral("，异常时段 %1").arg(segs);
            anomalyList << item;
        }
    }

    if (s.epochGapCount > 0) {
        anomalyList << QStringLiteral("存在 %1 个历元间断（共 %2 秒）")
                           .arg(s.epochGapCount)
                           .arg(s.totalGapSec, 0, 'f', 1);
        hasAnomaly = true;
    }

    if (hasAnomaly) {
        report += QStringLiteral("<p style='color:#dc2626;font-size:13px;margin-top:8px;'>"
                                 "<b>异常提示：</b>%1</p>")
                      .arg(anomalyList.join(QStringLiteral("；")));
    } else {
        report += QStringLiteral("<p style='color:#16a34a;font-size:13px;margin-top:8px;'>"
                                 "<b>数据质量正常，无明显异常。</b></p>");
    }

    m_reportLabel->setText(report);
}

// ═══════════════════════════════════════════════════════════════
//  卫星数曲线
// ═══════════════════════════════════════════════════════════════
void RawAnalysisPanel::showSatelliteChart(const RinexSummary &s)
{
    buildSatChart(s);
}

void RawAnalysisPanel::buildSatChart(const RinexSummary &s)
{
    // 清理旧图表（立即释放内存）
    clearChartPage(m_satSplitter);

    if (s.epochStats.isEmpty()) {
        auto *tip = new QLabel(QStringLiteral("暂无卫星数数据"));
        tip->setAlignment(Qt::AlignCenter);
        tip->setStyleSheet(QStringLiteral("color:#94a3b8;font-size:13px;"));
        m_satSplitter->addWidget(tip);
        return;
    }

    auto *chart = new QChart();
    chart->setTitle(QStringLiteral("可见卫星数 / 历元"));
    chart->setAnimationOptions(QChart::NoAnimation);

    static const QColor colors[] = {
        QColor(59, 130, 246),   // GPS  蓝
        QColor(239, 68, 68),    // GLO  红
        QColor(34, 197, 94),    // GAL  绿
        QColor(168, 85, 247),   // BDS  紫
        QColor(245, 158, 11),   // QZS  黄
        QColor(100, 116, 139),  // 全部 灰
    };
    static const QString labels[] = {
        QStringLiteral("GPS"), QStringLiteral("GLONASS"), QStringLiteral("Galileo"),
        QStringLiteral("BeiDou"), QStringLiteral("QZSS"), QStringLiteral("全部"),
    };
    auto getField = [](const EpochStat &es, int idx) -> int {
        switch (idx) {
            case 0: return es.gps;  case 1: return es.glo;
            case 2: return es.gal;  case 3: return es.bds;
            case 4: return es.qzs;  default: return es.total;
        }
    };

    const double t0 = s.epochStats.first().time;

    // Y 轴整数上限（取全部卫星数最大值）
    int maxTotal = 0;
    for (const auto &es : s.epochStats)
        maxTotal = qMax(maxTotal, es.total);

    QVector<QLineSeries *> seriesList;
    for (int i = 0; i < 6; ++i) {
        auto *series = new QLineSeries(chart);
        series->setName(labels[i]);
        series->setColor(colors[i]);
        for (const auto &es : s.epochStats)
            series->append(es.time - t0, getField(es, i));
        chart->addSeries(series);
        seriesList.append(series);
    }

    auto *axisX = new QValueAxis();
    axisX->setTitleText(QStringLiteral("时间（秒）"));
    axisX->setRange(0, s.epochStats.last().time - t0);
    chart->addAxis(axisX, Qt::AlignBottom);

    auto *axisY = new QValueAxis();
    axisY->setTitleText(QStringLiteral("卫星数"));
    axisY->setRange(0, maxTotal + 1);
    axisY->setLabelFormat("%d");          // 强制整数刻度显示
    axisY->setTickInterval(maxTotal > 20 ? 2.0 : 1.0);
    chart->addAxis(axisY, Qt::AlignLeft);

    for (auto *series : chart->series()) {
        series->attachAxis(axisX);
        series->attachAxis(axisY);
    }
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    auto *view = new ZoomableChartView(chart, axisX);
    view->setMinimumHeight(160);

    // 星座复选框（横排，默认全选，可单选某星座）
    auto *checkBar = new QWidget();
    auto *checkLayout = new QHBoxLayout(checkBar);
    checkLayout->setContentsMargins(0, 0, 0, 0);
    checkLayout->setSpacing(12);
    for (int i = 0; i < seriesList.size(); ++i) {
        auto *cb = new QCheckBox(labels[i], checkBar);
        cb->setChecked(true);
        QLineSeries *series = seriesList[i];
        connect(cb, &QCheckBox::toggled, this, [series](bool on) {
            series->setVisible(on);
        });
        checkLayout->addWidget(cb);
    }
    checkLayout->addStretch();

    // 容器：复选框栏 + 图表 + 横向滚动条
    auto *container = new QWidget();
    auto *v = new QVBoxLayout(container);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(4);
    v->addWidget(checkBar);
    v->addWidget(view, 1);

    auto *scrollBar = new QScrollBar(Qt::Horizontal);
    v->addWidget(scrollBar);
    attachTimeScrollBar(axisX, scrollBar);

    m_satSplitter->addWidget(container);

    // 占位 widget，拖动分隔条可调整图表高度占比
    auto *placeholder = new QWidget();
    m_satSplitter->addWidget(placeholder);
    m_satSplitter->setStretchFactor(0, 4);
    m_satSplitter->setStretchFactor(1, 1);
    m_satSplitter->setSizes({500, 120});
}

// ═══════════════════════════════════════════════════════════════
//  SNR 曲线
// ═══════════════════════════════════════════════════════════════
void RawAnalysisPanel::showSnrChart(const RinexSummary &s)
{
    buildSnrChart(s);
}

void RawAnalysisPanel::buildSnrChart(const RinexSummary &s)
{
    // 清理旧图表（立即释放内存）
    clearChartPage(m_snrSplitter);

    if (s.snrEpochMean.isEmpty()) {
        auto *tip = new QLabel(QStringLiteral("暂无 SNR 数据"));
        tip->setAlignment(Qt::AlignCenter);
        tip->setStyleSheet(QStringLiteral("color:#94a3b8;font-size:13px;"));
        m_snrSplitter->addWidget(tip);
        return;
    }

    auto *chart = new QChart();
    chart->setTitle(QStringLiteral("平均信噪比 SNR / 历元"));
    chart->setAnimationOptions(QChart::NoAnimation);

    const double t0 = s.snrEpochMean.first().first;

    auto *series = new QLineSeries(chart);
    series->setName(QStringLiteral("平均 SNR"));
    series->setColor(QColor(59, 130, 246));
    for (const auto &p : s.snrEpochMean)
        series->append(p.first - t0, p.second);
    chart->addSeries(series);

    auto *axisX = new QValueAxis();
    axisX->setTitleText(QStringLiteral("时间（秒）"));
    axisX->setRange(0, s.snrEpochMean.last().first - t0);
    chart->addAxis(axisX, Qt::AlignBottom);

    auto *axisY = new QValueAxis();
    axisY->setTitleText(QStringLiteral("SNR（dBHz）"));
    chart->addAxis(axisY, Qt::AlignLeft);

    series->attachAxis(axisX);
    series->attachAxis(axisY);
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    auto *view = new ZoomableChartView(chart, axisX);
    view->setMinimumHeight(160);

    // 容器：图表 + 横向滚动条
    auto *container = new QWidget();
    auto *v = new QVBoxLayout(container);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(4);
    v->addWidget(view, 1);

    auto *scrollBar = new QScrollBar(Qt::Horizontal);
    v->addWidget(scrollBar);
    attachTimeScrollBar(axisX, scrollBar);

    m_snrSplitter->addWidget(container);

    // 占位 widget，拖动分隔条可调整图表高度占比
    auto *placeholder = new QWidget();
    m_snrSplitter->addWidget(placeholder);
    m_snrSplitter->setStretchFactor(0, 4);
    m_snrSplitter->setStretchFactor(1, 1);
    m_snrSplitter->setSizes({500, 120});
}

void RawAnalysisPanel::clearChartPage(QSplitter *splitter)
{
    while (splitter->count() > 0) {
        QWidget *w = splitter->widget(0);
        w->setParent(nullptr);
        delete w;   // 立即析构，释放图表与数据点内存
    }
}

void RawAnalysisPanel::attachTimeScrollBar(QValueAxis *axisX, QScrollBar *bar)
{
    const double fullLo = axisX->min();
    const double fullHi = axisX->max();
    const double fullSpan = qMax(1.0, fullHi - fullLo);

    bar->setRange(0, qCeil(fullSpan));
    bar->setPageStep(qCeil(fullSpan));
    bar->setSingleStep(qMax(1, qCeil(fullSpan) / 200));
    bar->setValue(0);

    auto updating = std::make_shared<bool>(false);

    // 缩放导致 X 轴范围变化 → 同步滚动条
    QObject::connect(axisX, &QValueAxis::rangeChanged, bar,
                     [bar, axisX, fullLo, fullHi, fullSpan, updating](qreal lo, qreal hi) {
        if (*updating) return;
        *updating = true;
        if (hi - lo >= fullSpan - 1e-9) {
            // 缩到全范围，复位 X 轴与滚动条
            axisX->setRange(fullLo, fullHi);
            bar->blockSignals(true);
            bar->setValue(0);
            bar->setPageStep(qCeil(fullSpan));
            bar->blockSignals(false);
        } else {
            bar->blockSignals(true);
            bar->setValue(qRound(lo - fullLo));
            bar->setPageStep(qMax(1, qRound(hi - lo)));
            bar->blockSignals(false);
        }
        *updating = false;
    });

    // 拖动滚动条 → 平移 X 轴（保持可见窗口宽度不变）
    QObject::connect(bar, &QScrollBar::valueChanged, axisX,
                     [axisX, fullLo, bar, updating](int v) {
        if (*updating) return;
        *updating = true;
        const double span = qMax(1.0, static_cast<double>(bar->pageStep()));
        axisX->setRange(fullLo + v, fullLo + v + span);
        *updating = false;
    });
}

void RawAnalysisPanel::attachYScrollBar(QValueAxis *axisY, QScrollBar *bar)
{
    const double fullLo = axisY->min();
    const double fullHi = axisY->max();
    const double fullSpan = qMax(1.0, fullHi - fullLo);

    bar->setRange(0, qCeil(fullSpan));
    bar->setPageStep(qCeil(fullSpan));
    bar->setSingleStep(qMax(1, qCeil(fullSpan) / 200));
    bar->setValue(0);

    auto updating = std::make_shared<bool>(false);

    // 缩放导致 Y 轴范围变化 → 同步纵向滚动条
    QObject::connect(axisY, &QValueAxis::rangeChanged, bar,
                     [bar, axisY, fullLo, fullHi, fullSpan, updating](qreal lo, qreal hi) {
        if (*updating) return;
        *updating = true;
        if (hi - lo >= fullSpan - 1e-9) {
            axisY->setRange(fullLo, fullHi);
            bar->blockSignals(true);
            bar->setValue(0);
            bar->setPageStep(qCeil(fullSpan));
            bar->blockSignals(false);
        } else {
            bar->blockSignals(true);
            bar->setValue(qRound(fullHi - hi));
            bar->setPageStep(qMax(1, qRound(hi - lo)));
            bar->blockSignals(false);
        }
        *updating = false;
    });

    // 拖动纵向滚动条 → 平移 Y 轴（向上滚动向上移动）
    QObject::connect(bar, &QScrollBar::valueChanged, axisY,
                     [axisY, fullHi, bar, updating](int v) {
        if (*updating) return;
        *updating = true;
        const double span = qMax(1.0, static_cast<double>(bar->pageStep()));
        axisY->setRange(fullHi - v - span, fullHi - v);
        *updating = false;
    });
}

// ═══════════════════════════════════════════════════════════════
//  IMU 检测报告
// ═══════════════════════════════════════════════════════════════
void RawAnalysisPanel::showImuReport(const ImuSummary &s)
{
    const QString fmt = s.formatText.isEmpty()
                            ? QStringLiteral("文本表格（CSV/TXT）") : s.formatText;

    QString report = QStringLiteral(
        "<p style='font-size:14px;font-weight:700;color:#12263f;margin-bottom:6px;'>"
        "IMU 原始数据质量检测报告</p>"
        "<table cellspacing='0' cellpadding='2' style='font-size:13px;color:#33475b;'>"
        "<tr><td style='color:#63768c;width:110px;'>数据格式</td>"
        " <td><b>%1</b></td></tr>"
        "<tr><td style='color:#63768c;'>样本总数</td>"
        " <td><b>%2</b></td></tr>"
        "<tr><td style='color:#63768c;'>时长</td>"
        " <td><b>%3 秒</b></td></tr>"
        "<tr><td style='color:#63768c;'>采样率</td>"
        " <td><b>%4 Hz</b></td></tr>"
        "<tr><td style='color:#63768c;'>加计单位</td>"
        " <td><b>%5</b></td></tr>"
        "<tr><td style='color:#63768c;'>陀螺单位</td>"
        " <td><b>%6</b></td></tr>"
        "</table>")
        .arg(fmt)
        .arg(s.totalSamples)
        .arg(s.durationSec, 0, 'f', 1)
        .arg(s.sampleRate, 0, 'f', 2)
        .arg(s.accelUnitText)
        .arg(s.gyroUnitText);

    // ── 丢数检查 ──
    report += QStringLiteral(
        "<p style='font-size:13px;font-weight:600;color:#12263f;margin:12px 0 4px;'>"
        "丢数检查</p>"
        "<table cellspacing='0' cellpadding='2' style='font-size:13px;color:#33475b;'>"
        "<tr><td style='color:#63768c;width:110px;'>丢数事件</td><td><b>%1</b> 次</td></tr>"
        "<tr><td style='color:#63768c;'>丢样本数</td><td><b>%2</b></td></tr>"
        "<tr><td style='color:#63768c;'>总间断时长</td><td><b>%3</b> 秒</td></tr>"
        "</table>")
        .arg(s.gapEvents)
        .arg(s.gapCount)
        .arg(s.totalGapSec, 0, 'f', 3);

    // ── 回退检查 ──
    report += QStringLiteral(
        "<p style='font-size:13px;font-weight:600;color:#12263f;margin:12px 0 4px;'>"
        "回退检查</p>"
        "<table cellspacing='0' cellpadding='2' style='font-size:13px;color:#33475b;'>"
        "<tr><td style='color:#63768c;width:110px;'>回退次数</td><td><b>%1</b> 次</td></tr>"
        "<tr><td style='color:#63768c;'>单次最大回退</td><td><b>%2</b> 秒</td></tr>"
        "<tr><td style='color:#63768c;'>累计回退</td><td><b>%3</b> 秒</td></tr>"
        "</table>")
        .arg(s.rollbackCount)
        .arg(s.maxRollbackSec, 0, 'f', 3)
        .arg(s.totalRollbackSec, 0, 'f', 3);

    // 各轴统计
    auto axisRow = [](const QString &name, const AxisStat &st) {
        return QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td></tr>")
            .arg(name)
            .arg(st.min, 0, 'f', 3)
            .arg(st.max, 0, 'f', 3)
            .arg(st.mean(), 0, 'f', 3);
    };

    const QString accRows = axisRow(QStringLiteral("X"), s.accX) +
                            axisRow(QStringLiteral("Y"), s.accY) +
                            axisRow(QStringLiteral("Z"), s.accZ);
    const QString gyrRows = axisRow(QStringLiteral("X"), s.gyrX) +
                            axisRow(QStringLiteral("Y"), s.gyrY) +
                            axisRow(QStringLiteral("Z"), s.gyrZ);

    report += QStringLiteral(
        "<p style='font-size:13px;font-weight:600;color:#12263f;margin:12px 0 4px;'>"
        "加速度计统计（m/s²）</p>"
        "<table cellspacing='0' cellpadding='2' style='font-size:13px;color:#33475b;'>"
        "<tr><th style='text-align:left;color:#63768c;'>轴</th>"
        "<th style='text-align:left;color:#63768c;'>最小值</th>"
        "<th style='text-align:left;color:#63768c;'>最大值</th>"
        "<th style='text-align:left;color:#63768c;'>均值</th></tr>%1</table>"
        "<p style='font-size:13px;font-weight:600;color:#12263f;margin:12px 0 4px;'>"
        "陀螺仪统计（rad/s）</p>"
        "<table cellspacing='0' cellpadding='2' style='font-size:13px;color:#33475b;'>"
        "<tr><th style='text-align:left;color:#63768c;'>轴</th>"
        "<th style='text-align:left;color:#63768c;'>最小值</th>"
        "<th style='text-align:left;color:#63768c;'>最大值</th>"
        "<th style='text-align:left;color:#63768c;'>均值</th></tr>%2</table>")
        .arg(accRows, gyrRows);

    // ── 异常汇总 ──
    QStringList warns;
    if (s.gapEvents > 0) {
        warns << QStringLiteral("丢数 %1 次（共丢 %2 个样本，间断 %3 秒）")
                     .arg(s.gapEvents).arg(s.gapCount).arg(s.totalGapSec, 0, 'f', 3);
    }
    if (s.rollbackCount > 0) {
        warns << QStringLiteral("时间回退 %1 次（单次最大 %2 秒，累计 %3 秒）")
                     .arg(s.rollbackCount)
                     .arg(s.maxRollbackSec, 0, 'f', 3)
                     .arg(s.totalRollbackSec, 0, 'f', 3);
    }

    if (warns.isEmpty()) {
        report += QStringLiteral("<p style='color:#16a34a;font-size:13px;margin-top:8px;'>"
                                 "<b>数据质量正常，无丢数与回退。</b></p>");
    } else {
        report += QStringLiteral("<p style='color:#dc2626;font-size:13px;margin-top:8px;'>"
                                 "<b>异常提示：</b>%1</p>")
                      .arg(warns.join(QStringLiteral("；")));
    }

    m_reportLabel->setText(report);
}

// ═══════════════════════════════════════════════════════════════
//  IMU 曲线
// ═══════════════════════════════════════════════════════════════
void RawAnalysisPanel::buildImuTriChart(QSplitter *splitter, const ImuSummary &s,
                                        const QString &title, const QString &yTitle,
                                        bool isAccel)
{
    clearChartPage(splitter);

    if (s.samples.isEmpty()) {
        auto *tip = new QLabel(QStringLiteral("暂无数据"));
        tip->setAlignment(Qt::AlignCenter);
        tip->setStyleSheet(QStringLiteral("color:#94a3b8;font-size:13px;"));
        splitter->addWidget(tip);
        return;
    }

    auto getField = [&](const ImuSample &sp, int i) -> double {
        if (isAccel) {
            switch (i) {
            case 0: return sp.ax;
            case 1: return sp.ay;
            default: return sp.az;
            }
        } else {
            switch (i) {
            case 0: return sp.gx;
            case 1: return sp.gy;
            default: return sp.gz;
            }
        }
    };

    static const QColor colors[] = {
        QColor(59, 130, 246),   // X 蓝
        QColor(239, 68, 68),    // Y 红
        QColor(34, 197, 94),    // Z 绿
    };
    static const QStringList labels = {
        QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z"),
    };

    auto *chart = new QChart();
    chart->setTitle(title);
    chart->setAnimationOptions(QChart::NoAnimation);

    const double t0 = s.samples.first().t;
    const double t1 = s.samples.last().t;

    QVector<QLineSeries *> seriesList;
    for (int i = 0; i < 3; ++i) {
        auto *series = new QLineSeries(chart);
        series->setName(labels[i]);
        series->setColor(colors[i]);
        for (const ImuSample &sp : s.samples)
            series->append(sp.t - t0, getField(sp, i));
        chart->addSeries(series);
        seriesList.append(series);
    }

    auto *axisX = new QValueAxis();
    axisX->setTitleText(QStringLiteral("时间（秒）"));
    axisX->setRange(0, t1 - t0);
    chart->addAxis(axisX, Qt::AlignBottom);

    auto *axisY = new QValueAxis();
    axisY->setTitleText(yTitle);
    chart->addAxis(axisY, Qt::AlignLeft);

    for (auto *series : chart->series()) {
        series->attachAxis(axisX);
        series->attachAxis(axisY);
    }
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    auto *view = new ZoomableChartView(chart, axisX);
    view->setMinimumHeight(160);

    // 轴复选框（横排，默认全选，可单选某轴）
    auto *checkBar = new QWidget();
    auto *checkLayout = new QHBoxLayout(checkBar);
    checkLayout->setContentsMargins(0, 0, 0, 0);
    checkLayout->setSpacing(12);
    for (int i = 0; i < seriesList.size(); ++i) {
        auto *cb = new QCheckBox(labels[i], checkBar);
        cb->setChecked(true);
        QLineSeries *series = seriesList[i];
        connect(cb, &QCheckBox::toggled, this, [series](bool on) {
            series->setVisible(on);
        });
        checkLayout->addWidget(cb);
    }
    checkLayout->addStretch();

    auto *container = new QWidget();
    auto *v = new QVBoxLayout(container);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(4);
    v->addWidget(checkBar);
    v->addWidget(view, 1);

    auto *scrollBar = new QScrollBar(Qt::Horizontal);
    v->addWidget(scrollBar);
    attachTimeScrollBar(axisX, scrollBar);

    splitter->addWidget(container);

    auto *placeholder = new QWidget();
    splitter->addWidget(placeholder);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({500, 120});
}

void RawAnalysisPanel::buildImuAccChart(const ImuSummary &s)
{
    buildImuTriChart(m_imuAccSplitter, s,
                     QStringLiteral("加速度计三轴 / 时间"),
                     QStringLiteral("加速度（m/s²）"), true);
}

void RawAnalysisPanel::buildImuGyrChart(const ImuSummary &s)
{
    buildImuTriChart(m_imuGyrSplitter, s,
                     QStringLiteral("陀螺仪三轴 / 时间"),
                     QStringLiteral("角速度（rad/s）"), false);
}

void RawAnalysisPanel::buildImuMagChart(const ImuSummary &s)
{
    clearChartPage(m_imuMagSplitter);

    if (s.accelMag.isEmpty() && s.gyroMag.isEmpty()) {
        auto *tip = new QLabel(QStringLiteral("暂无模值数据"));
        tip->setAlignment(Qt::AlignCenter);
        tip->setStyleSheet(QStringLiteral("color:#94a3b8;font-size:13px;"));
        m_imuMagSplitter->addWidget(tip);
        return;
    }

    auto *chart = new QChart();
    chart->setTitle(QStringLiteral("加速度模值 / 角速度模值"));
    chart->setAnimationOptions(QChart::NoAnimation);

    const double t0 = s.samples.first().t;
    const double t1 = s.samples.last().t;

    // 加速度模值曲线（左轴）
    auto *accSeries = new QLineSeries(chart);
    accSeries->setName(QStringLiteral("加速度模值 |a|"));
    accSeries->setColor(QColor(59, 130, 246));
    for (const auto &p : s.accelMag)
        accSeries->append(p.first - t0, p.second);
    chart->addSeries(accSeries);

    // 角速度模值曲线（右轴）
    auto *gyrSeries = new QLineSeries(chart);
    gyrSeries->setName(QStringLiteral("角速度模值 |ω|"));
    gyrSeries->setColor(QColor(239, 68, 68));
    for (const auto &p : s.gyroMag)
        gyrSeries->append(p.first - t0, p.second);
    chart->addSeries(gyrSeries);

    auto *axisX = new QValueAxis();
    axisX->setTitleText(QStringLiteral("时间（秒）"));
    axisX->setRange(0, t1 - t0);
    chart->addAxis(axisX, Qt::AlignBottom);

    auto *axisYLeft = new QValueAxis();
    axisYLeft->setTitleText(QStringLiteral("加速度模值（m/s²）"));
    chart->addAxis(axisYLeft, Qt::AlignLeft);

    auto *axisYRight = new QValueAxis();
    axisYRight->setTitleText(QStringLiteral("角速度模值（rad/s）"));
    chart->addAxis(axisYRight, Qt::AlignRight);

    accSeries->attachAxis(axisX);
    accSeries->attachAxis(axisYLeft);
    gyrSeries->attachAxis(axisX);
    gyrSeries->attachAxis(axisYRight);

    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    auto *view = new ZoomableChartView(chart, axisX);
    view->setMinimumHeight(160);

    // 复选框：加速度模值 / 角速度模值
    auto *checkBar = new QWidget();
    auto *checkLayout = new QHBoxLayout(checkBar);
    checkLayout->setContentsMargins(0, 0, 0, 0);
    checkLayout->setSpacing(12);
    auto mkCb = [&](const QString &name, QLineSeries *series) {
        auto *cb = new QCheckBox(name, checkBar);
        cb->setChecked(true);
        connect(cb, &QCheckBox::toggled, this, [series](bool on) {
            series->setVisible(on);
        });
        checkLayout->addWidget(cb);
    };
    mkCb(QStringLiteral("加速度模值"), accSeries);
    mkCb(QStringLiteral("角速度模值"), gyrSeries);
    checkLayout->addStretch();

    auto *container = new QWidget();
    auto *v = new QVBoxLayout(container);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(4);
    v->addWidget(checkBar);
    v->addWidget(view, 1);

    auto *scrollBar = new QScrollBar(Qt::Horizontal);
    v->addWidget(scrollBar);
    attachTimeScrollBar(axisX, scrollBar);

    m_imuMagSplitter->addWidget(container);

    auto *placeholder = new QWidget();
    m_imuMagSplitter->addWidget(placeholder);
    m_imuMagSplitter->setStretchFactor(0, 4);
    m_imuMagSplitter->setStretchFactor(1, 1);
    m_imuMagSplitter->setSizes({500, 120});
}

// ═══════════════════════════════════════════════════════════════
//  真值检测报告
// ═══════════════════════════════════════════════════════════════
void RawAnalysisPanel::showTruthReport(const TruthSummary &s)
{
    qDebug() << "[TruthReport] lon" << s.minLon << s.maxLon
             << "lat" << s.minLat << s.maxLat << "h" << s.minH << s.maxH;
    QString report = QStringLiteral(
        "<p style='font-size:14px;font-weight:700;color:#12263f;margin-bottom:6px;'>"
        "真值数据质量检测报告</p>"
        "<table cellspacing='0' cellpadding='2' style='font-size:13px;color:#33475b;'>"
        "<tr><td style='color:#63768c;width:110px;'>样本总数</td>"
        " <td><b>%1</b></td></tr>"
        "<tr><td style='color:#63768c;'>时长</td>"
        " <td><b>%2 秒</b></td></tr>"
        "<tr><td style='color:#63768c;'>采样率</td>"
        " <td><b>%3 Hz</b></td></tr>"
        "<tr><td style='color:#63768c;'>纬度范围</td>"
        " <td><b>%4 ~ %5 deg</b></td></tr>"
        "<tr><td style='color:#63768c;'>经度范围</td>"
        " <td><b>%6 ~ %7 deg</b></td></tr>"
        "<tr><td style='color:#63768c;'>椭球高范围</td>"
        " <td><b>%8 ~ %9 m</b></td></tr>"
        "</table>")
        .arg(s.totalSamples)
        .arg(s.durationSec, 0, 'f', 1)
        .arg(s.sampleRate, 0, 'f', 2)
        .arg(s.minLat, 0, 'f', 7)
        .arg(s.maxLat, 0, 'f', 7)
        .arg(s.minLon, 0, 'f', 7)
        .arg(s.maxLon, 0, 'f', 7)
        .arg(s.minH, 0, 'f', 2)
        .arg(s.maxH, 0, 'f', 2);

    if (s.sampleRate > 0.0)
        report += QStringLiteral("<p style='color:#16a34a;font-size:13px;margin-top:8px;'>"
                                 "<b>数据质量正常。</b></p>");
    else
        report += QStringLiteral("<p style='color:#dc2626;font-size:13px;margin-top:8px;'>"
                                 "<b>异常提示：</b>时间间隔异常，无法计算采样率。</p>");

    m_reportLabel->setText(report);
}

// ═══════════════════════════════════════════════════════════════
//  真值曲线：经纬度轨迹
// ═══════════════════════════════════════════════════════════════
void RawAnalysisPanel::buildTruthTrajChart(const TruthSummary &s)
{
    stopTrajPlay();
    clearChartPage(m_truthTrajSplitter);

    if (s.samples.isEmpty()) {
        qDebug() << "[TruthTraj] samples empty";
        auto *tip = new QLabel(QStringLiteral("暂无轨迹数据"));
        tip->setAlignment(Qt::AlignCenter);
        tip->setStyleSheet(QStringLiteral("color:#94a3b8;font-size:13px;"));
        m_truthTrajSplitter->addWidget(tip);
        return;
    }
    qDebug() << "[TruthTraj] samples" << s.samples.size()
             << "first lon/lat" << s.samples.first().lon << s.samples.first().lat
             << "last lon/lat" << s.samples.last().lon << s.samples.last().lat;

    // 以经纬度中心点作为原点，画相对偏移并换算为米（经度按纬度余弦折算，保持轨迹几何比例）
    const double lat0 = (s.minLat + s.maxLat) / 2.0;
    const double lon0 = (s.minLon + s.maxLon) / 2.0;
    const double kMetersPerDeg = 111320.0;                    // 1 度纬度约 111320 米
    const double cosLat = qCos(qDegreesToRadians(lat0));

    auto *chart = new QChart();
    chart->setTitle(QStringLiteral("经纬度轨迹（相对中心点）"));
    chart->setAnimationOptions(QChart::NoAnimation);

    auto *series = new QLineSeries(chart);
    series->setName(QStringLiteral("轨迹"));
    series->setColor(QColor(59, 130, 246));
    const auto ds = downSample(s.samples, 10.0);
    m_trajDuration = s.durationSec;   // 数据总时长（秒），用于控制播放速度

    // 投影为米并统计包围盒，供 X/Y 等比例尺取轴范围
    m_trajPath.clear();
    m_trajPath.reserve(ds.size());
    m_trajTimes.clear();
    m_trajTimes.reserve(ds.size());
    double minE = 0, maxE = 0, minN = 0, maxN = 0;
    bool hasBox = false;
    for (const TruthSample &sp : ds) {
        const double e = (sp.lon - lon0) * kMetersPerDeg * cosLat;
        const double n = (sp.lat - lat0) * kMetersPerDeg;
        series->append(e, n);
        m_trajPath.append(QPointF(e, n));
        m_trajTimes.append(sp.utcSow);   // 与轨迹点一一对应，供点击/搜索时间
        if (!hasBox) { minE = maxE = e; minN = maxN = n; hasBox = true; }
        else {
            minE = qMin(minE, e); maxE = qMax(maxE, e);
            minN = qMin(minN, n); maxN = qMax(maxN, n);
        }
    }
    chart->addSeries(series);

    auto *axisX = new QValueAxis();
    axisX->setTitleText(QStringLiteral("东向（m）"));
    chart->addAxis(axisX, Qt::AlignBottom);

    auto *axisY = new QValueAxis();
    axisY->setTitleText(QStringLiteral("北向（m）"));
    chart->addAxis(axisY, Qt::AlignLeft);

    // 地图视图：X/Y 采用同一比例尺（等跨度），保持轨迹几何形状不被拉伸；
    // 参考 RTKLIB plot 的地图等比例显示方式
    const double cx = (minE + maxE) / 2.0;
    const double cy = (minN + maxN) / 2.0;
    const double half = qMax(maxE - minE, maxN - minN) / 2.0;
    const double span = qMax(half * 1.05, 1.0);
    axisX->setRange(cx - span, cx + span);
    axisY->setRange(cy - span, cy + span);

    series->attachAxis(axisX);
    series->attachAxis(axisY);
    chart->legend()->setVisible(false);

    // 终点（红色，固定）
    if (!m_trajPath.isEmpty()) {
        auto *endDot = new QScatterSeries(chart);
        endDot->setName(QStringLiteral("终点"));
        endDot->setColor(QColor(239, 68, 68));
        endDot->setMarkerSize(11.0);
        endDot->append(m_trajPath.last());
        chart->addSeries(endDot);
        endDot->attachAxis(axisX);
        endDot->attachAxis(axisY);
    }

    // 起点/播放点（绿色，点击“开始”后沿轨迹移动）
    m_trajPlayDot = new QScatterSeries(chart);
    m_trajPlayDot->setName(QStringLiteral("起点"));
    m_trajPlayDot->setColor(QColor(34, 197, 94));
    m_trajPlayDot->setMarkerSize(13.0);
    m_trajPlayDot->append(m_trajPath.isEmpty() ? QPointF(0, 0) : m_trajPath.first());
    chart->addSeries(m_trajPlayDot);
    m_trajPlayDot->attachAxis(axisX);
    m_trajPlayDot->attachAxis(axisY);

    auto *view = new ZoomableChartView(chart, axisX, axisY);
    view->setLinkedZoom(true);   // 地图模式下 X/Y 同步缩放，保持比例
    view->setRenderHint(QPainter::Antialiasing, false);   // 大数据量关闭抗锯齿
    view->setMinimumHeight(160);

    // ── 选中点高亮（黄色），配合点击/搜索显示该点 UTC 天内秒 ──
    auto *pickDot = new QScatterSeries(chart);
    pickDot->setColor(QColor(250, 204, 21));
    pickDot->setMarkerSize(14.0);
    pickDot->setName(QStringLiteral("选中点"));
    chart->addSeries(pickDot);
    pickDot->attachAxis(axisX);
    pickDot->attachAxis(axisY);
    pickDot->setVisible(false);

    auto *timeLabel = new QLabel(QStringLiteral("UTC 天内秒：点击轨迹上的点查看"));
    timeLabel->setStyleSheet(QStringLiteral("color:#33475b;font-size:12px;"));

    // 点击轨迹 → 查找最近轨迹点，显示其 UTC 天内秒
    view->setClickHandler([this, timeLabel, pickDot](const QPointF &v) {
        if (m_trajPath.isEmpty()) return;
        int best = 0;
        double bestD = std::numeric_limits<double>::max();
        for (int i = 0; i < m_trajPath.size(); ++i) {
            const QPointF d = m_trajPath.at(i) - v;
            const double dd = d.x() * d.x() + d.y() * d.y();
            if (dd < bestD) { bestD = dd; best = i; }
        }
        pickDot->clear();
        pickDot->append(m_trajPath.at(best));
        pickDot->setVisible(true);
        if (best < m_trajTimes.size() && m_trajTimes.at(best) >= 0)
            timeLabel->setText(QStringLiteral("UTC 天内秒：%1").arg(m_trajTimes.at(best), 0, 'f', 3));
        else
            timeLabel->setText(QStringLiteral("UTC 天内秒：（该文件无 UTC 时间列）"));
    });

    auto *container = new QWidget();
    auto *v = new QVBoxLayout(container);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(4);

    // 播放控制行：开始按钮（绿色点沿轨迹播放）+ UTC 天内秒搜索
    auto *playBtn = new QPushButton(QStringLiteral("开始"), container);
    playBtn->setFixedWidth(72);
    connect(playBtn, &QPushButton::clicked, this, &RawAnalysisPanel::onTrajPlayClicked);

    auto *findEdit = new QLineEdit(container);
    findEdit->setPlaceholderText(QStringLiteral("输入 UTC 天内秒"));
    findEdit->setFixedWidth(140);
    auto *findBtn = new QPushButton(QStringLiteral("定位"), container);
    findBtn->setFixedWidth(56);

    // 输入时间 → 定位到最接近的轨迹点
    auto doLocate = [this, findEdit, timeLabel, pickDot]() {
        bool ok = false;
        const double target = findEdit->text().trimmed().toDouble(&ok);
        if (!ok || m_trajTimes.isEmpty()) {
            timeLabel->setText(QStringLiteral("请输入有效的 UTC 天内秒"));
            return;
        }
        int best = -1;
        double bestD = std::numeric_limits<double>::max();
        for (int i = 0; i < m_trajTimes.size(); ++i) {
            if (m_trajTimes.at(i) < 0) continue;
            const double dd = qAbs(m_trajTimes.at(i) - target);
            if (dd < bestD) { bestD = dd; best = i; }
        }
        if (best < 0 || best >= m_trajPath.size()) {
            timeLabel->setText(QStringLiteral("未找到该时间对应的轨迹点"));
            return;
        }
        pickDot->clear();
        pickDot->append(m_trajPath.at(best));
        pickDot->setVisible(true);
        timeLabel->setText(QStringLiteral("UTC 天内秒：%1（已定位）")
                               .arg(m_trajTimes.at(best), 0, 'f', 3));
    };
    connect(findBtn, &QPushButton::clicked, this, [doLocate]() { doLocate(); });
    connect(findEdit, &QLineEdit::returnPressed, this, [doLocate]() { doLocate(); });

    auto *btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->setSpacing(6);
    btnRow->addWidget(playBtn);
    btnRow->addWidget(timeLabel);
    btnRow->addStretch();
    btnRow->addWidget(findEdit);
    btnRow->addWidget(findBtn);
    v->addLayout(btnRow);

    auto *bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(4);
    bodyLayout->addWidget(view, 1);
    auto *vScrollBar = new QScrollBar(Qt::Vertical);
    bodyLayout->addWidget(vScrollBar);
    v->addLayout(bodyLayout, 1);

    auto *hScrollBar = new QScrollBar(Qt::Horizontal);
    v->addWidget(hScrollBar);
    attachTimeScrollBar(axisX, hScrollBar);
    attachYScrollBar(axisY, vScrollBar);

    m_truthTrajSplitter->addWidget(container);

    auto *placeholder = new QWidget();
    m_truthTrajSplitter->addWidget(placeholder);
    m_truthTrajSplitter->setStretchFactor(0, 4);
    m_truthTrajSplitter->setStretchFactor(1, 1);
    m_truthTrajSplitter->setSizes({500, 120});
}

// ── 轨迹播放：点击“开始”后绿色起点沿轨迹移动 ──
void RawAnalysisPanel::onTrajPlayClicked()
{
    if (m_trajPath.size() < 2 || !m_trajPlayDot) return;

    m_trajPlayPos = 0.0;
    m_trajPlayDot->replace(0, m_trajPath.first());

    if (!m_trajPlayTimer) {
        m_trajPlayTimer = new QTimer(this);
        connect(m_trajPlayTimer, &QTimer::timeout, this, &RawAnalysisPanel::onTrajPlayTick);
    }
    m_trajPlayTimer->start(40);   // 25 fps
}

void RawAnalysisPanel::onTrajPlayTick()
{
    if (m_trajPath.size() < 2 || !m_trajPlayDot) {
        if (m_trajPlayTimer) m_trajPlayTimer->stop();
        return;
    }

    const double total = double(m_trajPath.size()) - 1.0;
    // 播放时长 = 数据真实时长，最长 60 秒；至少 1 秒避免过快
    const double playDur = qMax(1.0, qMin(m_trajDuration, 60.0));
    const double stepPerTick = total * (0.04 / playDur);   // 每帧 40ms 前进量

    m_trajPlayPos += stepPerTick;
    if (m_trajPlayPos >= total) {
        m_trajPlayPos = total;
        m_trajPlayTimer->stop();
    }
    m_trajPlayDot->replace(0, m_trajPath[int(qRound(m_trajPlayPos))]);
}

void RawAnalysisPanel::stopTrajPlay()
{
    if (m_trajPlayTimer) m_trajPlayTimer->stop();
    m_trajPlayDot = nullptr;
    m_trajPath.clear();
    m_trajPlayPos = 0.0;
    m_trajDuration = 0.0;
}

// ═══════════════════════════════════════════════════════════════
//  真值曲线：时间序列共用辅助（多曲线 + 复选框 + 滚动条缩放平移）
// ═══════════════════════════════════════════════════════════════
void RawAnalysisPanel::buildTruthTimeChart(QSplitter *splitter, const QString &title,
                                           const QString &yTitle, const QStringList &labels,
                                           const QVector<QColor> &colors,
                                           const QVector<QVector<QPointF>> &seriesData)
{
    clearChartPage(splitter);

    const int n = seriesData.size();
    if (n == 0) {
        auto *tip = new QLabel(QStringLiteral("暂无数据"));
        tip->setAlignment(Qt::AlignCenter);
        tip->setStyleSheet(QStringLiteral("color:#94a3b8;font-size:13px;"));
        splitter->addWidget(tip);
        return;
    }

    // 计算有效时间范围
    double tMin = 0.0, tMax = 0.0;
    bool hasData = false;
    for (const auto &sd : seriesData) {
        for (const QPointF &p : sd) {
            if (!hasData) { tMin = p.x(); tMax = p.x(); hasData = true; }
            else { tMin = qMin(tMin, p.x()); tMax = qMax(tMax, p.x()); }
        }
    }
    if (!hasData) {
        auto *tip = new QLabel(QStringLiteral("暂无数据"));
        tip->setAlignment(Qt::AlignCenter);
        tip->setStyleSheet(QStringLiteral("color:#94a3b8;font-size:13px;"));
        splitter->addWidget(tip);
        return;
    }

    auto *chart = new QChart();
    chart->setTitle(title);
    chart->setAnimationOptions(QChart::NoAnimation);

    QVector<QLineSeries *> seriesList;
    for (int i = 0; i < n; ++i) {
        auto *series = new QLineSeries(chart);
        series->setName(labels.value(i));
        series->setColor(colors.value(i, QColor(59, 130, 246)));
        for (const QPointF &p : seriesData[i])
            series->append(p.x(), p.y());
        chart->addSeries(series);
        seriesList.append(series);
    }

    auto *axisX = new QValueAxis();
    axisX->setTitleText(QStringLiteral("时间（秒）"));
    axisX->setRange(tMin, tMax);
    chart->addAxis(axisX, Qt::AlignBottom);

    auto *axisY = new QValueAxis();
    axisY->setTitleText(yTitle);
    chart->addAxis(axisY, Qt::AlignLeft);

    for (auto *series : chart->series()) {
        series->attachAxis(axisX);
        series->attachAxis(axisY);
    }
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    auto *view = new ZoomableChartView(chart, axisX, axisY);
    view->setRenderHint(QPainter::Antialiasing, false);
    view->setMinimumHeight(160);

    // 曲线复选框（默认全选，可单选某条曲线）
    auto *checkBar = new QWidget();
    auto *checkLayout = new QHBoxLayout(checkBar);
    checkLayout->setContentsMargins(0, 0, 0, 0);
    checkLayout->setSpacing(12);
    for (int i = 0; i < seriesList.size(); ++i) {
        auto *cb = new QCheckBox(labels.value(i), checkBar);
        cb->setChecked(true);
        QLineSeries *series = seriesList[i];
        connect(cb, &QCheckBox::toggled, this, [series](bool on) {
            series->setVisible(on);
        });
        checkLayout->addWidget(cb);
    }
    checkLayout->addStretch();

    auto *container = new QWidget();
    auto *v = new QVBoxLayout(container);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(4);
    v->addWidget(checkBar);

    auto *bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(4);
    bodyLayout->addWidget(view, 1);
    auto *vScrollBar = new QScrollBar(Qt::Vertical);
    bodyLayout->addWidget(vScrollBar);
    v->addLayout(bodyLayout, 1);

    auto *hScrollBar = new QScrollBar(Qt::Horizontal);
    v->addWidget(hScrollBar);
    attachTimeScrollBar(axisX, hScrollBar);
    attachYScrollBar(axisY, vScrollBar);

    splitter->addWidget(container);

    auto *placeholder = new QWidget();
    splitter->addWidget(placeholder);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({500, 120});
}

// ═══════════════════════════════════════════════════════════════
//  真值曲线：速度（Vel-N / Vel-E / Vel-U）
// ═══════════════════════════════════════════════════════════════
void RawAnalysisPanel::buildTruthVelChart(const TruthSummary &s)
{
    const auto ds = downSample(s.samples, 10.0);
    QVector<QVector<QPointF>> data(4);
    for (int i = 0; i < 4; ++i) data[i].reserve(ds.size());
    for (const TruthSample &sp : ds) {
        data[0].append(QPointF(sp.t, sp.velN));
        data[1].append(QPointF(sp.t, sp.velE));
        data[2].append(QPointF(sp.t, sp.velU));
        // 平面速度 = sqrt(N² + E²)
        data[3].append(QPointF(sp.t, qSqrt(sp.velN * sp.velN + sp.velE * sp.velE)));
    }
    buildTruthTimeChart(m_truthVelSplitter,
                        QStringLiteral("速度 / 时间"),
                        QStringLiteral("速度（m/s）"),
                        {QStringLiteral("Vel-N"), QStringLiteral("Vel-E"), QStringLiteral("Vel-U"),
                         QStringLiteral("水平速度")},
                        {QColor(59, 130, 246), QColor(239, 68, 68), QColor(34, 197, 94),
                         QColor(234, 88, 12)},
                        data);
}

// ═══════════════════════════════════════════════════════════════
//  真值曲线：位置与高程（纬度/经度左轴，椭球高右轴）
// ═══════════════════════════════════════════════════════════════
void RawAnalysisPanel::buildTruthPosChart(const TruthSummary &s)
{
    if (s.samples.isEmpty()) {
        clearChartPage(m_truthPosSplitter);
        auto *tip = new QLabel(QStringLiteral("暂无位置数据"));
        tip->setAlignment(Qt::AlignCenter);
        tip->setStyleSheet(QStringLiteral("color:#94a3b8;font-size:13px;"));
        m_truthPosSplitter->addWidget(tip);
        return;
    }

    // 纬度/经度直接用原始十进制度显示（各子图 Y 轴独立，无需换算为米）
    const auto ds = downSample(s.samples, 10.0);

    QVector<QVector<QPointF>> data(3);
    for (int i = 0; i < 3; ++i) data[i].reserve(ds.size());
    for (const TruthSample &sp : ds) {
        data[0].append(QPointF(sp.t, sp.lat));
        data[1].append(QPointF(sp.t, sp.lon));
        data[2].append(QPointF(sp.t, sp.h));
    }

    buildTruthStackedCharts(m_truthPosSplitter,
                            {QStringLiteral("纬度 / 时间"), QStringLiteral("经度 / 时间"), QStringLiteral("椭球高 / 时间")},
                            {QStringLiteral("纬度（deg）"), QStringLiteral("经度（deg）"), QStringLiteral("高程（m）")},
                            {QStringLiteral("纬度"), QStringLiteral("经度"), QStringLiteral("椭球高")},
                            {QColor(59, 130, 246), QColor(239, 68, 68), QColor(34, 197, 94)},
                            data);
}

// ═══════════════════════════════════════════════════════════════
//  真值曲线：垂直堆叠多子图（每个指标一个独立小图，各自 X 轴/滚动条）
// ═══════════════════════════════════════════════════════════════
void RawAnalysisPanel::buildTruthStackedCharts(QSplitter *splitter, const QStringList &titles,
                                               const QStringList &yTitles, const QStringList &names,
                                               const QVector<QColor> &colors,
                                               const QVector<QVector<QPointF>> &data,
                                               const QVector<int> &heights)
{
    clearChartPage(splitter);

    const int n = data.size();
    if (n == 0) {
        auto *tip = new QLabel(QStringLiteral("暂无数据"));
        tip->setAlignment(Qt::AlignCenter);
        tip->setStyleSheet(QStringLiteral("color:#94a3b8;font-size:13px;"));
        splitter->addWidget(tip);
        return;
    }

    // 全局时间范围（各子图 X 轴保持一致）
    double tMin = 0.0, tMax = 0.0;
    bool hasData = false;
    for (const auto &sd : data) {
        for (const QPointF &p : sd) {
            if (!hasData) { tMin = p.x(); tMax = p.x(); hasData = true; }
            else { tMin = qMin(tMin, p.x()); tMax = qMax(tMax, p.x()); }
        }
    }
    if (!hasData) {
        auto *tip = new QLabel(QStringLiteral("暂无数据"));
        tip->setAlignment(Qt::AlignCenter);
        tip->setStyleSheet(QStringLiteral("color:#94a3b8;font-size:13px;"));
        splitter->addWidget(tip);
        return;
    }

    auto *container = new QWidget();
    auto *v = new QVBoxLayout(container);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(2);

    QVector<QValueAxis*> xAxes;
    xAxes.reserve(n);

    for (int i = 0; i < n; ++i) {
        auto *chart = new QChart();
        chart->setTitle(titles.value(i));
        chart->setAnimationOptions(QChart::NoAnimation);

        auto *series = new QLineSeries(chart);
        series->setName(names.value(i));
        series->setColor(colors.value(i, QColor(59, 130, 246)));
        for (const QPointF &p : data[i])
            series->append(p.x(), p.y());
        chart->addSeries(series);

        auto *axisX = new QValueAxis();
        axisX->setRange(tMin, tMax);
        chart->addAxis(axisX, Qt::AlignBottom);

        auto *axisY = new QValueAxis();
        axisY->setTitleText(yTitles.value(i));
        chart->addAxis(axisY, Qt::AlignLeft);

        series->attachAxis(axisX);
        series->attachAxis(axisY);
        chart->legend()->setVisible(false);

        // 共用底部一个时间轴：仅最底部子图显示 X 轴标签与标题
        if (i < n - 1) {
            axisX->setLabelsVisible(false);
            axisX->setTitleVisible(false);
        } else {
            axisX->setTitleText(QStringLiteral("时间（秒）"));
        }

        auto *view = new ZoomableChartView(chart, axisX, axisY);
        view->setRenderHint(QPainter::Antialiasing, false);
        view->setMinimumHeight(120);

        v->addWidget(view, heights.value(i, 1));
        xAxes.append(axisX);
    }

    // 底部一个横向滚动条，控制所有子图 X 轴（共用时间轴）
    auto *hScrollBar = new QScrollBar(Qt::Horizontal);
    v->addWidget(hScrollBar);

    // X 轴联动：任一子图缩放/平移，同步其余子图
    auto syncing = std::make_shared<bool>(false);
    for (int a = 0; a < xAxes.size(); ++a) {
        QObject::connect(xAxes[a], &QValueAxis::rangeChanged, this,
                         [xAxes, syncing, a](qreal lo, qreal hi) {
            if (*syncing) return;
            *syncing = true;
            for (int b = 0; b < xAxes.size(); ++b) {
                if (b == a) continue;
                xAxes[b]->setRange(lo, hi);
            }
            *syncing = false;
        });
    }

    // 滚动条绑定主 X 轴（第一子图）
    attachTimeScrollBar(xAxes.first(), hScrollBar);

    splitter->addWidget(container);

    auto *placeholder = new QWidget();
    splitter->addWidget(placeholder);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({500, 120});
}

// ═══════════════════════════════════════════════════════════════
//  真值曲线：姿态（Roll / Pitch / Heading，三个独立子图）
// ═══════════════════════════════════════════════════════════════
void RawAnalysisPanel::buildTruthAttChart(const TruthSummary &s)
{
    if (s.samples.isEmpty()) {
        clearChartPage(m_truthAttSplitter);
        auto *tip = new QLabel(QStringLiteral("暂无姿态数据"));
        tip->setAlignment(Qt::AlignCenter);
        tip->setStyleSheet(QStringLiteral("color:#94a3b8;font-size:13px;"));
        m_truthAttSplitter->addWidget(tip);
        return;
    }

    const auto ds = downSample(s.samples, 10.0);

    QVector<QVector<QPointF>> data(3);
    for (int i = 0; i < 3; ++i) data[i].reserve(ds.size());

    // 航向角原始值本就在 0~360°，直接显示原始值，不做解缠（避免累加出 ±360 的异常值）
    for (const TruthSample &sp : ds) {
        data[0].append(QPointF(sp.t, sp.roll));
        data[1].append(QPointF(sp.t, sp.pitch));
        data[2].append(QPointF(sp.t, sp.heading));
    }

    buildTruthStackedCharts(m_truthAttSplitter,
                            {QStringLiteral("Roll / 时间"), QStringLiteral("Pitch / 时间"), QStringLiteral("Heading / 时间")},
                            {QStringLiteral("角度（deg）"), QStringLiteral("角度（deg）"), QStringLiteral("角度（deg）")},
                            {QStringLiteral("Roll"), QStringLiteral("Pitch"), QStringLiteral("Heading")},
                            {QColor(59, 130, 246), QColor(239, 68, 68), QColor(34, 197, 94)},
                            data,
                            {1, 1, 2});
}

// ═══════════════════════════════════════════════════════════════
//  真值曲线：使用卫星数
// ═══════════════════════════════════════════════════════════════
void RawAnalysisPanel::buildTruthSatChart(const TruthSummary &s)
{
    clearChartPage(m_truthSatSplitter);

    if (s.samples.isEmpty()) {
        auto *tip = new QLabel(QStringLiteral("暂无卫星数数据"));
        tip->setAlignment(Qt::AlignCenter);
        tip->setStyleSheet(QStringLiteral("color:#94a3b8;font-size:13px;"));
        m_truthSatSplitter->addWidget(tip);
        return;
    }

    const auto ds = downSample(s.samples, 10.0);

    int maxSat = 0;
    for (const TruthSample &sp : ds)
        maxSat = qMax(maxSat, sp.satUse);

    auto *chart = new QChart();
    chart->setTitle(QStringLiteral("使用卫星数 / 时间"));
    chart->setAnimationOptions(QChart::NoAnimation);

    auto *series = new QLineSeries(chart);
    series->setName(QStringLiteral("使用卫星数"));
    series->setColor(QColor(59, 130, 246));
    for (const TruthSample &sp : ds)
        series->append(sp.t, sp.satUse);
    chart->addSeries(series);

    auto *axisX = new QValueAxis();
    axisX->setTitleText(QStringLiteral("时间（秒）"));
    axisX->setRange(0, ds.last().t);
    chart->addAxis(axisX, Qt::AlignBottom);

    auto *axisY = new QValueAxis();
    axisY->setTitleText(QStringLiteral("卫星数"));
    axisY->setRange(0, maxSat + 1);
    axisY->setLabelFormat("%d");          // 卫星数为整数
    axisY->setTickInterval(maxSat > 20 ? 2.0 : 1.0);
    chart->addAxis(axisY, Qt::AlignLeft);

    series->attachAxis(axisX);
    series->attachAxis(axisY);
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    auto *view = new ZoomableChartView(chart, axisX, axisY);
    view->setRenderHint(QPainter::Antialiasing, false);
    view->setMinimumHeight(160);

    auto *container = new QWidget();
    auto *v = new QVBoxLayout(container);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(4);

    auto *bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(4);
    bodyLayout->addWidget(view, 1);
    auto *vScrollBar = new QScrollBar(Qt::Vertical);
    bodyLayout->addWidget(vScrollBar);
    v->addLayout(bodyLayout, 1);

    auto *hScrollBar = new QScrollBar(Qt::Horizontal);
    v->addWidget(hScrollBar);
    attachTimeScrollBar(axisX, hScrollBar);
    attachYScrollBar(axisY, vScrollBar);

    m_truthSatSplitter->addWidget(container);

    auto *placeholder = new QWidget();
    m_truthSatSplitter->addWidget(placeholder);
    m_truthSatSplitter->setStretchFactor(0, 4);
    m_truthSatSplitter->setStretchFactor(1, 1);
    m_truthSatSplitter->setSizes({500, 120});
}

// ═══════════════════════════════════════════════════════════════
//  降采样：按目标频率等间隔抽稀，用于缓解高频大数据量绘制卡顿
// ═══════════════════════════════════════════════════════════════
QVector<TruthSample> RawAnalysisPanel::downSample(const QVector<TruthSample> &samples,
                                                  double targetHz) const
{
    if (samples.size() < 2 || targetHz <= 0.0)
        return samples;

    // 估算原始采样率；若已 <= 目标采样率则无需降采样
    const double firstT = samples.first().t;
    const double lastT = samples.last().t;
    const double srcHz = (lastT > firstT)
                             ? double(samples.size() - 1) / (lastT - firstT)
                             : 0.0;
    if (srcHz <= targetHz)
        return samples;

    const double interval = 1.0 / targetHz;
    QVector<TruthSample> out;
    out.reserve(int(double(samples.size()) * targetHz / srcHz) + 2);

    double lastKept = -1e18;
    for (const TruthSample &sp : samples) {
        if (sp.t - lastKept >= interval) {
            out.append(sp);
            lastKept = sp.t;
        }
    }
    return out;
}

} // namespace Analysis
