#ifndef DATASIMULATOR_H
#define DATASIMULATOR_H

#include <QList>
#include <QObject>

#include "modules/simulator/simclock.h"

class QSerialPort;
class QTimer;
class SimProtocol;

// 数据模拟器：占用一个串口，由 PPS 整秒驱动输出。
// 每个整秒（onPpsTick）先推进时间基准并发第 0 帧（与 PPS 上升沿对齐），
// 再由秒内子定时器按 rateHz 均匀发剩余帧。
class DataSimulator : public QObject
{
    Q_OBJECT

public:
    explicit DataSimulator(QObject *parent = nullptr);
    ~DataSimulator() override;

    void setProtocols(const QList<SimProtocol *> &protocols);

    bool start(const QString &portName, int baudRate = 115200);
    void stop();
    bool isRunning() const;

public slots:
    void onPpsTick();   // PPS 整秒对齐

signals:
    void errorOccurred(const QString &msg);

private slots:
    void onSubTick();   // 秒内剩余帧

private:
    void sendAll();

    QSerialPort         *m_port = nullptr;
    QList<SimProtocol *> m_protocols;
    QTimer              *m_subTimer = nullptr;
    SimClock             m_clock;

    int m_rateHz     = 1;
    int m_framesSent = 0;   // 当前秒内已发帧数（含整秒帧）
};

#endif // DATASIMULATOR_H
