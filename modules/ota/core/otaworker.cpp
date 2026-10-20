#include "modules/ota/core/otaworker.h"
#include "common/serialconnection.h"
#include "common/pcanconnection.h"

#include <QDateTime>
#include <QThread>
#include <QEventLoop>
#include <QTimer>
#include <QFile>
#include <QFileInfo>

// ════════════════════════════════════════════════════════════════
//  流程常量：与量产版 C++ SDK（linux_uart_ota/C++）逐项对齐
// ════════════════════════════════════════════════════════════════
namespace {

// 自动探测波特率时依次尝试的候选值
constexpr int kBaudCandidates[] = {9600, 115200, 230400, 460800, 921600};
constexpr int kBaudCandidateCount = int(sizeof(kBaudCandidates) / sizeof(kBaudCandidates[0]));

constexpr int kConnectWaitMs       = 8000;   // 打开链路等待
constexpr int kPostRebootWaitMs    = 3000;   // 重启后等待 APP 启动
constexpr int kSetNoTimeoutMs      = 10000;  // AT+SETNO 压停数据流的整体超时
constexpr int kSetNoQuietMs        = 300;    // 每轮 AT+SETNO 后等待安静的时长
constexpr int kSetYesCount         = 3;      // AT+SETYES 发送次数
constexpr int kSetYesIntervalMs    = 100;
constexpr int kForsenseWindowCount = 40;     // 盲发停留指令覆盖软复位窗口的次数
constexpr int kForsenseIntervalMs  = 50;
constexpr int kForsenseSyncRetries = 20;     // 未探测到 BL 波特率时的兜底等待轮次
constexpr int kSyncTimeoutMs       = 1000;   // 同步 / 块重发前重同步的 ACK 等待
constexpr int kBlScanSyncTimeoutMs = 800;    // 探测 BL 波特率时单次试同步等待
constexpr int kEraseAckWaitMs      = 30000;  // 擦除 ACK 超时
constexpr int kBlockRetryMax       = 5;      // 单块烧写重发上限（量产版为 5）
constexpr int kBlockAckWaitMs      = 1000;   // 块 offset ACK 超时
constexpr int kUpgradeRounds       = 3;      // 擦除→烧写→校验整轮重试（仅 CRC 不一致才重试）
constexpr int kQuietBreakMs        = 100;    // 收到数据后静默多久视为读完整

qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

// 收数据摘要（hex + 可打印字符），便于区分 APP 数据流 / AT 应答 / BL 帧
QString hexAsciiDump(const QByteArray &d, int maxShow = 32)
{
    const int n = qMin(d.size(), maxShow);
    QString hex, ascii;
    for (int i = 0; i < n; ++i) {
        const unsigned char c = static_cast<unsigned char>(d.at(i));
        hex += QStringLiteral("%1 ").arg(int(c), 2, 16, QLatin1Char('0')).toUpper();
        ascii += (c >= 0x20 && c <= 0x7E) ? QLatin1Char(char(c)) : QLatin1Char('.');
    }
    return hex + QStringLiteral("| ") + ascii;
}

} // namespace

OtaWorker::OtaWorker(QObject *parent)
    : QObject(parent)
{
}

OtaWorker::~OtaWorker() = default;

// ═══════════════════════════════════════════════════
//  传输层（串口 / CAN）
// ═══════════════════════════════════════════════════

bool OtaWorker::openTransport(int baud)
{
    m_connected = false;
    m_transportError.clear();

    if (m_cfg.transport == Transport::Serial) {
        if (m_cfg.port.isEmpty()) {
            m_transportError = QStringLiteral("未选择串口");
            return false;
        }
        m_serial = new SerialConnection(this);
        connect(m_serial, &SerialConnection::dataReceived,
                this, &OtaWorker::onTransportData);
        connect(m_serial, &SerialConnection::connected,
                this, &OtaWorker::onTransportConnected);
        connect(m_serial, &SerialConnection::errorOccurred,
                this, &OtaWorker::onTransportError);
        // 8 数据位 / 无校验(0) / 1 停止位(0)
        m_serial->connectToPort(m_cfg.port, baud, 8, 0, 0, kConnectWaitMs);
    } else {
        m_can = new PcanConnection(this);
        connect(m_can, &PcanConnection::connected,
                this, &OtaWorker::onTransportConnected);
        connect(m_can, &PcanConnection::errorOccurred,
                this, &OtaWorker::onTransportError);
        connect(m_can, &PcanConnection::frameReceived,
                this, &OtaWorker::onCanFrame);
        m_can->connectToChannel(m_cfg.canChannel, m_cfg.canBaudCode);
    }
    return true;
}

void OtaWorker::closeTransport()
{
    if (m_serial) {
        m_serial->disconnectFromPort();
        m_serial->deleteLater();
        m_serial = nullptr;
    }
    if (m_can) {
        m_can->disconnectFromChannel();
        m_can->deleteLater();
        m_can = nullptr;
    }
}

void OtaWorker::tx(const QByteArray &frame)
{
    if (m_serial)
        m_serial->send(frame);
    else if (m_can)
        canSend(frame);
}

bool OtaWorker::setBaud(int baud)
{
    if (!m_serial)
        return false;
    m_serial->setBaudRate(baud);
    return true;
}

