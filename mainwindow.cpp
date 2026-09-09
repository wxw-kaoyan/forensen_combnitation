#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "modules/ntrip/ntrippagemanager.h"
#include "modules/ntrip/forwardertablemanager.h"
#include "modules/commtool/commtool.h"
#include "modules/analysis/compare/analysispanel.h"
#include "modules/analysis/raw/rawanalysispanel.h"
#include "modules/parse/parsepanel.h"
#include "modules/simulator/simulatorpanel.h"
#include "modules/ota/ui/otapanel.h"
#include "common/toolprocess.h"
#include "common/toolprocessmanager.h"
#include "common/uiinitializer.h"

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
        // 原始数据分析（RINEX 观测文件质量检核）
        auto *rawPanel = new Analysis::RawAnalysisPanel(ui->rawAnalysisTab);
        ui->verticalLayout_rawAnalysis->addWidget(rawPanel);

        // 对比分析（AnalysisPanel）
        m_analysisPanel = new Analysis::AnalysisPanel(ui->compareAnalysisTab);
        ui->verticalLayout_compareAnalysis->addWidget(m_analysisPanel);
    }

    // ---- PPS + 数据模拟面板 ----
    m_simulatorPanel = new SimulatorPanel(ui->simulatorTab);
    ui->verticalLayout_simulator->addWidget(m_simulatorPanel);

    // ---- OTA 升级面板（独立模块：串口 / TCP 固件升级） ----
    {
        auto *otaPanel = new OtaPanel(ui->otaTab);
        ui->verticalLayout_ota->addWidget(otaPanel);
    }

    // ---- 常用工具 ----
    m_toolManager = new ToolProcessManager(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}
