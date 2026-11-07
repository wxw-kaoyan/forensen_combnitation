#include "tcpconnection.h"

#include <QTcpSocket>
#include <QTimer>

TcpConnection::TcpConnection(QObject *parent)
    : QObject(parent)
{
    m_flushTimer = new QTimer(this);
    m_flushTimer->setSingleShot(false);  // 周期模式：每 10ms 触发一次 flush
    m_flushTimer->setInterval(10);       // 目标 100Hz，Windows 默认时钟精度 ≈15.6ms，实际 ≈64Hz
    m_flushTimer->start();               // 连接期间始终运行，不再在 readyRead 里 reset

    connect(m_flushTimer, &QTimer::timeout, this, &TcpConnection::onFlushBuffer);
}

TcpConnection::~TcpConnection()
{
    cleanup();
}

void TcpConnection::connectToHost(const QString &host, int port, int timeoutMs)
{
    cleanup();

    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected,      this, &TcpConnection::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected,   this, &TcpConnection::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::readyRead,      this, &TcpConnection::onSocketReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred,  this, &TcpConnection::onSocketError);

    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, &TcpConnection::onTimeout);

    emit logMessage(QStringLiteral("[系统] 正在连接 %1:%2...\n").arg(host).arg(port));
    m_socket->connectToHost(host, port);
    m_timeout->start(timeoutMs);
}

void TcpConnection::disconnectFromHost()
{
    cleanup();
    // 不再显式 emit disconnected() —— abort() 后 QTcpSocket::disconnected
    // 会自然触发 onSocketDisconnected → emit disconnected，避免双重发射
}

void TcpConnection::send(const QByteArray &data)
{
    if (!m_socket) {
        emit logMessage(QStringLiteral("[TCP] 发送失败：socket 为空\n"));
        return;
    }
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        emit logMessage(QStringLiteral("[TCP] 发送失败：socket 状态=%1\n")
                            .arg(static_cast<int>(m_socket->state())));
        return;
    }
    qint64 written = m_socket->write(data);
    emit logMessage(QStringLiteral("[TCP] 已发送 %1 字节: %2\n")
                        .arg(written)
                        .arg(QString::fromLatin1(data.left(100).trimmed())));
}

// ---- 内部槽 ----

void TcpConnection::onSocketConnected()
{
    if (m_timeout) {
        m_timeout->stop();
        m_timeout->deleteLater();
        m_timeout = nullptr;
    }
    emit connected();
}

void TcpConnection::onSocketDisconnected()
{
    emit disconnected();
}

void TcpConnection::onSocketReadyRead()
{
    if (!m_socket) return;
    m_buffer.append(m_socket->readAll());

    if (m_buffer.size() >= 4096)
        flushBuffer();
    else
        m_flushTimer->start();
}

void TcpConnection::onFlushBuffer()
{
    flushBuffer();
}

void TcpConnection::flushBuffer()
{
    if (m_buffer.isEmpty()) return;
    emit dataReceived(m_buffer);
    m_buffer.clear();
}

void TcpConnection::onSocketError()
{
    if (!m_socket) return;
    // 过滤 UnknownSocketError 等无意义错误，避免干扰正常连接流程
    if (m_socket->error() == QAbstractSocket::UnknownSocketError)
        return;
    emit errorOccurred(QStringLiteral("TCP: %1").arg(m_socket->errorString()));
}

void TcpConnection::onTimeout()
{
    emit errorOccurred(QStringLiteral("连接超时（1秒）"));
    disconnectFromHost();
}

void TcpConnection::cleanup()
{
    if (m_flushTimer) {
        m_flushTimer->stop();
    }
    flushBuffer();

    if (m_timeout) {
        m_timeout->stop();
        m_timeout->deleteLater();
        m_timeout = nullptr;
    }
    if (m_socket) {
        // 只断开 errorOccurred，防止 abort() 触发级联错误信号
        disconnect(m_socket, &QAbstractSocket::errorOccurred, this, &TcpConnection::onSocketError);
        m_socket->abort();  // 立即关闭，不阻塞（waitForDisconnected 在 Windows 可能挂死）
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}
