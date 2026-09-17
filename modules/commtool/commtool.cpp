#include "modules/commtool/commtool.h"
#include "common/tcpconnection.h"
#include "common/serialconnection.h"
#include "modules/commtool/atcmd/atcmdpanel.h"
#include "modules/commtool/atcmd/atcmdstore.h"
#include "modules/commtool/realtimeparsepanel.h"
#include "ui_mainwindow.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QFileDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSerialPortInfo>
#include <QTextBlock>
#include <QThread>

// ---- 按钮样式 ----
static const QString kConnectStyle =
    QStringLiteral("QPushButton { background: #27ae60; color: white; border-radius: 8px; "
                   "padding: 8px 14px; font-weight: 600; font-size: 14px; }"
                   "QPushButton:hover { background: #2ecc71; }");
static const QString kDisconnectStyle =
    QStringLiteral("QPushButton { background: #e67e22; color: white; border-radius: 8px; "
                   "padding: 8px 14px; font-weight: 600; font-size: 14px; }"
                   "QPushButton:hover { background: #f39c12; }");

CommTool::CommTool(Ui::MainWindow *ui, QObject *parent)
    : QObject(parent)
    , m_ui(ui)
    , m_connThread(new QThread(this))
{
    m_connThread->start();

    // ---- AT 指令面板 ----
    m_atPanel = m_ui->atCmdPanel;
    connect(m_atPanel, &AtCmdPanel::sendData,
            this, &CommTool::sendRawData);
    connect(m_atPanel, &AtCmdPanel::statusMessage,
            this, [this](const QString &msg) { appendRecvText(msg + QStringLiteral("\n")); });
    connect(m_atPanel, &AtCmdPanel::commandSent,
            this, [this](const QString &text) { m_ui->commandEdit->setPlainText(text); });

    // ---- 连接方式切换：串口 / TCP Client ----
    connect(m_ui->connectionModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) { applyMode(idx); });
    applyMode(m_ui->connectionModeComboBox->currentIndex());

    // ---- 串口下拉框展开时刷新列表 ----
    m_ui->commPortComboBox->installEventFilter(this);

    // ---- 连接/断开按钮 ----
    connect(m_ui->toolConnectButton, &QPushButton::clicked,
            this, &CommTool::onConnectClicked);

    // ---- 发送按钮 ----
    connect(m_ui->sendCommandButton, &QPushButton::clicked,
            this, &CommTool::onSendClicked);

    // ---- 常用指令 ----
    connect(m_ui->addFavoriteButton, &QPushButton::clicked,
            this, &CommTool::onAddFavoriteClicked);
    connect(m_ui->favoriteCmdComboBox, QOverload<int>::of(&QComboBox::activated),
            this, &CommTool::onFavoriteSelected);
    refreshFavoriteCommands();

    // ---- 实时解析 ----
    connect(m_ui->realtimeParsePanel, &RealtimeParsePanel::parseToggled,
            this, &CommTool::onParseToggled);

    // ---- 清空接收按钮 ----
    connect(m_ui->clearConsoleButton, &QPushButton::clicked,
            this, &CommTool::onClearRecvClicked);

    // ---- 保存文件按钮 ----
    connect(m_ui->saveConsoleButton, &QPushButton::clicked,
            this, &CommTool::onSaveFileClicked);

    // ---- HEX 显示切换 ----
    connect(m_ui->hexDisplayCheckBox, &QCheckBox::toggled,
            this, [this](bool checked) { m_hexDisplay = checked; });

    // 接收区是只读日志窗口，不需要撤销/重做历史（否则每次插入都累积一条记录）
    m_ui->toolConsoleEdit->setUndoRedoEnabled(false);

    // ---- 发送框指令补全：边打字边匹配 AT 指令库 ----
    setupCommandCompleter();

    // 初始状态
    setConnected(false);
}

