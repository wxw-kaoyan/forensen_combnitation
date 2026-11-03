#ifndef OTAPANEL_H
#define OTAPANEL_H

#include <QWidget>
#include "modules/ota/core/otaworker.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QCheckBox;
class QProgressBar;
class QLabel;
class QTextEdit;
class QThread;

// ════════════════════════════════════════════════════════════════
//  OtaPanel — 「OTA升级」面板
//  链路：串口（已实现）/ CAN（预留骨架，协议待定）
//  自动/手动进 BL；固件选择、进度、日志；可单独读取设备版本。
// ════════════════════════════════════════════════════════════════
class OtaPanel : public QWidget
{
    Q_OBJECT
public:
    explicit OtaPanel(QWidget *parent = nullptr);
    ~OtaPanel() override;

private slots:
    void refreshPorts();
    void onBrowseFirmware();
    void onLinkModeChanged(int index);
    void onProtocolChanged(int index);
    void onStart();
    void onReadVersion();
    void onCancel();
    void appendLog(const QString &msg);
    void onVersionRead(const QString &raw);
    void onPhaseChanged(OtaWorker::Phase phase, const QString &text);
    void onFinished(bool success, const QString &summary);

private:
    void startWorker(bool versionOnly);
    void setRunning(bool running);
    void updateFirmwareInfo(const QString &path);

    QComboBox   *m_linkCombo   = nullptr;

    // 串口行
    QWidget     *m_serialBox   = nullptr;
    QComboBox   *m_portCombo   = nullptr;
    QComboBox   *m_baudCombo   = nullptr;

    // CAN 行（预留）
    QWidget     *m_canBox      = nullptr;
    QComboBox   *m_canChannelCombo = nullptr;
    QComboBox   *m_canBaudCombo    = nullptr;
    QLineEdit   *m_canTxIdEdit = nullptr;
    QLineEdit   *m_canRxIdEdit = nullptr;
    QLabel      *m_canHint     = nullptr;

    // 固件
    QComboBox   *m_protocolCombo   = nullptr;   // 原极 OTA / 元鼎 YModem
    QComboBox   *m_blockSizeCombo  = nullptr;   // YModem 块大小 128 / 1024
    QCheckBox   *m_allowNoCrcCheck = nullptr;   // 固件名可无 #CRC（原极）
    QLineEdit   *m_fwEdit      = nullptr;
    QLabel      *m_fwInfoLabel = nullptr;

    QCheckBox   *m_autoResetCheck = nullptr;
    QPushButton *m_startBtn    = nullptr;
    QPushButton *m_versionBtn  = nullptr;
    QPushButton *m_cancelBtn   = nullptr;
    QProgressBar*m_progress    = nullptr;
    QLabel      *m_phaseLabel  = nullptr;
    QLabel      *m_versionLabel= nullptr;
    QTextEdit   *m_logEdit     = nullptr;

    QThread   *m_workerThread = nullptr;
    OtaWorker *m_worker       = nullptr;
};

#endif // OTAPANEL_H
