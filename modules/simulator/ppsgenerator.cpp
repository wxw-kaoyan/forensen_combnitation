#include "modules/simulator/ppsgenerator.h"

#include <QSerialPort>
#include <QTimer>

PpsGenerator::PpsGenerator(QObject *parent)
    : QObject(parent)
{
    m_secTimer = new QTimer(this);
    m_secTimer->setTimerType(Qt::PreciseTimer);
    m_secTimer->setInterval(1000);
    connect(m_secTimer, &QTimer::timeout, this, &PpsGenerator::onSecondTick);

    m_pulseTimer = new QTimer(this);
    m_pulseTimer->setTimerType(Qt::PreciseTimer);
    m_pulseTimer->setSingleShot(true);
    m_pulseTimer->setInterval(5);
    connect(m_pulseTimer, &QTimer::timeout, this, &PpsGenerator::onPulseOff);
}

PpsGenerator::~PpsGenerator()
{
    stop();
}

bool PpsGenerator::start(const QString &portName, int baudRate)
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
        emit errorOccurred(QStringLiteral("PPS 串口打开失败: %1").arg(err));
        return false;
    }

    m_port->setDataTerminalReady(false);
    m_secTimer->start();
    return true;
}

void PpsGenerator::stop()
{
    m_secTimer->stop();
    m_pulseTimer->stop();

    if (m_port) {
        m_port->setDataTerminalReady(false);
        m_port->close();
        m_port->deleteLater();
        m_port = nullptr;
    }
}

bool PpsGenerator::isRunning() const
{
    return m_port && m_port->isOpen();
}

void PpsGenerator::onSecondTick()
{
    if (!m_port || !m_port->isOpen())
        return;

    m_port->setDataTerminalReady(true);   // 上升沿
    m_pulseTimer->start();                 // 5ms 后拉低
    emit ppsTick();
}

void PpsGenerator::onPulseOff()
{
    if (m_port)
        m_port->setDataTerminalReady(false);
}