CommTool::~CommTool()
{
    // 通知工作线程断开
    if (m_tcpConn)
        QMetaObject::invokeMethod(m_tcpConn, &TcpConnection::disconnectFromHost, Qt::QueuedConnection);
    if (m_serialConn)
        QMetaObject::invokeMethod(m_serialConn, &SerialConnection::disconnectFromPort, Qt::QueuedConnection);

    // 线程结束时删除连接对象
    if (m_tcpConn)
        connect(m_connThread, &QThread::finished, m_tcpConn, &QObject::deleteLater);
    if (m_serialConn)
        connect(m_connThread, &QThread::finished, m_serialConn, &QObject::deleteLater);

    m_connThread->quit();
    m_connThread->wait(3000);

    if (m_recvFile.isOpen()) {
        m_recvFile.flush();
        m_recvFile.close();
    }
}

// ==================== 连接 ====================

void CommTool::applyMode(int idx)
{
    if (m_connected) return; // 连接中不允许切换

    bool isSerial = (idx == 0);
    m_ui->baudLabel->setVisible(isSerial);
    m_ui->baudComboBox->setVisible(isSerial);
    m_ui->paramLabel->setVisible(isSerial);
    m_ui->dataBitsComboBox->setVisible(isSerial);
    m_ui->parityComboBox->setVisible(isSerial);
    m_ui->stopBitsComboBox->setVisible(isSerial);
    m_ui->portLabel->setVisible(!isSerial);
    m_ui->toolPortEdit->setVisible(!isSerial);

    m_ui->toolAddressEdit->setVisible(!isSerial);
    m_ui->commPortComboBox->setVisible(isSerial);
    if (isSerial) {
        refreshSerialPorts();
    } else {
        m_ui->toolAddressEdit->setPlaceholderText(QStringLiteral("127.0.0.1"));
    }
}

void CommTool::onConnectClicked()
{
    if (m_connected) {
        // 断开
        if (m_tcpConn)
            QMetaObject::invokeMethod(m_tcpConn, &TcpConnection::disconnectFromHost, Qt::QueuedConnection);
        else if (m_serialConn)
            QMetaObject::invokeMethod(m_serialConn, &SerialConnection::disconnectFromPort, Qt::QueuedConnection);
        return;
    }

    bool isSerial = (m_ui->connectionModeComboBox->currentIndex() == 0);

    if (isSerial) {
        // ---- 串口 ----
        QString portName = m_ui->commPortComboBox->currentData().toString();
        if (portName.isEmpty() || portName == QStringLiteral("无可用串口")) {
            QMessageBox::warning(nullptr, QStringLiteral("连接失败"),
                                 QStringLiteral("没有可用的串口。"));
            return;
        }

        destroyConnection();
        m_serialConn = new SerialConnection; // 无父对象，稍后移入线程
        m_serialConn->moveToThread(m_connThread);

        connect(m_serialConn, &SerialConnection::connected, this, [this, portName]() {
            appendRecvText(QStringLiteral("[系统] 串口 %1 已打开 @%2\n")
                               .arg(portName, m_ui->baudComboBox->currentText()));
            setConnected(true);
        });
        connect(m_serialConn, &SerialConnection::disconnected, this, [this]() {
            appendRecvText(QStringLiteral("[系统] 已断开连接\n"));
            setConnected(false);
            destroyConnection();
        });
        connect(m_serialConn, &SerialConnection::dataReceived, this, [this](const QByteArray &data) {
            appendRecvData(data);
        });
        connect(m_serialConn, &SerialConnection::errorOccurred, this, [this](const QString &msg) {
            appendRecvText(QStringLiteral("[错误] %1\n").arg(msg));
            setConnected(false);
            destroyConnection();
        });

        QMetaObject::invokeMethod(m_serialConn, &SerialConnection::connectToPort, Qt::QueuedConnection,
            portName,
            m_ui->baudComboBox->currentText().toInt(),
            m_ui->dataBitsComboBox->currentText().toInt(),
            m_ui->parityComboBox->currentIndex(),
            m_ui->stopBitsComboBox->currentIndex(),
            1000);

    } else {
        // ---- TCP ----
        QString host = m_ui->toolAddressEdit->text().trimmed();
        if (host.isEmpty()) host = QStringLiteral("127.0.0.1");
        int port = m_ui->toolPortEdit->text().toInt();
        if (port < 1 || port > 65535) port = 2101;

        destroyConnection();
        m_tcpConn = new TcpConnection; // 无父对象，稍后移入线程
        m_tcpConn->moveToThread(m_connThread);

        connect(m_tcpConn, &TcpConnection::connected, this, [this]() {
            appendRecvText(QStringLiteral("[系统] TCP已连接\n"));
            setConnected(true);
        });
        connect(m_tcpConn, &TcpConnection::disconnected, this, [this]() {
            appendRecvText(QStringLiteral("[系统] 已断开连接\n"));
            setConnected(false);
            destroyConnection();
        });
        connect(m_tcpConn, &TcpConnection::dataReceived, this, [this](const QByteArray &data) {
            appendRecvData(data);
        });
        connect(m_tcpConn, &TcpConnection::errorOccurred, this, [this](const QString &msg) {
            appendRecvText(QStringLiteral("[错误] %1\n").arg(msg));
            setConnected(false);
            destroyConnection();
        });
        connect(m_tcpConn, &TcpConnection::logMessage, this, [this](const QString &msg) {
            appendRecvText(msg);
        });

        QMetaObject::invokeMethod(m_tcpConn, &TcpConnection::connectToHost, Qt::QueuedConnection,
            host,
            port,
            1000);
    }
}

