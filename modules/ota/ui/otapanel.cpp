#include "modules/ota/ui/otapanel.h"
#include "modules/ota/core/otaprotocol.h"
#include "common/pcanconnection.h"

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

namespace {

// 解析十六进制 ID（允许 "0x" 前缀；留空按 0 处理）
uint32_t parseHexId(const QLineEdit *edit, bool *ok)
{
    QString text = edit->text().trimmed();
    if (text.isEmpty()) {
        if (ok) *ok = true;
        return 0;
    }
    if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
        text = text.mid(2);
    return text.toUInt(ok, 16);
}

} // namespace

OtaPanel::OtaPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);

    // ── 链路设置 ──
    auto *linkGroup = new QGroupBox(QStringLiteral("链路设置"), this);
    auto *linkLay = new QGridLayout(linkGroup);

    m_linkCombo = new QComboBox;
    m_linkCombo->addItem(QStringLiteral("串口"), int(OtaWorker::Transport::Serial));
    m_linkCombo->addItem(QStringLiteral("CAN"),  int(OtaWorker::Transport::Can));
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
    m_baudCombo->addItem(QStringLiteral("0（自动探测）"));
    for (int b : {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600})
        m_baudCombo->addItem(QString::number(b));
    m_baudCombo->setCurrentText(QStringLiteral("0（自动探测）"));
    serLay->addWidget(new QLabel(QStringLiteral("串口:")));
    serLay->addWidget(m_portCombo);
    serLay->addWidget(refreshBtn);
    serLay->addWidget(new QLabel(QStringLiteral("APP 波特率(0=自动):")));
    serLay->addWidget(m_baudCombo);
    serLay->addStretch();
    linkLay->addWidget(m_serialBox, 1, 0, 1, 4);

    // CAN 行（预留：仅预留配置项与链路，流程待协议文档）
    m_canBox = new QWidget;
    auto *canLay = new QHBoxLayout(m_canBox);
    canLay->setContentsMargins(0, 0, 0, 0);
    m_canChannelCombo = new QComboBox;
    m_canChannelCombo->setMinimumWidth(150);
    m_canBaudCombo = new QComboBox;
    m_canTxIdEdit = new QLineEdit;
    m_canTxIdEdit->setMaximumWidth(80);
    m_canTxIdEdit->setPlaceholderText(QStringLiteral("hex"));
    m_canRxIdEdit = new QLineEdit;
    m_canRxIdEdit->setMaximumWidth(80);
    m_canRxIdEdit->setPlaceholderText(QStringLiteral("hex"));

    for (const auto &ch : PcanConnection::availableChannels())
        m_canChannelCombo->addItem(ch.name, ch.handle);
    for (const auto &bd : PcanConnection::availableBaudrates())
        m_canBaudCombo->addItem(bd.name, bd.code);

    canLay->addWidget(new QLabel(QStringLiteral("通道:")));
    canLay->addWidget(m_canChannelCombo);
    canLay->addWidget(new QLabel(QStringLiteral("波特率:")));
    canLay->addWidget(m_canBaudCombo);
    canLay->addWidget(new QLabel(QStringLiteral("下发 ID:")));
    canLay->addWidget(m_canTxIdEdit);
    canLay->addWidget(new QLabel(QStringLiteral("上收 ID:")));
    canLay->addWidget(m_canRxIdEdit);
    canLay->addStretch();
    linkLay->addWidget(m_canBox, 2, 0, 1, 4);

    m_canHint = new QLabel;
    if (!PcanConnection::isDriverAvailable())
        m_canHint->setText(QStringLiteral("⚠ PCAN 驱动不可用，CAN 链路无法使用（需安装 PEAK 驱动）"));
    else
        m_canHint->setText(QStringLiteral("提示：CAN 升级流程为预留接口，报文格式待协议文档确定"));
    linkLay->addWidget(m_canHint, 3, 0, 1, 4);
    m_canBox->setVisible(false);
    m_canHint->setVisible(false);

    root->addWidget(linkGroup);

    // ── 固件 ──
    auto *fwGroup = new QGroupBox(QStringLiteral("固件"), this);
    auto *fwLay = new QVBoxLayout(fwGroup);

    // 协议行：原极 OTA（55 AA）/ 元鼎 YModem
    auto *protoRow = new QHBoxLayout;
    m_protocolCombo = new QComboBox;
    m_protocolCombo->addItem(QStringLiteral("原极 OTA（55 AA 帧）"),
                             int(OtaWorker::Protocol::YuanJi));
    m_protocolCombo->addItem(QStringLiteral("元鼎 YModem OTA"),
                             int(OtaWorker::Protocol::YuanDing));
    m_blockSizeCombo = new QComboBox;
    m_blockSizeCombo->addItem(QStringLiteral("1024 字节/块（STX）"), int(YModemOta::kBlock1024));
    m_blockSizeCombo->addItem(QStringLiteral("128 字节/块（SOH）"), int(YModemOta::kBlock128));
    m_allowNoCrcCheck = new QCheckBox(
        QStringLiteral("固件文件名可无 #CRC（不做 CRC32 校验）"));
    m_allowNoCrcCheck->setChecked(true);
    protoRow->addWidget(new QLabel(QStringLiteral("升级协议:")));
    protoRow->addWidget(m_protocolCombo);
    protoRow->addWidget(new QLabel(QStringLiteral("YModem 块大小:")));
    protoRow->addWidget(m_blockSizeCombo);
    protoRow->addWidget(m_allowNoCrcCheck);
    protoRow->addStretch();
    fwLay->addLayout(protoRow);

    auto *fwRow = new QHBoxLayout;
    m_fwEdit = new QLineEdit;
    m_fwEdit->setReadOnly(true);
    m_fwEdit->setPlaceholderText(QStringLiteral("选择固件文件（原极 .firmware / 元鼎 .bin 均可）"));
    auto *browseBtn = new QPushButton(QStringLiteral("浏览..."));
    fwRow->addWidget(m_fwEdit);
    fwRow->addWidget(browseBtn);
    fwLay->addLayout(fwRow);
    m_fwInfoLabel = new QLabel(QStringLiteral("未选择固件"));
    fwLay->addWidget(m_fwInfoLabel);
    root->addWidget(fwGroup);

    // ── 选项 / 按钮 ──
    m_autoResetCheck = new QCheckBox(
        QStringLiteral("自动发送进 BL 指令（取消勾选 = 设备已手动进入 Bootloader，直接按 Bootloader 握手）"));
    m_autoResetCheck->setChecked(true);
    root->addWidget(m_autoResetCheck);

    auto *btnRow = new QHBoxLayout;
    m_startBtn = new QPushButton(QStringLiteral("开始升级"));
    m_versionBtn = new QPushButton(QStringLiteral("读取版本"));
    m_cancelBtn = new QPushButton(QStringLiteral("取消"));
    m_cancelBtn->setEnabled(false);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_versionBtn);
    btnRow->addWidget(m_cancelBtn);
    btnRow->addStretch();
    root->addLayout(btnRow);

    m_versionLabel = new QLabel(QStringLiteral("版本: —"));
    root->addWidget(m_versionLabel);

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
    connect(m_protocolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OtaPanel::onProtocolChanged);
    connect(m_startBtn, &QPushButton::clicked, this, &OtaPanel::onStart);
    connect(m_versionBtn, &QPushButton::clicked, this, &OtaPanel::onReadVersion);
    connect(m_cancelBtn, &QPushButton::clicked, this, &OtaPanel::onCancel);

    refreshPorts();
    onProtocolChanged(m_protocolCombo->currentIndex());
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
    const int transport = m_linkCombo->itemData(index).toInt();
    const bool serial = (transport == int(OtaWorker::Transport::Serial));
    m_serialBox->setVisible(serial);
    m_canBox->setVisible(!serial);
    m_canHint->setVisible(!serial);
    // 读取版本只走串口的 AT 通道
    m_versionBtn->setEnabled(serial && m_worker == nullptr);
}