void OtaWorker::onTransportData(const QByteArray &data)
{
    m_rawBuf.append(data);
    for (char c : data)
        feedRxByte(static_cast<uint8_t>(c));
    // 长时间等待期间防止原始缓冲无限增长（正常 AT / ACK 应答都是短报文）
    if (m_rawBuf.size() > 8192)
        m_rawBuf = m_rawBuf.right(4096);
    m_lastRxMs = nowMs();
}

void OtaWorker::onTransportConnected()
{
    m_connected = true;
}

void OtaWorker::onTransportError(const QString &msg)
{
    m_transportError = msg;
}

void OtaWorker::onCanFrame(quint32 id, const QByteArray &data, quint8 msgType, quint64 timestampUs)
{
    Q_UNUSED(msgType);
    Q_UNUSED(timestampUs);
    // 只收升级上行报文（ID 由配置指定，待 CAN 升级协议文档确认）
    if (id != m_cfg.canRxId)
        return;
    m_canRxBuf.append(data);
}

// ═══════════════════════════════════════════════════
//  ACK 接收状态机（上行帧头 AA 55；移植自参考 imu_rx）
// ═══════════════════════════════════════════════════

void OtaWorker::resetAckParser()
{
    m_rxState = WaitSync1;
    m_rxId = m_rxLen = 0;
    m_rxPayload.clear();
    m_rxCount = 0;
    m_rxCrcAccum = 1;
    clearAcks();
}

static inline uint32_t rdLe32(const QByteArray &p, int off)
{
    return uint32_t(uint8_t(p[off]))
         | (uint32_t(uint8_t(p[off + 1])) << 8)
         | (uint32_t(uint8_t(p[off + 2])) << 16)
         | (uint32_t(uint8_t(p[off + 3])) << 24);
}

void OtaWorker::feedRxByte(uint8_t data)
{
    switch (m_rxState) {
    case WaitSync1:
        if (data == 0xAA) {
            m_rxState = WaitSync2;
            m_rxCrcAccum = OtaProtocol::crc32(&data, 1, 1);
        }
        break;
    case WaitSync2:
        if (data == 0x55) {
            m_rxCrcAccum = OtaProtocol::crc32(&data, 1, m_rxCrcAccum);
            m_rxState = WaitId1;
        } else if (data == 0xAA) {
            m_rxCrcAccum = OtaProtocol::crc32(&data, 1, 1);
        } else {
            m_rxState = WaitSync1;
        }
        break;
    case WaitId1:
        m_rxIdTmp = data;
        m_rxCrcAccum = OtaProtocol::crc32(&data, 1, m_rxCrcAccum);
        m_rxState = WaitId2;
        break;
    case WaitId2:
        m_rxId = uint16_t(data << 8) | m_rxIdTmp;  // 参考: id = data<<8 | id_temp
        m_rxCrcAccum = OtaProtocol::crc32(&data, 1, m_rxCrcAccum);
        m_rxState = WaitLen1;
        break;
    case WaitLen1:
        m_rxLenTmp = data;
        m_rxCrcAccum = OtaProtocol::crc32(&data, 1, m_rxCrcAccum);
        m_rxState = WaitLen2;
        break;
    case WaitLen2: {
        m_rxLen = uint16_t(data << 8) | m_rxLenTmp;
        m_rxCrcAccum = OtaProtocol::crc32(&data, 1, m_rxCrcAccum);
        if (m_rxLen > 0 && m_rxLen <= 1000) {
            m_rxCount = 0;
            m_rxPayload.clear();
            m_rxPayload.reserve(m_rxLen);
            m_rxState = Payload;
        } else {
            m_rxState = WaitSync1;
        }
        break;
    }
    case Payload:
        m_rxPayload.append(char(data));
        m_rxCrcAccum = OtaProtocol::crc32(&data, 1, m_rxCrcAccum);
        if (++m_rxCount == m_rxLen)
            m_rxState = WaitCrc1;
        break;
    case WaitCrc1:
        m_rxCrcTmp[0] = data;
        m_rxState = WaitCrc2;
        break;
    case WaitCrc2:
        m_rxCrcTmp[1] = data;
        m_rxState = WaitCrc3;
        break;
    case WaitCrc3:
        m_rxCrcTmp[2] = data;
        m_rxState = WaitCrc4;
        break;
    case WaitCrc4: {
        m_rxCrcTmp[3] = data;
        uint32_t crc = uint32_t(m_rxCrcTmp[0])
                     | (uint32_t(m_rxCrcTmp[1]) << 8)
                     | (uint32_t(m_rxCrcTmp[2]) << 16)
                     | (uint32_t(data) << 24);
        if (crc == m_rxCrcAccum && m_rxPayload.size() >= 8) {
            uint32_t word0 = rdLe32(m_rxPayload, 0);
            uint32_t word1 = rdLe32(m_rxPayload, 4);
            if (m_rxId == OtaProtocol::ACK_CMD) {
                // 参考实现：command(word0)==0x10 视为 OK
                if (word0 == OtaProtocol::OK)
                    m_ackOk = true;
            } else if (m_rxId == OtaProtocol::ACK_CRC) {
                m_crcAck = word1;
                m_gotCrcAck = true;
            } else if (m_rxId == OtaProtocol::ACK_OFFSET) {
                m_offsetAck = word1;
                m_gotOffsetAck = true;
            }
        }
        m_rxState = WaitSync1;
        break;
    }
    }
}