void CommTool::destroyConnection()
{
    if (m_tcpConn) {
        disconnect(m_tcpConn, nullptr, this, nullptr);
        m_tcpConn->deleteLater();
        m_tcpConn = nullptr;
    }
    if (m_serialConn) {
        disconnect(m_serialConn, nullptr, this, nullptr);
        m_serialConn->deleteLater();
        m_serialConn = nullptr;
    }
    // 重置流式解析器
    m_framer.reset();
    // 防御性重置：确保连接失败后按钮状态正确恢复
    if (m_connected) {
        m_connected = false;
        m_ui->toolConnectButton->setText(QStringLiteral("连接"));
        m_ui->toolConnectButton->setStyleSheet(kConnectStyle);
        m_ui->connectionModeComboBox->setEnabled(true);
    }
}

void CommTool::setConnected(bool connected)
{
    m_connected = connected;
    m_ui->toolConnectButton->setText(connected ? QStringLiteral("断开") : QStringLiteral("连接"));
    m_ui->toolConnectButton->setStyleSheet(connected ? kDisconnectStyle : kConnectStyle);
    m_ui->connectionModeComboBox->setEnabled(!connected);
}

// ==================== 发送 ====================

void CommTool::sendRawData(const QByteArray &data)
{
    if (m_tcpConn) {
        bool ok = QMetaObject::invokeMethod(m_tcpConn, &TcpConnection::send, Qt::QueuedConnection, data);
        if (!ok)
            appendRecvText(QStringLiteral("[错误] invokeMethod TCP send 失败\n"));
    } else if (m_serialConn) {
        bool ok = QMetaObject::invokeMethod(m_serialConn, &SerialConnection::send, Qt::QueuedConnection, data);
        if (!ok)
            appendRecvText(QStringLiteral("[错误] invokeMethod Serial send 失败\n"));
    } else {
        appendRecvText(QStringLiteral("[提示] 请先建立连接\n"));
    }
}

void CommTool::onSendClicked()
{
    QString text = m_ui->commandEdit->toPlainText();
    if (text.isEmpty()) return;

    if (m_ui->sendNewlineCheckBox->isChecked())
        text.append(QStringLiteral("\r\n"));

    QByteArray data;
    if (m_ui->sendHexCheckBox->isChecked()) {
        QString hex = text.simplified().remove(QLatin1Char(' '));
        data = QByteArray::fromHex(hex.toLatin1());
    } else {
        data = text.toLatin1();
    }

    sendRawData(data);
}

// ==================== 接收区 ====================

void CommTool::onClearRecvClicked()
{
    m_ui->toolConsoleEdit->clear();
    m_recvBytes = 0;
    m_ui->recvCountLabel->setText(QStringLiteral("0 Bytes"));
}

