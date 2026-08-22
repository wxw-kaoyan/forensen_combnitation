#include "dataviewerdialog.h"

#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QTimer>

DataViewerDialog::DataViewerDialog(const QString &title, QWidget *parent)
    : QDialog(parent)
    , m_flushTimer(new QTimer(this))
{
    setWindowTitle(title);
    resize(600, 400);
    setAttribute(Qt::WA_DeleteOnClose, false);

    auto *layout = new QVBoxLayout(this);

    auto *headerLayout = new QHBoxLayout;
    auto *label = new QLabel(QStringLiteral("原始数据 (ASCII):"), this);
    headerLayout->addWidget(label);
    headerLayout->addStretch();

    m_pauseBtn = new QPushButton(QStringLiteral("暂停"), this);
    connect(m_pauseBtn, &QPushButton::clicked, this, &DataViewerDialog::togglePause);
    headerLayout->addWidget(m_pauseBtn);

    auto *clearBtn = new QPushButton(QStringLiteral("清空"), this);
    connect(clearBtn, &QPushButton::clicked, this, &DataViewerDialog::clearData);
    headerLayout->addWidget(clearBtn);

    layout->addLayout(headerLayout);

    m_textView = new QPlainTextEdit(this);
    m_textView->setReadOnly(true);
    m_textView->setFont(QFont("Consolas", 10));
    m_textView->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(m_textView);

    // 200ms 定时刷一次，避免高频数据阻塞主线程
    m_flushTimer->setInterval(200);
    connect(m_flushTimer, &QTimer::timeout, this, &DataViewerDialog::flushBuffer);
    m_flushTimer->start();
}

void DataViewerDialog::appendData(const QByteArray &data)
{
    if (m_paused || data.isEmpty()) return;
    m_buffer.append(data);
}

void DataViewerDialog::flushBuffer()
{
    if (m_buffer.isEmpty() || !m_textView) return;

    QScrollBar *bar = m_textView->verticalScrollBar();
    bool atBottom = bar ? (bar->value() >= bar->maximum() - 10) : true;

    m_textView->moveCursor(QTextCursor::End);
    m_textView->insertPlainText(QString::fromLatin1(m_buffer));
    m_buffer.clear();

    if (atBottom && bar) {
        bar->setValue(bar->maximum());
    }
}

void DataViewerDialog::togglePause()
{
    if (!m_paused) {
        flushBuffer();   // 暂停前先把已攒的刷出来，画面定格在暂停那一刻
        m_paused = true;
        m_pauseBtn->setText(QStringLiteral("继续"));
    } else {
        m_paused = false;
        m_pauseBtn->setText(QStringLiteral("暂停"));
    }
}

void DataViewerDialog::clearData()
{
    m_buffer.clear();
    if (m_textView)
        m_textView->clear();
}