// ═══════════════════════════════════════════════════
//  可取消的等待 / 睡眠 / 原始读取
// ═══════════════════════════════════════════════════

bool OtaWorker::waitEvent(int timeoutMs, const std::function<bool()> &cond)
{
    if (cond())
        return true;
    QEventLoop loop;
    QTimer tick;
    tick.setInterval(50);
    connect(&tick, &QTimer::timeout, &loop, [&]() {
        if (m_cancel.load() || cond())
            loop.quit();
    });
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    tick.start();
    loop.exec();
    tick.stop();
    return !m_cancel.load() && cond();
}

bool OtaWorker::sleepMs(int ms)
{
    int waited = 0;
    while (waited < ms) {
        if (m_cancel.load())
            return false;
        int step = qMin(20, ms - waited);
        QThread::msleep(step);
        waited += step;
    }
    return true;
}

bool OtaWorker::waitConnected(int timeoutMs)
{
    // 打开失败（如串口被占用）会立刻置 m_transportError，不必干等满超时
    return waitEvent(timeoutMs, [this] {
        return m_connected || !m_transportError.isEmpty();
    });
}

// 收原始字节直到串口静默 quietMs（或整体超时 timeoutMs）；返回收到字节数，内容在 m_rawBuf
int OtaWorker::readQuiet(int timeoutMs, int quietMs)
{
    m_rawBuf.clear();
    m_lastRxMs = nowMs();
    waitEvent(timeoutMs, [this, quietMs] {
        return !m_rawBuf.isEmpty() && (nowMs() - m_lastRxMs) >= quietMs;
    });
    return m_rawBuf.size();
}

bool OtaWorker::sendText(const char *text)
{
    if (!m_serial)
        return false;
    m_serial->send(QByteArray(text));
    return true;
}

void OtaWorker::fail(const QString &reason)
{
    emit phaseChanged(Phase::Failed, reason);
    emit logMessage(QStringLiteral("✗ %1").arg(reason));
    closeTransport();
    emit finished(false, reason);
}

// ═══════════════════════════════════════════════════
//  AT 通道与波特率探测（量产版 SDK 的 detectAppBaud 族）
// ═══════════════════════════════════════════════════

// 连续发 AT+SETNO 让设备停止主动输出，直到串口安静
bool OtaWorker::atPrepareChannel()
{
    const qint64 deadline = nowMs() + kSetNoTimeoutMs;
    while (nowMs() < deadline) {
        if (m_cancel.load())
            return false;

        if (!sendText(OtaProtocol::kCmdSetNo))
            return false;
        readQuiet(kSetNoQuietMs, kQuietBreakMs);
        if (m_rawBuf.isEmpty())
            return true;

        // 设备对 SETNO 的应答（"OK\r\n"）也会被读到，不能把它误判成"还在输出"。
        // 判定：全部由可打印 ASCII / \r\n 组成且包含 "OK"，视为 SETNO 应答 = 通道已安静；
        // 只有读到非应答数据（设备主动上报的二进制帧）才算没压停。
        bool onlyAtReply = true;
        for (char c : m_rawBuf) {
            const unsigned char ch = static_cast<unsigned char>(c);
            if (ch == '\r' || ch == '\n' || (ch >= 0x20 && ch <= 0x7E))
                continue;
            onlyAtReply = false;
            break;
        }
        if (onlyAtReply && m_rawBuf.contains("OK"))
            return true;

        emit logMessage(QStringLiteral("[警告] AT+SETNO 后串口仍有 %1 字节数据输出，继续重发 | 前 %2 字节: %3")
                            .arg(m_rawBuf.size())
                            .arg(qMin(m_rawBuf.size(), 32))
                            .arg(hexAsciiDump(m_rawBuf)));
    }
    return false;
}

// 在给定波特率下试读 APP 版本（AT+VERSION），应答含 "+VERSION" 才算命中
bool OtaWorker::atProbe(int candidate)
{
    if (!setBaud(candidate))
        return false;
    if (!sleepMs(20))
        return false;

    if (!atPrepareChannel()) {
        m_setNoOk = false;
        return false;
    }
    m_setNoOk = true;

    if (!sendText(OtaProtocol::kCmdVersion))
        return false;
    readQuiet(300, kQuietBreakMs);

    // 命中标准必须含版本响应特征：只判"非空"会误判——波特率不匹配时收到的
    // 仍是持续流动的字节，缓冲非空但全是乱码。
    const QString probe = QString::fromLatin1(m_rawBuf);
    if (!probe.contains(QStringLiteral("+VERSION")))
        return false;
    m_versionBuf = probe;
    return true;
}

// 找 APP 波特率；APP 无应答则去找已处于 Bootloader 的设备
bool OtaWorker::detectAppBaud()
{
    for (int i = 0; i < kBaudCandidateCount; ++i) {
        if (m_cancel.load())
            return false;
        const int candidate = kBaudCandidates[i];
        if (atProbe(candidate)) {
            m_appBaud = candidate;
            m_appBaudKnown = true;
            m_baudDetected = true;
            m_inBl = false;
            emit logMessage(QStringLiteral("探测到 APP 波特率：%1").arg(candidate));
            return true;
        }
    }

    // 覆盖"本来就没有 APP"的情况；Bootloader 波特率同样不假定为 115200
    if (scanBlBaud()) {
        m_baudDetected = true;
        m_inBl = true;
        m_appBaudKnown = false;
        emit logMessage(QStringLiteral("设备已在 bootloader，波特率：%1").arg(m_blBaud));
        return true;
    }
    return false;
}