void CommTool::onSaveFileClicked()
{
    if (m_saving) {
        // 停止保存
        m_saving = false;
        m_ui->saveConsoleButton->setText(QStringLiteral("保存文件"));
        m_ui->saveConsoleButton->setStyleSheet(
            QStringLiteral("QPushButton { background: #17324d; color: white; border-radius: 8px; "
                           "padding: 8px 14px; font-weight: 600; }"
                           "QPushButton:hover { background: #24476b; }"));
        if (m_recvFile.isOpen()) {
            m_recvFile.flush();
            m_recvFile.close();
        }
        appendRecvText(QStringLiteral("[系统] 已停止保存\n"));
        return;
    }

    // 开始保存：打开文件
    QString path = QFileDialog::getSaveFileName(
        nullptr,
        QStringLiteral("保存接收数据"),
        QStringLiteral("comm_recv.log"),
        QStringLiteral("日志文件 (*.log *.txt);;所有文件 (*)"));
    if (path.isEmpty()) return;

    m_savePath = path;
    m_recvFile.setFileName(path);
    if (!m_recvFile.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(nullptr, QStringLiteral("保存文件"),
                             QStringLiteral("无法写入文件: %1").arg(m_recvFile.errorString()));
        return;
    }

    m_saving = true;
    m_ui->saveConsoleButton->setText(QStringLiteral("停止保存"));
    m_ui->saveConsoleButton->setStyleSheet(
        QStringLiteral("QPushButton { background: #e74c3c; color: white; border-radius: 8px; "
                       "padding: 8px 14px; font-weight: 600; }"
                       "QPushButton:hover { background: #c0392b; }"));

    appendRecvText(QStringLiteral("[系统] 开始保存到 %1\n").arg(path));
}

void CommTool::onParseToggled(bool enabled)
{
    m_parsing = enabled;
    m_framer.reset(); // 切换模式时清空半包缓冲，避免残留

    // 开始解析时输出一行表头，方便对数据列
    if (enabled && m_ui->realtimeParsePanel) {
        const QString hdr = RealtimeParsePanel::headerLine(
            m_ui->realtimeParsePanel->currentProtocol());
        if (!hdr.isEmpty())
            appendRecvText(hdr);
    }
}

void CommTool::appendRecvData(const QByteArray &data)
{
    // 累计接收字节数（累加计数，替代原先"每段数据全量导出文档"的 O(n) 做法）
    m_recvBytes += data.size();
    m_ui->recvCountLabel->setText(
        m_recvBytes >= 1024
            ? QStringLiteral("%1 KB").arg(QString::number(m_recvBytes / 1024.0, 'f', 1))
            : QStringLiteral("%1 Bytes").arg(m_recvBytes));

    // 流式保存到文件（始终保存原始字节，不做任何编码转换）
    if (m_saving && m_recvFile.isOpen()) {
        m_recvFile.write(data);
    }

    // 解析模式：只输出选中协议的解析结果，其余全部过滤
    if (m_parsing) {
        const ProtocolID pid = m_ui->realtimeParsePanel
            ? m_ui->realtimeParsePanel->currentProtocol() : ProtocolID::Debug_IMU;
        const QList<ParsedFrame> frames = m_framer.feed(data);
        for (const ParsedFrame &f : frames) {
            if (f.protocol == pid)
                appendRecvText(RealtimeParsePanel::frameToText(f));
        }
        return;
    }

    // 普通模式：显示原始数据
    // 每段数据必须以换行结尾：二进制流本身几乎不含 '\n'，否则整个接收区会挤在
    // 一个 block 里 —— 行数裁剪失效、QPlainTextEdit 排版开销随文档无限增长直至卡死
    QString text;
    if (m_hexDisplay) {
        text = QString::fromLatin1(data.toHex(' ').toUpper()) + QStringLiteral("\n");
    } else {
        text = QString::fromLatin1(data) + QStringLiteral("\n");
    }
    appendRecvText(text);
}

void CommTool::appendRecvText(const QString &text)
{
    if (text.isEmpty()) return;

    QPlainTextEdit *edit = m_ui->toolConsoleEdit;
    QScrollBar *bar = edit->verticalScrollBar();
    bool atBottom = bar ? (bar->value() >= bar->maximum() - 10) : true;

    edit->moveCursor(QTextCursor::End);
    edit->insertPlainText(text);

    // 限制最多保留 200 行
    int lines = edit->document()->blockCount();
    if (lines > kMaxRecvLines) {
        QTextBlock block = edit->document()->findBlockByNumber(lines - kMaxRecvLines - 1);
        if (block.isValid()) {
            QTextCursor cur(block);
            cur.movePosition(QTextCursor::Start, QTextCursor::KeepAnchor);
            cur.removeSelectedText();
        }
    }

    // 字节计数已在 appendRecvData 中累加，这里不再全量导出文档

    if (atBottom && bar)
        bar->setValue(bar->maximum());
}

