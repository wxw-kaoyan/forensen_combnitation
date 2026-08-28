#include "modules/simulator/datasimulator.h"

#include <QDateTime>
#include <QSerialPort>
#include <QTimer>

#include "modules/simulator/simprotocol.h"

DataSimulator::DataSimulator(QObject *parent)
    : QObject(parent)
{
    // 初始时间基准：取系统当前 UTC 时间（含闰秒换算为 GPS 时间）
    m_clock = simClockFromUtc(QDateTime::currentDateTimeUtc());

    m_subTimer = new QTimer(this);
    m_subTimer->setTimerType(Qt::PreciseTimer);
    connect(m_subTimer, &QTimer::timeout, this, &DataSimulator::onSubTick);
}

DataSimulator::~DataSimulator()
{
    stop();
    qDeleteAll(m_protocols);
    m_protocols.clear();
}

void DataSimulator::setProtocols(const QList<SimProtocol *> &protocols)
{
    qDeleteAll(m_protocols);
    m_protocols = protocols;
}

bool DataSimulator::start(const QString &portName, int baudRate)
{
    if (m_port)
        return true;

    m_port = new QSerialPort(portName, this);
    m_port->setBaudRate(baudRate);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port->open(QIODevice::ReadWrite)) {
        const QString err = m_port->errorString();
        delete m_port;
        m_port = nullptr;
        emit errorOccurred(QStringLiteral("数据串口打开失败: %1").arg(err));
        return false;
    }

    // 面板为全局统一频率，所有协议 rateHz 相同，取第一个即可
    m_rateHz = m_protocols.isEmpty() ? 1 : m_protocols.first()->rateHz;
    m_framesSent = 0;
    return true;
}

void DataSimulator::stop()
{
    m_subTimer->stop();

    if (m_port) {
        m_port->close();
        m_port->deleteLater();
        m_port = nullptr;
    }
}

bool DataSimulator::isRunning() const
{
    return m_port && m_port->isOpen();
}

void DataSimulator::onPpsTick()
{
    // 取系统当前 UTC 时间并向下取整到秒，换算为 GPS 时间（与 PPS 上升沿对齐整秒）
    QDateTime now = QDateTime::currentDateTimeUtc();
    QDateTime sec = now;
    sec.setTime(QTime(sec.time().hour(), sec.time().minute(), sec.time().second()));
    m_clock = simClockFromUtc(sec);

    if (!m_port || !m_port->isOpen())
        return;

    // 整秒帧：所有协议第 0 帧，与 PPS 上升沿对齐发送
    sendAll();
    m_framesSent = 1;

    if (m_rateHz > 1) {
        m_subTimer->start(1000 / m_rateHz);
    } else {
        m_subTimer->stop();
    }
}

void DataSimulator::onSubTick()
{
    if (m_framesSent >= m_rateHz) {
        m_subTimer->stop();
        return;
    }

    sendAll();
    ++m_framesSent;

    if (m_framesSent >= m_rateHz)
        m_subTimer->stop();
}

void DataSimulator::sendAll()
{
    if (!m_port || !m_port->isOpen())
        return;

    for (SimProtocol *p : m_protocols) {
        const QByteArray frame = p->encode(m_clock);
        if (!frame.isEmpty())
            m_port->write(frame);
    }
}
