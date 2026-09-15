#include "forwardertablemanager.h"
#include "ntripdialog.h"
#include "modules/ntrip/datapool/dataviewerdialog.h"

#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QHBoxLayout>
#include <QWidget>
#include <QInputDialog>
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QFileDialog>
#include <QCoreApplication>
#include <QDir>

static const int kGgaColumn = 3;

static const QString kOpStartStyle =
    QStringLiteral("QPushButton { background: #27ae60; color: white; border-radius: 4px; "
                   "padding: 4px 10px; font-size: 12px; }"
                   "QPushButton:hover { background: #2ecc71; }");
static const QString kOpStopStyle =
    QStringLiteral("QPushButton { background: #e67e22; color: white; border-radius: 4px; "
                   "padding: 4px 10px; font-size: 12px; }"
                   "QPushButton:hover { background: #f39c12; }");
static const QString kOpDeleteStyle =
    QStringLiteral("QPushButton { background: #c0392b; color: white; border-radius: 4px; "
                   "padding: 4px 10px; font-size: 12px; }"
                   "QPushButton:hover { background: #e74c3c; }");

ForwarderTableManager::ForwarderTableManager(QTableWidget *table,
                                             QAbstractButton *addTcpBtn,
                                             QAbstractButton *addSerialBtn,
                                             QObject *parent)
    : QObject(parent)
    , m_table(table)
    , m_addTcpBtn(addTcpBtn)
    , m_addSerialBtn(addSerialBtn)
{
    connect(m_addTcpBtn, &QAbstractButton::clicked,
            this, &ForwarderTableManager::onAddTcpTarget);
    connect(m_addSerialBtn, &QAbstractButton::clicked,
            this, &ForwarderTableManager::onAddSerialTarget);

    // 表中 GGA 源勾选互斥
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (item->column() != kGgaColumn) return;
        if (item->checkState() != Qt::Checked) return;

        auto *dlg = NTRIPLoginDialog::instance(qobject_cast<QWidget*>(this->parent()));
        int row = item->row();

        // 表中互斥
        m_table->blockSignals(true);
        for (int r = 0; r < m_table->rowCount(); ++r) {
            if (r == row) continue;
            QTableWidgetItem *i = m_table->item(r, kGgaColumn);
            if (i) i->setCheckState(Qt::Unchecked);
        }
        m_table->blockSignals(false);

        dlg->setForwarderGgaSource(row, true);
        emit tableGgaSourceChecked();
    });

    connectSignals();
}

// ---- NTRIPLoginDialog 信号 → 同步表格 ----

void ForwarderTableManager::connectSignals()
{
    auto *dlg = NTRIPLoginDialog::instance(qobject_cast<QWidget*>(parent()));

    connect(dlg, &NTRIPLoginDialog::forwarderAdded, this,
            [this](int, const ForwardTarget::Config &) { rebuildTable(); });

    connect(dlg, &NTRIPLoginDialog::forwarderRemoved, this,
            [this](int index) {
                closeDataViewerForIndex(index);
                rebuildTable();
            });

    connect(dlg, &NTRIPLoginDialog::forwarderStatusChanged, this,
            &ForwarderTableManager::updateRowStatus);

    connect(dlg, &NTRIPLoginDialog::forwarderSaveStateChanged, this,
            [this](int index, bool saving) {
                if (index < 0 || index >= m_table->rowCount()) return;
                auto *opWidget = m_table->cellWidget(index, 5);
                if (auto *saveBtn = opWidget ? opWidget->findChild<QPushButton *>(QStringLiteral("saveBtn")) : nullptr) {
                    saveBtn->setText(saving ? QStringLiteral("保存中...") : QStringLiteral("保存数据"));
                }
            });
}

// ---- 新增目标 ----

void ForwarderTableManager::onAddTcpTarget()
{
    bool ok = false;
    QString addr = QInputDialog::getText(
        qobject_cast<QWidget*>(parent()), QStringLiteral("新增 TCP 目标"),
        QStringLiteral("目标地址 (host):"),
        QLineEdit::Normal, QStringLiteral("127.0.0.1"), &ok);
    if (!ok || addr.trimmed().isEmpty()) return;

    int port = QInputDialog::getInt(
        qobject_cast<QWidget*>(parent()), QStringLiteral("新增 TCP 目标"),
        QStringLiteral("端口号:"), 2101, 1, 65535, 1, &ok);
    if (!ok) return;

    ForwardTarget::Config cfg;
    cfg.type        = ForwardTarget::TCP;
    cfg.address     = addr.trimmed();
    cfg.portOrBaud  = port;
    cfg.isGgaSource = false;

    NTRIPLoginDialog::instance(qobject_cast<QWidget*>(parent()))->addForwardTarget(cfg);
}

