#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "modules/ntrip/ntrippagemanager.h"
#include "modules/ntrip/forwardertablemanager.h"
#include "modules/commtool/commtool.h"
#include "modules/analysis/analysispanel.h"
#include "modules/parse/parsepanel.h"
#include "common/toolprocess.h"
#include "common/toolprocessmanager.h"
#include "common/uiinitializer.h"
#include <QVBoxLayout>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    UiInitializer::apply(this);  //初始化窗口尺寸+样式

    // ---- NTRIP 页面（独立模块：配置 / FixGGA / 转发表 / 保存数据） ----
    auto *ftm = new ForwarderTableManager(
        ui->ntripForwardTableWidget,
        ui->addTcpTargetButton,
        ui->addSerialTargetButton,
        this);

    m_ntripPage = new NtripPageManager(
        {ui->ntripConfigButton, ui->ntripLogEdit,
         ui->ntripConnectStatusLabel, ui->ntripRateLabel,
         ui->fixGgaCheckBox, ui->fixGgaFrame,
         ui->fixLonEdit, ui->fixLatEdit, ui->fixHeightEdit,
         ui->horizontalLayout_7},
        ftm, this);

    // ---- Comm Tool ----
    {
        ui->commMainHLayout->setStretch(0, 0);
        ui->commMainHLayout->setStretch(1, 1);
        ui->connectionModeComboBox->setCurrentIndex(0);
        m_commTool = new CommTool(ui, this);
    }

    // ---- 数据解析面板 ----
    m_parsePanel = new ParsePanel(ui->parseTab);
    ui->verticalLayout_5->addWidget(m_parsePanel);

    // ---- 数据分析：原始数据分析 + 对比分析（两个顶层页面） ----
    {
        // 原始数据分析（壳子，待开发）
        QVBoxLayout *rawLayout = ui->verticalLayout_rawAnalysis;
        rawLayout->setContentsMargins(40, 40, 40, 40);
        QLabel *rawHint = new QLabel(
            QStringLiteral("<html><head/><body>"
                           "<p style='font-size:16px;font-weight:700;color:#12263f;margin-bottom:12px;'>原始数据分析</p>"
                           "<p style='font-size:13px;color:#617487;line-height:1.8;'>"
                           "功能待开发，主要分析内容：<br/>"
                           "· 数据完整性检查（帧丢失、断点数统计）<br/>"
                           "· 各协议帧频与分布统计<br/>"
                           "· CRC 校验错误检测<br/>"
                           "· 数据连续性与时序异常标记<br/>"
                           "· 原始观测值质量（卫星数、SNR、RTK 固定率）<br/>"
                           "</p></body></html>"), ui->rawAnalysisTab);
        rawHint->setWordWrap(true);
        rawLayout->addWidget(rawHint);
        rawLayout->addStretch();

        // 对比分析（AnalysisPanel）
        m_analysisPanel = new Analysis::AnalysisPanel(ui->compareAnalysisTab);
        ui->verticalLayout_compareAnalysis->addWidget(m_analysisPanel);
    }

    // ---- 常用工具 ----
    m_toolManager = new ToolProcessManager(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}
