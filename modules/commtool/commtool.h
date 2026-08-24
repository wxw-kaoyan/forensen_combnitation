#ifndef COMMTOOL_H
#define COMMTOOL_H

#include <QObject>
#include <QFile>
#include <QEvent>

#include "modules/commtool/streamframer.h"

class QThread;
class TcpConnection;
class SerialConnection;
class AtCmdPanel;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

// ---- 串口 / TCP 调试工具（UI 控制器） ----
// 使用 TcpConnection / SerialConnection 基础设施层，零耦合。
// 工作线程在构造时启动，连接对象按需创建/销毁。
class CommTool : public QObject
{
    Q_OBJECT

public:
    explicit CommTool(Ui::MainWindow *ui, QObject *parent = nullptr);
    ~CommTool() override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onConnectClicked();
    void onSendClicked();
    void onClearRecvClicked();
    void onSaveFileClicked();
    void sendRawData(const QByteArray &data);
    void onAddFavoriteClicked();
    void onFavoriteSelected(int index);
    void onParseToggled(bool enabled);

private:
    void applyMode(int idx);
    void appendRecvText(const QString &text);
    void appendRecvData(const QByteArray &data);
    void setConnected(bool connected);
    void destroyConnection();
    void refreshSerialPorts();
    void refreshFavoriteCommands();

    Ui::MainWindow *m_ui;

    // AT 指令面板
    AtCmdPanel *m_atPanel = nullptr;

    // 工作线程
    QThread *m_connThread = nullptr;

    // 当前活跃的连接（二选一）
    TcpConnection    *m_tcpConn    = nullptr;
    SerialConnection *m_serialConn = nullptr;

    // 流式实时解析
    StreamFramer m_framer;
    bool m_parsing = false;

    bool m_connected = false;
    bool m_hexDisplay = false;
    bool m_saving = false;

    // 接收区
    QFile  m_recvFile;
    QString m_savePath;
    static constexpr int kMaxRecvLines = 200;
};

#endif // COMMTOOL_H
