#ifndef TOOLPROCESS_H
#define TOOLPROCESS_H

#include <QObject>
#include <QProcess>
#include <QString>

class QPushButton;
class QLabel;

// 外部工具进程管理器
// 每个实例管理一个外部 .exe，负责：查找 → 启动 → 状态同步 → 退出清理
// 新增工具只需在 MainWindow 里 new ToolProcess(...) 即可，无需写重复代码
class ToolProcess : public QObject
{
    Q_OBJECT

public:
    struct Config {
        QString exeName;            // 文件名，如 "Converter3.0.7.exe"
        QString displayName;        // 日志/提示用的中文名，如 "和芯解析"
        QPushButton *button = nullptr;   // 关联的启动按钮
        QLabel      *statusLabel = nullptr; // 关联的状态标签
        QStringList  arguments;      // 启动参数，可选
    };

    // 工厂方法：从 Config 创建并初始化
    static ToolProcess *create(const Config &cfg, QObject *parent = nullptr);

    // 启动 / 终止
    void launch();                  // 若已在运行则忽略
    void terminate();               // 强制终止
    bool isRunning() const;

signals:
    // 状态变更 → 外部自行决定 UI 更新方式
    void started();
    void stopped(int exitCode, QProcess::ExitStatus status);
    void errorOccurred(const QString &msg);

private slots:
    void onProcessStateChanged(QProcess::ProcessState state);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessErrorOccurred(QProcess::ProcessError error);

private:
    explicit ToolProcess(const Config &cfg, QObject *parent = nullptr);
    ~ToolProcess() override;

    QString resolveExePath() const;  // 查找 exe：相对 → 绝对后备
    void cleanupProcess();

    Config    m_cfg;
    QProcess *m_process = nullptr;
    QString   m_resolvedPath;

    static const QString kFallbackDir;  // 绝对路径后备目录
};

// 批量注册工具，一行一个：m_converter = ToolProcess::create({...}, this);
// 示例：
//   ToolProcess::create({
//       .exeName = "Converter3.0.7.exe",
//       .displayName = "和芯解析",
//       .button = ui->converterButton,
//       .statusLabel = ui->converterStatusLabel
//   }, this);

#endif // TOOLPROCESS_H
