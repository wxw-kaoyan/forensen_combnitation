#include "rawanalysispanel.h"
#include "rinexreader.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>

namespace Analysis {

RawAnalysisPanel::RawAnalysisPanel(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

void RawAnalysisPanel::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    // ── 顶部：文件选择 ──
    auto *fileRow = new QHBoxLayout();
    fileRow->setSpacing(8);
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(QStringLiteral("选择 RINEX 观测文件（.O / .26O / .obs）"));
    m_browseBtn = new QPushButton(QStringLiteral("浏览..."), this);
    m_analyzeBtn = new QPushButton(QStringLiteral("开始分析"), this);
    fileRow->addWidget(m_pathEdit, 1);
    fileRow->addWidget(m_browseBtn);
    fileRow->addWidget(m_analyzeBtn);
    root->addLayout(fileRow);

    connect(m_browseBtn, &QPushButton::clicked, this, &RawAnalysisPanel::onBrowse);
    connect(m_analyzeBtn, &QPushButton::clicked, this, &RawAnalysisPanel::onAnalyze);

    // ── 中部：概览 ──
    m_summaryLabel = new QLabel(QStringLiteral("尚未分析"), this);
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_summaryLabel->setStyleSheet(
        QStringLiteral("background:#f7f9fb;border:1px solid #dde4ec;border-radius:6px;"
                       "padding:14px;font-size:13px;color:#33475b;"));
    root->addWidget(m_summaryLabel);

    // ── 底部：SNR 表 ──
    auto *snrTitle = new QLabel(QStringLiteral("信噪比 SNR（dBHz）"), this);
    snrTitle->setStyleSheet(QStringLiteral("font-weight:600;font-size:13px;color:#12263f;"));
    root->addWidget(snrTitle);

    m_snrTable = new QTableWidget(this);
    m_snrTable->setColumnCount(5);
    m_snrTable->setHorizontalHeaderLabels(
        {QStringLiteral("系统"), QStringLiteral("频点"), QStringLiteral("最小值"),
         QStringLiteral("最大值"), QStringLiteral("均值")});
    m_snrTable->horizontalHeader()->setStretchLastSection(true);
    m_snrTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_snrTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_snrTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_snrTable->setAlternatingRowColors(true);
    root->addWidget(m_snrTable, 1);
}

void RawAnalysisPanel::onBrowse()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 RINEX 观测文件"), QString(),
        QStringLiteral("RINEX 观测文件 (*.O *.o *.obs *.rnx);;所有文件 (*.*)"));
    if (!path.isEmpty())
        m_pathEdit->setText(path);
}

void RawAnalysisPanel::onAnalyze()
{
    const QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择 RINEX 观测文件"));
        return;
    }

    RinexSummary s;
    QString err;
    if (!RinexReader::analyze(path, &s, &err)) {
        QMessageBox::warning(this, QStringLiteral("分析失败"), err);
        return;
    }

    showSummary(s);
}

void RawAnalysisPanel::showSummary(const RinexSummary &s)
{
    // ── 概览文本 ──
    QString sysSummary;
    {
        const int masks[] = {Sys::GPS, Sys::GLO, Sys::GAL, Sys::QZS, Sys::BDS};
        for (int m : masks) {
            if (!(s.systemMask & m)) continue;
            const int nSat = s.satsPerSys.value(m).size();
            const int nMiss = s.missingSatsPerSys.value(m).size();
            sysSummary += QStringLiteral("<b>%1</b>：%2 颗卫星")
                              .arg(RinexSummary::sysName(m)).arg(nSat);
            if (nMiss > 0)
                sysSummary += QStringLiteral("（掉星 %1）").arg(nMiss);
            sysSummary += QStringLiteral("<br/>");
        }
    }

    m_summaryLabel->setText(
        QStringLiteral(
            "<p style='font-size:15px;font-weight:700;color:#12263f;margin-bottom:8px;'>观测文件质量摘要</p>"
            "<table cellspacing='0' cellpadding='2' style='font-size:13px;color:#33475b;'>"
            "<tr><td>观测记录总数</td><td><b>%1</b></td></tr>"
            "<tr><td>历元总数</td><td><b>%2</b></td></tr>"
            "<tr><td>时长</td><td><b>%3 秒</b></td></tr>"
            "<tr><td>采样率</td><td><b>%4 Hz</b></td></tr>"
            "<tr><td>丢历元数</td><td><b>%5</b>（总间断 %6 秒）</td></tr>"
            "</table>"
            "<p style='font-size:13px;font-weight:600;color:#12263f;margin:10px 0 4px;'>系统构成</p>"
            "%7")
            .arg(s.totalObsRecords)
            .arg(s.totalEpochs)
            .arg(s.durationSec, 0, 'f', 1)
            .arg(s.sampleRate, 0, 'f', 2)
            .arg(s.epochGapCount)
            .arg(s.totalGapSec, 0, 'f', 1)
            .arg(sysSummary));

    // ── SNR 表 ──
    m_snrTable->setRowCount(0);
    int row = 0;
    const int masks[] = {Sys::GPS, Sys::GLO, Sys::GAL, Sys::QZS, Sys::BDS};
    for (int m : masks) {
        if (!s.snrPerSysCode.contains(m)) continue;
        const auto &codeMap = s.snrPerSysCode[m];
        for (auto it = codeMap.constBegin(); it != codeMap.constEnd(); ++it) {
            const SnrStat &st = it.value();
            m_snrTable->insertRow(row);
            m_snrTable->setItem(row, 0, new QTableWidgetItem(RinexSummary::sysName(m)));
            m_snrTable->setItem(row, 1, new QTableWidgetItem(it.key()));
            m_snrTable->setItem(row, 2, new QTableWidgetItem(QString::number(st.min, 'f', 1)));
            m_snrTable->setItem(row, 3, new QTableWidgetItem(QString::number(st.max, 'f', 1)));
            m_snrTable->setItem(row, 4, new QTableWidgetItem(QString::number(st.mean(), 'f', 1)));
            ++row;
        }
    }
}

} // namespace Analysis
