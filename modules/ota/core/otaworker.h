#ifndef OTAWORKER_H
#define OTAWORKER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <atomic>
#include <functional>
#include "modules/ota/core/otaprotocol.h"

class SerialConnection;
class PcanConnection;

// ════════════════════════════════════════════════════════════════
//  OtaWorker — OTA 升级控制状态机（工作线程内运行）
//
//  两种链路：
//    · Serial：已实现。流程移植自量产版 C++ SDK（linux_uart_ota/C++），
//      与旧参照版的差别在于「波特率不假定、AT 通道压制/恢复、进 BL 覆盖复位窗口」：
//        读升级前版本(自动模式下同时探测 APP 波特率) → 发进 BL 指令
//        → 探测 Bootloader 波特率 → 盲发停留指令覆盖软复位窗口 → 同步
//        → 擦除 → 64B/块烧写 → CRC 校验(不一致才整轮重试) → 无条件重启
//        → 重探 APP 波特率 → 读升级后版本 → AT+SETYES 恢复设备主动输出。
//      升级是否成功以 CRC 读回比对为准，读版本只作参考。
//    · Can：预留骨架。CAN 升级是另一套流程（帧格式/时序不同），
//      协议文档确定前只保留阶段接口（canEnterBoot/canSyncBoot/canErase/
//      canProgram/canVerifyCrc/canReboot）与传输接线（common/pcanconnection），
//      上层阶段切换/进度/日志/取消逻辑无需改动。
// ════════════════════════════════════════════════════════════════
class OtaWorker : public QObject
{
    Q_OBJECT
public:
    enum class Transport { Serial, Can };
    // 升级协议：原极（55 AA 帧）/ 元鼎（X50 子设备 YModem）
    enum class Protocol { YuanJi, YuanDing };
    enum class Phase {
        Idle, Connecting, Resetting, Handshaking, Erasing,
        Programming, Verifying, Rebooting, Done, Failed
    };
    Q_ENUM(Phase)

    struct Config {
        Transport transport = Transport::Serial;
        Protocol  protocol  = Protocol::YuanJi;

        // ── 串口 ──
        QString port;
        int     appBaud  = 0;      // APP 当前波特率；0 = 自动探测（依次试候选波特率）
        bool    autoReset = true;  // true=发进BL指令自动进 BL；false=设备已手动进 BL

        // ── 固件 ──
        QString firmwarePath;
        // 原极协议：固件名可不带 #CRC（true=允许，此时跳过 CRC32 校验）
        bool    allowNoNameCrc = true;
        // 元鼎 YModem：数据块大小 128（SOH）/ 1024（STX）
        int     ymodemBlockSize = YModemOta::kBlock1024;

        // ── CAN（预留，协议确定后启用）──
        int      canChannel  = 0;     // PcanConnection::availableChannels() 的 handle
        int      canBaudCode = 0;     // PcanConnection::availableBaudrates() 的 code
        uint32_t canTxId     = 0;     // 下发报文 ID（待协议文档）
        uint32_t canRxId     = 0;     // 上收报文 ID（待协议文档）
        bool     canExtended = false; // 是否扩展帧

        // 只读取版本，不擦写（UI「读取版本」按钮用；仅串口链路）
        bool    versionOnly = false;
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
    void versionRead(const QString &raw);   // 设备版本应答原文（不做解析）
    void finished(bool success, const QString &summary);

private slots:
    void onTransportData(const QByteArray &data);
    void onTransportConnected();
    void onTransportError(const QString &msg);
    void onCanFrame(quint32 id, const QByteArray &data, quint8 msgType, quint64 timestampUs);

private:
    // ── 传输层 ──
    bool openTransport(int baud);
    void closeTransport();
    void tx(const QByteArray &frame);
    bool waitConnected(int timeoutMs);
    bool setBaud(int baud);

    // ── 通用工具 ──
    void fail(const QString &reason);
    bool waitEvent(int timeoutMs, const std::function<bool()> &cond);
    bool sleepMs(int ms);
    int  readQuiet(int timeoutMs, int quietMs);   // 收原始字节直到串口静默 quietMs
    bool sendText(const char *text);              // 发 AT / 文本指令（裸字符串）