bool OtaWorker::probeAppBaudOnly()
{
    for (int i = 0; i < kBaudCandidateCount; ++i) {
        if (m_cancel.load())
            return false;
        const int candidate = kBaudCandidates[i];
        if (atProbe(candidate)) {
            m_appBaud = candidate;
            m_appBaudKnown = true;
            emit logMessage(QStringLiteral("APP 波特率：%1").arg(candidate));
            return true;
        }
    }
    m_appBaudKnown = false;
    return false;
}

// 逐个候选波特率短促探测，命中即记录 Bootloader 波特率
bool OtaWorker::scanBlBaud()
{
    for (int i = 0; i < kBaudCandidateCount; ++i) {
        if (m_cancel.load())
            return false;
        const int candidate = kBaudCandidates[i];
        if (!setBaud(candidate))
            return false;
        for (int k = 0; k < 5; ++k) {
            if (!sendText(OtaProtocol::kCmdStay))
                return false;
            if (!sleepMs(20))
                return false;
        }
        if (doSync(kBlScanSyncTimeoutMs)) {
            m_blBaud = candidate;
            return true;
        }
    }
    return false;
}

bool OtaWorker::readVersionInternal(int windowMs, QString *out)
{
    m_versionBuf.clear();

    // 自动模式：首次调用先探测 APP 波特率（探测过程中已收到版本应答）
    if (m_autoBaud && !m_baudDetected) {
        if (!detectAppBaud()) {
            emit logMessage(QStringLiteral("[错误] 未找到可通信的波特率：APP 无应答，bootloader 也未同步"));
            return false;
        }
        if (m_inBl) {
            emit logMessage(QStringLiteral("设备已在 bootloader，未读到 APP 版本"));
            emit versionRead(QString());
            if (out) out->clear();
            return true;
        }
        emit logMessage(QStringLiteral("版本应答：%1").arg(m_versionBuf.trimmed()));
        emit versionRead(m_versionBuf);
        if (out) *out = m_versionBuf;
        return true;
    }

    // 先让设备停止主动输出，确认串口安静后再读版本
    if (!atPrepareChannel()) {
        m_setNoOk = false;
        emit logMessage(QStringLiteral("[错误] AT+SETNO 后串口仍有数据输出，无法读取版本"));
        return false;
    }
    m_setNoOk = true;

    if (!sendText(OtaProtocol::kCmdVersion))
        return false;
    readQuiet(windowMs, kQuietBreakMs);
    m_versionBuf = QString::fromLatin1(m_rawBuf);
    emit logMessage(QStringLiteral("版本应答：%1").arg(m_versionBuf.trimmed()));
    emit versionRead(m_versionBuf);
    if (out) *out = m_versionBuf;
    return true;
}

// ═══════════════════════════════════════════════════
//  同步 / 擦除 / 烧写 / 校验 / 重启
// ═══════════════════════════════════════════════════

bool OtaWorker::getSyncAck(int timeoutMs)
{
    clearAcks();
    return waitEvent(timeoutMs, [this] { return m_ackOk; });
}

bool OtaWorker::doSync(int timeoutMs)
{
    const QByteArray syncCmd = OtaProtocol::buildLongCmd(OtaProtocol::GET_SYNC);
    for (int i = 0; i < 3; ++i) {
        tx(syncCmd);
        if (!sleepMs(20))
            return false;
    }
    return getSyncAck(timeoutMs);
}

// 进 BL：发进 BL 指令 → 探测 BL 波特率 → 盲发停留指令覆盖软复位窗口 → 同步
bool OtaWorker::enterBootloader()
{
    if (m_cfg.autoReset) {
        emit logMessage(QStringLiteral("使用 APP 当前波特率 %1 发送进入 bootloader 指令").arg(m_appBaud));
        const QByteArray enter = OtaProtocol::buildEnterBootCmd();
        for (int i = 0; i < 3; ++i) {
            tx(enter);
            if (!sleepMs(20))
                return false;
        }
        if (m_serial)
            m_serial->drainTx(500);   // 等 3 帧全部发出，避免切波特率时字节卡在半途
        if (!sleepMs(100))
            return false;
    } else {
        emit logMessage(QStringLiteral("手动模式：请确认设备已进入 Bootloader"));
    }

    emit logMessage(QStringLiteral("正在探测 bootloader 波特率..."));
    if (scanBlBaud()) {
        emit logMessage(QStringLiteral("bootloader 波特率：%1").arg(m_blBaud));
        if (m_cfg.autoReset) {
            // 覆盖软复位窗口：盲发停留指令，确保 bootloader 及时收到
            for (int i = 0; i < kForsenseWindowCount; ++i) {
                if (!sendText(OtaProtocol::kCmdStay))
                    return false;
                if (!sleepMs(kForsenseIntervalMs))
                    return false;
            }
        }
        if (doSync(kSyncTimeoutMs)) {
            emit logMessage(QStringLiteral("bootloader 同步成功"));
            return true;
        }
    }

    // 兜底：切到 115200 盲发停留指令，保留手动断电再上电的机会
    emit logMessage(QStringLiteral("未探测到 bootloader 波特率，改用 %1 继续等待")
                        .arg(OtaProtocol::kBootBaud));
    if (m_serial) {
        setBaud(OtaProtocol::kBootBaud);
        m_blBaud = OtaProtocol::kBootBaud;
    }
    for (int i = 0; i < kForsenseWindowCount; ++i) {
        if (!sendText(OtaProtocol::kCmdStay))
            return false;
        if (!sleepMs(kForsenseIntervalMs))
            return false;
    }
    for (int retry = 0; retry < kForsenseSyncRetries; ++retry) {
        if (m_cancel.load())
            return false;
        if (!sendText(OtaProtocol::kCmdStay))
            return false;
        if (!sleepMs(kForsenseIntervalMs))
            return false;
        if (doSync(kSyncTimeoutMs)) {
            emit logMessage(QStringLiteral("bootloader 同步成功"));
            return true;
        }
        emit logMessage(QStringLiteral("等待 bootloader 同步，可在此期间对设备断电再上电... (%1/%2)")
                            .arg(retry + 1).arg(kForsenseSyncRetries));
    }
    return false;
}

