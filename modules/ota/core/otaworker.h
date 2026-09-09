#ifndef OTAWORKER_H
#define OTAWORKER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <atomic>
#include <functional>
#include "modules/ota/core/otaprotocol.h"

class SerialConnection;
class TcpConnection;

// ════════════════════════════════════════════════════════════════
//  OtaWorker — OTA 升级控制状态机（工作线程内运行）
//  支持串口 / TCP 两种链路，帧协议一致（见 otaprotocol.h）。
//  流程：进BL → 握手 → 擦除 → 64B/块烧写 → CRC校验 → 重启。
// ════════════════════════════════════════════════════════════════
class OtaWorker : public QObject
{
    Q_OBJECT
public:
    enum class Transport { Serial, Tcp };
    enum class Phase {
        Idle, Connecting, Resetting, Handshaking, Erasing,
        Programming, Verifying, Rebooting, Done, Failed
    };
    Q_ENUM(Phase)

    struct Config {
        Transport transport = Transport::Serial;
        // 串口
        QString port;
        int     baud = OtaProtocol::kBootBaud;  // 自动模式=应用运行波特率；手动模式=BL波特率(115200)
        // TCP
        QString host;
        int     tcpPort = 0;
        // 固件
        QString firmwarePath;
        // true=发魔术帧自动进BL(随后切115200)；false=设备已手动进入BL
        bool    autoReset = true;
    };

    explicit OtaWorker(QObject *parent = nullptr);
    ~OtaWorker() override;

    void setConfig(const Config &cfg) { m_cfg = cfg; }

public slots:
    void start();   // worker 线程入口
    void cancel();

signals:
    void phaseChanged(OtaWorker::Phase phase, const QString &text);
    void progress(int percent);
    void logMessage(const QString &msg);
    void finished(bool success, const QString &summary);

private slots:
    void onTransportData(const QByteArray &data);
    void onTransportConnected();
    void onTransportError(const QString &msg);

private:
    // ── 传输层 ──
    bool openTransport(int baud);
    void closeTransport();
    void tx(const QByteArray &frame);
    bool waitConnected(int timeoutMs);

    // ── 升级流程 ──
    bool syncWithBootloader(int maxRounds);
    bool eraseChip();
    bool programFirmware(const QByteArray &fw);
    bool verifyCrc(uint32_t expectCrc);
    void rebootDevice();
    void fail(const QString &reason);

    // ── ACK 接收（AA 55 状态机，移植自参考 imu_rx）──
    void resetAckParser();
    void feedRxByte(uint8_t b);
    void clearAcks() { m_ackOk = m_gotCrcAck = m_gotOffsetAck = false; }

    // 可取消的等待/睡眠
    bool waitEvent(int timeoutMs, const std::function<bool()> &cond);
    bool sleepMs(int ms);

    Config   m_cfg;
    SerialConnection *m_serial = nullptr;
    TcpConnection    *m_tcp    = nullptr;
    std::atomic<bool> m_cancel{false};
    bool     m_connected = false;
    QString  m_transportError;

    // ACK 状态机
    enum RxState { WaitSync1, WaitSync2, WaitId1, WaitId2,
                   WaitLen1, WaitLen2, Payload,
                   WaitCrc1, WaitCrc2, WaitCrc3, WaitCrc4 };
    RxState   m_rxState = WaitSync1;
    uint16_t  m_rxId = 0, m_rxLen = 0;
    uint8_t   m_rxIdTmp = 0, m_rxLenTmp = 0;
    QByteArray m_rxPayload;
    int       m_rxCount = 0;
    uint8_t   m_rxCrcTmp[4] = {};
    uint32_t  m_rxCrcAccum = 1;

    // 最近 ACK
    bool     m_ackOk = false;       // 收到 0x0064 且 command==0x10
    bool     m_gotCrcAck = false;
    uint32_t m_crcAck = 0;          // 0x753C result
    bool     m_gotOffsetAck = false;
    uint32_t m_offsetAck = 0;       // 0x753D result
};

#endif // OTAWORKER_H
