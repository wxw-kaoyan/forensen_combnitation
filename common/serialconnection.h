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
};

#endif // SERIALCONNECTION_H
