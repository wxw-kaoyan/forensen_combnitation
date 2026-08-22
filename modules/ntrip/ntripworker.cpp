#include "modules/ntrip/ntripworker.h"
#include "common/tcpconnection.h"
#include "modules/ntrip/datapool/datapool.h"

#include <QDateTime>

NTRIPWorker::NTRIPWorker(QObject *parent)
    : QObject(parent)
    , m_conn(new TcpConnection(this))
    , m_ggaTimer(new QTimer(this))
{
    // TcpConnection 信号 → NTRIP 业务逻辑
    connect(m_conn, &TcpConnection::connected,    this, &NTRIPWorker::onConnected);
    connect(m_conn, &TcpConnection::disconnected, this, &NTRIPWorker::onDisconnected);
    connect(m_conn, &TcpConnection::dataReceived, this, &NTRIPWorker::onDataReceived);
    connect(m_conn, &TcpConnection::errorOccurred, this, &NTRIPWorker::onConnectionError);

    m_ggaTimer->setInterval(1000); // 1 Hz
    connect(m_ggaTimer, &QTimer::timeout, this, &NTRIPWorker::sendGGA);
}

NTRIPWorker::~NTRIPWorker()
{
    stopGgaTimer();
}

// ---- 连接 / 断开 ----

void NTRIPWorker::start(const QString &host, quint16 port,
                         const QString &mountpoint,
                         const QString &user, const QString &password)
{
    m_host       = host;
    m_port       = port;
    m_mountpoint = mountpoint;
    m_user       = user;
    m_password   = password;

    m_hasReceivedHeader = false;
    m_ggaSent = false;
    m_headerBuffer.clear();

    // 重置速率统计
    m_bytesInInterval = 0;
    m_totalBytes = 0;

    emit statusChanged(QStringLiteral("[1] 正在连接 %1:%2 ...").arg(host).arg(port));
    m_conn->connectToHost(host, static_cast<int>(port));
}

void NTRIPWorker::stop()
{
    stopGgaTimer();
    m_ggaSent = false;
    m_hasReceivedHeader = false;
    m_headerBuffer.clear();

    if (m_connected)
        emit statusChanged(QStringLiteral("正在断开..."));
    m_conn->disconnectFromHost();
}

void NTRIPWorker::setGgaPosition(double lon, double lat, double height)
{
    m_ggaLon = lon;
    m_ggaLat = lat;
    m_ggaHeight = height;
}

void NTRIPWorker::setUseFixGGA(bool enabled)
{
    m_useFixGGA = enabled;
    if (enabled) {
        if (m_connected && m_ggaSent)
            m_ggaTimer->start();
    } else {
        stopGgaTimer();
    }
}

void NTRIPWorker::forwardGGA(const QByteArray &nmeaSentence)
{
    if (!m_connected) return;
    m_conn->send(nmeaSentence);
    if (!nmeaSentence.endsWith("\r\n"))
        m_conn->send(QByteArrayLiteral("\r\n"));
}

// ---- TcpConnection 槽 ----

void NTRIPWorker::onConnected()
{
    m_connected = true;
    emit connected();
    emit statusChanged(QStringLiteral("[2] TCP已连接，发送NTRIP HTTP请求..."));

    sendNtripRequest();

    if (m_useFixGGA)
        sendGGA();

    // 启动速率统计（单次定时器链）
    m_bytesInInterval = 0;
    m_totalBytes = 0;
    QTimer::singleShot(1000, this, &NTRIPWorker::onRateTick);
}

void NTRIPWorker::onDisconnected()
{
    m_connected = false;
    stopGgaTimer();
    m_ggaSent = false;
    emit disconnected();
    emit statusChanged(QStringLiteral("已断开"));
}

