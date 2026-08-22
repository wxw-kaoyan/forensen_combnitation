#include "ntrippagemanager.h"
#include "ntripdialog.h"
#include "forwardertablemanager.h"
#include "modules/ntrip/datapool/datapool.h"

#include <QPushButton>
#include <QPlainTextEdit>
#include <QLabel>
#include <QCheckBox>
#include <QFrame>
#include <QLineEdit>
#include <QMainWindow>
#include <QStatusBar>
#include <QBoxLayout>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>

NtripPageManager::NtripPageManager(const Widgets &w, ForwarderTableManager *ftm,
                                     QObject *parent)
    : QObject(parent)
    , m_w(w)
    , m_ftm(ftm)
{
    loadFixGgaConfig();
    setupConnections();
    createSaveButton();
}

NtripPageManager::~NtripPageManager()
{
    if (m_saveFile.isOpen())
        m_saveFile.close();
}

// ---- FixGGA 配置加载 ----

void NtripPageManager::loadFixGgaConfig()
{
    QStringList searchPaths = {
        QCoreApplication::applicationDirPath(),
        QDir(QCoreApplication::applicationDirPath()).filePath(".."),
        QDir(QCoreApplication::applicationDirPath()).filePath("../.."),
    };
    QString configPath;
    for (const auto &p : searchPaths) {
        QString candidate = QDir(p).filePath("config.json");
        if (QFile::exists(candidate)) { configPath = candidate; break; }
    }
    if (configPath.isEmpty()) return;

    QFile f(configPath);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    QJsonObject fixGGA = doc.object().value("fixGGA").toObject();
    if (fixGGA.isEmpty()) return;

    m_w.fixLonEdit->setText(QString::number(fixGGA.value("longitude").toDouble(121.47370), 'f', 5));
    m_w.fixLatEdit->setText(QString::number(fixGGA.value("latitude").toDouble(31.23040), 'f', 5));
    m_w.fixHeightEdit->setText(QString::number(fixGGA.value("height").toDouble(10.0), 'f', 1));
}

// ---- 信号连接 ----

void NtripPageManager::setupConnections()
{
    auto *dlg = NTRIPLoginDialog::instance(qobject_cast<QWidget*>(parent()));

    // 配置按钮
    connect(m_w.configButton, &QPushButton::clicked, this, [dlg, this]() {
        connect(dlg, &NTRIPLoginDialog::connectionStatus,
                m_w.logEdit, &QPlainTextEdit::appendPlainText,
                Qt::UniqueConnection);
        dlg->show();
        dlg->raise();
        dlg->activateWindow();
    });

    // 状态卡片
    connect(dlg, &NTRIPLoginDialog::connectionStateChanged, this,
            [this](bool connected) {
                m_w.connectStatusLabel->setText(connected ? QStringLiteral("已连接") : QStringLiteral("未连接"));
                m_w.connectStatusLabel->setStyleSheet(
                    connected
                        ? QStringLiteral("font-size: 14px; font-weight: 700; color: #27ae60;")
                        : QStringLiteral("font-size: 14px; font-weight: 700; color: #12263f;"));
            });

    connect(dlg, &NTRIPLoginDialog::dataRateChanged, this,
            [this](const QString &rate, qint64) {
                m_w.rateLabel->setText(rate);
            });

    // FixGGA 与转发表 GGA 源互斥
    connect(m_ftm, &ForwarderTableManager::tableGgaSourceChecked, this, [this, dlg]() {
        m_w.fixGgaCheckBox->blockSignals(true);
        m_w.fixGgaCheckBox->setChecked(false);
        m_w.fixGgaCheckBox->blockSignals(false);
        dlg->setUseFixGGA(false);
    });

    // FixGGA 同步
    auto syncFixGGA = [this, dlg]() {
        if (m_w.fixGgaCheckBox->isChecked()) {
            dlg->setFixGGA(m_w.fixLonEdit->text().toDouble(),
                           m_w.fixLatEdit->text().toDouble(),
                           m_w.fixHeightEdit->text().toDouble());
            dlg->setUseFixGGA(true);
        } else {
            dlg->setUseFixGGA(false);
        }
    };

    m_w.fixGgaFrame->setVisible(m_w.fixGgaCheckBox->isChecked());
    connect(m_w.fixGgaCheckBox, &QCheckBox::toggled, this, [this, syncFixGGA](bool checked) {
        m_w.fixGgaFrame->setEnabled(checked);
        m_w.fixGgaFrame->setVisible(checked);
        syncFixGGA();
        if (checked) m_ftm->clearAllGgaSources();
    });
    connect(m_w.fixLonEdit, &QLineEdit::textChanged, this, syncFixGGA);
    connect(m_w.fixLatEdit, &QLineEdit::textChanged, this, syncFixGGA);
    connect(m_w.fixHeightEdit, &QLineEdit::textChanged, this, syncFixGGA);
    syncFixGGA();
}

