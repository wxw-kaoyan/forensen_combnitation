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
//  串口 / TCP 两种链路；自动/手动进 BL；固件选择、进度、日志。
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
    void onStart();
    void onCancel();
    void appendLog(const QString &msg);
    void onPhaseChanged(OtaWorker::Phase phase, const QString &text);
    void onFinished(bool success, const QString &summary);

private:
    void setRunning(bool running);
    void updateFirmwareInfo(const QString &path);

    QComboBox   *m_linkCombo   = nullptr;
    QWidget     *m_serialBox   = nullptr;
    QComboBox   *m_portCombo   = nullptr;
    QComboBox   *m_baudCombo   = nullptr;
    QWidget     *m_tcpBox      = nullptr;
    QLineEdit   *m_hostEdit    = nullptr;
    QLineEdit   *m_tcpPortEdit = nullptr;
    QLineEdit   *m_fwEdit      = nullptr;
    QLabel      *m_fwInfoLabel = nullptr;
    QCheckBox   *m_autoResetCheck = nullptr;
    QPushButton *m_startBtn    = nullptr;
    QPushButton *m_cancelBtn   = nullptr;
    QProgressBar*m_progress    = nullptr;
    QLabel      *m_phaseLabel  = nullptr;
    QTextEdit   *m_logEdit     = nullptr;

    QThread   *m_workerThread = nullptr;
    OtaWorker *m_worker       = nullptr;
};

#endif // OTAPANEL_H
