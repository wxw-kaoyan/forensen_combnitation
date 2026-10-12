#include "modules/ntrip/ntripdialog.h"
#include "ui_NTRIP.h"
#include "modules/ntrip/ntripworker.h"
#include "modules/ntrip/datapool/datapool.h"
#include "modules/ntrip/forwardtarget.h"

#include <QThread>
#include <QCloseEvent>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QDir>

// ---- 单例 ----

NTRIPLoginDialog *NTRIPLoginDialog::instance(QWidget *parent)
{
    static NTRIPLoginDialog *inst = new NTRIPLoginDialog(parent);
    return inst;
}

NTRIPLoginDialog::NTRIPLoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::NTRIPLoginDialog)
    , m_thread(new QThread(this))
    , m_worker(new NTRIPWorker)   // 无父对象，moveToThread 后由线程管理
    , m_dataPool(new DataPool(2 * 1024 * 1024, this))  // 2 MB 环形缓冲
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, false); // 关闭仅隐藏，不析构

    // 加载配置文件（多路径查找，兼容 Qt Creator 调试）
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
        if (!configPath.isEmpty()) {
            QFile f(configPath);
            if (f.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
                f.close();
                QJsonObject ntrip = doc.object().value("ntrip").toObject();
                if (!ntrip.isEmpty()) {
                    setHost(      ntrip.value("host").toString("vrs.sixents.com"));
                    setPort(      QString::number(ntrip.value("port").toInt(8002)));
                    setMountpoint(ntrip.value("mountpoint").toString("RTCM32_GRECJ2"));
                    setUser(      ntrip.value("user").toString("qmet3594"));
                    setPassword(  ntrip.value("password").toString("HWXykJ0l"));
                }
            }
        }
    }

    updateButtonStates();

    // Start / Stop 按钮
    connect(ui->startButton, &QPushButton::clicked, this, &NTRIPLoginDialog::startConnection);
    connect(ui->stopButton,  &QPushButton::clicked, this, &NTRIPLoginDialog::stopConnection);

    // ---- 工作线程 ----
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    // 把 DataPool 传给 worker（同线程安全）
    m_worker->setDataPool(m_dataPool);

    // 状态回传 → 转发信号给主窗口
    connect(m_worker, &NTRIPWorker::statusChanged, this, &NTRIPLoginDialog::connectionStatus);

    // 速率 / 状态 透传给主窗口
    connect(m_worker, &NTRIPWorker::dataRateChanged, this, &NTRIPLoginDialog::dataRateChanged);
    connect(m_worker, &NTRIPWorker::connected, this, [this]() { emit connectionStateChanged(true); });
    connect(m_worker, &NTRIPWorker::disconnected, this, [this]() { emit connectionStateChanged(false); });

    // 连接/断开 → 更新按钮
    connect(m_worker, &NTRIPWorker::connected,    this, [this]() {
        m_connected = true;
        updateButtonStates();
    });
    connect(m_worker, &NTRIPWorker::disconnected, this, [this]() {
        m_connected = false;
        updateButtonStates();
    });
    connect(m_worker, &NTRIPWorker::errorOccurred, this, [this](const QString &err) {
        emit connectionStatus(QStringLiteral("错误: ") + err);
        m_connected = false;
        updateButtonStates();
    });

    // ---- 转发目标信号桥接（worker 线程 → 主线程）----
    connect(m_worker, &NTRIPWorker::forwarderAdded, this, &NTRIPLoginDialog::forwarderAdded);
    connect(m_worker, &NTRIPWorker::forwarderRemoved, this, &NTRIPLoginDialog::forwarderRemoved);
    connect(m_worker, &NTRIPWorker::forwarderStatusChanged, this, [this](int index, const QString &status) {
        // 更新运行状态缓存
        if (index >= 0 && index < m_forwarderRunning.size()) {
            m_forwarderRunning[index] = (status.contains(QStringLiteral("已连接")) ||
                                         status.contains(QStringLiteral("已打开")));
        }
        emit forwarderStatusChanged(index, status);
    });

    // 转发目标原始数据信号代理
    connect(m_worker, &NTRIPWorker::forwarderRawData, this, &NTRIPLoginDialog::forwarderRawData);

    // 转发目标保存状态信号代理
    connect(m_worker, &NTRIPWorker::forwarderSaveStateChanged, this, &NTRIPLoginDialog::forwarderSaveStateChanged);

    m_thread->start();
}

NTRIPLoginDialog::~NTRIPLoginDialog()
{
    m_worker->stop();
    m_thread->quit();
    m_thread->wait(3000);
    delete ui;
}

void NTRIPLoginDialog::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore(); // 不关闭，只隐藏
}

DataPool *NTRIPLoginDialog::dataPool() const { return m_dataPool; }

// 各字段统一去掉首尾空白：手输/粘贴很容易带上看不见的空格，
// 而空格会改变 base64(user:password) 或挂载点/端口，导致 caster 报 401/404
QString NTRIPLoginDialog::host() const       { return ui->ntripHostEdit->text().trimmed(); }
QString NTRIPLoginDialog::port() const       { return ui->ntripPortEdit->text().trimmed(); }
QString NTRIPLoginDialog::mountpoint() const { return ui->ntripMountEdit->text().trimmed(); }
QString NTRIPLoginDialog::user() const       { return ui->ntripUserEdit->text().trimmed(); }
QString NTRIPLoginDialog::password() const   { return ui->ntripPasswordEdit->text().trimmed(); }
bool    NTRIPLoginDialog::isConnected() const { return m_connected; }