void NTRIPWorker::onDataReceived(const QByteArray &raw)
{
    // 阶段 1：解析 HTTP/ICY 响应头
    if (!m_hasReceivedHeader) {
        m_headerBuffer.append(raw);

        // 检查标准 HTTP \r\n\r\n（双换行）
        int headerEnd = m_headerBuffer.indexOf("\r\n\r\n");
        if (headerEnd >= 0) {
            m_hasReceivedHeader = true;
            QByteArray header = m_headerBuffer.left(headerEnd);
            emit statusChanged(QStringLiteral("[5] HTTP响应 (标准):\n") + header);

            if (!header.contains("200")) {
                emit errorOccurred(QStringLiteral("NTRIP 服务器返回错误:\n") + header);
                m_conn->disconnectFromHost();
                return;
            }

            QByteArray body = m_headerBuffer.mid(headerEnd + 4);
            m_headerBuffer.clear();
            if (!body.isEmpty())
                emit dataReady(body);
            return;
        }

        // 检查 ICY 协议（单行 \r\n，如 "ICY 200 OK\r\n"）
        int lineEnd = m_headerBuffer.indexOf("\r\n");
        if (lineEnd >= 0) {
            QByteArray firstLine = m_headerBuffer.left(lineEnd);
            if (firstLine.contains("200") && firstLine.size() < 80) {
                m_hasReceivedHeader = true;
                emit statusChanged(QStringLiteral("[5] ICY响应: ") + firstLine);

                QByteArray body = m_headerBuffer.mid(lineEnd + 2);
                m_headerBuffer.clear();
                if (!body.isEmpty())
                    emit dataReady(body);
                return;
            }
        }

        emit statusChanged(QStringLiteral("[4] 等待HTTP响应头... (已收到 %1 字节)").arg(m_headerBuffer.size()));
        return;
    }

    // 阶段 2：RTCM 数据流
    m_bytesInInterval += raw.size();
    m_totalBytes += raw.size();
    // 直接写入 DataPool，避免跨线程跳转（DataPool 内部有 QReadWriteLock，线程安全）
    if (m_dataPool)
        m_dataPool->feed(raw);
    emit dataReady(raw);
}

void NTRIPWorker::onConnectionError(const QString &err)
{
    m_connected = false;
    emit errorOccurred(err);
}

// ---- NTRIP 协议 ----

void NTRIPWorker::sendNtripRequest()
{
    QByteArray auth = (m_user + ":" + m_password).toUtf8().toBase64();

    QString req =
        QStringLiteral(
            "GET /%1 HTTP/1.0\r\n"
            "Host: %2\r\n"
            "User-Agent: NTRIP TASoftware/1.0\r\n"
            "Authorization: Basic %3\r\n"
            "\r\n")
        .arg(m_mountpoint)
        .arg(m_host + ":" + QString::number(m_port))
        .arg(QString::fromLatin1(auth));

    emit statusChanged(QStringLiteral("[3] HTTP请求:\n") + req);
    m_conn->send(req.toUtf8());
}

void NTRIPWorker::sendGGA()
{
    if (!m_connected) return;

    QString lat = toNmeaLat(m_ggaLat);
    QString lon = toNmeaLon(m_ggaLon);
    QString alt = QString::number(m_ggaHeight, 'f', 1);

    QByteArray ggaBody = QStringLiteral("GPGGA,000000.00,%1,%2,0,00,99.9,%3,M,0.0,M,,")
                             .arg(lat, lon, alt)
                             .toUtf8();
    QByteArray checksum = nmeaChecksum(ggaBody);
    QByteArray gga = "$" + ggaBody + "*" + checksum + "\r\n";

    m_conn->send(gga);

    if (!m_ggaSent) {
        m_ggaSent = true;
        emit statusChanged(
            QStringLiteral("[7] FixGGA 模式 (lon=%1 lat=%2 alt=%3)，1Hz 定时器启动")
                .arg(QString::number(m_ggaLon, 'f', 5))
                .arg(QString::number(m_ggaLat, 'f', 5))
                .arg(alt));

        if (!m_ggaTimer->isActive())
            startGgaTimer();
    }
}

QByteArray NTRIPWorker::nmeaChecksum(const QByteArray &sentence)
{
    quint8 cs = 0;
    for (int i = 0; i < sentence.size(); ++i)
        cs ^= static_cast<quint8>(sentence[i]);
    return QByteArray::number(cs, 16).toUpper().rightJustified(2, '0');
}

void NTRIPWorker::startGgaTimer()
{
    m_ggaTimer->start();
    emit statusChanged(QStringLiteral("[8] GGA 1Hz 定时发送已启动"));
}

void NTRIPWorker::stopGgaTimer()
{
    if (m_ggaTimer->isActive())
        m_ggaTimer->stop();
}

