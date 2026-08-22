#ifndef ATPARAMDIALOG_H
#define ATPARAMDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;
class QLabel;

// AT 指令参数输入弹窗
class AtParamDialog : public QDialog
{
    Q_OBJECT
public:
    // cmdName: 如 "AT+SET_BAUD=", paramHint: 如 "例如 115200"
    explicit AtParamDialog(const QString &cmdName,
                           const QString &description,
                           const QString &paramHint,
                           QWidget *parent = nullptr);

    QString paramValue() const;

private:
    QLineEdit *m_edit = nullptr;
};

#endif // ATPARAMDIALOG_H