bool OtaWorker::eraseChip()
{
    clearAcks();
    emit logMessage(QStringLiteral("正在擦除，最长等待 %1 秒...").arg(kEraseAckWaitMs / 1000));
    tx(OtaProtocol::buildLongCmd(OtaProtocol::CHIP_ERASE));
    if (!sleepMs(2000))
        return false;
    if (!waitEvent(kEraseAckWaitMs, [this] { return m_ackOk; })) {
        emit logMessage(QStringLiteral("擦除应答超时"));
        return false;
    }
    emit logMessage(QStringLiteral("擦除成功"));
    return true;
}

bool OtaWorker::programFirmware(const QByteArray &fw)
{
    const uint32_t total = uint32_t(fw.size());
    uint32_t sent = 0;

    emit logMessage(QStringLiteral("正在写入（%1 字节，%2 字节/块），请稍候...")
                        .arg(total).arg(OtaProtocol::kBlockSize));

    while (sent < total) {
        if (m_cancel.load())
            return false;

        const uint32_t n = qMin<uint32_t>(uint32_t(OtaProtocol::kBlockSize), total - sent);
        const QByteArray frame = OtaProtocol::buildDataFrame(
            sent, uint16_t(n),
            reinterpret_cast<const uint8_t*>(fw.constData()) + sent);

        if (!sendBlockWithRetry(frame, sent))
            return false;

        sent += n;
        emit progress(qBound(0, int((quint64(sent) * 100) / total), 100));
    }
    emit logMessage(QStringLiteral("固件全部发送完成（%1 字节）").arg(total));
    return true;
}

// 发一块并等设备确认该 offset；未确认则重同步后重发，超 kBlockRetryMax 次失败
bool OtaWorker::sendBlockWithRetry(const QByteArray &frame, uint32_t offset)
{
    int retries = 0;
    while (true) {
        if (m_cancel.load())
            return false;

        clearAcks();
        tx(frame);
        if (!sleepMs(20))
            return false;
        if (waitEvent(kBlockAckWaitMs, [this, offset] {
                return m_gotOffsetAck && m_offsetAck == offset;
            }))
            return true;

        if (++retries > kBlockRetryMax) {
            emit logMessage(QStringLiteral("数据包确认超时：偏移 %1，已重试 %2 次")
                                .arg(offset).arg(kBlockRetryMax));
            return false;
        }
        emit logMessage(QStringLiteral("等待数据包确认：偏移 %1，重试 %2/%3")
                            .arg(offset).arg(retries).arg(kBlockRetryMax));
        if (!doSync(kSyncTimeoutMs))
            emit logMessage(QStringLiteral("重同步未确认，继续重发本块"));
    }
}

OtaWorker::CrcCheck OtaWorker::verifyCrc(uint32_t expectCrc)
{
    clearAcks();
    const QByteArray crcCmd = OtaProtocol::buildLongCmd(OtaProtocol::GET_CRC);
    for (int i = 0; i < 10; ++i) {
        tx(crcCmd);
        if (!sleepMs(20))
            return CrcCheck::NoAck;
    }

    emit logMessage(QStringLiteral("准备获取设备端 CRC"));
    if (!sleepMs(1000))
        return CrcCheck::NoAck;
    if (!waitEvent(1000, [this] { return m_gotCrcAck; })) {
        emit logMessage(QStringLiteral("未收到芯片 CRC 应答"));
        return CrcCheck::NoAck;
    }

    emit logMessage(QStringLiteral("文件名中的 CRC：%1，设备端 CRC：%2").arg(expectCrc).arg(m_crcAck));
    if (m_crcAck != expectCrc) {
        emit logMessage(QStringLiteral("CRC 校验不通过"));
        return CrcCheck::Mismatch;
    }
    emit logMessage(QStringLiteral("CRC 校验通过"));
    return CrcCheck::Ok;
}

