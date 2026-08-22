#include "modules/ntrip/forwardtarget.h"
#include "modules/ntrip/datapool/datapool.h"

#include <QTcpSocket>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QFile>

// ============ ForwardTarget 基类 ============

ForwardTarget::ForwardTarget(const Config &cfg, DataPool *pool, QObject *parent)
    : QObject(parent), m_config(cfg), m_pool(pool) {}

void ForwardTarget::setGgaSource(bool enabled)
{
    m_config.isGgaSource = enabled;
}

void ForwardTarget::readFromPool()
{
    if (m_consumerId < 0 || !m_pool) return;
    QByteArray data = m_pool->read(m_consumerId);
    if (data.isEmpty()) return;
    // 子类需要实现具体的 writeToOutput
}

void ForwardTarget::enableSave(const QString &filePath)
{
    disableSave();
    m_saveFile = new QFile(filePath, this);
    if (m_saveFile->open(QIODevice::WriteOnly)) {
        emit saveStateChanged(true);
    } else {
        emit statusChanged(QStringLiteral("无法创建保存文件: ") + filePath);
        delete m_saveFile;
        m_saveFile = nullptr;
    }
}

void ForwardTarget::disableSave()
{
    if (m_saveFile) {
        m_saveFile->close();
        delete m_saveFile;
        m_saveFile = nullptr;
        emit saveStateChanged(false);
    }
}

void ForwardTarget::saveNonGgaData(const QByteArray &raw)
{
    if (!m_saveFile || !m_saveFile->isOpen()) return;
    m_saveFile->write(raw);
}

// 工厂
ForwardTarget *ForwardTarget::create(const Config &cfg, DataPool *pool,
                                     QObject *parent)
{
    switch (cfg.type) {
    case TCP:
        return new TcpForwardTarget(cfg, pool, parent);
    case Serial:
        return new SerialForwardTarget(cfg, pool, parent);
    }
    return nullptr;
}


// ============ TcpForwardTarget ============

TcpForwardTarget::TcpForwardTarget(const Config &cfg, DataPool *pool,
                                   QObject *parent)
    : ForwardTarget(cfg, pool, parent)
    , m_connectTimer(new QTimer(this))
{
    m_socket = new QTcpSocket(this);
    m_connectTimer->setSingleShot(true);
    m_connectTimer->setInterval(1000); // 1 秒超时
    connect(m_connectTimer, &QTimer::timeout, this, &TcpForwardTarget::onConnectTimeout);
    connect(m_socket, &QTcpSocket::connected,    this, &TcpForwardTarget::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpForwardTarget::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead,    this, &TcpForwardTarget::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        QString err = m_socket->errorString();
        m_connectTimer->stop();
        m_running = false;
        emit statusChanged(QStringLiteral("TCP 连接 %1:%2 失败: %3")
                               .arg(m_config.address)
                               .arg(m_config.portOrBaud)
                               .arg(err));
        emit errorOccurred(err);
    });
}

TcpForwardTarget::~TcpForwardTarget()
{
    stop();
}

void TcpForwardTarget::start()
{
    if (m_running) return;

    m_consumerId = m_pool->subscribe();
    connect(m_pool, &DataPool::dataAvailable, this, &TcpForwardTarget::onPoolData,
            Qt::UniqueConnection);

    m_socket->connectToHost(m_config.address, static_cast<quint16>(m_config.portOrBaud));
    m_connectTimer->start();
    m_running = true;
    emit statusChanged(QStringLiteral("TCP 正在连接 %1:%2 ...")
                           .arg(m_config.address)
                           .arg(m_config.portOrBaud));
}

void TcpForwardTarget::stop()
{
    m_connectTimer->stop();
    disconnect(m_pool, &DataPool::dataAvailable, this, &TcpForwardTarget::onPoolData);

    if (m_socket) {
        if (m_socket->state() == QAbstractSocket::ConnectedState) {
            m_socket->disconnectFromHost();
        } else if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->abort();
        }
    }
    m_consumerId = -1;
    m_running = false;
    emit statusChanged(QStringLiteral("TCP %1:%2 已断开")
                           .arg(m_config.address)
                           .arg(m_config.portOrBaud));
}

void TcpForwardTarget::onPoolData()
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) return;
    QByteArray data = m_pool->read(m_consumerId);
    if (!data.isEmpty()) {
        m_socket->write(data);
    }
}

void TcpForwardTarget::onConnected()
{
    m_connectTimer->stop();
    emit statusChanged(QStringLiteral("TCP 已连接 %1:%2")
                           .arg(m_config.address)
                           .arg(m_config.portOrBaud));
}

