#include "normalfunctionmodule.h"

#include "modules/parse/parser/core/protocoldefs.h"     // ParsedFrame 定义
#include "modules/normalfunction/navcalib/navcalibpanel.h"
#include "modules/normalfunction/allan/allanpanel.h"
#include "modules/normalfunction/calib612/calib612panel.h"
#include "modules/normalfunction/tempdrift/tempdriftpanel.h"
#include "ui_mainwindow.h"

#include <QPushButton>
#include <QThread>
#include <QWidget>
#include <QBoxLayout>

namespace {

// 找「直接包含」w 的那个 layout。
// 不能用 QLayout::indexOf()：QBoxLayout 的实现会递归钻进子 layout，
// 会在外层 VBox 上就返回 >=0，导致插回 VBox（按钮被拉成整行横条）。
QBoxLayout *findDirectLayout(QLayout *root, QWidget *w)
{
    if (!root || !w) return nullptr;
    for (int i = 0; i < root->count(); ++i) {
        QLayoutItem *it = root->itemAt(i);
        if (it && it->widget() == w)
            return qobject_cast<QBoxLayout*>(root);
    }
    for (int i = 0; i < root->count(); ++i) {
        QLayoutItem *it = root->itemAt(i);
        if (it && it->layout()) {
            if (auto *found = findDirectLayout(it->layout(), w))
                return found;
        }
    }
    return nullptr;
}

} // namespace

NormalFunctionModule::NormalFunctionModule(Ui::MainWindow *ui, QObject *parent)
    : QObject(parent), m_ui(ui)
{
    // ---- 在「超达投影」按钮右侧横排插入离线工具入口 ----
    // 注意：navCalibButton 实际位于嵌套的 QHBoxLayout(toolsLayout3) 里，
    // 而 parentWidget()->layout() 是外层 QVBoxLayout。直接往 VBox 里插会得到
    // 一条撑满整行宽度的横条（看起来"特别大"）。必须先定位承载 navBtn 的横向 layout。
    if (m_ui && m_ui->navCalibButton) {
        QPushButton *navBtn = m_ui->navCalibButton;
        QWidget *host = navBtn->parentWidget();
        QBoxLayout *navRow = host ? findDirectLayout(host->layout(), navBtn) : nullptr;

        if (navRow) {
            auto makeToolButton = [host, navBtn](const QString &text, const QString &tip) {
                auto *btn = new QPushButton(text, host);
                btn->setToolTip(tip);
                btn->setMinimumSize(navBtn->minimumSize());      // 与超达投影同尺寸
                btn->setSizePolicy(navBtn->sizePolicy());
                btn->setFont(navBtn->font());
                btn->setStyleSheet(navBtn->styleSheet());
                if (!navBtn->icon().isNull()) {
                    btn->setIcon(navBtn->icon());
                    btn->setIconSize(navBtn->iconSize());
                }
                return btn;
            };

            int pos = navRow->count();
            for (int i = 0; i < navRow->count(); ++i) {
                QLayoutItem *it = navRow->itemAt(i);
                if (it && it->widget() == navBtn) { pos = i + 1; break; }
            }

            auto *allanBtn = makeToolButton(
                QStringLiteral("Allan 方差分析"),
                QStringLiteral("离线 CSV → Allan σ-τ 曲线 + 噪声参数"));
            navRow->insertWidget(pos, allanBtn);
            connect(allanBtn, &QPushButton::clicked, this, &NormalFunctionModule::openAllan);

            auto *calibBtn = makeToolButton(
                QStringLiteral("6+12 位置标定"),
                QStringLiteral("离线 IMU 六轴 → 加计 12 位置 / 陀螺 6 位置标定参数"));
            navRow->insertWidget(pos + 1, calibBtn);
            connect(calibBtn, &QPushButton::clicked, this, &NormalFunctionModule::openCalib612);

            auto *tempBtn = makeToolButton(
                QStringLiteral("拉温分析"),
                QStringLiteral("温度循环数据 → 平滑/峰峰值/标准差 + 保温平台 + 温度特性曲线"));
            navRow->insertWidget(pos + 2, tempBtn);
            connect(tempBtn, &QPushButton::clicked, this, &NormalFunctionModule::openTempDrift);
        }
    }

    // ---- 离线工具后台线程（常驻，避免首次点按钮才启动卡一下）----
    // 面板创建时把各自的 Worker 构造在这个线程上，moveToThread 后常驻
    m_allanThread = new QThread(this);
    m_allanThread->start();
    m_calibThread = new QThread(this);
    m_calibThread->start();
    m_tempThread = new QThread(this);
    m_tempThread->start();
}

NormalFunctionModule::~NormalFunctionModule()
{
    for (QThread *t : {m_allanThread, m_calibThread, m_tempThread}) {
        if (!t) continue;
        t->quit();
        t->wait(1000);   // 给后台计算收尾留 1s
        delete t;
    }
    m_allanThread = nullptr;
    m_calibThread = nullptr;
    m_tempThread = nullptr;
}

// ───────────────── NavCalib 接口 ─────────────────
bool NormalFunctionModule::isNavCalibCollecting() const
{
    return m_navCalib && m_navCalib->isCollecting();
}

void NormalFunctionModule::submitNavCalibFrame(const ParsedFrame &f)
{
    if (m_navCalib) m_navCalib->submitFrame(f);
}

bool NormalFunctionModule::navCalibStrictCrc() const
{
    return m_navCalib && m_navCalib->strictCrc();
}

void NormalFunctionModule::stopNavCalibCollecting()
{
    if (m_navCalib) m_navCalib->stopCollecting();
}

// ───────────────── 入口 ─────────────────
void NormalFunctionModule::openNavCalib()
{
    if (!m_navCalib && m_ui)
        m_navCalib = new NavCalibPanel(m_ui->centralwidget);

    if (m_navCalib) {
        m_navCalib->show();
        m_navCalib->raise();
        m_navCalib->activateWindow();
    }
}

void NormalFunctionModule::openAllan()
{
    if (!m_allanPanel && m_ui)
        m_allanPanel = new AllanPanel(m_ui->centralwidget, m_allanThread);

    if (m_allanPanel) {
        m_allanPanel->show();
        m_allanPanel->raise();
        m_allanPanel->activateWindow();
    }
}

void NormalFunctionModule::openCalib612()
{
    if (!m_calibPanel && m_ui)
        m_calibPanel = new Calib612Panel(m_ui->centralwidget, m_calibThread);

    if (m_calibPanel) {
        m_calibPanel->show();
        m_calibPanel->raise();
        m_calibPanel->activateWindow();
    }
}

void NormalFunctionModule::openTempDrift()
{
    if (!m_tempPanel && m_ui)
        m_tempPanel = new TempDriftPanel(m_ui->centralwidget, m_tempThread);

    if (m_tempPanel) {
        m_tempPanel->show();
        m_tempPanel->raise();
        m_tempPanel->activateWindow();
    }
}