void OtaPanel::onProtocolChanged(int index)
{
    Q_UNUSED(index);
    const bool ym = (m_protocolCombo->currentData().toInt()
                     == int(OtaWorker::Protocol::YuanDing));
    m_blockSizeCombo->setEnabled(ym && m_worker == nullptr);
    m_allowNoCrcCheck->setEnabled(!ym && m_worker == nullptr);
    m_allowNoCrcCheck->setToolTip(
        ym ? QStringLiteral("YModem 用 Block0 声明固件长度，不需要 #CRC")
           : QString());
    // YModem 由设备回 'C' 握手，不需要「自动进 BL」选项
    m_autoResetCheck->setEnabled(!ym && m_worker == nullptr);
}

void OtaPanel::onBrowseFirmware()
{
    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择固件文件"), QString(),
        QStringLiteral("固件文件 (*.firmware *.bin);;所有文件 (*)"));
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
    if (nameCrc) {
        text += QStringLiteral("　文件名 CRC: %1").arg(nameCrc);
    } else {
        text += QStringLiteral("　文件名 CRC: 未找到(#CRC)%1")
                    .arg(m_allowNoCrcCheck->isChecked()
                             ? QStringLiteral("（允许，跳过校验）")
                             : QStringLiteral("（升级会报错）"));
    }
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
    m_canBox->setEnabled(!running);
    m_fwEdit->setEnabled(!running);
    m_protocolCombo->setEnabled(!running);
    m_allowNoCrcCheck->setEnabled(!running
        && m_protocolCombo->currentData().toInt() != int(OtaWorker::Protocol::YuanDing));
    m_blockSizeCombo->setEnabled(!running
        && m_protocolCombo->currentData().toInt() == int(OtaWorker::Protocol::YuanDing));
    m_autoResetCheck->setEnabled(!running
        && m_protocolCombo->currentData().toInt() != int(OtaWorker::Protocol::YuanDing));
    m_startBtn->setEnabled(!running);
    m_versionBtn->setEnabled(!running && m_linkCombo->currentData().toInt()
                                          == int(OtaWorker::Transport::Serial));
    m_cancelBtn->setEnabled(running);
}

