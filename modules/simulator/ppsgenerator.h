#ifndef PPSGENERATOR_H
#define PPSGENERATOR_H

#include <QObject>
#include <atomic>
#include <thread>

class QSerialPort;

// PPS 发生器（高精度软件版）：占用一个串口，用 DTR 线每秒输出一次上升沿脉冲（脉宽 5ms）。
// 实现：独立线程 + QElapsedTimer（Windows 底层即 QPC，分辨率 ~100ns），
// 首拍对齐到 UTC 整秒，之后按绝对节拍网格（+1s）自校准，目标 µs 级抖动。
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
    void ppsTick();                     // 每秒触发一次（上升沿）
    void errorOccurred(const QString &msg);

private:
    void runLoop();                     // 高精度脉冲线程入口
    void setDtr(bool high);             // DTR 电平切换（Win 原生 API，其他平台 Qt 兜底）

    QSerialPort     *m_port = nullptr;
    std::thread      m_thread;
    std::atomic<bool> m_running{false};
};

#endif // PPSGENERATOR_H
