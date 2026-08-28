#ifndef SIMULATORPANEL_H
#define SIMULATORPANEL_H

#include <QList>
#include <QWidget>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class DataSimulator;
class PpsGenerator;
class SimProtocol;

// PPS + 数据模拟面板：PPS 串口 + 数据串口 + 协议勾选 + 启停
class SimulatorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SimulatorPanel(QWidget *parent = nullptr);
    ~SimulatorPanel() override;

private slots:
    void refreshPorts();
    void reloadProtocols();
    void selectAll();
    void deselectAll();
    void onStartStop();

private:
    void buildUi();
    void populatePorts(QComboBox *combo);
    QString jsonPath() const;
    void log(const QString &msg);
    void stopSimulation();

    PpsGenerator  *m_pps  = nullptr;
    DataSimulator *m_data = nullptr;

    QComboBox    *m_ppsPortCombo   = nullptr;
    QComboBox    *m_ppsBaudCombo   = nullptr;
    QComboBox    *m_dataPortCombo  = nullptr;
    QComboBox    *m_dataBaudCombo  = nullptr;
    QPushButton  *m_refreshButton  = nullptr;
    QPushButton  *m_reloadButton   = nullptr;
    QPushButton  *m_selectAllButton  = nullptr;
    QPushButton  *m_deselectAllButton = nullptr;
    QPushButton  *m_startButton    = nullptr;
    QSpinBox     *m_rateSpin       = nullptr;   // 全局频率 1-100Hz
    QTableWidget *m_protocolTable  = nullptr;
    QPlainTextEdit *m_logEdit      = nullptr;
    QLabel       *m_statusLabel    = nullptr;

    QList<SimProtocol *> m_displayProtocols;
};

#endif // SIMULATORPANEL_H