void OtaWorker::rebootDevice()
{
    const QByteArray rebootCmd = OtaProtocol::buildLongCmd(OtaProtocol::REBOOT);
    for (int i = 0; i < 10; ++i) {
        tx(rebootCmd);
        if (!sleepMs(20))
            return;
    }
    emit logMessage(QStringLiteral("已发送重启指令，设备将运行新固件"));
}

// ═══════════════════════════════════════════════════
//  串口升级主流程
// ═══════════════════════════════════════════════════

void OtaWorker::runSerialUpgrade()
{
    m_autoBaud     = (m_cfg.appBaud == 0);
    m_appBaud      = m_autoBaud ? kBaudCandidates[0] : m_cfg.appBaud;
    m_appBaudKnown = !m_autoBaud;
    m_baudDetected = false;
    m_inBl         = false;
    m_setNoOk      = false;
    m_rawBuf.clear();

    // ── 0. 固件预检（纯本地，先于进入 BL：固件不合法时不把设备留在 Bootloader）──
    QFile fwFile(m_cfg.firmwarePath);
    if (!fwFile.open(QIODevice::ReadOnly)) {
        fail(QStringLiteral("无法打开固件文件：%1").arg(m_cfg.firmwarePath));
        return;
    }
    const QByteArray fw = fwFile.readAll();
    fwFile.close();

    const QString fileName = QFileInfo(m_cfg.firmwarePath).fileName();
    if (!fileName.endsWith(QStringLiteral(".firmware"), Qt::CaseInsensitive))
        emit logMessage(QStringLiteral("提示：固件扩展名不是 .firmware（继续执行）"));

    const uint32_t expectCrc = OtaProtocol::extractCrcFromFileName(fileName);
    if (expectCrc == 0) {
        fail(QStringLiteral("固件文件名缺少 #CRC<CRC32> 标识，无法做升级后校验"));
        return;
    }
    if (fw.size() <= 0 || fw.size() % 4 != 0) {
        fail(QStringLiteral("固件大小 %1 非法（必须为 4 的倍数）").arg(fw.size()));
        return;
    }
    emit logMessage(QStringLiteral("固件：%1，大小 %2 字节，文件名 CRC=%3")
                        .arg(fileName).arg(fw.size()).arg(expectCrc));
    QString crcErr;
    const uint32_t localCrc = OtaProtocol::crc32File(m_cfg.firmwarePath, &crcErr);
    if (localCrc != 0) {
        emit logMessage(QStringLiteral("本地计算 CRC=%1%2")
                            .arg(localCrc)
                            .arg(localCrc == expectCrc ? QStringLiteral("（与文件名一致）")
                                                       : QStringLiteral("（⚠ 与文件名不一致！）")));
    } else if (!crcErr.isEmpty()) {
        emit logMessage(QStringLiteral("本地 CRC 计算失败：%1").arg(crcErr));
    }

    // ── 1. 连接链路 ──
    emit phaseChanged(Phase::Connecting, QStringLiteral("连接设备..."));
    if (!openTransport(m_appBaud) || !waitConnected(kConnectWaitMs) || !m_connected) {
        fail(QStringLiteral("连接失败：%1").arg(
                 m_transportError.isEmpty() ? QStringLiteral("超时") : m_transportError));
        return;
    }
    emit logMessage(QStringLiteral("串口 %1 已打开（初始波特率 %2）")
                        .arg(m_cfg.port).arg(m_autoBaud ? kBaudCandidates[0] : m_cfg.appBaud));

    // ── 2. 读升级前版本（自动模式下同时完成 APP 波特率探测）──
    emit logMessage(QStringLiteral("[1/5] 进入 bootloader 并完成同步"));
    emit logMessage(QStringLiteral("读取升级前 IMU 版本"));
    readVersionInternal(600, nullptr);
    if (m_autoBaud && !m_baudDetected) {
        fail(QStringLiteral("无法确定可通信的波特率，已停止升级"));
        return;
    }
    // 已知 APP 波特率场景下，SETNO 没压停数据流就发进 BL 指令必然被淹没，
    // 直接判失败，避免后续同步干等浪费时间
    if (!m_inBl && !m_setNoOk) {
        fail(QStringLiteral("AT+SETNO 未压停数据流，进 bootloader 指令会被淹没，已放弃本轮升级"));
        return;
    }

    // ── 3. 进入 Bootloader 并同步 ──
    if (m_inBl)
        emit logMessage(QStringLiteral("设备已在 bootloader（波特率 %1），跳过进入指令").arg(m_blBaud));
    else
        emit phaseChanged(Phase::Resetting, QStringLiteral("发送进入指令，进入 Bootloader..."));
    emit phaseChanged(Phase::Handshaking, QStringLiteral("与 Bootloader 握手..."));
    if (!enterBootloader()) {
        fail(QStringLiteral("Bootloader 同步失败：请检查波特率，或对设备断电再上电后重试"));
        return;
    }

    // ── 4. 擦除 → 烧写 → 校验（仅 CRC 不一致才整轮重试）──
    bool ok = false;
    for (int round = 1; round <= kUpgradeRounds; ++round) {
        if (m_cancel.load()) { fail(QStringLiteral("已取消")); return; }

        if (round > 1) {
            emit logMessage(QStringLiteral("───── 第 %1 轮重试（CRC 未通过）─────").arg(round));
            if (!doSync(kSyncTimeoutMs)) {
                fail(QStringLiteral("重试前重新同步失败"));
                return;
            }
        }

        emit phaseChanged(Phase::Erasing, QStringLiteral("擦除 Flash..."));
        emit logMessage(QStringLiteral("[2/5] 擦除 APP 区域"));
        if (!eraseChip()) {
            fail(QStringLiteral("擦除失败，已停止升级"));
            return;
        }

        emit phaseChanged(Phase::Programming, QStringLiteral("烧写固件..."));
        emit progress(0);
        emit logMessage(QStringLiteral("[3/5] 写入固件"));
        if (!programFirmware(fw)) {
            fail(QStringLiteral("烧写中断，已停止升级；设备 APP 可能已不完整"));
            return;
        }

        emit phaseChanged(Phase::Verifying, QStringLiteral("CRC 校验..."));
        emit logMessage(QStringLiteral("[4/5] CRC 校验"));
        const CrcCheck rc = verifyCrc(expectCrc);
        if (rc != CrcCheck::Ok) {
            if (rc == CrcCheck::Mismatch) {
                emit logMessage(QStringLiteral("[警告] CRC 校验失败，将重新擦除并写入"));
                continue;
            }
            fail(QStringLiteral("CRC 通信失败，已停止升级"));
            return;
        }

        ok = true;
        break;
    }

    if (!ok) {
        fail(QStringLiteral("升级失败：擦除/烧写/校验重试 %1 轮仍未通过").arg(kUpgradeRounds));
        return;
    }

    // ── 5. 重启与收尾（成功以 CRC 为准，读版本只作参考）──
    if (!sleepMs(20)) { fail(QStringLiteral("已取消")); return; }
    emit progress(100);

    emit phaseChanged(Phase::Rebooting, QStringLiteral("重启设备..."));
    emit logMessage(QStringLiteral("[5/5] 发送重启指令，让设备运行新 APP"));
    rebootDevice();

    emit logMessage(QStringLiteral("等待设备运行新 APP（%1 秒）...").arg(kPostRebootWaitMs / 1000));
    if (!sleepMs(kPostRebootWaitMs)) { fail(QStringLiteral("已取消")); return; }

    if (!m_appBaudKnown) {
        emit logMessage(QStringLiteral("升级后重新探测 APP 波特率"));
        probeAppBaudOnly();
    }
    if (!m_appBaudKnown) {
        emit logMessage(QStringLiteral("[警告] 未能探测到升级后的 APP 波特率，跳过版本读取（不影响升级结果）"));
    } else {
        setBaud(m_appBaud);
        emit logMessage(QStringLiteral("读取升级后 IMU 版本"));
        QString ver;
        readVersionInternal(600, &ver);
        if (ver.trimmed().isEmpty())
            emit logMessage(QStringLiteral("[警告] 未能读到升级后的版本（不影响升级结果）"));
    }

    // 恢复设备主动输出（多发几次确保设备收到）
    emit logMessage(QStringLiteral("发送 AT+SETYES 恢复数据输出"));
    for (int i = 0; i < kSetYesCount; ++i) {
        if (!sendText(OtaProtocol::kCmdSetYes))
            break;
        if (i + 1 < kSetYesCount && !sleepMs(kSetYesIntervalMs))
            break;
    }

    closeTransport();
    emit phaseChanged(Phase::Done, QStringLiteral("升级成功"));
    emit logMessage(QStringLiteral("===== ✓ 升级成功，对设备重新上电后即可正常使用 ====="));
    emit finished(true, QStringLiteral("升级成功"));
}