void ForwarderTableManager::onAddSerialTarget()
{
    const auto ports = QSerialPortInfo::availablePorts();
    QStringList portNames;
    for (const auto &p : ports) {
        QString desc = p.portName();
        if (!p.description().isEmpty())
            desc += QStringLiteral(" - ") + p.description();
        portNames << desc;
    }

    if (portNames.isEmpty()) {
        QMessageBox::information(qobject_cast<QWidget*>(parent()), QStringLiteral("新增串口目标"),
                                 QStringLiteral("当前没有检测到可用串口。"));
        return;
    }

    bool ok = false;
    QString selected = QInputDialog::getItem(qobject_cast<QWidget*>(parent()), QStringLiteral("新增串口目标"),
                                             QStringLiteral("选择串口:"),
                                             portNames, 0, false, &ok);
    if (!ok) return;
    QString portName = selected.section(QStringLiteral(" - "), 0, 0);

    int baud = QInputDialog::getInt(qobject_cast<QWidget*>(parent()), QStringLiteral("新增串口目标"),
                                    QStringLiteral("波特率:"),
                                    115200, 1200, 921600, 100, &ok);
    if (!ok) return;

    QStringList dataBitsList = {QStringLiteral("8"), QStringLiteral("7"), QStringLiteral("6"), QStringLiteral("5")};
    QString dataBitsStr = QInputDialog::getItem(qobject_cast<QWidget*>(parent()), QStringLiteral("新增串口目标"),
                                                QStringLiteral("数据位:"), dataBitsList, 0, false, &ok);
    if (!ok) return;
    int dataBits = dataBitsStr.toInt();

    QStringList parityList = {
        QStringLiteral("无校验 (N)"),
        QStringLiteral("偶校验 (E)"),
        QStringLiteral("奇校验 (O)")};
    int parityIdx = parityList.indexOf(QInputDialog::getItem(qobject_cast<QWidget*>(parent()), QStringLiteral("新增串口目标"),
                                              QStringLiteral("校验位:"), parityList, 0, false, &ok));
    if (!ok) return;
    static const int kParityMap[] = {0, 2, 3};
    int parity = kParityMap[parityIdx];

    QStringList stopBitsList = {QStringLiteral("1"), QStringLiteral("1.5"), QStringLiteral("2")};
    int stopIdx = stopBitsList.indexOf(QInputDialog::getItem(qobject_cast<QWidget*>(parent()), QStringLiteral("新增串口目标"),
                                            QStringLiteral("停止位:"), stopBitsList, 0, false, &ok));
    if (!ok) return;
    static const int kStopMap[] = {1, 3, 2};
    int stopBits = kStopMap[stopIdx];

    ForwardTarget::Config cfg;
    cfg.type        = ForwardTarget::Serial;
    cfg.address     = portName.trimmed();
    cfg.portOrBaud  = baud;
    cfg.dataBits    = dataBits;
    cfg.parity      = parity;
    cfg.stopBits    = stopBits;
    cfg.isGgaSource = false;

    NTRIPLoginDialog::instance(qobject_cast<QWidget*>(parent()))->addForwardTarget(cfg);
}

// ---- 清除所有 GGA 源 ----

void ForwarderTableManager::clearAllGgaSources()
{
    auto *dlg = NTRIPLoginDialog::instance(qobject_cast<QWidget*>(parent()));
    m_table->blockSignals(true);
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QTableWidgetItem *i = m_table->item(r, kGgaColumn);
        if (i) i->setCheckState(Qt::Unchecked);
    }
    m_table->blockSignals(false);
    for (int r = 0; r < dlg->forwarderCount(); ++r) {
        dlg->setForwarderGgaSource(r, false);
    }
}

// ---- 表格重建 ----