    // ── ACK 接收（AA 55 状态机，移植自参考 imu_rx）──
    void resetAckParser();
    void feedRxByte(uint8_t b);
    void clearAcks() { m_ackOk = m_gotCrcAck = m_gotOffsetAck = false; }

    // ── 串口：AT 通道与波特率探测 ──
    bool atPrepareChannel();                 // 连发 AT+SETNO 压停设备主动输出
    bool atProbe(int candidate);             // 某波特率下试读 APP 版本
    bool detectAppBaud();                    // 找 APP 波特率；找不到则找 Bootloader 波特率
    bool probeAppBaudOnly();                 // 升级后重新定位设备
    bool scanBlBaud();                       // 逐波特率试同步，定位 Bootloader 波特率
    bool readVersionInternal(int windowMs, QString *out);

    // ── 串口：升级流程 ──
    bool doSync(int timeoutMs);
    bool getSyncAck(int timeoutMs);
    bool enterBootloader();                  // 进 BL：发指令 → 探测 BL 波特率 → 停留指令窗口 → 同步
    bool eraseChip();
    bool programFirmware(const QByteArray &fw);
    bool sendBlockWithRetry(const QByteArray &frame, uint32_t offset);

    enum class CrcCheck { Ok, Mismatch, NoAck };
    CrcCheck verifyCrc(uint32_t expectCrc);
    void rebootDevice();

    void runSerialUpgrade();
    void runReadVersionOnly();
    void runCanUpgrade();

    // ── 元鼎 YModem 升级流程（串口）──
    void runYmodemUpgrade();
    void runYmodemReadVersion();
    bool ymDetectBaud(QString *text = nullptr, quint16 *ver = nullptr);  // 用版本查询帧找通信波特率
    bool ymReadVersion(QString *text, quint16 *ver);
    bool ymEnterOta();                           // 发 FF FF 01，等 'C'
    bool ymSendFileInfo(const QString &fileName, qint64 size);
    bool ymSendData(const QByteArray &fw);       // 逐块发送（128/1024）
    bool ymSendEot();
    bool ymSendEndBlock();
    int  ymWaitByte(int timeoutMs);              // 返回 ACK/NAK/'C'，超时 -1
    bool ymSendAndWaitAck(const QByteArray &frame, int timeoutMs, int retryMax,
                          const QString &what);

    // ── CAN 升级流程（预留骨架：待 CAN 升级协议文档确定后填写）──
    bool canEnterBoot();
    bool canSyncBoot();
    bool canErase();
    bool canProgram(const QByteArray &fw);
    bool canVerifyCrc(uint32_t expectCrc);
    bool canReboot();
    void canSend(const QByteArray &payload);

    // ── 成员 ──
    Config   m_cfg;
    SerialConnection *m_serial = nullptr;
    PcanConnection   *m_can    = nullptr;
    std::atomic<bool> m_cancel{false};
    bool     m_connected = false;
    QString  m_transportError;

    // 元鼎 YModem 模式：上行是单字节 ACK/NAK/'C'，不喂 AA55 状态机
    bool     m_ymMode = false;

    // 波特率探测状态
    int      m_appBaud = OtaProtocol::kBootBaud;  // 探测到的 / 用户指定的 APP 波特率
    int      m_blBaud  = OtaProtocol::kBootBaud;  // 探测到的 Bootloader 波特率
    bool     m_autoBaud      = false;  // cfg.appBaud == 0
    bool     m_baudDetected  = false;  // 自动模式下是否已完成探测
    bool     m_inBl          = false;  // 探测时发现设备已在 Bootloader（无 APP）
    bool     m_appBaudKnown  = false;  // 是否已确定 APP 波特率
    bool     m_setNoOk       = false;  // 最近一次 AT+SETNO 是否成功压停数据流
    QString  m_versionBuf;

    // 原始接收缓冲（AT 应答 / 静默判定）；同时喂给 ACK 状态机
    QByteArray m_rawBuf;
    qint64     m_lastRxMs = 0;

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

    // CAN 收帧缓冲（预留）
    QByteArray m_canRxBuf;
};

#endif // OTAWORKER_H
