#ifndef PCANCONNECTION_H
#define PCANCONNECTION_H

#include <QList>
#include <QObject>

class QTimer;

// 通用 PCAN 连接（PEAK PCAN-Basic API）
// 运行时动态加载 PCANBasic.dll（随 PEAK 驱动安装，位于 System32），
// 不需要链接导入库；驱动未安装时 isDriverAvailable() 为 false 并报错降级。
// 与 SerialConnection / TcpConnection 相同的使用方式：本对象需放入工作线程
// （接收无回调 API，靠定时器轮询排空接收队列）。
class PcanConnection : public QObject
{
    Q_OBJECT

public:
    // 通道 / 波特率描述（供 UI 下拉框填充）
    struct ChannelInfo { quint8 handle; QString name; };
    struct BaudInfo    { quint16 code;  QString name; };

    // PCANBasic.dll 是否可加载（驱动是否已安装）
    static bool isDriverAvailable();
    // 即插即用通道列表（USB/PCI/PCC/LAN 各 16 个）
    static QList<ChannelInfo> availableChannels();
    // 波特率列表（BTR0BTR1 码，5 kbit/s ~ 1 Mbit/s）
    static QList<BaudInfo> availableBaudrates();
    // 状态码 → 中文描述（未知码回退到 DLL 的 CAN_GetErrorText）
    static QString statusText(quint32 status);

    explicit PcanConnection(QObject *parent = nullptr);
    ~PcanConnection() override;

    bool isConnected() const { return m_connected; }

public slots:
    // 打开通道：channelHandle 取 availableChannels() 的 handle，
    // baudCode 取 availableBaudrates() 的 code
    void connectToChannel(int channelHandle, int baudCode);
    void disconnectFromChannel();
    // 发送一帧（data ≤ 8 字节；extended = true 时为扩展帧，ID 有效位 29 bit）
    void send(quint32 id, const QByteArray &data, bool extended = false);

signals:
    void connected();
    void disconnected();
    // msgType 为 PCAN 消息类型位（bit1=扩展帧 bit0=远程帧）；
    // timestampUs 为设备时间戳（微秒，含溢出累计）
    void frameReceived(quint32 id, const QByteArray &data,
                       quint8 msgType, quint64 timestampUs);
    void errorOccurred(const QString &msg);
    void logMessage(const QString &msg);

private slots:
    void onPollTimer();

private:
    static bool loadApi();   // 加载 DLL 并解析导出函数（进程级共享，只做一次）

    void cleanup();

    QTimer *m_pollTimer    = nullptr;
    quint8  m_channel      = 0;      // 当前通道句柄
    bool    m_connected    = false;
    int     m_lastBusState = 0;      // 上次总线状态（状态变化才提示，避免刷屏）
};

#endif // PCANCONNECTION_H
