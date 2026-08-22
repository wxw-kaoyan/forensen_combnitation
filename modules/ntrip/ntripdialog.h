#ifndef NTRIPDIALOG_H
#define NTRIPDIALOG_H

#include <QDialog>
#include <QVector>

class QThread;
class NTRIPWorker;
class DataPool;

#include "forwardtarget.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class NTRIPLoginDialog;
}
QT_END_NAMESPACE

// 单例 NTRIP 配置对话框
// - 整个应用生命周期内只有一份实例
// - 内部持有独立工作线程处理 TCP 连接，不阻塞主界面
// - ForwardTarget（转发目标）全部在 NTRIPWorker 所在工作线程执行
// - DataPool 作为中转缓冲区，转发目标从此拉取数据
class NTRIPLoginDialog : public QDialog
{
    Q_OBJECT

public:
    static NTRIPLoginDialog *instance(QWidget *parent = nullptr);

    DataPool *dataPool() const;

    QString host() const;
    QString port() const;
    QString mountpoint() const;
    QString user() const;
    QString password() const;

    void setHost(const QString &v);
    void setPort(const QString &v);
    void setMountpoint(const QString &v);
    void setUser(const QString &v);
    void setPassword(const QString &v);
    void setFixGGA(double lon, double lat, double height);
    void setUseFixGGA(bool enabled);
    void forwardGGA(const QByteArray &nmeaSentence);

    // 转发目标管理（代理到工作线程）
    int  addForwardTarget(const ForwardTarget::Config &cfg);
    void removeForwardTarget(int index);
    void startForwardTarget(int index);
    void stopForwardTarget(int index);
    int  forwarderCount() const;
    ForwardTarget::Config forwarderConfigAt(int index) const;
    bool isForwarderRunning(int index) const;
    void setForwarderGgaSource(int index, bool enabled);
    void enableForwarderSave(int index, const QString &filePath);
    void disableForwarderSave(int index);

    bool isConnected() const;

signals:
    void connectionStatus(const QString &msg);
    void dataRateChanged(const QString &rateText, qint64 totalBytes);
    void connectionStateChanged(bool connected);
    void forwarderAdded(int index, const ForwardTarget::Config &cfg);
    void forwarderRemoved(int index);
    void forwarderStatusChanged(int index, const QString &status);
    void forwarderRawData(int index, const QByteArray &data);
    void forwarderSaveStateChanged(int index, bool saving);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    explicit NTRIPLoginDialog(QWidget *parent = nullptr);
    ~NTRIPLoginDialog() override;

    void startConnection();
    void stopConnection();
    void updateButtonStates();

    Ui::NTRIPLoginDialog *ui;
    QThread *m_thread;
    NTRIPWorker *m_worker;
    DataPool *m_dataPool;
    bool m_connected = false;

    // 转发目标配置 / 运行状态缓存（主线程读写，仅供 UI 查询）
    QVector<ForwardTarget::Config> m_forwarderConfigs;
    QVector<bool> m_forwarderRunning;
};

#endif // NTRIPDIALOG_H