void ForwarderTableManager::rebuildTable()
{
    auto *dlg = NTRIPLoginDialog::instance(qobject_cast<QWidget*>(parent()));

    m_table->blockSignals(true);
    while (m_table->rowCount() > 0)
        m_table->removeRow(0);

    for (int i = 0; i < dlg->forwarderCount(); ++i) {
        ForwardTarget::Config cfg = dlg->forwarderConfigAt(i);
        int row = m_table->rowCount();
        m_table->insertRow(row);

        // 类型
        auto *typeItem = new QTableWidgetItem(
            cfg.type == ForwardTarget::TCP ? QStringLiteral("TCP") : QStringLiteral("串口"));
        typeItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 0, typeItem);

        // 地址
        auto *addrItem = new QTableWidgetItem(cfg.address);
        addrItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 1, addrItem);

        // 端口/波特率
        QString portText;
        if (cfg.type == ForwardTarget::Serial) {
            QString pStr;
            switch (cfg.parity) {
            case 2:  pStr = QStringLiteral("E"); break;
            case 3:  pStr = QStringLiteral("O"); break;
            default: pStr = QStringLiteral("N"); break;
            }
            QString sStr;
            switch (cfg.stopBits) {
            case 2:  sStr = QStringLiteral("2"); break;
            case 3:  sStr = QStringLiteral("1.5"); break;
            default: sStr = QStringLiteral("1"); break;
            }
            portText = QStringLiteral("%1/%2/%3/%4")
                           .arg(cfg.portOrBaud).arg(cfg.dataBits).arg(pStr).arg(sStr);
        } else {
            portText = QString::number(cfg.portOrBaud);
        }
        auto *portItem = new QTableWidgetItem(portText);
        portItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 2, portItem);

        // GGA源
        auto *ggaItem = new QTableWidgetItem();
        ggaItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        ggaItem->setCheckState(cfg.isGgaSource ? Qt::Checked : Qt::Unchecked);
        m_table->setItem(row, 3, ggaItem);

        // 状态
        auto *statusItem = new QTableWidgetItem(
            dlg->isForwarderRunning(i) ? QStringLiteral("运行中") : QStringLiteral("未启动"));
        statusItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 4, statusItem);

        // 操作列
        auto *opWidget = new QWidget();
        auto *opLayout = new QHBoxLayout(opWidget);
        opLayout->setContentsMargins(4, 2, 4, 2);
        opLayout->setSpacing(6);

        int idx = i;

        bool running = dlg->isForwarderRunning(i);
        auto *toggleBtn = new QPushButton(running ? QStringLiteral("停止") : QStringLiteral("启动"));
        toggleBtn->setObjectName(QStringLiteral("toggleBtn"));
        toggleBtn->setStyleSheet(running ? kOpStopStyle : kOpStartStyle);
        connect(toggleBtn, &QPushButton::clicked, this, [this, idx]() {
            auto *d = NTRIPLoginDialog::instance(qobject_cast<QWidget*>(parent()));
            if (d->isForwarderRunning(idx))
                d->stopForwardTarget(idx);
            else
                d->startForwardTarget(idx);
        });
        opLayout->addWidget(toggleBtn);

        auto *viewBtn = new QPushButton(QStringLiteral("数据查看"));
        viewBtn->setStyleSheet(
            QStringLiteral("QPushButton{background:#17a2b8;color:#fff;border:none;border-radius:3px;"
                           "padding:3px 8px;font-size:11px;}"
                           "QPushButton:hover{background:#138496;}"));
        connect(viewBtn, &QPushButton::clicked, this, [this, idx]() { openDataViewer(idx); });
        opLayout->addWidget(viewBtn);

        auto *saveBtn = new QPushButton(QStringLiteral("保存数据"));
        saveBtn->setObjectName(QStringLiteral("saveBtn"));
        saveBtn->setStyleSheet(
            QStringLiteral("QPushButton{background:#8e44ad;color:#fff;border:none;border-radius:3px;"
                           "padding:3px 8px;font-size:11px;}"
                           "QPushButton:hover{background:#9b59b6;}"));
        connect(saveBtn, &QPushButton::clicked, this, [this, idx, saveBtn]() {
            auto *d = NTRIPLoginDialog::instance(qobject_cast<QWidget*>(parent()));
            if (saveBtn->text() == QStringLiteral("保存中...")) {
                d->disableForwarderSave(idx);
                saveBtn->setText(QStringLiteral("保存数据"));
            } else {
                ForwardTarget::Config c = d->forwarderConfigAt(idx);
                QString defaultName = c.type == ForwardTarget::TCP
                    ? QStringLiteral("tcp_%1_%2.txt").arg(c.address).arg(c.portOrBaud)
                    : QStringLiteral("serial_%1.txt").arg(c.address);
                QString defaultPath = QDir(QCoreApplication::applicationDirPath()).filePath(defaultName);
                QString path = QFileDialog::getSaveFileName(
                    qobject_cast<QWidget*>(parent()), QStringLiteral("选择保存路径"),
                    defaultPath, QStringLiteral("文本文件 (*.txt);;所有文件 (*)"));
                if (!path.isEmpty()) {
                    d->enableForwarderSave(idx, path);
                    saveBtn->setText(QStringLiteral("保存中..."));
                }
            }
        });
        opLayout->addWidget(saveBtn);

        auto *deleteBtn = new QPushButton(QStringLiteral("删除"));
        deleteBtn->setStyleSheet(kOpDeleteStyle);
        connect(deleteBtn, &QPushButton::clicked, this, [this, idx]() {
            auto *d = NTRIPLoginDialog::instance(qobject_cast<QWidget*>(parent()));
            d->stopForwardTarget(idx);
            d->removeForwardTarget(idx);
        });
        opLayout->addWidget(deleteBtn);

        m_table->setCellWidget(row, 5, opWidget);
    }

    m_table->blockSignals(false);
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setStretchLastSection(true);
}

