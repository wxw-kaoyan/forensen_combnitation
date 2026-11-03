#ifndef SERIALCONNECTION_H
#define SERIALCONNECTION_H

#include <QObject>

class QSerialPort;
class QTimer;

// 通用串口连接（需放入工作线程）
// 提供 connect / disconnect / send / receive 基础能力，内置 1s 超时
class SerialConnection : public QObject
{
    Q_OBJECT

public:
    explicit SerialConnection(QObject *parent = nullptr);
    ~SerialConnection() override;

public slots:
    void connectToPort(const QString &portName, int baudRate,
                       int dataBits, int parityIdx, int stopBitsIdx,
                       int timeoutMs = 1000);
    void disconnectFromPort();
    void send(const QByteArray &data);
    // 已打开状态下热切换波特率（升级探测波特率用）；串口未打开时忽略
    void setBaudRate(int baudRate);
    // 等待发送缓冲区排空，返回是否在超时内写完（切波特率前用，防止字节卡在半途）
    bool drainTx(int timeoutMs = 1000);

    // 逐帧刷新：true 时收到数据立即发射；false 时按 30ms 合并后发射（默认）
    // 30ms 合并会把 100Hz 数据压成约 33Hz，实时解析「逐帧刷新」时必须关掉
    void setImmediateEmit(bool immediate);

signals:
    void connected();
    void disconnected();
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &msg);
    void logMessage(const QString &msg);

private slots:
    void onSerialReadyRead();
    void onSerialError();
    void onTimeout();
    void onFlushBuffer();

private:
    void cleanup();
    void flushBuffer();

    QSerialPort *m_serialPort  = nullptr;
    QTimer      *m_timeout     = nullptr;
    QTimer      *m_flushTimer  = nullptr;
    QByteArray   m_buffer;
    bool         m_immediateEmit = false;   // true=收到即发，跳过 30ms 合并
};

#endif // SERIALCONNECTION_H