// 只读版本（不擦写）：UI「读取版本」按钮走这里
void OtaWorker::runReadVersionOnly()
{
    m_autoBaud     = (m_cfg.appBaud == 0);
    m_appBaud      = m_autoBaud ? kBaudCandidates[0] : m_cfg.appBaud;
    m_appBaudKnown = !m_autoBaud;
    m_baudDetected = false;
    m_inBl         = false;
    m_setNoOk      = false;
    m_rawBuf.clear();

    emit logMessage(QStringLiteral("===== 读取设备版本 ====="));
    emit phaseChanged(Phase::Connecting, QStringLiteral("连接设备..."));
    if (!openTransport(m_appBaud) || !waitConnected(kConnectWaitMs) || !m_connected) {
        fail(QStringLiteral("连接失败：%1").arg(
                 m_transportError.isEmpty() ? QStringLiteral("超时") : m_transportError));
        return;
    }

    QString ver;
    readVersionInternal(600, &ver);
    closeTransport();

    if (ver.trimmed().isEmpty()) {
        const QString why = m_inBl ? QStringLiteral("设备处于 Bootloader，未读到 APP 版本")
                                   : QStringLiteral("未读到版本应答");
        emit phaseChanged(Phase::Failed, why);
        emit logMessage(QStringLiteral("✗ %1").arg(why));
        emit finished(false, why);
        return;
    }

    emit phaseChanged(Phase::Done, QStringLiteral("版本读取完成"));
    emit logMessage(QStringLiteral("===== ✓ 版本读取完成 ====="));
    emit finished(true, QStringLiteral("版本：%1").arg(ver.trimmed()));
}

// ═══════════════════════════════════════════════════
//  CAN 升级（预留骨架）
// ═══════════════════════════════════════════════════

