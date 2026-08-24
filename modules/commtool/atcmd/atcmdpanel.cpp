#include "atcmdpanel.h"
#include "atcmdstore.h"
#include "ui_atcmdpanel.h"
#include "atparamdialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

// ==================== 构造 ====================

AtCmdPanel::AtCmdPanel(QWidget *parent)
    : QWidget(parent)
    , m_ui(new Ui::AtCmdPanel)
{
    m_ui->setupUi(this);

    // 从指令库 JSON 加载 3 页指令列表
    const QList<AtCmdCategory> categories = AtCmdStore::loadCategories();
    if (categories.size() >= 3) {
        populateCommands(m_ui->commonScrollContent, categories.at(0).commands);
        populateCommands(m_ui->chipScrollContent,   categories.at(1).commands);
        populateCommands(m_ui->navScrollContent,    categories.at(2).commands);
    }
}

AtCmdPanel::~AtCmdPanel()
{
    delete m_ui;
}

// ==================== UI 搭建 ====================

void AtCmdPanel::populateCommands(QWidget *scrollContent,
                                  const QList<AtCmdInfo> &cmds)
{
    auto *layout = new QVBoxLayout(scrollContent);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    for (const auto &cmd : cmds)
        layout->addWidget(createCmdEntry(cmd));
    layout->addStretch();
}

QWidget *AtCmdPanel::createCmdEntry(const AtCmdInfo &cmd)
{
    auto *row = new QWidget(this);
    auto *hLayout = new QHBoxLayout(row);
    hLayout->setContentsMargins(4, 2, 4, 2);
    hLayout->setSpacing(4);

    // 指令名 — 扁平按钮，看起来像文字但可点击发送
    const QString cmdBtnStyle = QStringLiteral(
        "QPushButton { text-align: left; font-family: 'Consolas','Courier New',monospace; "
        "font-size: 12px; color: #12263f; font-weight: 600; border: none; background: transparent; "
        "padding: 2px 4px; }"
        "QPushButton:hover { color: #1a5276; text-decoration: underline; }");
    auto *cmdBtn = new QPushButton(cmd.command, row);
    cmdBtn->setFlat(true);
    cmdBtn->setCursor(Qt::PointingHandCursor);
    cmdBtn->setMinimumWidth(180);
    cmdBtn->setStyleSheet(cmdBtnStyle);
    hLayout->addWidget(cmdBtn);

    // 功能说明
    auto *descLabel = new QLabel(cmd.description, row);
    descLabel->setStyleSheet(QStringLiteral("color: #617487; font-size: 11px;"));
    hLayout->addWidget(descLabel, 1);

    // 推送箭头 → 视觉提示可发送
    auto *arrowLabel = new QLabel(QStringLiteral("\u279C"), row);
    arrowLabel->setStyleSheet(QStringLiteral("color: #17324d; font-size: 14px;"));
    hLayout->addWidget(arrowLabel);

    // 点击触发发送，有参指令走弹窗
    AtCmdInfo cmdCopy = cmd;
    connect(cmdBtn, &QPushButton::clicked, this, [this, cmdCopy]() {
        onSendClicked(cmdCopy, nullptr);
    });

    return row;
}

// ==================== 发送逻辑 ====================

void AtCmdPanel::onSendClicked(const AtCmdInfo &cmd, QLineEdit *paramEdit)
{
    // 1) 有参指令 → 弹窗输入或从输入框读取
    QString paramValue;
    if (!cmd.paramHint.isEmpty()) {
        if (paramEdit && !paramEdit->text().trimmed().isEmpty()) {
            paramValue = paramEdit->text().trimmed();
        } else {
            AtParamDialog dlg(cmd.command, cmd.description, cmd.paramHint, this);
            if (dlg.exec() != QDialog::Accepted)
                return;
            paramValue = dlg.paramValue();
            if (paramValue.isEmpty()) return;
            // 回填到输入框
            if (paramEdit) paramEdit->setText(paramValue);
        }
    }

    // 2) 拼完整指令
    QString fullCmd = cmd.command;
    if (!cmd.command.endsWith(QLatin1Char('=')) && !paramValue.isEmpty())
        fullCmd += QLatin1Char('=');
    fullCmd += paramValue;
    fullCmd += QStringLiteral("\r\n");
    QByteArray cmdData = fullCmd.toUtf8();

    // 3) 确认弹窗
    QString confirmText = QStringLiteral("即将发送:\n\n  %1").arg(fullCmd.trimmed());
    auto result = QMessageBox::question(
        this, QStringLiteral("确认发送"), confirmText,
        QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
    if (result != QMessageBox::Ok)
        return;

    // 4) 直接发送
    emit sendData(cmdData);
    emit commandSent(QString::fromUtf8(cmdData).trimmed());
    emit statusMessage(QStringLiteral("[AT] 已发送: %1").arg(fullCmd.trimmed()));
}