QString NTRIPWorker::toNmeaLat(double deg)
{
    double adeg = qAbs(deg);
    int d = static_cast<int>(adeg);
    double m = (adeg - d) * 60.0;
    return QStringLiteral("%1%2,%3")
        .arg(d, 2, 10, QLatin1Char('0'))
        .arg(m, 7, 'f', 4, QLatin1Char('0'))
        .arg(deg >= 0 ? QLatin1Char('N') : QLatin1Char('S'));
}

QString NTRIPWorker::toNmeaLon(double deg)
{
    double adeg = qAbs(deg);
    int d = static_cast<int>(adeg);
    double m = (adeg - d) * 60.0;
    return QStringLiteral("%1%2,%3")
        .arg(d, 3, 10, QLatin1Char('0'))
        .arg(m, 7, 'f', 4, QLatin1Char('0'))
        .arg(deg >= 0 ? QLatin1Char('E') : QLatin1Char('W'));
}

// ---- 速率统计（1Hz 定时器回调）----

void NTRIPWorker::onRateTick()
{
    if (m_bytesInInterval < 1024) {
        emit dataRateChanged(
            QStringLiteral("%1 B/s").arg(m_bytesInInterval),
            m_totalBytes);
    } else if (m_bytesInInterval < 1024 * 1024) {
        emit dataRateChanged(
            QStringLiteral("%1 KB/s").arg(m_bytesInInterval / 1024.0, 0, 'f', 1),
            m_totalBytes);
    } else {
        emit dataRateChanged(
            QStringLiteral("%1 MB/s").arg(m_bytesInInterval / (1024.0 * 1024.0), 0, 'f', 2),
            m_totalBytes);
    }

    m_bytesInInterval = 0;
    QTimer::singleShot(1000, this, &NTRIPWorker::onRateTick);
}

// ---- 转发目标管理（工作线程内执行）----

void NTRIPWorker::setDataPool(DataPool *pool)
{
    m_dataPool = pool;
}

void NTRIPWorker::addForwardTarget(const ForwardTarget::Config &cfg)
{
    if (!m_dataPool) return;

    ForwardTarget *ft = ForwardTarget::create(cfg, m_dataPool, this);
    int index = m_forwarders.size();
    m_forwarders.append(ft);

    // 状态回传（跨线程 → 主线程 UI）
    connect(ft, &ForwardTarget::statusChanged, this,
            [this, index](const QString &s) { emit forwarderStatusChanged(index, s); });

    // GGA 源 → 直接调 forwardGGA（同线程，不走 invokeMethod）
    connect(ft, &ForwardTarget::ggaDataReceived, this, &NTRIPWorker::forwardGGA);

    // 原始数据 → 代理给 UI（跨线程）
    connect(ft, &ForwardTarget::rawDataReceived, this,
            [this, index](const QByteArray &data) { emit forwarderRawData(index, data); });

    // 保存状态 → 代理给 UI
    connect(ft, &ForwardTarget::saveStateChanged, this,
            [this, index](bool saving) { emit forwarderSaveStateChanged(index, saving); });

    emit forwarderAdded(index, cfg);
    emit forwarderStatusChanged(index, QStringLiteral("未启动"));
}

void NTRIPWorker::removeForwardTarget(int index)
{
    if (index < 0 || index >= m_forwarders.size()) return;

    ForwardTarget *ft = m_forwarders[index];
    ft->stop();
    m_forwarders.removeAt(index);

    emit forwarderRemoved(index);
    ft->deleteLater();
}

void NTRIPWorker::startForwardTarget(int index)
{
    if (index < 0 || index >= m_forwarders.size()) return;
    m_forwarders[index]->start();
}

void NTRIPWorker::stopForwardTarget(int index)
{
    if (index < 0 || index >= m_forwarders.size()) return;
    m_forwarders[index]->stop();
}

void NTRIPWorker::setForwardTargetGgaSource(int index, bool enabled)
{
    if (index < 0 || index >= m_forwarders.size()) return;
    m_forwarders[index]->setGgaSource(enabled);
}

void NTRIPWorker::enableForwarderSave(int index, const QString &filePath)
{
    if (index < 0 || index >= m_forwarders.size()) return;
    m_forwarders[index]->enableSave(filePath);
}

void NTRIPWorker::disableForwarderSave(int index)
{
    if (index < 0 || index >= m_forwarders.size()) return;
    m_forwarders[index]->disableSave();
}
