#ifndef NTRIPWORKER_H
#define NTRIPWORKER_H

#include <QObject>
#include <QTimer>
#include <QVector>

class TcpConnection;

#include "forwardtarget.h"

class DataPool;

// 在独立线程中运行的 NTRIP 连接工作器
// 负责 NTRIP HTTP/1.0 握手、GGA 发送、接收 RTCM 数据流
// 底层 TCP 连接委托给 TcpConnection 基础设施
// 此外持有所有 ForwardTarget（转发目标），转发逻辑均在此工作线程执行，不阻塞主线程
// GGA 双模式:
//   FixGGA 模式 — 主界面填入经纬高, 1Hz 定时器自动生成 $GPGGA
//   外部源模式 — TCP/串口转发目标输入的真实 $GPGGA, 来一条转一条
class NTRIPWorker : public QObject
{
    Q_OBJECT

public:
    explicit NTRIPWorker(QObject *parent = nullptr);
    ~NTRIPWorker() override;

    void setDataPool(DataPool *pool);

public slots:
    void start(const QString &host, quint16 port,
               const QString &mountpoint,
               const QString &user, const QString &password);
    void stop();
    void setGgaPosition(double lon, double lat, double height);
    void setUseFixGGA(bool enabled);              // 切换 FixGGA / 外部源模式
    void forwardGGA(const QByteArray &nmeaSentence); // 外部源模式：透传 GGA 到 socket

    // 转发目标管理（全部在工作线程执行）
    void addForwardTarget(const ForwardTarget::Config &cfg);
    void removeForwardTarget(int index);
    void startForwardTarget(int index);
    void stopForwardTarget(int index);
    void setForwardTargetGgaSource(int index, bool enabled);

    // 转发目标数据保存
    void enableForwarderSave(int index, const QString &filePath);
    void disableForwarderSave(int index);

signals:
    void statusChanged(const QString &msg);   // 状态文本（含 HTTP 响应头）
    void dataReady(const QByteArray &data);   // RTCM 原始数据块
    void connected();                          // TCP 已连接
    void disconnected();                       // TCP 已断开
    void errorOccurred(const QString &err);   // 错误信息
    void dataRateChanged(const QString &rateText, qint64 totalBytes);  // 1Hz 速率更新 + 累计字节

    // 转发目标状态回传（→ 主线程 UI）
    void forwarderAdded(int index, const ForwardTarget::Config &cfg);
    void forwarderRemoved(int index);
    void forwarderStatusChanged(int index, const QString &status);
    void forwarderRawData(int index, const QByteArray &data);
    void forwarderSaveStateChanged(int index, bool saving);

private slots:
    void onConnected();
    void onDisconnected();
    void onDataReceived(const QByteArray &data);
    void onConnectionError(const QString &err);
    void onRateTick();    // 1Hz 速率计算

private:
    void sendNtripRequest();
    void sendGGA();
    void startGgaTimer();
    void stopGgaTimer();
    static QByteArray nmeaChecksum(const QByteArray &sentence);
    static QString toNmeaLat(double deg);
    static QString toNmeaLon(double deg);

    TcpConnection *m_conn = nullptr;   // 底层 TCP 连接
    QTimer *m_ggaTimer = nullptr;

    QString m_host;
    int     m_port = 0;
    QString m_mountpoint;
    QString m_user;
    QString m_password;

    bool m_connected = false;
    bool m_hasReceivedHeader = false;
    bool m_ggaSent = false;
    bool m_useFixGGA = true;  // 默认 FixGGA 模式
    QByteArray m_headerBuffer;

    double m_ggaLon = 0.0;
    double m_ggaLat = 0.0;
    double m_ggaHeight = 0.0;

    // 速率统计
    qint64 m_bytesInInterval = 0;   // 当前 1 秒窗口内的字节数
    qint64 m_totalBytes = 0;        // 累计字节数

    // 转发目标（全部在此工作线程中运行）
    DataPool *m_dataPool = nullptr;
    QVector<ForwardTarget *> m_forwarders;
};

#endif // NTRIPWORKER_H