void OtaPanel::onStart()
{
    startWorker(false);
}

void OtaPanel::onReadVersion()
{
    startWorker(true);
}

void OtaPanel::startWorker(bool versionOnly)
{
    if (m_worker)
        return;

    OtaWorker::Config cfg;
    cfg.versionOnly = versionOnly;
    cfg.autoReset = m_autoResetCheck->isChecked();
    cfg.protocol = OtaWorker::Protocol(m_protocolCombo->currentData().toInt());
    cfg.ymodemBlockSize  = m_blockSizeCombo->currentData().toInt();
    cfg.allowNoNameCrc   = m_allowNoCrcCheck->isChecked();

    if (cfg.protocol == OtaWorker::Protocol::YuanDing
        && m_linkCombo->currentData().toInt() != int(OtaWorker::Transport::Serial)) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("元鼎 YModem 升级仅支持串口链路。"));
        return;
    }

    if (m_linkCombo->currentData().toInt() == int(OtaWorker::Transport::Serial)) {
        cfg.transport = OtaWorker::Transport::Serial;
        cfg.port = m_portCombo->currentData().toString();
        if (cfg.port.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("提示"),
                                 QStringLiteral("没有可用串口。"));
            return;
        }
        // 波特率留空 / 填 0 = 自动探测（APP 与 Bootloader 波特率都会自动定位）
        bool baudOk = false;
        const int baud = m_baudCombo->currentText().trimmed().toInt(&baudOk);
        cfg.appBaud = (baudOk && baud > 0) ? baud : 0;
    } else {
        if (versionOnly) {
            QMessageBox::warning(this, QStringLiteral("提示"),
                                 QStringLiteral("读取版本仅支持串口链路。"));
            return;
        }
        cfg.transport = OtaWorker::Transport::Can;
        cfg.canChannel  = m_canChannelCombo->currentData().toInt();
        cfg.canBaudCode = m_canBaudCombo->currentData().toInt();
        bool txOk = false, rxOk = false;
        cfg.canTxId = parseHexId(m_canTxIdEdit, &txOk);
        cfg.canRxId = parseHexId(m_canRxIdEdit, &rxOk);
        if (!txOk || !rxOk) {
            QMessageBox::warning(this, QStringLiteral("提示"),
                                 QStringLiteral("CAN 下发/上收 ID 需填写十六进制（如 7E0）。"));
            return;
        }
    }

    if (!versionOnly) {
        const QString fw = m_fwEdit->text().trimmed();
        if (fw.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择固件文件。"));
            return;
        }
        cfg.firmwarePath = fw;
    }

    m_workerThread = new QThread(this);
    m_worker = new OtaWorker();
    m_worker->setConfig(cfg);
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, m_worker, &OtaWorker::start);
    connect(m_worker, &OtaWorker::logMessage, this, &OtaPanel::appendLog);
    connect(m_worker, &OtaWorker::progress, m_progress, &QProgressBar::setValue);
    connect(m_worker, &OtaWorker::versionRead, this, &OtaPanel::onVersionRead);
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

void OtaPanel::onVersionRead(const QString &raw)
{
    const QString text = raw.trimmed();
    m_versionLabel->setText(text.isEmpty() ? QStringLiteral("版本: —（无应答）")
                                            : QStringLiteral("版本: %1").arg(text));
}

void OtaPanel::onPhaseChanged(OtaWorker::Phase phase, const QString &text)
{
    QString phaseName;
    switch (phase) {
    case OtaWorker::Phase::Idle:         phaseName = QStringLiteral("就绪"); break;
    case OtaWorker::Phase::Connecting:   phaseName = QStringLiteral("连接"); break;
    case OtaWorker::Phase::Resetting:    phaseName = QStringLiteral("进Bootloader"); break;
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
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    if (success) {
        m_phaseLabel->setText(QStringLiteral("✓ %1").arg(summary));
        m_logEdit->append(QStringLiteral("[%1] ✓ %2").arg(ts, summary));
        QMessageBox::information(this, QStringLiteral("升级完成"), summary);
    } else {
        m_phaseLabel->setText(QStringLiteral("✗ %1").arg(summary));
        m_logEdit->append(QStringLiteral("[%1] ✗ %2").arg(ts, summary));
        QMessageBox::warning(this, QStringLiteral("升级失败"), summary);
    }
    m_worker = nullptr;
    m_workerThread = nullptr;
}
