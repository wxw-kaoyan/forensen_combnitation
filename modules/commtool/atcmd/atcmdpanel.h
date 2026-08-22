#ifndef ATCMDPANEL_H
#define ATCMDPANEL_H

#include <QWidget>
#include <QList>

class QLineEdit;
class QVBoxLayout;

QT_BEGIN_NAMESPACE
namespace Ui {
class AtCmdPanel;
}
QT_END_NAMESPACE

// 单条 AT 指令描述
struct AtCmdInfo
{
    QString command;       // 指令名，如 "AT+SET_BAUD="
    QString description;   // 功能说明
    QString paramHint;     // 参数提示，空串表示无参
};

class AtCmdPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AtCmdPanel(QWidget *parent = nullptr);
    ~AtCmdPanel() override;

signals:
    void sendData(const QByteArray &data);
    void statusMessage(const QString &msg);
    void commandSent(const QString &text);  // 已发送的指令回显到发送框

private slots:
    void onSendClicked(const AtCmdInfo &cmd, QLineEdit *paramEdit);

private:
    void populateCommands(QWidget *scrollContent,
                          const QList<AtCmdInfo> &cmds);
    QWidget *createCmdEntry(const AtCmdInfo &cmd);

    Ui::AtCmdPanel *m_ui = nullptr;

    static QList<AtCmdInfo> commonCmds();
    static QList<AtCmdInfo> chipCmds();
    static QList<AtCmdInfo> navCmds();
};

#endif // ATCMDPANEL_H
