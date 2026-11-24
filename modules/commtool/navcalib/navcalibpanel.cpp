#include "navcalibpanel.h"
#include "navcalibplotwindow.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

NavCalibPanel::NavCalibPanel(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("超达投影估计"));
    resize(980, 660);
    setMinimumSize(820, 560);

    // ── 左侧：操作 + 结果 ──
    auto *hint = new QLabel(
        QStringLiteral("车辆原地缓慢旋转一圈以上（越慢越好）。"
                       "默认用位置法（与 MATLAB 一致，圆心要尽量不动）；"
                       "速度法作为备选，整车平移不影响结果。"
                       "设备需输出 0x0166 融合帧，并保持 RTK 固定解。"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#8496a8; font-size:11px;"));

    auto *group = new QGroupBox(QStringLiteral("水平杆臂标定"), this);
    auto *vbox  = new QVBoxLayout(group);
    vbox->setSpacing(6);

    m_startBtn = new QPushButton(QStringLiteral("开始采集"), group);
    m_resetBtn = new QPushButton(QStringLiteral("重置"), group);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_resetBtn);
    btnRow->addStretch();

    // CRC 默认不勾选，与全局一致；坏帧靠 3σ 剔除兜底
    m_crcCheck = new QCheckBox(QStringLiteral("CRC 校验"), group);
    m_crcCheck->setChecked(false);
    m_crcCheck->setToolTip(QStringLiteral(
        "勾选后严格校验 CRC32。默认关闭——部分固件 CRC 算法与本机实现不一致，\n"
        "开启后可能一帧都收不到。坏帧另有 3σ 野值剔除兜底。"));

    auto *crcRow = new QHBoxLayout;
    crcRow->setSpacing(6);
    crcRow->addWidget(m_crcCheck);
    crcRow->addStretch();

    // 解算方式：位置法（原生，MATLAB 对齐）/ 速度法（备选）
    m_method = new QComboBox(group);
    m_method->addItem(QStringLiteral("位置法（4 参数，原模型）"),
                      static_cast<int>(LeverArmEstimator::Method::Position));
    m_method->addItem(QStringLiteral("速度法（2 参数，备选）"),
                      static_cast<int>(LeverArmEstimator::Method::Velocity));
    m_method->setToolTip(QStringLiteral(
        "位置法：P = C + R(ψ)·l，同时解圆心与杆臂（与 MATLAB calibrate_lever_arm.m 一致）。\n"
        "速度法：对位置求导消掉平移，只解杆臂；转速低于 0.2 m/s 时结果偏小。"));
    auto *methodRow = new QHBoxLayout;
    methodRow->setSpacing(6);
    methodRow->addWidget(new QLabel(QStringLiteral("解算方式"), group));
    methodRow->addWidget(m_method, 1);

    m_statusLabel = new QLabel(
        QStringLiteral("点「开始采集」，让车辆原地缓慢转一圈以上"), group);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#617487; font-size:11px;"));

    m_resultEdit = new QPlainTextEdit(group);
    m_resultEdit->setReadOnly(true);
    m_resultEdit->setMinimumHeight(240);
    m_resultEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_resultEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_resultEdit->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background:#f7fafc; border:1px solid #d8e2ec;"
        " border-radius:6px; color:#12263f; }"));

    // 位置法：设备当前配置，用于给出增量 Δ；速度法：仅用于对比差值
    m_curLx = new QDoubleSpinBox(group);
    m_curLy = new QDoubleSpinBox(group);
    for (QDoubleSpinBox *sp : {m_curLx, m_curLy}) {
        sp->setRange(-20.0, 20.0);
        sp->setDecimals(4);
        sp->setSingleStep(0.001);
        sp->setSuffix(QStringLiteral(" m"));
        sp->setToolTip(QStringLiteral("设备里当前配置的杆臂：位置法用它算增量 Δ，"
                                      "速度法只用于对比"));
    }

    auto *cfgRow = new QHBoxLayout;
    cfgRow->setSpacing(6);
    cfgRow->addWidget(new QLabel(QStringLiteral("当前 lx"), group));
    cfgRow->addWidget(m_curLx, 1);
    cfgRow->addWidget(new QLabel(QStringLiteral("ly"), group));
    cfgRow->addWidget(m_curLy, 1);

    vbox->addLayout(btnRow);
    vbox->addLayout(crcRow);
    vbox->addLayout(methodRow);
    vbox->addWidget(m_statusLabel);
    vbox->addWidget(m_resultEdit, 1);
    vbox->addLayout(cfgRow);

    auto *left = new QWidget(this);
    left->setMinimumWidth(360);
    left->setMaximumWidth(430);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);
    leftLayout->addWidget(hint);
    leftLayout->addWidget(group, 1);

    // ── 右侧：实时轨迹图 ──
    m_plot = new NavCalibPlotWindow(this);

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);
    mainLayout->addWidget(left, 0);
    mainLayout->addWidget(m_plot, 1);

    // ── 交互 ──
    connect(m_startBtn, &QPushButton::clicked, this, &NavCalibPanel::onStartStop);
    connect(m_resetBtn, &QPushButton::clicked, this, &NavCalibPanel::onReset);
    connect(m_curLx, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { refreshResult(); });
    connect(m_curLy, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { refreshResult(); });
    connect(m_method, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { refreshResult(); m_plotDirty = true; });

    // 文字逐帧刷新，图 5Hz
    m_plotTimer = new QTimer(this);
    m_plotTimer->setInterval(200);
    connect(m_plotTimer, &QTimer::timeout, this, &NavCalibPanel::onRefreshPlot);
    m_plotTimer->start();
}

