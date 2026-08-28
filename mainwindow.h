#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class CommTool;
class ToolProcessManager;
class NtripPageManager;
class ParsePanel;
class SimulatorPanel;

namespace Analysis { class AnalysisPanel; }

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    Ui::MainWindow *ui;
    CommTool *m_commTool = nullptr;

    // NTRIP 页面（独立模块：配置 / FixGGA / 转发表 / 保存数据）
    NtripPageManager *m_ntripPage = nullptr;

    // 数据解析面板
    ParsePanel *m_parsePanel = nullptr;

    // 常用工具 — 外部进程（批量管理）
    ToolProcessManager *m_toolManager = nullptr;

    // 数据分析面板
    Analysis::AnalysisPanel *m_analysisPanel = nullptr;

    // PPS + 数据模拟面板
    SimulatorPanel *m_simulatorPanel = nullptr;
};

#endif // MAINWINDOW_H
