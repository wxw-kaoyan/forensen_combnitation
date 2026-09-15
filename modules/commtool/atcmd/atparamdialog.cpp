#include "atparamdialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

AtParamDialog::AtParamDialog(const QString &cmdName,
                             const QString &description,
                             const QString &paramHint,
                             QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(cmdName.startsWith(QLatin1Char('$'))
                       ? (cmdName.contains(QStringLiteral("KMD"), Qt::CaseInsensitive)
                              ? QStringLiteral("凯芯 KMD 指令参数")
                              : QStringLiteral("NMEA 风格指令参数"))
                       : QStringLiteral("AT 指令参数"));
    setMinimumWidth(380);

    auto *layout = new QVBoxLayout(this);

    // 指令名称
    auto *cmdLabel = new QLabel(
        QStringLiteral("<b>%1</b>").arg(cmdName.toHtmlEscaped()), this);
    cmdLabel->setStyleSheet(QStringLiteral("font-size: 14px; margin-bottom: 4px;"));
    layout->addWidget(cmdLabel);

    // 功能说明
    auto *descLabel = new QLabel(description, this);
    descLabel->setStyleSheet(QStringLiteral("color: #70849a; font-size: 12px;"));
    layout->addWidget(descLabel);

    // 参数输入
    auto *form = new QFormLayout;
    form->setSpacing(6);

    m_edit = new QLineEdit(this);
    m_edit->setPlaceholderText(paramHint.isEmpty() ? QStringLiteral("请输入参数") : paramHint);

    form->addRow(QStringLiteral("参数:"), m_edit);
    layout->addLayout(form);

    // 按钮
    auto *btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btnBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确认"));
    btnBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(btnBox);
}

QString AtParamDialog::paramValue() const
{
    return m_edit ? m_edit->text().trimmed() : QString();
}
