#include "modules/simulator/simulatorpanel.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QFile>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include "modules/simulator/datasimulator.h"
#include "modules/simulator/ppsgenerator.h"
#include "modules/simulator/simprotocol.h"
#include "modules/simulator/simprotocolregistry.h"

namespace {

QComboBox *makeBaudCombo(QWidget *parent, const QString &currentText)
{
    auto *combo = new QComboBox(parent);
    const QStringList rates = {QStringLiteral("9600"),  QStringLiteral("19200"),
                               QStringLiteral("38400"), QStringLiteral("57600"),
                               QStringLiteral("115200"), QStringLiteral("230400"),
                               QStringLiteral("460800"), QStringLiteral("921600")};
    combo->addItems(rates);
    combo->setCurrentText(currentText);
    return combo;
}

} // namespace

SimulatorPanel::SimulatorPanel(QWidget *parent)
    : QWidget(parent)
{
    m_pps  = new PpsGenerator(this);
    m_data = new DataSimulator(this);

    // PPS 整秒对齐时推进数据模拟器时间基准
    connect(m_pps, &PpsGenerator::ppsTick, m_data, &DataSimulator::onPpsTick);

    connect(m_pps, &PpsGenerator::errorOccurred, this,
            [this](const QString &msg) { log(msg); });
    connect(m_data, &DataSimulator::errorOccurred, this,
            [this](const QString &msg) { log(msg); });

    buildUi();
    refreshPorts();
    reloadProtocols();
}

SimulatorPanel::~SimulatorPanel()
{
    stopSimulation();
    qDeleteAll(m_displayProtocols);
    m_displayProtocols.clear();
}

