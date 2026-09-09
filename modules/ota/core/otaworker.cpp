#include "modules/ota/core/otaworker.h"
#include "common/serialconnection.h"
#include "common/tcpconnection.h"

#include <QThread>
#include <QEventLoop>
#include <QTimer>
#include <QFile>
#include <QFileInfo>

// ════════════════════════════════════════════════════════════════
//  流程常量（提取自参考 Linux_UATR_OTA/uploadSDK.c 中的硬编码值）
// ════════════════════════════════════════════════════════════════
namespace {
constexpr int kSyncRoundsMax    = 30;    // 握手总轮次上限
constexpr int kEraseRetries     = 3;     // 擦除重试次数
constexpr int kEraseWaitMs      = 30000; // 擦除 ACK 超时 30s
constexpr int kBlockRetryMax    = 100;   // 单块烧写重试上限
constexpr int kBlockAckWaitMs   = 1000;  // 块 offset ACK 超时
constexpr int kUpgradeRounds    = 3;     // 擦除→烧写→校验 整轮重试
constexpr int kConnectWaitMs    = 8000;
}

OtaWorker::OtaWorker(QObject *parent)
    : QObject(parent)
{
}

OtaWorker::~OtaWorker() = default;

// ═══════════════════════════════════════════════════
//  传输层
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
        if (m_cfg.host.isEmpty() || m_cfg.tcpPort <= 0) {
            m_transportError = QStringLiteral("TCP 地址或端口无效");
            return false;
        }
        m_tcp = new TcpConnection(this);
        connect(m_tcp, &TcpConnection::dataReceived,
                this, &OtaWorker::onTransportData);
        connect(m_tcp, &TcpConnection::connected,
                this, &OtaWorker::onTransportConnected);
        connect(m_tcp, &TcpConnection::errorOccurred,
                this, &OtaWorker::onTransportError);
        m_tcp->connectToHost(m_cfg.host, m_cfg.tcpPort, kConnectWaitMs);
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
    if (m_tcp) {
        m_tcp->disconnectFromHost();
        m_tcp->deleteLater();
        m_tcp = nullptr;
    }
}

void OtaWorker::tx(const QByteArray &frame)
{
    if (m_serial)
        m_serial->send(frame);
    else if (m_tcp)
        m_tcp->send(frame);
}

void OtaWorker::onTransportData(const QByteArray &data)
{
    for (char c : data)
        feedRxByte(static_cast<uint8_t>(c));
}

void OtaWorker::onTransportConnected()
{
    m_connected = true;
}

