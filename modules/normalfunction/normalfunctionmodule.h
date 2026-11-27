#ifndef NORMALFUNCTIONMODULE_H
#define NORMALFUNCTIONMODULE_H

#include <QObject>

class QWidget;
class QPushButton;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// 前向声明：子面板
class NavCalibPanel;
class AllanPanel;
class Calib612Panel;
class TempDriftPanel;

// ════════════════════════════════════════════════════════════════
//  常用工具模块门面类：持有所有子面板的生命周期、统一入口、
//  屏蔽 commtool 对子模块内部的直接依赖。
//
//  两类子模块：
//    * NavCalib — 实时类，需要 commtool 串收到帧时调 submitFrame()
//      （紧耦合：commtool 持有 NormalFunctionModule，只通过它暴露的
//      最小接口间接访问 NavCalib）
//    * Allan    — 离线类，完全不依赖串口/commtool。
//      自己读文件、后台线程算、自己显示。
//    * Calib612 — 离线类，6+12 位置标定（加计 12 位置 + 陀螺 6 位置），
//      与 Allan 同构：选 6 列 → 后台线程算 → 表格 + 检段日志。
//    * TempDrift — 离线类，拉温（温漂）分析：6 轴 + 温度 → 平滑/峰峰值/标准差 +
//      保温平台筛选 + 值-温度特性曲线。
// ════════════════════════════════════════════════════════════════
class NormalFunctionModule : public QObject
{
    Q_OBJECT

public:
    explicit NormalFunctionModule(Ui::MainWindow *ui, QObject *parent = nullptr);
    ~NormalFunctionModule() override;

    // ---- NavCalib 接口（给 commtool 用，最小集合）----
    bool isNavCalibCollecting() const;
    void submitNavCalibFrame(const class ParsedFrame &f);
    bool navCalibStrictCrc() const;
    void stopNavCalibCollecting();

    // ---- 入口（按钮点）----
    void openNavCalib();
    void openAllan();
    void openCalib612();
    void openTempDrift();

private:
    Ui::MainWindow *m_ui = nullptr;

    // 子面板：首次点击时构造（延迟）
    NavCalibPanel  *m_navCalib    = nullptr;
    AllanPanel     *m_allanPanel  = nullptr;
    Calib612Panel  *m_calibPanel  = nullptr;
    TempDriftPanel *m_tempPanel   = nullptr;

    // 离线工具后台线程（构造时创建并常驻，moveToThread worker）
    QThread *m_allanThread = nullptr;
    QThread *m_calibThread = nullptr;
    QThread *m_tempThread  = nullptr;
};

#endif // NORMALFUNCTIONMODULE_H