void SimulatorPanel::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    // 标题
    auto *title = new QLabel(QStringLiteral("PPS + 数据模拟"), this);
    title->setStyleSheet(QStringLiteral("font-size:15px;font-weight:700;color:#12263f;"));
    root->addWidget(title);

    auto *desc = new QLabel(
        QStringLiteral("一个串口输出 PPS 上升沿(5ms)，另一个串口按协议库输出数据。协议固定值在 sim_protocols.json 中修改，无需重新编译。"),
        this);
    desc->setStyleSheet(QStringLiteral("font-size:12px;color:#617487;"));
    desc->setWordWrap(true);
    root->addWidget(desc);

    // 串口配置区
    auto *portGroup = new QGroupBox(QStringLiteral("串口配置"), this);
    auto *portLayout = new QGridLayout(portGroup);
    portLayout->setHorizontalSpacing(8);
    portLayout->setVerticalSpacing(6);

    portLayout->addWidget(new QLabel(QStringLiteral("PPS 串口"), portGroup), 0, 0);
    m_ppsPortCombo = new QComboBox(portGroup);
    m_ppsPortCombo->setMinimumWidth(140);
    portLayout->addWidget(m_ppsPortCombo, 0, 1);
    portLayout->addWidget(new QLabel(QStringLiteral("波特率"), portGroup), 0, 2);
    m_ppsBaudCombo = makeBaudCombo(portGroup, QStringLiteral("115200"));
    portLayout->addWidget(m_ppsBaudCombo, 0, 3);

    portLayout->addWidget(new QLabel(QStringLiteral("数据串口"), portGroup), 1, 0);
    m_dataPortCombo = new QComboBox(portGroup);
    m_dataPortCombo->setMinimumWidth(140);
    portLayout->addWidget(m_dataPortCombo, 1, 1);
    portLayout->addWidget(new QLabel(QStringLiteral("波特率"), portGroup), 1, 2);
    m_dataBaudCombo = makeBaudCombo(portGroup, QStringLiteral("115200"));
    portLayout->addWidget(m_dataBaudCombo, 1, 3);

    m_refreshButton = new QPushButton(QStringLiteral("刷新串口"), portGroup);
    portLayout->addWidget(m_refreshButton, 0, 4);
    connect(m_refreshButton, &QPushButton::clicked, this, &SimulatorPanel::refreshPorts);

    root->addWidget(portGroup);

    // 协议选择区
    auto *protoGroup = new QGroupBox(QStringLiteral("模拟协议"), this);
    auto *protoLayout = new QVBoxLayout(protoGroup);

    auto *protoBtnRow = new QHBoxLayout;
    m_reloadButton = new QPushButton(QStringLiteral("重新加载 JSON"), protoGroup);
    m_selectAllButton = new QPushButton(QStringLiteral("全选"), protoGroup);
    m_deselectAllButton = new QPushButton(QStringLiteral("全不选"), protoGroup);
    protoBtnRow->addWidget(m_reloadButton);
    protoBtnRow->addWidget(m_selectAllButton);
    protoBtnRow->addWidget(m_deselectAllButton);
    protoBtnRow->addStretch(1);
    protoLayout->addLayout(protoBtnRow);

    connect(m_reloadButton, &QPushButton::clicked, this, &SimulatorPanel::reloadProtocols);
    connect(m_selectAllButton, &QPushButton::clicked, this, &SimulatorPanel::selectAll);
    connect(m_deselectAllButton, &QPushButton::clicked, this, &SimulatorPanel::deselectAll);

    m_protocolTable = new QTableWidget(protoGroup);
    m_protocolTable->setColumnCount(4);
    m_protocolTable->setHorizontalHeaderLabels(
        {QStringLiteral("启用"), QStringLiteral("协议"), QStringLiteral("格式"),
         QStringLiteral("GPS时间")});
    m_protocolTable->horizontalHeader()->setStretchLastSection(true);
    m_protocolTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_protocolTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_protocolTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_protocolTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_protocolTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_protocolTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_protocolTable->verticalHeader()->setVisible(false);
    m_protocolTable->setMinimumHeight(160);
    protoLayout->addWidget(m_protocolTable);

    root->addWidget(protoGroup);

    // 启停 + 状态
    auto *ctrlRow = new QHBoxLayout;
    m_startButton = new QPushButton(QStringLiteral("启动模拟"), this);
    m_startButton->setMinimumHeight(42);
    m_startButton->setStyleSheet(
        QStringLiteral("QPushButton { background:#2980b9; color:white; border-radius:6px; font-weight:600; }"
                       "QPushButton:hover { background:#3498db; }"));
    m_statusLabel = new QLabel(QStringLiteral("未启动"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("font-size:13px;color:#42556d;font-weight:600;"));

    auto *rateLabel = new QLabel(QStringLiteral("频率(Hz)"), this);
    m_rateSpin = new QSpinBox(this);
    m_rateSpin->setRange(1, 100);
    m_rateSpin->setValue(1);
    m_rateSpin->setSuffix(QStringLiteral(" Hz"));

    ctrlRow->addWidget(m_startButton);
    ctrlRow->addWidget(rateLabel);
    ctrlRow->addWidget(m_rateSpin);
    ctrlRow->addWidget(m_statusLabel);
    ctrlRow->addStretch(1);
    root->addLayout(ctrlRow);

    connect(m_startButton, &QPushButton::clicked, this, &SimulatorPanel::onStartStop);

    // 日志
    m_logEdit = new QPlainTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumHeight(120);
    m_logEdit->setFont(QFont(QStringLiteral("Consolas"), 10));
    root->addWidget(m_logEdit);

    root->addStretch(1);
}

void SimulatorPanel::populatePorts(QComboBox *combo)
{
    if (!combo)
        return;

    const QString current = combo->currentText();
    combo->clear();
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos)
        combo->addItem(info.portName());

    if (combo->count() == 0)
        combo->addItem(QStringLiteral("(无可用串口)"));

    const int idx = combo->findText(current);
    if (idx >= 0)
        combo->setCurrentIndex(idx);
}

void SimulatorPanel::refreshPorts()
{
    populatePorts(m_ppsPortCombo);
    populatePorts(m_dataPortCombo);
    log(QStringLiteral("已刷新串口列表"));
}

QString SimulatorPanel::jsonPath() const
{
    const QString inApp = QCoreApplication::applicationDirPath() + QStringLiteral("/sim_protocols.json");
    if (QFile::exists(inApp))
        return inApp;

    const QString cwd = QStringLiteral("sim_protocols.json");
    if (QFile::exists(cwd))
        return cwd;

    return inApp; // 默认返回，加载失败时提示
}

