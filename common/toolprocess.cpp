#include "toolprocess.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>

const QString ToolProcess::kFallbackDir = QStringLiteral("C:/Users/wxw/Desktop/QT/TASoftware");

// ===================================================================
// 构造 / 析构
// ===================================================================

ToolProcess::ToolProcess(const Config &cfg, QObject *parent)
    : QObject(parent), m_cfg(cfg)
{
}

ToolProcess::~ToolProcess()
{
    cleanupProcess();
}

ToolProcess *ToolProcess::create(const Config &cfg, QObject *parent)
{
    auto *tp = new ToolProcess(cfg, parent);

    // 按钮点击 → 启动
    if (tp->m_cfg.button) {
        QObject::connect(tp->m_cfg.button, &QPushButton::clicked, tp, [tp]() {
            tp->launch();
        });
    }

    // 默认：状态变更自动更新 UI 标签和按钮使能
    QObject::connect(tp, &ToolProcess::started, tp, [tp]() {
        if (tp->m_cfg.button)      tp->m_cfg.button->setEnabled(false);
        if (tp->m_cfg.statusLabel) {
            tp->m_cfg.statusLabel->setText(QStringLiteral("运行中"));
            tp->m_cfg.statusLabel->setStyleSheet(
                QStringLiteral("font-size:12px;color:#27ae60;font-weight:600;"));
        }
    });

    QObject::connect(tp, &ToolProcess::stopped, tp, [tp](int, QProcess::ExitStatus) {
        if (tp->m_cfg.button)      tp->m_cfg.button->setEnabled(true);
        if (tp->m_cfg.statusLabel) {
            tp->m_cfg.statusLabel->setText(QStringLiteral("未启动"));
            tp->m_cfg.statusLabel->setStyleSheet(
                QStringLiteral("font-size:12px;color:#617487;"));
        }
    });

    return tp;
}

// ===================================================================
// 查找 exe 路径：先相对路径 → 再绝对后备
// ===================================================================

QString ToolProcess::resolveExePath() const
{
    // 1. 程序同目录
    QString path = QDir(QCoreApplication::applicationDirPath())
                       .filePath(m_cfg.exeName);
    if (QFile::exists(path))
        return path;

    // 2. 固定后备目录
    path = QDir(kFallbackDir).filePath(m_cfg.exeName);
    if (QFile::exists(path))
        return path;

    return {};
}

// ===================================================================
// 启动
// ===================================================================

void ToolProcess::launch()
{
    if (isRunning())
        return;

    m_resolvedPath = resolveExePath();
    if (m_resolvedPath.isEmpty()) {
        QMessageBox::warning(nullptr,
                             QStringLiteral("启动失败"),
                             QStringLiteral("找不到 %1\n(%2)\n"
                                            "请确认文件存在于程序目录或 %3 下。")
                                 .arg(m_cfg.exeName, m_cfg.displayName, kFallbackDir));
        return;
    }

    // 清理旧实例
    cleanupProcess();

    m_process = new QProcess(this);

    connect(m_process, &QProcess::stateChanged,
            this, &ToolProcess::onProcessStateChanged);
    connect(m_process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &ToolProcess::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &ToolProcess::onProcessErrorOccurred);

    m_process->setProgram(m_resolvedPath);
    if (!m_cfg.arguments.isEmpty())
        m_process->setArguments(m_cfg.arguments);
    m_process->setWorkingDirectory(QFileInfo(m_resolvedPath).absolutePath());
    m_process->start();
}

void ToolProcess::terminate()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000))
            m_process->kill();
    }
}

bool ToolProcess::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

// ===================================================================
// 内部槽
// ===================================================================

void ToolProcess::onProcessStateChanged(QProcess::ProcessState state)
{
    if (state == QProcess::Running) {
        emit started();
    }
    // Starting / NotRunning 不单独发射，finished 统一处理
}

void ToolProcess::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    emit stopped(exitCode, status);
}

void ToolProcess::onProcessErrorOccurred(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    if (m_process) {
        emit errorOccurred(
            QStringLiteral("%1 进程错误: %2")
                .arg(m_cfg.displayName, m_process->errorString()));
    }
}

void ToolProcess::cleanupProcess()
{
    if (m_process) {
        // 断开所有信号，防止析构时触发 stateChanged/finished
        m_process->disconnect(this);
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(2000);
        }
        m_process->deleteLater();
        m_process = nullptr;
    }
}
