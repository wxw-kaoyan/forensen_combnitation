#ifndef DATAVIEWERDIALOG_H
#define DATAVIEWERDIALOG_H

#include <QDialog>

class QPlainTextEdit;
class QPushButton;
class QTimer;

// 数据查看窗口 —— 显示串口/TCP 接收到的原始数据，用于排查问题
class DataViewerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DataViewerDialog(const QString &title, QWidget *parent = nullptr);

public slots:
    void appendData(const QByteArray &data);

private slots:
    void flushBuffer();
    void togglePause();

private:
    void clearData();

    QPlainTextEdit *m_textView = nullptr;
    QPushButton    *m_pauseBtn = nullptr;
    QByteArray      m_buffer;      // 攒一批再刷，避免 UI 频繁更新阻塞主线程
    QTimer         *m_flushTimer = nullptr;
    bool            m_paused = false;  // 暂停显示：转发照常，仅停止 UI 刷新
};

#endif // DATAVIEWERDIALOG_H
