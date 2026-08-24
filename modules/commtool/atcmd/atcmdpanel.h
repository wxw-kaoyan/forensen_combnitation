#ifndef ATCMDPANEL_H
#define ATCMDPANEL_H

#include <QWidget>
#include <QList>

#include "atcmdstore.h"

class QLineEdit;
class QVBoxLayout;

QT_BEGIN_NAMESPACE
namespace Ui {
class AtCmdPanel;
}
QT_END_NAMESPACE

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
};

#endif // ATCMDPANEL_H