bool NavCalibPanel::strictCrc() const
{
    return m_crcCheck && m_crcCheck->isChecked();
}

LeverArmEstimator::Method NavCalibPanel::currentMethod() const
{
    if (m_method
        && m_method->currentData().toInt()
               == static_cast<int>(LeverArmEstimator::Method::Velocity)) {
        return LeverArmEstimator::Method::Velocity;
    }
    return LeverArmEstimator::Method::Position;
}

void NavCalibPanel::refreshResult()
{
    if (!m_collecting && m_result.totalFrames == 0)
        return;
    m_result = m_est.solve(currentMethod());
    updateText();
    m_plotDirty = true;
}

void NavCalibPanel::submitFrame(const ParsedFrame &f)
{
    if (!m_collecting || f.protocol != ProtocolID::FusionResult)
        return;

    m_est.addFusion(f.fusion);
    m_result = m_est.solve(currentMethod());   // 逐帧解算（10Hz 开销可忽略）
    updateText();
    m_plotDirty = true;
}

void NavCalibPanel::stopCollecting()
{
    if (!m_collecting)
        return;
    onStartStop();   // 复用停止逻辑
}

void NavCalibPanel::closeEvent(QCloseEvent *event)
{
    stopCollecting();   // 关窗即停止采集，避免后台空转
    QDialog::closeEvent(event);
}

void NavCalibPanel::onStartStop()
{
    m_collecting = !m_collecting;
    m_startBtn->setText(m_collecting ? QStringLiteral("停止采集")
                                     : QStringLiteral("开始采集"));

    if (m_collecting) {
        m_est.reset();
        m_result = LeverArmEstimator::Result{};
        m_resultEdit->clear();
        m_statusLabel->setText(QStringLiteral("采集中… 请让车辆原地缓慢旋转"));
        m_plot->clearData();
    } else {
        m_result = m_est.solve(currentMethod());
        updateText();
        m_statusLabel->setText(QStringLiteral("已停止采集"));
    }
    m_plotDirty = true;
}

void NavCalibPanel::onReset()
{
    m_est.reset();
    m_result = LeverArmEstimator::Result{};
    m_resultEdit->clear();
    m_statusLabel->setText(m_collecting
        ? QStringLiteral("采集中… 请让车辆原地缓慢旋转")
        : QStringLiteral("点「开始采集」，让车辆原地缓慢转一圈以上"));
    m_plot->clearData();
    m_plotDirty = true;
}

void NavCalibPanel::onRefreshPlot()
{
    if (!m_plotDirty)
        return;
    m_plotDirty = false;
    pushToPlot();
}

void NavCalibPanel::pushToPlot()
{
    // 位置法：叠加补偿后点云（正常应收缩成一个点）。
    // 速度法：解出的杆臂属于速度观测点，与已投影的位置不同参考点，只画运动形状。
    if (m_result.method == LeverArmEstimator::Method::Position && m_result.posValid) {
        m_plot->setData(m_est.rawTrack(),
                        m_est.projectedTrack(m_result.lx, m_result.ly),
                        true);
    } else {
        m_plot->setData(m_est.rawTrack(), QVector<QPointF>(), false);
    }
}