// ---- 保存数据按钮 ----

void NtripPageManager::createSaveButton()
{
    m_saveDataButton = new QPushButton(QStringLiteral("保存数据"));
    m_saveDataButton->setMinimumSize(80, 42);
    m_saveDataButton->setStyleSheet(
        "QPushButton { background: #27ae60; color: white; border-radius: 8px; "
        "padding: 8px 14px; font-weight: 600; }"
        "QPushButton:hover { background: #2ecc71; }");
    connect(m_saveDataButton, &QPushButton::clicked, this, &NtripPageManager::onToggleSaveData);
    m_w.headerLayout->insertWidget(1, m_saveDataButton);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setInterval(200);
}

void NtripPageManager::onToggleSaveData()
{
    auto *dlg = NTRIPLoginDialog::instance(qobject_cast<QWidget*>(parent()));

    if (m_saveFile.isOpen()) {
        m_saveTimer->stop();
        disconnect(m_saveTimer, &QTimer::timeout, this, nullptr);

        if (m_saveConsumerId >= 0 && dlg->dataPool()) {
            QByteArray remain = dlg->dataPool()->read(m_saveConsumerId);
            if (!remain.isEmpty()) m_saveFile.write(remain);
        }
        m_saveConsumerId = -1;

        m_saveFile.flush();
        qint64 written = m_saveFile.size();
        m_saveFile.close();
        m_saveDataButton->setText(QStringLiteral("保存数据"));
        m_saveDataButton->setStyleSheet(
            "QPushButton { background: #27ae60; color: white; border-radius: 8px; "
            "padding: 8px 14px; font-weight: 600; }"
            "QPushButton:hover { background: #2ecc71; }");

        auto *mw = qobject_cast<QMainWindow*>(parent());
        if (mw) mw->statusBar()->showMessage(
            QStringLiteral("已停止保存 (%1 KB)").arg(written / 1024), 5000);
        return;
    }

    auto *w = qobject_cast<QWidget*>(parent());
    QString dir = QFileDialog::getExistingDirectory(w, QStringLiteral("选择保存目录"));
    if (dir.isEmpty()) return;

    QString defaultName = QStringLiteral("ntrip_%1.bin")
                              .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    bool ok = false;
    QString name = QInputDialog::getText(w, QStringLiteral("文件名"),
                                         QStringLiteral("输入文件名:"), QLineEdit::Normal,
                                         defaultName, &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    if (!name.endsWith(".bin", Qt::CaseInsensitive)) name += ".bin";

    QString path = QDir(dir).filePath(name);
    m_saveFile.setFileName(path);
    if (!m_saveFile.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(w, QStringLiteral("错误"),
                             QStringLiteral("无法创建文件: %1").arg(path));
        return;
    }

    if (dlg->dataPool())
        m_saveConsumerId = dlg->dataPool()->subscribe();

    connect(m_saveTimer, &QTimer::timeout, this, [this, dlg]() {
        if (!m_saveFile.isOpen()) return;
        auto *pool = dlg->dataPool();
        if (!pool) return;
        QByteArray data = pool->read(m_saveConsumerId);
        if (data.isEmpty()) return;
        m_saveFile.write(data);
    });
    m_saveTimer->start();

    m_saveDataButton->setText(QStringLiteral("停止保存"));
    m_saveDataButton->setStyleSheet(
        "QPushButton { background: #e67e22; color: white; border-radius: 8px; "
        "padding: 8px 14px; font-weight: 600; }"
        "QPushButton:hover { background: #f39c12; }");

    auto *mw = qobject_cast<QMainWindow*>(parent());
    if (mw) mw->statusBar()->showMessage(QStringLiteral("正在保存到: %1").arg(path), 0);
}
