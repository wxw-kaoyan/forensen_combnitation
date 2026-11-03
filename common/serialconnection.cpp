#include "serialconnection.h"

#include <QSerialPort>
#include <QTimer>

SerialConnection::SerialConnection(QObject *parent)
    : QObject(parent)
{
    m_flushTimer = new QTimer(this);
    m_flushTimer->setSingleShot(true);
    m_flushTimer->setInterval(30);
    connect(m_flushTimer, &QTimer::timeout, this, &SerialConnection::onFlushBuffer);
}

SerialConnection::~SerialConnection()
{
    cleanup();
}

void SerialConnection::connectToPort(const QString &portName, int baudRate,
                                      int dataBits, int parityIdx, int stopBitsIdx,
                                      int timeoutMs)
{
    cleanup();

    m_serialPort = new QSerialPort(this);
    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(baudRate);
    m_serialPort->setDataBits(static_cast<QSerialPort::DataBits>(dataBits));

    static const QSerialPort::Parity kParityMap[] = {
        QSerialPort::NoParity, QSerialPort::EvenParity, QSerialPort::OddParity
    };
    static const QSerialPort::StopBits kStopMap[] = {
        QSerialPort::OneStop, QSerialPort::OneAndHalfStop, QSerialPort::TwoStop
    };
    m_serialPort->setParity(kParityMap[parityIdx]);
    m_serialPort->setStopBits(kStopMap[stopBitsIdx]);

    connect(m_serialPort, &QSerialPort::readyRead,     this, &SerialConnection::onSerialReadyRead);
    connect(m_serialPort, &QSerialPort::errorOccurred, this, &SerialConnection::onSerialError);

    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, &SerialConnection::onTimeout);
    m_timeout->start(timeoutMs);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        QString err = m_serialPort->errorString();
        // 权限类错误基本都是串口被别的进程占着（Windows 下 Access is denied）
        const bool busy = (m_serialPort->error() == QSerialPort::PermissionError);
        cleanup();
        if (busy) {
            emit errorOccurred(QStringLiteral("串口 %1 被其他进程占用，请先关闭使用该设备的进程：%2")
                                   .arg(portName, err));
        } else {
            emit errorOccurred(QStringLiteral("串口 %1 打开失败: %2").arg(portName, err));
        }
        return;
    }

    if (m_timeout) {
        m_timeout->stop();
        m_timeout->deleteLater();
        m_timeout = nullptr;
    }
    emit connected();
}

void SerialConnection::disconnectFromPort()
{
    cleanup();
    emit disconnected();
}

void SerialConnection::send(const QByteArray &data)
{
    if (m_serialPort && m_serialPort->isOpen())
        m_serialPort->write(data);
}

void SerialConnection::setBaudRate(int baudRate)
{
    if (m_serialPort && m_serialPort->isOpen())
        m_serialPort->setBaudRate(baudRate);
}

bool SerialConnection::drainTx(int timeoutMs)
{
    if (!m_serialPort || !m_serialPort->isOpen())
        return false;
    return m_serialPort->waitForBytesWritten(timeoutMs);
}

void SerialConnection::setImmediateEmit(bool immediate)
{
    m_immediateEmit = immediate;
    if (immediate && m_flushTimer)
        m_flushTimer->stop();   // 切到即时发射时，丢掉待触发的合并定时器
}

// ---- 内部槽 ----

void SerialConnection::onSerialReadyRead()
{
    if (!m_serialPort) return;
    m_buffer.append(m_serialPort->readAll());

    // 逐帧刷新模式：收到就发，不再等 30ms 合并（否则 100Hz 会被压成约 33Hz）
    if (m_immediateEmit) {
        flushBuffer();
        return;
    }

    // 缓冲区超过 4KB 立即发射，避免内存暴涨
    if (m_buffer.size() >= 4096)
        flushBuffer();
    else
        m_flushTimer->start();  // 30ms 无新数据后自动发射
}

void SerialConnection::onFlushBuffer()
{
    flushBuffer();
}

void SerialConnection::flushBuffer()
{
    if (m_buffer.isEmpty()) return;
    emit dataReceived(m_buffer);
    m_buffer.clear();
}

void SerialConnection::onSerialError()
{
    if (!m_serialPort) return;
    // Qt Windows 平台打开串口时常先发一次 NoError 的 errorOccurred，过滤掉
    if (m_serialPort->error() == QSerialPort::NoError)
        return;
    emit errorOccurred(QStringLiteral("串口: %1").arg(m_serialPort->errorString()));
}

void SerialConnection::onTimeout()
{
    emit errorOccurred(QStringLiteral("连接超时（1秒）"));
    disconnectFromPort();
}

void SerialConnection::cleanup()
{
    if (m_flushTimer) {
        m_flushTimer->stop();
    }
    // 清空前发射剩余数据
    flushBuffer();

    if (m_timeout) {
        m_timeout->stop();
        m_timeout->deleteLater();
        m_timeout = nullptr;
    }
    if (m_serialPort) {
        // 只断开 errorOccurred，防止 close/deleteLater 触发级联错误信号
        disconnect(m_serialPort, &QSerialPort::errorOccurred, this, &SerialConnection::onSerialError);
        m_serialPort->close();
        m_serialPort->deleteLater();
        m_serialPort = nullptr;
    }
}