// ---- 常用指令 ----

void CommTool::onAddFavoriteClicked()
{
    const QString full = m_ui->commandEdit->toPlainText().trimmed();
    if (full.isEmpty()) {
        QMessageBox::warning(nullptr, QStringLiteral("添加常用指令"),
                             QStringLiteral("请先在发送框输入指令。"));
        return;
    }

    QString cmd = full;
    QString hint;
    const int eq = full.indexOf(QLatin1Char('='));
    if (eq >= 0) {
        cmd = full.left(eq + 1);
        const QString val = full.mid(eq + 1).trimmed();
        if (!val.isEmpty())
            hint = QStringLiteral("例如 %1").arg(val);
    }

    if (AtCmdStore::exists(cmd)) {
        QMessageBox::information(nullptr, QStringLiteral("添加常用指令"),
                                 QStringLiteral("指令 %1 已存在于指令库中。").arg(cmd));
        return;
    }

    AtCmdInfo info;
    info.command     = cmd;
    info.description = QStringLiteral("自定义指令");
    info.paramHint   = hint;

    if (!AtCmdStore::addCustom(info)) {
        QMessageBox::warning(nullptr, QStringLiteral("添加常用指令"),
                             QStringLiteral("保存失败，请检查 at_commands.json 是否可写。"));
        return;
    }

    refreshFavoriteCommands();
    QMessageBox::information(nullptr, QStringLiteral("添加常用指令"),
                             QStringLiteral("已添加到常用指令：%1").arg(cmd));
}

void CommTool::onFavoriteSelected(int index)
{
    if (index < 0) return;
    const QString cmd = m_ui->favoriteCmdComboBox->itemData(index).toString();
    if (!cmd.isEmpty())
        m_ui->commandEdit->setPlainText(cmd);
}

void CommTool::refreshFavoriteCommands()
{
    m_ui->favoriteCmdComboBox->blockSignals(true);
    m_ui->favoriteCmdComboBox->clear();
    const QList<AtCmdInfo> custom = AtCmdStore::loadCustom();
    for (const auto &c : custom)
        m_ui->favoriteCmdComboBox->addItem(c.command, c.command);
    m_ui->favoriteCmdComboBox->blockSignals(false);
}

// ---- 串口列表刷新 ----

void CommTool::refreshSerialPorts()
{
    QString current = m_ui->commPortComboBox->currentData().toString();
    m_ui->commPortComboBox->blockSignals(true);
    m_ui->commPortComboBox->clear();

    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &p : ports) {
        QString desc = p.portName();
        if (!p.description().isEmpty())
            desc += QStringLiteral(" - ") + p.description();
        m_ui->commPortComboBox->addItem(desc, p.portName());
    }
    if (m_ui->commPortComboBox->count() == 0) {
        m_ui->commPortComboBox->addItem(QStringLiteral("无可用串口"));
    } else if (!current.isEmpty()) {
        // 恢复之前选中的串口
        int idx = m_ui->commPortComboBox->findData(current);
        if (idx >= 0)
            m_ui->commPortComboBox->setCurrentIndex(idx);
    }
    m_ui->commPortComboBox->blockSignals(false);
}

bool CommTool::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_ui->commPortComboBox && event->type() == QEvent::MouseButtonPress) {
        // 仅串口模式且未连接时刷新
        if (!m_connected && m_ui->connectionModeComboBox->currentIndex() == 0)
            refreshSerialPorts();
    }

    // 发送框补全：候选浮层可见时接管 回车/Tab/Esc（↑↓ 交给 QCompleter 自己处理）
    if (obj == m_ui->commandEdit && event->type() == QEvent::KeyPress
        && m_cmdCompleter && m_cmdCompleter->popup()->isVisible()) {
        switch (static_cast<QKeyEvent *>(event)->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            acceptCurrentCompletion();
            return true;
        case Qt::Key_Escape:
            m_cmdCompleter->popup()->hide();
            return true;
        default:
            break;
        }
    }

    return QObject::eventFilter(obj, event);
}

// ==================== 发送框指令补全 ====================

