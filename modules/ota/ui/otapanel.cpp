#include "modules/ota/ui/otapanel.h"
#include "modules/ota/core/otaprotocol.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QProgressBar>
#include <QTextEdit>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QThread>
#include <QDateTime>

OtaPanel::OtaPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);

    // ── 链路设置 ──
    auto *linkGroup = new QGroupBox(QStringLiteral("链路设置"), this);
    auto *linkLay = new QGridLayout(linkGroup);

    m_linkCombo = new QComboBox;
    m_linkCombo->addItem(QStringLiteral("串口"),   int(OtaWorker::Transport::Serial));
    m_linkCombo->addItem(QStringLiteral("TCP 网络"), int(OtaWorker::Transport::Tcp));
    linkLay->addWidget(new QLabel(QStringLiteral("链路:")), 0, 0);
    linkLay->addWidget(m_linkCombo, 0, 1);

    // 串口行
    m_serialBox = new QWidget;
    auto *serLay = new QHBoxLayout(m_serialBox);
    serLay->setContentsMargins(0, 0, 0, 0);
    m_portCombo = new QComboBox;
    m_portCombo->setMinimumWidth(150);
    auto *refreshBtn = new QPushButton(QStringLiteral("刷新串口"));
    m_baudCombo = new QComboBox;
    m_baudCombo->setEditable(true);
    for (int b : {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600})
        m_baudCombo->addItem(QString::number(b));
    m_baudCombo->setCurrentText(QString::number(OtaProtocol::kBootBaud));
    serLay->addWidget(new QLabel(QStringLiteral("串口:")));
    serLay->addWidget(m_portCombo);
    serLay->addWidget(refreshBtn);
    serLay->addWidget(new QLabel(QStringLiteral("波特率:")));
    serLay->addWidget(m_baudCombo);
    serLay->addStretch();
    linkLay->addWidget(m_serialBox, 1, 0, 1, 4);

    // TCP 行
    m_tcpBox = new QWidget;
    auto *tcpLay = new QHBoxLayout(m_tcpBox);
    tcpLay->setContentsMargins(0, 0, 0, 0);
    m_hostEdit = new QLineEdit;
    m_hostEdit->setPlaceholderText(QStringLiteral("192.168.1.100"));
    m_tcpPortEdit = new QLineEdit;
    m_tcpPortEdit->setPlaceholderText(QStringLiteral("端口"));
    m_tcpPortEdit->setMaximumWidth(90);
    tcpLay->addWidget(new QLabel(QStringLiteral("IP:")));
    tcpLay->addWidget(m_hostEdit);
    tcpLay->addWidget(new QLabel(QStringLiteral("端口:")));
    tcpLay->addWidget(m_tcpPortEdit);
    tcpLay->addStretch();
    linkLay->addWidget(m_tcpBox, 2, 0, 1, 4);
    m_tcpBox->setVisible(false);

    root->addWidget(linkGroup);

    // ── 固件 ──
    auto *fwGroup = new QGroupBox(QStringLiteral("固件"), this);
    auto *fwLay = new QVBoxLayout(fwGroup);
    auto *fwRow = new QHBoxLayout;
    m_fwEdit = new QLineEdit;
    m_fwEdit->setReadOnly(true);
    m_fwEdit->setPlaceholderText(QStringLiteral("选择 .firmware 固件文件（文件名需含 #CRC<CRC32>）"));
    auto *browseBtn = new QPushButton(QStringLiteral("浏览..."));
    fwRow->addWidget(m_fwEdit);
    fwRow->addWidget(browseBtn);
    fwLay->addLayout(fwRow);
    m_fwInfoLabel = new QLabel(QStringLiteral("未选择固件"));
    fwLay->addWidget(m_fwInfoLabel);
    root->addWidget(fwGroup);

    // ── 选项 / 按钮 ──
    m_autoResetCheck = new QCheckBox(
        QStringLiteral("自动发复位帧进入 Bootloader（取消勾选 = 设备已手动进入 BL，串口按 115200）"));
    m_autoResetCheck->setChecked(true);
    root->addWidget(m_autoResetCheck);

    auto *btnRow = new QHBoxLayout;
    m_startBtn = new QPushButton(QStringLiteral("开始升级"));
    m_cancelBtn = new QPushButton(QStringLiteral("取消"));
    m_cancelBtn->setEnabled(false);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_cancelBtn);
    btnRow->addStretch();
    root->addLayout(btnRow);

    m_progress = new QProgressBar;
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    root->addWidget(m_progress);

    m_phaseLabel = new QLabel(QStringLiteral("就绪"));
    root->addWidget(m_phaseLabel);

    m_logEdit = new QTextEdit;
    m_logEdit->setReadOnly(true);
    root->addWidget(m_logEdit, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &OtaPanel::refreshPorts);
    connect(browseBtn, &QPushButton::clicked, this, &OtaPanel::onBrowseFirmware);
    connect(m_linkCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OtaPanel::onLinkModeChanged);
    connect(m_startBtn, &QPushButton::clicked, this, &OtaPanel::onStart);
    connect(m_cancelBtn, &QPushButton::clicked, this, &OtaPanel::onCancel);

    refreshPorts();
}