void NTRIPLoginDialog::setHost(const QString &v)       { ui->ntripHostEdit->setText(v); }
void NTRIPLoginDialog::setPort(const QString &v)       { ui->ntripPortEdit->setText(v); }
void NTRIPLoginDialog::setMountpoint(const QString &v)  { ui->ntripMountEdit->setText(v); }
void NTRIPLoginDialog::setUser(const QString &v)       { ui->ntripUserEdit->setText(v); }
void NTRIPLoginDialog::setPassword(const QString &v)   { ui->ntripPasswordEdit->setText(v); }

void NTRIPLoginDialog::setFixGGA(double lon, double lat, double height)
{
    QMetaObject::invokeMethod(m_worker, "setGgaPosition", Qt::QueuedConnection,
                              Q_ARG(double, lon),
                              Q_ARG(double, lat),
                              Q_ARG(double, height));
}

void NTRIPLoginDialog::setUseFixGGA(bool enabled)
{
    QMetaObject::invokeMethod(m_worker, "setUseFixGGA", Qt::QueuedConnection,
                              Q_ARG(bool, enabled));
}

void NTRIPLoginDialog::forwardGGA(const QByteArray &nmeaSentence)
{
    QMetaObject::invokeMethod(m_worker, "forwardGGA", Qt::QueuedConnection,
                              Q_ARG(QByteArray, nmeaSentence));
}

// ---- 连接 / 断开 ----

void NTRIPLoginDialog::startConnection()
{
    // 防重复点击：立即禁用两个按钮
    ui->startButton->setEnabled(false);
    ui->stopButton->setEnabled(false);

    // 通过跨线程信号调用 worker
    QMetaObject::invokeMethod(m_worker, "start", Qt::QueuedConnection,
                              Q_ARG(QString, host()),
                              Q_ARG(quint16, port().toUShort()),
                              Q_ARG(QString, mountpoint()),
                              Q_ARG(QString, user()),
                              Q_ARG(QString, password()));
}

void NTRIPLoginDialog::stopConnection()
{
    QMetaObject::invokeMethod(m_worker, "stop", Qt::QueuedConnection);
}

void NTRIPLoginDialog::updateButtonStates()
{
    ui->startButton->setEnabled(!m_connected);
    ui->stopButton->setEnabled(m_connected);
}

// ---- 转发目标管理（全部代理到工作线程）----

int NTRIPLoginDialog::addForwardTarget(const ForwardTarget::Config &cfg)
{
    // 缓存配置（主线程可安全读取）
    int index = m_forwarderConfigs.size();
    m_forwarderConfigs.append(cfg);
    m_forwarderRunning.append(false);

    QMetaObject::invokeMethod(m_worker, "addForwardTarget", Qt::QueuedConnection,
                              Q_ARG(ForwardTarget::Config, cfg));
    return index;
}

void NTRIPLoginDialog::removeForwardTarget(int index)
{
    if (index < 0 || index >= m_forwarderConfigs.size()) return;
    m_forwarderConfigs.removeAt(index);
    m_forwarderRunning.removeAt(index);
    QMetaObject::invokeMethod(m_worker, "removeForwardTarget", Qt::QueuedConnection,
                              Q_ARG(int, index));
}

void NTRIPLoginDialog::startForwardTarget(int index)
{
    if (index >= 0 && index < m_forwarderRunning.size())
        m_forwarderRunning[index] = true;
    QMetaObject::invokeMethod(m_worker, "startForwardTarget", Qt::QueuedConnection,
                              Q_ARG(int, index));
}

void NTRIPLoginDialog::stopForwardTarget(int index)
{
    if (index >= 0 && index < m_forwarderRunning.size())
        m_forwarderRunning[index] = false;
    QMetaObject::invokeMethod(m_worker, "stopForwardTarget", Qt::QueuedConnection,
                              Q_ARG(int, index));
}

int NTRIPLoginDialog::forwarderCount() const
{
    return m_forwarderConfigs.size();
}

ForwardTarget::Config NTRIPLoginDialog::forwarderConfigAt(int index) const
{
    if (index < 0 || index >= m_forwarderConfigs.size())
        return ForwardTarget::Config();
    return m_forwarderConfigs[index];
}

bool NTRIPLoginDialog::isForwarderRunning(int index) const
{
    if (index < 0 || index >= m_forwarderRunning.size()) return false;
    return m_forwarderRunning[index];
}

void NTRIPLoginDialog::setForwarderGgaSource(int index, bool enabled)
{
    if (index >= 0 && index < m_forwarderConfigs.size()) {
        m_forwarderConfigs[index].isGgaSource = enabled;
    }
    QMetaObject::invokeMethod(m_worker, "setForwardTargetGgaSource", Qt::QueuedConnection,
                              Q_ARG(int, index), Q_ARG(bool, enabled));
}

void NTRIPLoginDialog::enableForwarderSave(int index, const QString &filePath)
{
    QMetaObject::invokeMethod(m_worker, "enableForwarderSave", Qt::QueuedConnection,
                              Q_ARG(int, index), Q_ARG(QString, filePath));
}

void NTRIPLoginDialog::disableForwarderSave(int index)
{
    QMetaObject::invokeMethod(m_worker, "disableForwarderSave", Qt::QueuedConnection,
                              Q_ARG(int, index));
}