void CommTool::setupCommandCompleter()
{
    // 汇总全库指令（5 个分类 + 自定义）。候选串 = 指令名左对齐补空格到统一宽度 + 说明，
    // 这样列表里能读到说明，补全时又能按固定宽度确定性还原出指令名。
    QStringList names;
    QStringList descs;
    int width = 0;

    auto append = [&](const AtCmdInfo &c) {
        if (c.command.isEmpty() || names.contains(c.command))
            return;   // 同一指令可能出现在多个分类，去重
        names << c.command;
        descs << c.description;
        width = qMax(width, int(c.command.size()));
    };

    for (const AtCmdCategory &cat : AtCmdStore::loadCategories())
        for (const AtCmdInfo &c : cat.commands)
            append(c);
    for (const AtCmdInfo &c : AtCmdStore::loadCustom())
        append(c);

    if (names.isEmpty())
        return;

    width += 2;
    m_cmdNameWidth = width;

    QStringList items;
    items.reserve(names.size());
    for (int i = 0; i < names.size(); ++i) {
        QString line = names.at(i).leftJustified(width, QLatin1Char(' '));
        if (!descs.at(i).isEmpty())
            line += descs.at(i);
        items << line;
    }

    m_cmdCompleter = new QCompleter(items, this);
    m_cmdCompleter->setWidget(m_ui->commandEdit);          // 挂到发送框上
    m_cmdCompleter->setCompletionMode(QCompleter::PopupCompletion);
    m_cmdCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_cmdCompleter->setFilterMode(Qt::MatchStartsWith);    // 前缀匹配
    m_cmdCompleter->setMaxVisibleItems(12);

    // 后装的事件过滤器会先被调用：由本类接管回车/Tab/Esc，行为更确定
    m_ui->commandEdit->installEventFilter(this);

    connect(m_cmdCompleter, QOverload<const QString &>::of(&QCompleter::activated),
            this, [this](const QString &text) { applyCompletion(text); });
    connect(m_ui->commandEdit, &QPlainTextEdit::textChanged,
            this, &CommTool::refreshCommandCompleter);
}

// 发送框当前行的文本（补全以此前缀去匹配）
QString CommTool::currentLineText() const
{
    QTextCursor tc = m_ui->commandEdit->textCursor();
    tc.select(QTextCursor::LineUnderCursor);
    return tc.selectedText().trimmed();
}

void CommTool::refreshCommandCompleter()
{
    if (m_completing || !m_cmdCompleter)
        return;

    const QString prefix = currentLineText();
    if (prefix.isEmpty() || prefix == m_lastCompleted) {
        m_cmdCompleter->popup()->hide();
        return;
    }

    m_cmdCompleter->setCompletionPrefix(prefix);

    QAbstractItemView *popup = m_cmdCompleter->popup();
    if (m_cmdCompleter->completionCount() == 0) {
        popup->hide();   // 没有候选就不弹（例如已在输入参数部分）
        return;
    }

    popup->setCurrentIndex(m_cmdCompleter->completionModel()->index(0, 0));
    QRect rect = m_ui->commandEdit->cursorRect();
    rect.setWidth(popup->sizeHintForColumn(0)
                  + popup->verticalScrollBar()->sizeHint().width());
    m_cmdCompleter->complete(rect);
}

void CommTool::acceptCurrentCompletion()
{
    if (!m_cmdCompleter)
        return;
    const QModelIndex idx = m_cmdCompleter->popup()->currentIndex();
    if (!idx.isValid())
        return;
    applyCompletion(idx.data(Qt::EditRole).toString());
}

void CommTool::applyCompletion(const QString &text)
{
    // 候选串 = 指令名 + 空格补位 + 说明，按固定宽度取回指令名
    const QString cmd = text.left(m_cmdNameWidth).trimmed();
    if (cmd.isEmpty())
        return;

    m_completing = true;
    QTextCursor tc = m_ui->commandEdit->textCursor();
    tc.select(QTextCursor::LineUnderCursor);   // 整行替换，参数由用户接着输
    tc.insertText(cmd);
    m_ui->commandEdit->setTextCursor(tc);
    m_completing = false;

    m_lastCompleted = cmd;
    if (m_cmdCompleter)
        m_cmdCompleter->popup()->hide();
}