// ---- 行状态更新 ----

void ForwarderTableManager::updateRowStatus(int index, const QString &status)
{
    if (index < 0 || index >= m_table->rowCount()) return;

    auto *item = m_table->item(index, 4);
    if (item) item->setText(status);

    auto *opWidget = m_table->cellWidget(index, 5);
    if (auto *toggleBtn = opWidget ? opWidget->findChild<QPushButton *>(QStringLiteral("toggleBtn")) : nullptr) {
        auto *dlg = NTRIPLoginDialog::instance(qobject_cast<QWidget*>(parent()));
        bool running = dlg->isForwarderRunning(index);
        toggleBtn->setText(running ? QStringLiteral("停止") : QStringLiteral("启动"));
        toggleBtn->setStyleSheet(running ? kOpStopStyle : kOpStartStyle);
    }
}

// ---- 数据查看器 ----

void ForwarderTableManager::openDataViewer(int index)
{
    auto *dlg = NTRIPLoginDialog::instance(qobject_cast<QWidget*>(parent()));
    ForwardTarget::Config cfg = dlg->forwarderConfigAt(index);
    if (cfg.address.isEmpty() && cfg.type == ForwardTarget::TCP) return;

    if (m_dataViewers.contains(index)) {
        DataViewerDialog *viewer = m_dataViewers[index];
        if (viewer) { viewer->show(); viewer->raise(); viewer->activateWindow(); }
        return;
    }

    QString title = cfg.type == ForwardTarget::TCP
                        ? QStringLiteral("数据查看 - TCP %1:%2").arg(cfg.address).arg(cfg.portOrBaud)
                        : QStringLiteral("数据查看 - 串口 %1").arg(cfg.address);

    auto *viewer = new DataViewerDialog(title);
    viewer->setAttribute(Qt::WA_DeleteOnClose);

    // 上下文用 viewer（而非常驻的 this）：查看器窗口关闭时连接自动解除，
    // 避免重复“打开查看器”导致同一数据块被追加多份
    connect(dlg, &NTRIPLoginDialog::forwarderRawData, viewer,
            [index, viewer](int idx, const QByteArray &data) {
                if (idx != index) return;
                viewer->appendData(data);
            });

    connect(viewer, &QObject::destroyed, this, [this, index]() {
        m_dataViewers.remove(index);
    });

    m_dataViewers[index] = viewer;
    viewer->show();
}

void ForwarderTableManager::closeDataViewerForIndex(int index)
{
    if (m_dataViewers.contains(index)) {
        DataViewerDialog *viewer = m_dataViewers[index];
        if (viewer) { viewer->close(); viewer->deleteLater(); }
        m_dataViewers.remove(index);
    }
}
