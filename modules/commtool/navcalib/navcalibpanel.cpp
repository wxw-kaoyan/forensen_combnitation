#include "navcalibpanel.h"
#include "navcalibplotwindow.h"

#include <QCheckBox>
#include <QCloseEvent>
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

NavCalibPanel::NavCalibPanel(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("超达投影估计"));
    resize(980, 660);
    setMinimumSize(820, 560);

    // ── 左侧：操作 + 结果 ──
    auto *hint = new QLabel(
        QStringLiteral("车辆绕固定点原地缓慢旋转一圈以上（越慢越好）。"
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

    // 当前配置（可选）：填了才能算增量
    m_curLx = new QDoubleSpinBox(group);
    m_curLy = new QDoubleSpinBox(group);
    for (QDoubleSpinBox *sp : {m_curLx, m_curLy}) {
        sp->setRange(-20.0, 20.0);
        sp->setDecimals(4);
        sp->setSingleStep(0.001);
        sp->setSuffix(QStringLiteral(" m"));
        sp->setToolTip(QStringLiteral("设备里当前已配置的杆臂值，可留 0；用于计算增量 Δ"));
    }

    auto *cfgRow = new QHBoxLayout;
    cfgRow->setSpacing(6);
    cfgRow->addWidget(new QLabel(QStringLiteral("当前 lx"), group));
    cfgRow->addWidget(m_curLx, 1);
    cfgRow->addWidget(new QLabel(QStringLiteral("ly"), group));
    cfgRow->addWidget(m_curLy, 1);

    vbox->addLayout(btnRow);
    vbox->addLayout(crcRow);
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
            this, [this](double) { updateText(); });
    connect(m_curLy, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { updateText(); });

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

void NavCalibPanel::submitFrame(const ParsedFrame &f)
{
    if (!m_collecting || f.protocol != ProtocolID::FusionResult)
        return;

    m_est.addFusion(f.fusion);
    m_result = m_est.solve();   // 逐帧解算（10Hz 开销可忽略）
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
        m_result = m_est.solve();
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
    m_plot->setData(m_est.rawTrack(),
                    m_est.projectedTrack(m_result.lx, m_result.ly),
                    m_result.valid,
                    m_result.centerN, m_result.centerE,
                    m_result.lx, m_result.ly);
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

    const double curLx = m_curLx ? m_curLx->value() : 0.0;
    const double curLy = m_curLy ? m_curLy->value() : 0.0;
    const double sugLx = -r.lx;   // 与 MATLAB 脚本输出约定一致
    const double sugLy = -r.ly;

    lines << QStringLiteral("拟合      lx = %1 m   ly = %2 m")
                 .arg(r.lx, 0, 'f', 4).arg(r.ly, 0, 'f', 4);
    lines << QStringLiteral("建议填写  lx = %1 m   ly = %2 m")
                 .arg(sugLx, 0, 'f', 4).arg(sugLy, 0, 'f', 4);
    lines << QStringLiteral("当前配置  lx = %1 m   ly = %2 m")
                 .arg(curLx, 0, 'f', 4).arg(curLy, 0, 'f', 4);
    lines << QStringLiteral("增量      Δlx = %1 m  Δly = %2 m")
                 .arg(sugLx - curLx, 0, 'f', 4)
                 .arg(sugLy - curLy, 0, 'f', 4);
    lines << QStringLiteral("圆心      N = %1 m   E = %2 m")
                 .arg(r.centerN, 0, 'f', 3).arg(r.centerE, 0, 'f', 3);
    lines << QStringLiteral("残差 RMS  = %1 mm")
                 .arg(r.residualRms * 1000.0, 0, 'f', 1);
    lines << QStringLiteral("补偿后散布 = %1 mm")
                 .arg(r.projSpreadRms * 1000.0, 0, 'f', 1);
    lines << QStringLiteral("原始散布  N %1 / E %2 mm")
                 .arg(r.srcSpreadN * 1000.0, 0, 'f', 1)
                 .arg(r.srcSpreadE * 1000.0, 0, 'f', 1);
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