OtaPanel::~OtaPanel()
{
    if (m_workerThread) {
        if (m_worker)
            QMetaObject::invokeMethod(m_worker, &OtaWorker::cancel, Qt::QueuedConnection);
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
}

// ═══════════════════════════════════════════════════

void OtaPanel::refreshPorts()
{
    m_portCombo->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &p : ports) {
        QString label = p.portName();
        if (!p.description().isEmpty())
            label += QStringLiteral(" - ") + p.description();
        m_portCombo->addItem(label, p.portName());
    }
    if (ports.isEmpty())
        m_portCombo->addItem(QStringLiteral("无可用串口"), QString());
}

void OtaPanel::onLinkModeChanged(int index)
{
    const bool serial = (m_linkCombo->itemData(index).toInt()
                         == int(OtaWorker::Transport::Serial));
    m_serialBox->setVisible(serial);
    m_tcpBox->setVisible(!serial);
}

void OtaPanel::onBrowseFirmware()
{
    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择固件文件"), QString(),
        QStringLiteral("固件文件 (*.firmware);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    m_fwEdit->setText(path);
    updateFirmwareInfo(path);
}

void OtaPanel::updateFirmwareInfo(const QString &path)
{
    QFileInfo fi(path);
    const QString name = fi.fileName();
    uint32_t nameCrc = OtaProtocol::extractCrcFromFileName(name);
    QString crcErr;
    uint32_t localCrc = OtaProtocol::crc32File(path, &crcErr);

    QString text = QStringLiteral("大小: %1 KB").arg(fi.size() / 1024.0, 0, 'f', 1);
    text += QStringLiteral("　文件名 CRC: %1").arg(
        nameCrc ? QString::number(nameCrc) : QStringLiteral("未找到(#CRC)"));
    if (localCrc) {
        text += QStringLiteral("　本地 CRC: %1").arg(localCrc);
        if (nameCrc)
            text += (localCrc == nameCrc) ? QStringLiteral(" ✓一致")
                                          : QStringLiteral(" ✗不一致");
    } else if (!crcErr.isEmpty()) {
        text += QStringLiteral("　本地 CRC: %1").arg(crcErr);
    }
    m_fwInfoLabel->setText(text);
}

void OtaPanel::setRunning(bool running)
{
    m_linkCombo->setEnabled(!running);
    m_serialBox->setEnabled(!running);
    m_tcpBox->setEnabled(!running);
    m_fwEdit->setEnabled(!running);
    m_autoResetCheck->setEnabled(!running);
    m_startBtn->setEnabled(!running);
    m_cancelBtn->setEnabled(running);
}

void OtaPanel::onStart()
{
    if (m_worker)
        return;

    const QString fw = m_fwEdit->text().trimmed();
    if (fw.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请先选择固件文件。"));
        return;
    }

    OtaWorker::Config cfg;
    cfg.firmwarePath = fw;
    cfg.autoReset = m_autoResetCheck->isChecked();

    if (m_linkCombo->currentData().toInt() == int(OtaWorker::Transport::Serial)) {
        cfg.transport = OtaWorker::Transport::Serial;
        cfg.port = m_portCombo->currentData().toString();
        if (cfg.port.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("提示"),
                                 QStringLiteral("没有可用串口。"));
            return;
        }
        // 手动进 BL 时固定 115200；自动模式用用户选择的应用波特率
        cfg.baud = cfg.autoReset ? m_baudCombo->currentText().toInt()
                                 : OtaProtocol::kBootBaud;
    } else {
        cfg.transport = OtaWorker::Transport::Tcp;
        cfg.host = m_hostEdit->text().trimmed();
        cfg.tcpPort = m_tcpPortEdit->text().trimmed().toInt();
        if (cfg.host.isEmpty() || cfg.tcpPort <= 0 || cfg.tcpPort > 65535) {
            QMessageBox::warning(this, QStringLiteral("提示"),
                                 QStringLiteral("请填写有效的 TCP IP 和端口。"));
            return;
        }
    }

    m_workerThread = new QThread(this);
    m_worker = new OtaWorker();
    m_worker->setConfig(cfg);
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, m_worker, &OtaWorker::start);
    connect(m_worker, &OtaWorker::logMessage, this, &OtaPanel::appendLog);
    connect(m_worker, &OtaWorker::progress, m_progress, &QProgressBar::setValue);
    connect(m_worker, &OtaWorker::phaseChanged, this, &OtaPanel::onPhaseChanged);
    connect(m_worker, &OtaWorker::finished, this, &OtaPanel::onFinished);
    connect(m_worker, &OtaWorker::finished, m_workerThread, &QThread::quit);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);

    m_logEdit->clear();
    m_progress->setValue(0);
    setRunning(true);
    m_workerThread->start();
}