void NavCalibPanel::updateText()
{
    const LeverArmEstimator::Result &r = m_result;

    if (r.totalFrames == 0) {
        m_resultEdit->setPlainText(
            QStringLiteral("等待 0x0166 数据…\n"
                           "若一直无数据：确认设备协议、波特率，以及 CRC 勾选状态。\n"));
        return;
    }

    QStringList lines;
    lines << QStringLiteral("已转 %1 圈（航向跨度 %2°）")
                 .arg(r.circles, 0, 'f', 2)
                 .arg(r.yawSpanDeg, 0, 'f', 0);
    lines << QStringLiteral("采集 %1 s · 固定解 %2 / 总 %3 帧")
                 .arg(r.durationSec, 0, 'f', 1)
                 .arg(r.fixedFrames)
                 .arg(r.totalFrames);
    if (r.rejectedFrames > 0)
        lines << QStringLiteral("3σ 剔除 %1 帧").arg(r.rejectedFrames);
    lines << QStringLiteral("──────────────────────────────");

    if (!r.valid) {
        lines << QStringLiteral("结果：不可用");
        lines << r.reason;
        m_resultEdit->setPlainText(lines.join(QLatin1Char('\n')) + QLatin1Char('\n'));
        return;
    }

    const bool posMode = (r.method == LeverArmEstimator::Method::Position);
    const double lmag  = std::hypot(r.lx, r.ly);

    // 位置法：圆心有意义；速度法：没有圆心
    if (posMode) {
        lines << QStringLiteral("圆心      N = %1 m   E = %2 m")
                     .arg(r.centerN, 0, 'f', 3)
                     .arg(r.centerE, 0, 'f', 3);
        if (r.centerDrift > 0.0)
            lines << QStringLiteral("圆心漂移  各段最大 %1 mm")
                         .arg(r.centerDrift * 1000.0, 0, 'f', 1);
    }

    lines << (posMode
                  ? QStringLiteral("杆臂      lx = %1 m   ly = %2 m   |l| = %3 m")
                  : QStringLiteral("速度杆臂  lx = %1 m   ly = %2 m   |l| = %3 m"))
                 .arg(r.lx, 0, 'f', 4)
                 .arg(r.ly, 0, 'f', 4)
                 .arg(lmag, 0, 'f', 4);
    lines << QStringLiteral("杆臂误差  ±%1 / ±%2 mm（%3 帧）")
                 .arg(r.leverStdLx * 1000.0, 0, 'f', 1)
                 .arg(r.leverStdLy * 1000.0, 0, 'f', 1)
                 .arg(r.usedFrames);
    if (r.leverChecked) {
        lines << QStringLiteral("分段一致性  %1 段最大差 %2 mm%3")
                     .arg(r.segCount)
                     .arg(r.leverSpread * 1000.0, 0, 'f', 1)
                     .arg(r.leverStable ? QString()
                                        : QStringLiteral("（不一致，结果不可信）"));
    }

    if (posMode) {
        lines << QStringLiteral("位置残差 RMS = %1 mm")
                     .arg(r.residualRms * 1000.0, 0, 'f', 1);
        if (r.velValid)
            lines << QStringLiteral("参考（速度法） lx = %1   ly = %2   |l| = %3 m")
                         .arg(r.velLx, 0, 'f', 4)
                         .arg(r.velLy, 0, 'f', 4)
                         .arg(std::hypot(r.velLx, r.velLy), 0, 'f', 4);
    } else {
        if (r.speedGate > 0.0)
            lines << QStringLiteral("只用 |v| ≥ %1 m/s 的帧（平均 %2 m/s）")
                         .arg(r.speedGate, 0, 'f', 2)
                         .arg(r.meanSpeed, 0, 'f', 2);
        else
            lines << QStringLiteral("平均转速 %1 m/s（偏慢，杆臂可能偏小）")
                         .arg(r.meanSpeed, 0, 'f', 2);
        lines << QStringLiteral("速度残差 RMS = %1 mm/s")
                     .arg(r.residualRms * 1000.0, 0, 'f', 1);
        if (r.posValid)
            lines << QStringLiteral("参考（位置法） lx = %1   ly = %2   |l| = %3 m")
                         .arg(r.posLx, 0, 'f', 4)
                         .arg(r.posLy, 0, 'f', 4)
                         .arg(std::hypot(r.posLx, r.posLy), 0, 'f', 4);
    }

    // 位置法解的是残余（0x0166 已按配置投影过），给增量；
    // 速度法是绝对杆臂，直接给应填值（设备约定与解算符号相反，同 MATLAB）。
    const double curLx = m_curLx ? m_curLx->value() : 0.0;
    const double curLy = m_curLy ? m_curLy->value() : 0.0;
    if (posMode) {
        lines << QStringLiteral("增量      Δlx = %1 m   Δly = %2 m（叠加到当前配置）")
                     .arg(-r.lx, 0, 'f', 4)
                     .arg(-r.ly, 0, 'f', 4);
        if (std::fabs(curLx) > 1e-9 || std::fabs(curLy) > 1e-9)
            lines << QStringLiteral("应填投影点  lx = %1 m   ly = %2 m")
                         .arg(curLx - r.lx, 0, 'f', 4)
                         .arg(curLy - r.ly, 0, 'f', 4);
    } else {
        lines << QStringLiteral("应填投影点  lx = %1 m   ly = %2 m")
                     .arg(-r.lx, 0, 'f', 4)
                     .arg(-r.ly, 0, 'f', 4);
    }
    lines << QStringLiteral("质量：%1").arg(qualityText());

    m_resultEdit->setPlainText(lines.join(QLatin1Char('\n')) + QLatin1Char('\n'));
}

QString NavCalibPanel::qualityText() const
{
    const LeverArmEstimator::Result &r = m_result;
    if (!r.valid)
        return r.reason;
    if (r.moving)
        return QStringLiteral("差 —— %1").arg(r.reason);
    if (!r.enoughTurn)
        return QStringLiteral("参考 —— %1").arg(r.reason);
    if (!r.fullTurn)
        return QStringLiteral("良 —— %1").arg(r.reason);
    return r.reason.isEmpty() ? QStringLiteral("优")
                              : QStringLiteral("优（%1）").arg(r.reason);
}