void SimulatorPanel::reloadProtocols()
{
    qDeleteAll(m_displayProtocols);
    m_displayProtocols.clear();
    m_protocolTable->setRowCount(0);

    m_displayProtocols = loadSimProtocols(jsonPath());

    if (m_displayProtocols.isEmpty()) {
        log(QStringLiteral("未加载到任何协议，请检查 %1").arg(jsonPath()));
        return;
    }

    m_protocolTable->setRowCount(m_displayProtocols.size());
    for (int i = 0; i < m_displayProtocols.size(); ++i) {
        const SimProtocol *p = m_displayProtocols.at(i);

        auto *chk = new QTableWidgetItem;
        chk->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        chk->setCheckState(p->enabled ? Qt::Checked : Qt::Unchecked);
        m_protocolTable->setItem(i, 0, chk);

        auto *nameItem = new QTableWidgetItem(p->name);
        m_protocolTable->setItem(i, 1, nameItem);

        auto *fmtItem = new QTableWidgetItem(p->format);
        m_protocolTable->setItem(i, 2, fmtItem);

        auto *timeItem = new QTableWidgetItem(p->hasGpsTime ? QStringLiteral("有") : QStringLiteral("无"));
        m_protocolTable->setItem(i, 3, timeItem);
    }

    log(QStringLiteral("已加载 %1 个协议").arg(m_displayProtocols.size()));
}

void SimulatorPanel::selectAll()
{
    for (int i = 0; i < m_protocolTable->rowCount(); ++i)
        if (auto *item = m_protocolTable->item(i, 0))
            item->setCheckState(Qt::Checked);
}

void SimulatorPanel::deselectAll()
{
    for (int i = 0; i < m_protocolTable->rowCount(); ++i)
        if (auto *item = m_protocolTable->item(i, 0))
            item->setCheckState(Qt::Unchecked);
}

void SimulatorPanel::onStartStop()
{
    if (m_pps->isRunning() || m_data->isRunning()) {
        stopSimulation();
        return;
    }

    // 依据面板勾选状态，从 JSON 重新创建选中协议（数据模拟器持有其所有权）
    QList<SimProtocol *> selected = loadSimProtocols(jsonPath());
    for (int i = 0; i < m_protocolTable->rowCount(); ++i) {
        const QString name = m_protocolTable->item(i, 1)->text();
        const bool checked = m_protocolTable->item(i, 0)->checkState() == Qt::Checked;
        if (!checked) {
            for (int j = 0; j < selected.size();) {
                if (selected.at(j)->name == name) {
                    delete selected.at(j);
                    selected.removeAt(j);
                } else {
                    ++j;
                }
            }
        }
    }

    if (selected.isEmpty()) {
        log(QStringLiteral("请至少勾选一个协议"));
        return;
    }

    // 全局频率覆盖各协议默认频率
    const int rateHz = m_rateSpin->value();
    for (SimProtocol *p : selected)
        p->rateHz = rateHz;

    const QString ppsPort  = m_ppsPortCombo->currentText();
    const QString dataPort = m_dataPortCombo->currentText();
    if (ppsPort == dataPort) {
        log(QStringLiteral("PPS 串口与数据串口不能相同"));
        qDeleteAll(selected);
        return;
    }

    m_data->setProtocols(selected);

    bool ok = true;
    ok = m_pps->start(ppsPort, m_ppsBaudCombo->currentText().toInt()) && ok;
    ok = m_data->start(dataPort, m_dataBaudCombo->currentText().toInt()) && ok;

    if (ok) {
        m_startButton->setText(QStringLiteral("停止模拟"));
        m_statusLabel->setText(QStringLiteral("运行中"));
        log(QStringLiteral("已启动：PPS=%1 数据=%2 协议=%3个 频率=%4Hz")
                .arg(ppsPort, dataPort)
                .arg(selected.size())
                .arg(rateHz));
    } else {
        // 部分启动失败时，统一停止
        stopSimulation();
    }
}

void SimulatorPanel::stopSimulation()
{
    m_pps->stop();
    m_data->stop();
    m_startButton->setText(QStringLiteral("启动模拟"));
    m_statusLabel->setText(QStringLiteral("已停止"));
}

void SimulatorPanel::log(const QString &msg)
{
    m_logEdit->appendPlainText(QStringLiteral("[模拟] %1").arg(msg));
}