void OtaPanel::onCancel()
{
    if (m_worker) {
        appendLog(QStringLiteral("用户请求取消，等待当前操作退出..."));
        QMetaObject::invokeMethod(m_worker, &OtaWorker::cancel, Qt::QueuedConnection);
        m_cancelBtn->setEnabled(false);
    }
}

void OtaPanel::appendLog(const QString &msg)
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    m_logEdit->append(QStringLiteral("[%1] %2").arg(ts, msg));
}

void OtaPanel::onPhaseChanged(OtaWorker::Phase phase, const QString &text)
{
    QString phaseName;
    switch (phase) {
    case OtaWorker::Phase::Idle:         phaseName = QStringLiteral("就绪"); break;
    case OtaWorker::Phase::Connecting:   phaseName = QStringLiteral("连接"); break;
    case OtaWorker::Phase::Resetting:    phaseName = QStringLiteral("复位进BL"); break;
    case OtaWorker::Phase::Handshaking:  phaseName = QStringLiteral("握手"); break;
    case OtaWorker::Phase::Erasing:      phaseName = QStringLiteral("擦除"); break;
    case OtaWorker::Phase::Programming:  phaseName = QStringLiteral("烧写"); break;
    case OtaWorker::Phase::Verifying:    phaseName = QStringLiteral("校验"); break;
    case OtaWorker::Phase::Rebooting:    phaseName = QStringLiteral("重启"); break;
    case OtaWorker::Phase::Done:         phaseName = QStringLiteral("完成"); break;
    case OtaWorker::Phase::Failed:       phaseName = QStringLiteral("失败"); break;
    }
    m_phaseLabel->setText(QStringLiteral("阶段: %1 — %2").arg(phaseName, text));
}

void OtaPanel::onFinished(bool success, const QString &summary)
{
    setRunning(false);
    if (success) {
        m_phaseLabel->setText(QStringLiteral("✓ %1").arg(summary));
        QMessageBox::information(this, QStringLiteral("升级完成"), summary);
    } else {
        m_phaseLabel->setText(QStringLiteral("✗ %1").arg(summary));
        QMessageBox::warning(this, QStringLiteral("升级失败"), summary);
    }
    m_worker = nullptr;
    m_workerThread = nullptr;
}
