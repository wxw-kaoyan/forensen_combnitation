#ifndef PPSGENERATOR_H
#define PPSGENERATOR_H

#include <QObject>

class QSerialPort;
class QTimer;

// PPS 发生器：占用一个串口，用 DTR 线每秒输出一次上升沿脉冲（脉宽 5ms）
class PpsGenerator : public QObject
{
    Q_OBJECT

public:
    explicit PpsGenerator(QObject *parent = nullptr);
    ~PpsGenerator() override;

    bool start(const QString &portName, int baudRate = 115200);
    void stop();
    bool isRunning() const;

signals:
    void ppsTick();                     // 每秒触发一次（整秒对齐）
    void errorOccurred(const QString &msg);

private slots:
    void onSecondTick();
    void onPulseOff();

private:
    QSerialPort *m_port       = nullptr;
    QTimer      *m_secTimer   = nullptr;   // 1Hz
    QTimer      *m_pulseTimer = nullptr;   // 5ms 单次
};

#endif // PPSGENERATOR_H