void TcpForwardTarget::onDisconnected()
{
    if (m_running) {
        m_running = false;
        emit statusChanged(QStringLiteral("TCP %1:%2 已断开")
                               .arg(m_config.address)
                               .arg(m_config.portOrBaud));
    }
}

void TcpForwardTarget::onReadyRead()
{
    QByteArray raw = m_socket->readAll();
    emit rawDataReceived(raw);
    saveNonGgaData(raw);
    if (!m_config.isGgaSource) return;

    // 提取 $GPGGA 行
    for (int i = 0; i < raw.size(); ) {
        if (raw[i] == '$') {
            int end = raw.indexOf('\n', i);
            if (end < 0) end = raw.size();
            QByteArray line = raw.mid(i, end - i).trimmed();
        if (line.startsWith("$") && line.contains("GGA")) {
                emit ggaDataReceived(line + "\r\n");
            }
            i = end + 1;
        } else {
            ++i;
        }
    }
}

void TcpForwardTarget::onConnectTimeout()
{
    m_socket->abort();
    m_running = false;
    emit statusChanged(QStringLiteral("TCP 连接 %1:%2 超时")
                           .arg(m_config.address)
                           .arg(m_config.portOrBaud));
}

// ============ SerialForwardTarget ============

SerialForwardTarget::SerialForwardTarget(const Config &cfg, DataPool *pool,
                                         QObject *parent)
    : ForwardTarget(cfg, pool, parent)
{
    m_serial = new QSerialPort(this);
    connect(m_serial, &QSerialPort::readyRead, this, &SerialForwardTarget::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError) {
        QString err = m_serial->errorString();
        if (err.isEmpty()) return;
        m_running = false;
        m_serial->close();
        emit statusChanged(QStringLiteral("串口 %1 错误: %2")
                               .arg(m_config.address)
                               .arg(err));
        emit errorOccurred(err);
    });
}

SerialForwardTarget::~SerialForwardTarget()
{
    stop();
}

void SerialForwardTarget::start()
{
    if (m_running) return;

    m_consumerId = m_pool->subscribe();
    connect(m_pool, &DataPool::dataAvailable, this, &SerialForwardTarget::onPoolData,
            Qt::UniqueConnection);

    m_serial->setPortName(m_config.address);
    m_serial->setBaudRate(m_config.portOrBaud);
    m_serial->setDataBits(static_cast<QSerialPort::DataBits>(m_config.dataBits));
    m_serial->setParity(static_cast<QSerialPort::Parity>(m_config.parity));
    m_serial->setStopBits(static_cast<QSerialPort::StopBits>(m_config.stopBits));

    // 检查 COM 口是否存在
    QSerialPortInfo info(m_config.address);
    if (info.isNull()) {
        emit errorOccurred(QStringLiteral("串口 %1 不存在").arg(m_config.address));
        return;
    }

    // 屏蔽信号防止 open 期间 readyRead 同步触发导致 crash
    m_serial->blockSignals(true);
    bool ok = m_serial->open(QIODevice::ReadWrite);
    m_serial->blockSignals(false);

    if (ok) {
        m_running = true;
        emit statusChanged(QStringLiteral("串口 %1 已打开 @%2")
                               .arg(m_config.address, QString::number(m_config.portOrBaud)));
    } else {
        emit errorOccurred(QStringLiteral("串口 %1 打开失败: %2")
                               .arg(m_config.address, m_serial->errorString()));
    }
}

void SerialForwardTarget::stop()
{
    if (!m_running) return;

    disconnect(m_pool, &DataPool::dataAvailable, this, &SerialForwardTarget::onPoolData);
    m_consumerId = -1;

    if (m_serial->isOpen()) {
        m_serial->close();
    }
    m_running = false;
    emit statusChanged(QStringLiteral("串口 已关闭"));
}

void SerialForwardTarget::onPoolData()
{
    if (!m_serial || !m_serial->isOpen()) return;
    QByteArray data = m_pool->read(m_consumerId);
    if (!data.isEmpty()) {
        m_serial->write(data);
    }
}

void SerialForwardTarget::onReadyRead()
{
    QByteArray raw = m_serial->readAll();
    emit rawDataReceived(raw);
    saveNonGgaData(raw);
    if (!m_config.isGgaSource) return;

    // 提取 $xxGGA 行
    for (int i = 0; i < raw.size(); ) {
        if (raw[i] == '$') {
            int end = raw.indexOf('\n', i);
            if (end < 0) end = raw.size();
            QByteArray line = raw.mid(i, end - i).trimmed();
        if (line.startsWith("$") && line.contains("GGA")) {
                emit ggaDataReceived(line + "\r\n");
            }
            i = end + 1;
        } else {
            ++i;
        }
    }
}
