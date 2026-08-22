#ifndef TCPCONNECTION_H
#define TCPCONNECTION_H

#include <QObject>

class QTcpSocket;
class QTimer;

// 通用 TCP 客户端连接（需放入工作线程）
// 提供 connect / disconnect / send / receive 基础能力，内置 1s 超时
class TcpConnection : public QObject
{
    Q_OBJECT

public:
    explicit TcpConnection(QObject *parent = nullptr);
    ~TcpConnection() override;

public slots:
    void connectToHost(const QString &host, int port, int timeoutMs = 1000);
    void disconnectFromHost();
    void send(const QByteArray &data);

signals:
    void connected();
    void disconnected();
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &msg);
    void logMessage(const QString &msg);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError();
    void onTimeout();
    void onFlushBuffer();

private:
    void cleanup();
    void flushBuffer();

    QTcpSocket *m_socket     = nullptr;
    QTimer     *m_timeout    = nullptr;
    QTimer     *m_flushTimer = nullptr;
    QByteArray  m_buffer;
};

#endif // TCPCONNECTION_H
