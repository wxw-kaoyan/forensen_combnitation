#include "modules/simulator/ppsgenerator.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QSerialPort>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
constexpr qint64 kSecNs        = 1000000000LL;    // 1 秒（ns）
constexpr qint64 kPulseUs      = 5000;            // 脉宽 5ms
constexpr qint64 kSpinMarginNs = 2000000LL;       // 忙等窗口 2ms
} // namespace

PpsGenerator::PpsGenerator(QObject *parent)
    : QObject(parent)
{
}

PpsGenerator::~PpsGenerator()
{
    stop();
}

bool PpsGenerator::start(const QString &portName, int baudRate)
{
    if (m_running.load())
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

    setDtr(false);
    m_running.store(true);
    m_thread = std::thread(&PpsGenerator::runLoop, this);
    return true;
}

void PpsGenerator::stop()
{
    if (m_running.exchange(false) && m_thread.joinable())
        m_thread.join();

    if (m_port) {
        setDtr(false);
        m_port->close();
        m_port->deleteLater();
        m_port = nullptr;
    }
}

bool PpsGenerator::isRunning() const
{
    return m_running.load();
}

void PpsGenerator::runLoop()
{
#ifdef Q_OS_WIN
    timeBeginPeriod(1);                                 // 将 Sleep 粒度提升到 ~1ms
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif

    QElapsedTimer ref;
    ref.start();

    // 首个上升沿对齐到下一个 UTC 整秒，使 PPS 秒边界与系统时间一致
    const qint64 wallNowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 refNowNs  = ref.nsecsElapsed();
    qint64 targetNs = refNowNs + ((wallNowMs / 1000 + 1) * 1000 - wallNowMs) * 1000000;

    while (m_running.load()) {
        // 睡眠到目标前 2ms，剩余时间忙等自旋，规避系统睡眠精度导致的抖动
        qint64 remainNs = targetNs - ref.nsecsElapsed();
        if (remainNs > kSpinMarginNs)
            QThread::usleep(static_cast<unsigned long>((remainNs - kSpinMarginNs) / 1000));

        while (m_running.load() && ref.nsecsElapsed() < targetNs) {
        }

        if (!m_running.load())
            break;

        setDtr(true);       // 上升沿（µs 级对齐）
        emit ppsTick();     // 通知数据模拟器（跨线程排队投递到主线程）

        // 控制下降沿：脉宽 5ms
        const qint64 fallNs = ref.nsecsElapsed() + kPulseUs * 1000;
        remainNs = fallNs - ref.nsecsElapsed();
        if (remainNs > kSpinMarginNs)
            QThread::usleep(static_cast<unsigned long>((remainNs - kSpinMarginNs) / 1000));
        while (m_running.load() && ref.nsecsElapsed() < fallNs) {
        }
        setDtr(false);      // 下降沿

        // 绝对节拍网格：单拍过冲只影响本拍，不向下一拍累积漂移
        targetNs += kSecNs;
    }

    setDtr(false);

#ifdef Q_OS_WIN
    timeEndPeriod(1);
#endif
}

void PpsGenerator::setDtr(bool high)
{
    if (!m_port)
        return;

#ifdef Q_OS_WIN
    // 直调原生 API：绕过 Qt 串口对象（归属主线程），并降低电平切换延迟
    const HANDLE h = reinterpret_cast<HANDLE>(m_port->handle());
    if (h && h != INVALID_HANDLE_VALUE)
        EscapeCommFunction(h, high ? SETDTR : CLRDTR);
#else
    m_port->setDataTerminalReady(high);
#endif
}
