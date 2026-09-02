#ifndef FORWARDTARGET_H
#define FORWARDTARGET_H

#include <QObject>
#include <QByteArray>

class DataPool;
class QTcpSocket;
class QSerialPort;
class QTimer;
class QFile;

// ---- 转发目标基类 ----
// 每个转发目标是一个 DataPool 消费者：从池中拉取 RTCM 数据 → 推送到外部设备
// 若被标记为 GGA 源，则逆向接收设备的 $GPGGA → 转给 NTRIP caster
class ForwardTarget : public QObject
{
    Q_OBJECT

public:
    enum Type { TCP, Serial };

    struct Config {
        Type   type        = TCP;
        QString address;     // TCP: host, Serial: COM port name
        int    portOrBaud = 0; // TCP: port number, Serial: baud rate
        bool   isGgaSource = false;
        // 串口专用参数
        int    dataBits   = 8;  // 5/6/7/8
        int    parity     = 0;  // 0=无校验 2=偶校验 3=奇校验 (QSerialPort::Parity)
        int    stopBits   = 1;  // 1=1位 2=2位 3=1.5位 (QSerialPort::StopBits)
    };

    // 工厂方法
    static ForwardTarget *create(const Config &cfg, DataPool *pool,
                                 QObject *parent = nullptr);

    Config config() const { return m_config; }
    bool   isRunning() const { return m_running; }
    void   setGgaSource(bool enabled);

    virtual void start() = 0;
    virtual void stop()  = 0;

    // 数据保存（保存客户端收到的非 GGA 数据）
    void enableSave(const QString &filePath);
    void disableSave();
    bool isSaving() const { return m_saveFile != nullptr; }

signals:
    void statusChanged(const QString &status);
    void errorOccurred(const QString &err);
    void ggaDataReceived(const QByteArray &nmea);   // GGA 源模式时发出
    void rawDataReceived(const QByteArray &data);    // 设备原始数据（用于调试查看）
    void saveStateChanged(bool saving);

protected:
    ForwardTarget(const Config &cfg, DataPool *pool, QObject *parent);
    void readFromPool(); // 从 DataPool 拉取并写入输出设备
    void saveData(const QByteArray &raw); // 过滤 GGA 后保存
    QList<QByteArray> extractGgaLines(QByteArray &buffer); // 从缓冲中提取完整 GGA 行

    Config    m_config;
    DataPool *m_pool  = nullptr;
    int       m_consumerId = -1;
    bool      m_running = false;

    QByteArray m_rxBuffer; // 接收缓冲：跨 readyRead 分片拼接未完成的行

    // 数据保存
    QFile *m_saveFile = nullptr;
};


// ---- TCP 转发目标 ----
class TcpForwardTarget : public ForwardTarget
{
    Q_OBJECT

public:
    explicit TcpForwardTarget(const Config &cfg, DataPool *pool,
                              QObject *parent = nullptr);
    ~TcpForwardTarget() override;

    void start() override;
    void stop()  override;

private slots:
    void onPoolData();
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onConnectTimeout();

private:
    QTcpSocket *m_socket = nullptr;
    QTimer    *m_connectTimer = nullptr;
};


// ---- 串口转发目标 ----
class SerialForwardTarget : public ForwardTarget
{
    Q_OBJECT

public:
    explicit SerialForwardTarget(const Config &cfg, DataPool *pool,
                                 QObject *parent = nullptr);
    ~SerialForwardTarget() override;

    void start() override;
    void stop()  override;

private slots:
    void onPoolData();
    void onReadyRead();

private:
    QSerialPort *m_serial = nullptr;
};

Q_DECLARE_METATYPE(ForwardTarget::Config)

#endif // FORWARDTARGET_H