void OtaWorker::onTransportError(const QString &msg)
{
    m_transportError = msg;
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
        if (m_rxLen > 0 && m_rxLen < 10000) {
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
//  可取消的等待 / 睡眠
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
    return waitEvent(timeoutMs, [this] { return m_connected; });
}

// ═══════════════════════════════════════════════════
//  升级流程各阶段
// ═══════════════════════════════════════════════════

// 握手：握手串 ×10 → GET_SYNC ×3 → 等 0x0064/OK
bool OtaWorker::syncWithBootloader(int maxRounds)
{
    const QByteArray hs = OtaProtocol::handshakeString();
    const QByteArray syncCmd = OtaProtocol::buildLongCmd(OtaProtocol::GET_SYNC);

    for (int round = 0; round < maxRounds; ++round) {
        if (m_cancel.load())
            return false;

        for (int i = 0; i < 10; ++i) {
            tx(hs);
            if (!sleepMs(50)) return false;
        }
        clearAcks();
        for (int i = 0; i < 3; ++i) {
            tx(syncCmd);
            if (!sleepMs(20)) return false;
        }
        if (waitEvent(1000, [this] { return m_ackOk; })) {
            emit logMessage(QStringLiteral("Bootloader 同步成功"));
            return true;
        }
        emit logMessage(QStringLiteral("等待 Bootloader 同步... (%1/%2)")
                            .arg(round + 1).arg(maxRounds));
    }
    return false;
}

bool OtaWorker::eraseChip()
{
    const QByteArray eraseCmd = OtaProtocol::buildLongCmd(OtaProtocol::CHIP_ERASE);
    for (int tries = 1; tries <= kEraseRetries; ++tries) {
        if (m_cancel.load())
            return false;
        clearAcks();
        tx(eraseCmd);
        emit logMessage(QStringLiteral("开始擦除 Flash（第 %1 次）...").arg(tries));
        if (!sleepMs(2000))
            return false;
        if (waitEvent(kEraseWaitMs, [this] { return m_ackOk; })) {
            emit logMessage(QStringLiteral("擦除成功"));
            return true;
        }
        emit logMessage(QStringLiteral("擦除应答超时，重试..."));
    }
    return false;
}

bool OtaWorker::programFirmware(const QByteArray &fw)
{
    const uint32_t total = uint32_t(fw.size());
    uint32_t sent = 0;

    while (sent < total) {
        if (m_cancel.load())
            return false;

        uint32_t n = qMin<uint32_t>(OtaProtocol::kBlockSize, total - sent);
        QByteArray frame = OtaProtocol::buildDataFrame(
            sent, uint16_t(n),
            reinterpret_cast<const uint8_t*>(fw.constData()) + sent);

        int retries = 0;
        while (true) {
            if (m_cancel.load())
                return false;
            clearAcks();
            tx(frame);
            if (!sleepMs(20))
                return false;
            if (waitEvent(kBlockAckWaitMs,
                          [this, sent] { return m_gotOffsetAck && m_offsetAck == sent; }))
                break;

            if (++retries > kBlockRetryMax) {
                emit logMessage(QStringLiteral("烧写失败：offset %1 重试超过 %2 次")
                                    .arg(sent).arg(kBlockRetryMax));
                return false;
            }
            emit logMessage(QStringLiteral("offset %1 未确认，重同步后重发（第 %2 次）")
                                .arg(sent).arg(retries));
            syncWithBootloader(2);
        }

        sent += n;
        int percent = int((uint64_t(sent) * 100) / total);
        emit progress(qBound(0, percent, 100));
    }
    emit logMessage(QStringLiteral("固件全部发送完成（%1 字节）").arg(total));
    return true;
}

bool OtaWorker::verifyCrc(uint32_t expectCrc)
{
    const QByteArray crcCmd = OtaProtocol::buildLongCmd(OtaProtocol::GET_CRC);
    clearAcks();
    for (int i = 0; i < 10; ++i) {
        tx(crcCmd);
        if (!sleepMs(20))
            return false;
    }
    emit logMessage(QStringLiteral("等待芯片回算 CRC..."));
    if (!sleepMs(1000))
        return false;
    if (!waitEvent(3000, [this] { return m_gotCrcAck; })) {
        emit logMessage(QStringLiteral("未收到芯片 CRC 应答"));
        return false;
    }
    emit logMessage(QStringLiteral("芯片 CRC=%1，文件名预期 CRC=%2")
                        .arg(m_crcAck).arg(expectCrc));
    if (m_crcAck != expectCrc) {
        emit logMessage(QStringLiteral("CRC 校验不通过"));
        return false;
    }
    emit logMessage(QStringLiteral("CRC 校验通过"));
    return true;
}

void OtaWorker::rebootDevice()
{
    const QByteArray rebootCmd = OtaProtocol::buildLongCmd(OtaProtocol::REBOOT);
    for (int i = 0; i < 10; ++i) {
        tx(rebootCmd);
        sleepMs(20);
    }
    emit logMessage(QStringLiteral("已发送重启命令，设备将运行新固件"));
}

void OtaWorker::fail(const QString &reason)
{
    emit phaseChanged(Phase::Failed, reason);
    emit logMessage(QStringLiteral("✗ %1").arg(reason));
    closeTransport();
    emit finished(false, reason);
}

// ═══════════════════════════════════════════════════
//  升级主入口（worker 线程）
// ═══════════════════════════════════════════════════

void OtaWorker::start()
{
    m_cancel = false;
    resetAckParser();

    emit progress(0);
    emit logMessage(QStringLiteral("===== 开始 OTA 升级 ====="));

    // ── 1. 连接传输链路 ──
    emit phaseChanged(Phase::Connecting, QStringLiteral("连接设备..."));
    if (!openTransport(m_cfg.baud) || !waitConnected(kConnectWaitMs)) {
        fail(QStringLiteral("连接失败：%1").arg(
                 m_transportError.isEmpty() ? QStringLiteral("超时") : m_transportError));
        return;
    }
    emit logMessage(m_cfg.transport == Transport::Serial
                        ? QStringLiteral("串口 %1 已打开 @%2").arg(m_cfg.port).arg(m_cfg.baud)
                        : QStringLiteral("TCP 已连接 %1:%2").arg(m_cfg.host).arg(m_cfg.tcpPort));

    // ── 2. 进入 Bootloader ──
    if (m_cfg.autoReset) {
        emit phaseChanged(Phase::Resetting, QStringLiteral("发送复位指令，进入 Bootloader..."));
        const QByteArray magic = OtaProtocol::buildResetMagic();
        for (int i = 0; i < 3; ++i) {
            tx(magic);
            sleepMs(50);
        }
        emit logMessage(QStringLiteral("已发送硬复位魔术帧 ×3，等待芯片复位..."));
        if (!sleepMs(2000)) { fail(QStringLiteral("已取消")); return; }

        if (m_cfg.transport == Transport::Serial) {
            // BL 固定 115200：关闭后重新打开
            closeTransport();
            emit logMessage(QStringLiteral("切换波特率为 Bootloader 固定速率 %1...")
                                .arg(OtaProtocol::kBootBaud));
            if (!openTransport(OtaProtocol::kBootBaud) || !waitConnected(kConnectWaitMs)) {
                fail(QStringLiteral("切换 %1 波特率失败").arg(OtaProtocol::kBootBaud));
                return;
            }
        }
    } else {
        emit logMessage(QStringLiteral("手动模式：请确认设备已进入 Bootloader（串口 %1）")
                            .arg(OtaProtocol::kBootBaud));
    }

    // ── 3. Bootloader 握手 ──
    emit phaseChanged(Phase::Handshaking, QStringLiteral("与 Bootloader 握手..."));
    if (!syncWithBootloader(kSyncRoundsMax)) {
        fail(QStringLiteral("Bootloader 同步失败：请检查波特率，或对设备重新上电后重试"));
        return;
    }

    // ── 4. 加载固件 ──
    QFile fwFile(m_cfg.firmwarePath);
    if (!fwFile.open(QIODevice::ReadOnly)) {
        fail(QStringLiteral("无法打开固件文件：%1").arg(m_cfg.firmwarePath));
        return;
    }
    QByteArray fw = fwFile.readAll();
    fwFile.close();

    const QString fileName = QFileInfo(m_cfg.firmwarePath).fileName();
    if (!fileName.endsWith(QStringLiteral(".firmware"), Qt::CaseInsensitive))
        emit logMessage(QStringLiteral("提示：固件扩展名不是 .firmware（继续执行）"));

    uint32_t expectCrc = OtaProtocol::extractCrcFromFileName(fileName);
    if (expectCrc == 0) {
        fail(QStringLiteral("固件文件名缺少 #CRC<CRC32> 标识，无法做升级后校验"));
        return;
    }
    if (fw.size() % 4 != 0) {
        fail(QStringLiteral("固件大小 %1 不是 4 的倍数").arg(fw.size()));
        return;
    }
    emit logMessage(QStringLiteral("固件：%1，大小 %2 字节，文件名 CRC=%3")
                        .arg(fileName).arg(fw.size()).arg(expectCrc));
    QString crcErr;
    uint32_t localCrc = OtaProtocol::crc32File(m_cfg.firmwarePath, &crcErr);
    if (localCrc != 0) {
        emit logMessage(QStringLiteral("本地计算 CRC=%1%2")
                            .arg(localCrc)
                            .arg(localCrc == expectCrc ? QStringLiteral("（与文件名一致）")
                                                       : QStringLiteral("（⚠ 与文件名不一致！）")));
    }

    // ── 5. 擦除 → 烧写 → 校验，最多 kUpgradeRounds 轮 ──
    bool ok = false;
    for (int round = 1; round <= kUpgradeRounds; ++round) {
        if (m_cancel.load()) { fail(QStringLiteral("已取消")); return; }
        if (round > 1) {
            emit logMessage(QStringLiteral("───── 第 %1 轮升级重试 ─────").arg(round));
            if (!syncWithBootloader(5)) {
                fail(QStringLiteral("重试前重新同步失败"));
                return;
            }
        }

        emit phaseChanged(Phase::Erasing, QStringLiteral("擦除 Flash..."));
        if (!eraseChip()) {
            emit logMessage(QStringLiteral("擦除阶段失败，重新开始本轮"));
            continue;
        }

        emit phaseChanged(Phase::Programming, QStringLiteral("烧写固件..."));
        emit progress(0);
        if (!programFirmware(fw)) {
            emit logMessage(QStringLiteral("烧写阶段失败，重新开始本轮"));
            continue;
        }

        emit phaseChanged(Phase::Verifying, QStringLiteral("CRC 校验..."));
        if (!verifyCrc(expectCrc)) {
            emit logMessage(QStringLiteral("校验阶段失败，重新开始本轮"));
            continue;
        }

        ok = true;
        break;
    }

    if (!ok) {
        fail(QStringLiteral("升级失败：擦除/烧写/校验重试 %1 轮仍未通过").arg(kUpgradeRounds));
        return;
    }

    // ── 6. 重启 ──
    emit phaseChanged(Phase::Rebooting, QStringLiteral("重启设备..."));
    rebootDevice();
    closeTransport();

    emit progress(100);
    emit phaseChanged(Phase::Done, QStringLiteral("升级成功"));
    emit logMessage(QStringLiteral("===== ✓ 升级成功，对设备重新上电后即可正常使用 ====="));
    emit finished(true, QStringLiteral("升级成功"));
}

void OtaWorker::cancel()
{
    m_cancel = true;
}