void OtaWorker::canSend(const QByteArray &payload)
{
    if (!m_can)
        return;
    // TODO(CAN-OTA): 报文切分 / 流控规则待 CAN 升级协议文档确定；当前先按 8 字节/帧切分
    for (int off = 0; off < payload.size(); off += 8)
        m_can->send(m_cfg.canTxId, payload.mid(off, 8), m_cfg.canExtended);
}

bool OtaWorker::canEnterBoot()
{
    // TODO(CAN-OTA): 发送进入 Bootloader 报文并等待设备确认
    emit logMessage(QStringLiteral("[CAN] 进入 Bootloader 步骤未实现（预留接口）"));
    return false;
}

bool OtaWorker::canSyncBoot()
{
    // TODO(CAN-OTA): 握手/同步报文与应答判定
    emit logMessage(QStringLiteral("[CAN] 同步 Bootloader 步骤未实现（预留接口）"));
    return false;
}

bool OtaWorker::canErase()
{
    // TODO(CAN-OTA): 擦除报文与超时
    emit logMessage(QStringLiteral("[CAN] 擦除步骤未实现（预留接口）"));
    return false;
}

bool OtaWorker::canProgram(const QByteArray &fw)
{
    // TODO(CAN-OTA): 分包下发（切分/流控/块确认）
    emit logMessage(QStringLiteral("[CAN] 烧写步骤未实现（预留接口），固件 %1 字节").arg(fw.size()));
    return false;
}

bool OtaWorker::canVerifyCrc(uint32_t expectCrc)
{
    // TODO(CAN-OTA): 读回设备端 CRC 并比对
    emit logMessage(QStringLiteral("[CAN] 校验步骤未实现（预留接口），期望 CRC=%1").arg(expectCrc));
    return false;
}

bool OtaWorker::canReboot()
{
    // TODO(CAN-OTA): 重启报文
    emit logMessage(QStringLiteral("[CAN] 重启步骤未实现（预留接口）"));
    return false;
}

void OtaWorker::runCanUpgrade()
{
    // ── CAN 升级：与串口是两套流程（帧格式 / 寻址 / 时序均不同），
    //    协议文档确定前只做传输接线与阶段骨架；文档到位后只需填充 canXxx 各步实现，
    //    阶段切换 / 进度 / 日志 / 取消逻辑无需改动。──
    emit progress(0);
    emit logMessage(QStringLiteral("===== 开始 CAN OTA 升级 ====="));
    emit logMessage(QStringLiteral("提示：CAN 升级流程为预留接口，待协议文档确定后实现"));

    emit phaseChanged(Phase::Connecting, QStringLiteral("打开 CAN 通道..."));
    if (!openTransport(0) || !waitConnected(kConnectWaitMs) || !m_connected) {
        fail(QStringLiteral("连接失败：%1").arg(
                 m_transportError.isEmpty() ? QStringLiteral("超时") : m_transportError));
        return;
    }
    emit logMessage(QStringLiteral("CAN 通道已打开（channel=%1, bitrateCode=%2, txId=0x%3, rxId=0x%4）")
                        .arg(m_cfg.canChannel).arg(m_cfg.canBaudCode)
                        .arg(m_cfg.canTxId, 0, 16).arg(m_cfg.canRxId, 0, 16));

    const struct { const char *name; bool ok; } steps[] = {
        {"canEnterBoot", canEnterBoot()},
        {"canSyncBoot",  canSyncBoot()},
        {"canErase",     canErase()},
    };
    for (const auto &s : steps) {
        if (!s.ok) {
            fail(QStringLiteral("CAN 升级流程尚未实现（%1）：待 CAN 升级协议文档确定后填充")
                     .arg(QString::fromLatin1(s.name)));
            return;
        }
    }

    emit phaseChanged(Phase::Programming, QStringLiteral("烧写固件..."));
    if (!canProgram(QByteArray())) {
        fail(QStringLiteral("CAN 升级流程尚未实现（canProgram）：待 CAN 升级协议文档确定后填充"));
        return;
    }
    emit phaseChanged(Phase::Verifying, QStringLiteral("CRC 校验..."));
    if (!canVerifyCrc(0)) {
        fail(QStringLiteral("CAN 升级流程尚未实现（canVerifyCrc）：待 CAN 升级协议文档确定后填充"));
        return;
    }
    emit phaseChanged(Phase::Rebooting, QStringLiteral("重启设备..."));
    if (!canReboot()) {
        fail(QStringLiteral("CAN 升级流程尚未实现（canReboot）：待 CAN 升级协议文档确定后填充"));
        return;
    }

    closeTransport();
    emit progress(100);
    emit phaseChanged(Phase::Done, QStringLiteral("升级成功"));
    emit finished(true, QStringLiteral("升级成功"));
}

// ═══════════════════════════════════════════════════
//  入口
// ═══════════════════════════════════════════════════

void OtaWorker::start()
{
    m_cancel = false;
    resetAckParser();
    m_rawBuf.clear();
    m_lastRxMs = 0;

    emit progress(0);

    if (m_cfg.versionOnly) {
        runReadVersionOnly();
        return;
    }
    if (m_cfg.transport == Transport::Can) {
        runCanUpgrade();
        return;
    }
    runSerialUpgrade();
}

void OtaWorker::cancel()
{
    m_cancel = true;
}
