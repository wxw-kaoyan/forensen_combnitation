#include "dataviewerdialog.h"

#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QTimer>

namespace {
// 转义渲染：可打印 ASCII 原样，\r/\n 转义为字面量，其余字节显示 \xNN
QString toEscapedText(const QByteArray &data)
{
    QString out;
    out.reserve(data.size());
    const char *p = data.constData();
    for (int i = 0; i < data.size(); ++i) {
        const uchar uc = static_cast<uchar>(p[i]);
        if (uc == '\r') {
            out += QStringLiteral("\\r");
        } else if (uc == '\n') {
            out += QStringLiteral("\\n");
        } else if (uc >= 0x20 && uc <= 0x7E) {
            out += QChar(uc);
        } else {
            out += QStringLiteral("\\x%1").arg(uc, 2, 16, QChar('0'));
        }
    }
    return out;
}
} // namespace

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

    m_escapedBtn = new QPushButton(QStringLiteral("转义显示"), this);
    m_escapedBtn->setCheckable(true);
    m_escapedBtn->setChecked(m_escaped);
    connect(m_escapedBtn, &QPushButton::toggled, this, &DataViewerDialog::toggleEscaped);
    headerLayout->addWidget(m_escapedBtn);

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
    if (m_escaped)
        m_textView->insertPlainText(toEscapedText(m_buffer));
    else
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

void DataViewerDialog::toggleEscaped(bool on)
{
    if (m_escaped == on) return;
    flushBuffer();        // 把已攒数据按旧格式刷完
    m_escaped = on;
    m_textView->clear();  // 清掉旧格式内容，后续数据用新格式显示
}

void DataViewerDialog::clearData()
{
    m_buffer.clear();
    if (m_textView)
        m_textView->clear();
}
