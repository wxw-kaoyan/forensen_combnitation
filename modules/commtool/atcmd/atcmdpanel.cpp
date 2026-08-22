#include "atcmdpanel.h"
#include "ui_atcmdpanel.h"
#include "atparamdialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

// ==================== 指令数据 ====================

QList<AtCmdInfo> AtCmdPanel::commonCmds()
{
    return {
        {QStringLiteral("AT+SETNO"),     QStringLiteral("停止输出数据"),         QString()},
        {QStringLiteral("AT+SETYES"),    QStringLiteral("开启数据流"),           QString()},
        {QStringLiteral("AT+VERSION"),   QStringLiteral("查询版本号"),           QString()},
        {QStringLiteral("AT+CONFIG"),    QStringLiteral("查询当前配置"),         QString()},
        {QStringLiteral("AT+SAVE"),      QStringLiteral("保存参数到 Flash"),     QString()},
        {QStringLiteral("AT+RESET"),     QStringLiteral("重启模块"),             QString()},
        {QStringLiteral("AT+SET_BAUD="), QStringLiteral("设置串口波特率"),       QStringLiteral("例如 115200")},
        {QStringLiteral("AT+OUTRATE="),  QStringLiteral("配置数据输出频率"),     QStringLiteral("例如 50")},
        {QStringLiteral("AT+SET_LPF="),  QStringLiteral("设置低通滤波"),         QStringLiteral("例如 102")},
        {QStringLiteral("AT+PARAM="),    QStringLiteral("修改参数表(位置,值)"),  QStringLiteral("位置,值")},
    };
}

QList<AtCmdInfo> AtCmdPanel::chipCmds()
{
    return {
        // 坐标系 & 姿态
        {QStringLiteral("AT+SET_ORIENT="),       QStringLiteral("设置IMU坐标系"),           QStringLiteral("例如 101")},
        {QStringLiteral("AT+GET_ORIENT"),         QStringLiteral("查询当前坐标系"),          QString()},
        {QStringLiteral("AT+SET_ATT_AXIS="),      QStringLiteral("姿态轴方向(0前右下/1前左上/2右前上)"), QStringLiteral("例如 0")},
        {QStringLiteral("AT+SET_ATT_ORIENTATION="), QStringLiteral("横滚俯仰取反(00/01/10/11)"), QStringLiteral("例如 00")},
        {QStringLiteral("AT+GET_ATT_ORIENTATION"),  QStringLiteral("查询取反状态"),           QString()},
        {QStringLiteral("AT+INSTALL_ANGLE="),       QStringLiteral("配置安装方向(x,y,z)"),    QStringLiteral("例如 180,0,0")},
        // 航向
        {QStringLiteral("AT+SET_HEADING="),           QStringLiteral("航向角重置(1°分辨率)"),     QStringLiteral("例如 90")},
        {QStringLiteral("AT+SET_HEADING_HIGH_LSB="),  QStringLiteral("航向角高精度重置(0.01°)"), QStringLiteral("例如 9000")},
        // 数据流
        {QStringLiteral("AT+SET_STREAM1="), QStringLiteral("串口1数据流模式"),        QStringLiteral("例如 51")},
        {QStringLiteral("AT+GET_STREAM1"),  QStringLiteral("查询串口1数据流"),        QString()},
        // 标定
        {QStringLiteral("AT+INST_CLEAR"),   QStringLiteral("清空标定缓冲区"),         QString()},
        {QStringLiteral("AT+INST_CALIB"),   QStringLiteral("开始标定流程"),           QString()},
        {QStringLiteral("AT+INST_COLLECT"), QStringLiteral("标定采集状态"),           QString()},
        {QStringLiteral("AT+TRIM_CALIB"),   QStringLiteral("姿态校准(非水平安装)"),   QString()},
        {QStringLiteral("AT+MAG_CALIB="),   QStringLiteral("磁标定(1单面/2双面)"),    QStringLiteral("例如 1")},
        // PPS & 杂项
        {QStringLiteral("AT+NOPPS="),             QStringLiteral("PPS接入(0已接入/1未接入)"),  QStringLiteral("例如 1")},
        {QStringLiteral("AT+OUT_GYRO_NOBIAS="),   QStringLiteral("陀螺零偏输出(0扣除/1不扣除)"), QStringLiteral("例如 0")},
        {QStringLiteral("AT+IMUPARAM_RESET"),      QStringLiteral("重置参数表"),                QString()},
    };
}

QList<AtCmdInfo> AtCmdPanel::navCmds()
{
    return {
        // ════════════ 常用 ════════════
        // ── 杆臂 & 安装 ──
        {QStringLiteral("AT+CLUB_VECTOR="),      QStringLiteral("主天线杆臂(x,y,z,米)"),          QStringLiteral("例如 0,0,0")},
        {QStringLiteral("AT+RTK_ANGLE="),         QStringLiteral("RTK双天线安装偏差角(度)"),       QStringLiteral("例如 0")},
        // ── 位置平滑 ──
        {QStringLiteral("AT+POS_SMOOTH="),        QStringLiteral("位置平滑(1开启/0关闭)"),         QStringLiteral("例如 1")},
        // ── 低速模式 ──
        {QStringLiteral("AT+LOW_SPEED_MODE="),    QStringLiteral("低速模式(1开启/0关闭)"),         QStringLiteral("例如 1")},
        // ── 双天线航向约束 ──
        {QStringLiteral("AT+USE_GNSS_HEADING="),  QStringLiteral("双天线航向约束(1开启/0关闭)"),  QStringLiteral("例如 1")},
        // ── GNSS双天线安装角标定 ──
        {QStringLiteral("AT+EST_GNSS_ANGLE"),     QStringLiteral("GNSS双天线安装角标定"),          QString()},
        // ── NMEA协议输出 ──
        {QStringLiteral("AT+GPGGA="),             QStringLiteral("GPGGA输出频率(Hz,0关闭)"),       QStringLiteral("例如 5")},
        {QStringLiteral("AT+GPRMC="),             QStringLiteral("GPRMC输出频率(Hz,0关闭)"),       QStringLiteral("例如 1")},
        {QStringLiteral("AT+GPHDT="),             QStringLiteral("GPHDT航向输出频率(Hz,0关闭)"),   QStringLiteral("例如 1")},
        {QStringLiteral("AT+GPVTG="),             QStringLiteral("GPVTG地面速度输出频率(Hz)"),     QStringLiteral("例如 1")},
        {QStringLiteral("AT+GPZDA="),             QStringLiteral("GPZDA时间日期输出频率(Hz)"),     QStringLiteral("例如 1")},
        {QStringLiteral("AT+GPATT="),             QStringLiteral("GPATT自定义报文输出频率(Hz)"),   QStringLiteral("例如 1")},
        {QStringLiteral("AT+GPYJ="),              QStringLiteral("GPYJ自定义报文输出频率(Hz)"),    QStringLiteral("例如 1")},
        // ── 停止输出 ──
        {QStringLiteral("AT+SETNO"),              QStringLiteral("停止所有NMEA语句输出"),           QString()},

        // ════════════ 不常用 ════════════
        // ── 杆臂（扩展）──
        {QStringLiteral("AT+OD_VECTOR="),         QStringLiteral("后轴中心杆臂(x,y,z,米)"),         QStringLiteral("例如 0,1,0")},
        {QStringLiteral("AT+PROJ_VECTOR="),       QStringLiteral("投影点位置(x,y,z,米)"),           QStringLiteral("例如 0,0,0")},
        {QStringLiteral("AT+LEVER_ARM_ENABLE"),   QStringLiteral("使能自动杆臂测量"),               QString()},
        {QStringLiteral("AT+LEVER_ARM_DISABLE"),  QStringLiteral("关闭自动杆臂测量"),               QString()},
        // ── 车辆模型 ──
        {QStringLiteral("AT+WHEEL_BASE="),        QStringLiteral("车辆轴距(米)"),                    QStringLiteral("例如 2.8")},
        {QStringLiteral("AT+CAR_TYPE="),          QStringLiteral("车辆类型(1正常/-1后轮)"),          QStringLiteral("例如 1")},
        {QStringLiteral("AT+614P="),              QStringLiteral("双天线/前轮IMU配置"),              QStringLiteral("例如 100")},
        {QStringLiteral("AT+WHEEL_RATE_CF="),     QStringLiteral("前轮转角反馈滤波系数"),            QStringLiteral("例如 0.1")},
        {QStringLiteral("AT+WHEEL_RK"),           QStringLiteral("前轮转角R阵调节"),                 QString()},
        // ── 初始化 & 对准 ──
        {QStringLiteral("AT+START_INIT"),         QStringLiteral("滤波器初始化复位"),               QString()},
        {QStringLiteral("AT+ALIGN_VEL="),         QStringLiteral("初始化速度(m/s)"),                 QStringLiteral("例如 5")},
        {QStringLiteral("AT+SINGLE_INIT="),       QStringLiteral("单点初始化(0关/1开)"),             QStringLiteral("例如 1")},
        {QStringLiteral("AT+HEADING_CORRECTION="),QStringLiteral("低速系数(1~10)"),                  QStringLiteral("例如 5")},
        {QStringLiteral("AT+SLOPE="),             QStringLiteral("坡度场景配置"),                    QStringLiteral("例如 0.05")},
        // ── 数据流输出 ──
        {QStringLiteral("AT+SETNAV"),             QStringLiteral("输出组合导航数据"),               QString()},
        {QStringLiteral("AT+SETAG"),              QStringLiteral("输出AG数据流"),                    QString()},
        {QStringLiteral("AT+SETDEBUG"),           QStringLiteral("输出调试数据流"),                  QString()},
        {QStringLiteral("AT+SETAHRS"),            QStringLiteral("输出AHRS数据"),                   QString()},
        {QStringLiteral("AT+KSXT="),              QStringLiteral("KSXT报文频率"),                    QStringLiteral("例如 50")},
        {QStringLiteral("AT+SETE3"),              QStringLiteral("联适协议输出"),                    QString()},
        {QStringLiteral("AT+SETS1"),              QStringLiteral("S1联适协议"),                      QString()},
        // ── CAN ──
        {QStringLiteral("AT+CAN_BAUD="),          QStringLiteral("CAN波特率(250/500)"),              QStringLiteral("例如 500")},
        {QStringLiteral("AT+CAN_ODR="),           QStringLiteral("CAN输出频率"),                     QStringLiteral("例如 50")},
        {QStringLiteral("AT+SET_CAN_REVERSE="),   QStringLiteral("CAN输出配置(0通用/99联适)"),       QStringLiteral("例如 0")},
        // ── GNSS/RTK ──
        {QStringLiteral("AT+GNSS_CARD="),         QStringLiteral("RTK协议(OEM/UNICORE)"),            QStringLiteral("例如 UNICORE")},
        {QStringLiteral("AT+DRTK_USE="),          QStringLiteral("融合双天线数据"),                  QStringLiteral("例如 1")},
        // ── 约束 & 滤波器 ──
        {QStringLiteral("AT+VEL_VAR_CONSTRAIN="), QStringLiteral("速度方差约束"),                    QStringLiteral("例如 0.01")},
        {QStringLiteral("AT+NHC_RATE="),          QStringLiteral("NHC频率"),                         QStringLiteral("例如 20")},
    };
}

// ==================== 构造 ====================

AtCmdPanel::AtCmdPanel(QWidget *parent)
    : QWidget(parent)
    , m_ui(new Ui::AtCmdPanel)
{
    m_ui->setupUi(this);

    // 填充 3 页指令列表
    populateCommands(m_ui->commonScrollContent, commonCmds());
    populateCommands(m_ui->chipScrollContent,   chipCmds());
    populateCommands(m_ui->navScrollContent,    navCmds());
}

AtCmdPanel::~AtCmdPanel()
{
    delete m_ui;
}

// ==================== UI 搭建 ====================

void AtCmdPanel::populateCommands(QWidget *scrollContent,
                                  const QList<AtCmdInfo> &cmds)
{
    auto *layout = new QVBoxLayout(scrollContent);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    for (const auto &cmd : cmds)
        layout->addWidget(createCmdEntry(cmd));
    layout->addStretch();
}

QWidget *AtCmdPanel::createCmdEntry(const AtCmdInfo &cmd)
{
    auto *row = new QWidget(this);
    auto *hLayout = new QHBoxLayout(row);
    hLayout->setContentsMargins(4, 2, 4, 2);
    hLayout->setSpacing(4);

    // 指令名 — 扁平按钮，看起来像文字但可点击发送
    const QString cmdBtnStyle = QStringLiteral(
        "QPushButton { text-align: left; font-family: 'Consolas','Courier New',monospace; "
        "font-size: 12px; color: #12263f; font-weight: 600; border: none; background: transparent; "
        "padding: 2px 4px; }"
        "QPushButton:hover { color: #1a5276; text-decoration: underline; }");
    auto *cmdBtn = new QPushButton(cmd.command, row);
    cmdBtn->setFlat(true);
    cmdBtn->setCursor(Qt::PointingHandCursor);
    cmdBtn->setMinimumWidth(180);
    cmdBtn->setStyleSheet(cmdBtnStyle);
    hLayout->addWidget(cmdBtn);

    // 功能说明
    auto *descLabel = new QLabel(cmd.description, row);
    descLabel->setStyleSheet(QStringLiteral("color: #617487; font-size: 11px;"));
    hLayout->addWidget(descLabel, 1);

    // 推送箭头 → 视觉提示可发送
    auto *arrowLabel = new QLabel(QStringLiteral("\u279C"), row);
    arrowLabel->setStyleSheet(QStringLiteral("color: #17324d; font-size: 14px;"));
    hLayout->addWidget(arrowLabel);

    // 点击触发发送，有参指令走弹窗
    AtCmdInfo cmdCopy = cmd;
    connect(cmdBtn, &QPushButton::clicked, this, [this, cmdCopy]() {
        onSendClicked(cmdCopy, nullptr);
    });

    return row;
}

// ==================== 发送逻辑 ====================

void AtCmdPanel::onSendClicked(const AtCmdInfo &cmd, QLineEdit *paramEdit)
{
    // 1) 有参指令 → 弹窗输入或从输入框读取
    QString paramValue;
    if (!cmd.paramHint.isEmpty()) {
        if (paramEdit && !paramEdit->text().trimmed().isEmpty()) {
            paramValue = paramEdit->text().trimmed();
        } else {
            AtParamDialog dlg(cmd.command, cmd.description, cmd.paramHint, this);
            if (dlg.exec() != QDialog::Accepted)
                return;
            paramValue = dlg.paramValue();
            if (paramValue.isEmpty()) return;
            // 回填到输入框
            if (paramEdit) paramEdit->setText(paramValue);
        }
    }

    // 2) 拼完整指令
    QString fullCmd = cmd.command;
    if (!cmd.command.endsWith(QLatin1Char('=')) && !paramValue.isEmpty())
        fullCmd += QLatin1Char('=');
    fullCmd += paramValue;
    fullCmd += QStringLiteral("\r\n");
    QByteArray cmdData = fullCmd.toUtf8();

    // 3) 确认弹窗
    QString confirmText = QStringLiteral("即将发送:\n\n  %1").arg(fullCmd.trimmed());
    auto result = QMessageBox::question(
        this, QStringLiteral("确认发送"), confirmText,
        QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
    if (result != QMessageBox::Ok)
        return;

    // 4) 直接发送
    emit sendData(cmdData);
    emit commandSent(QString::fromUtf8(cmdData).trimmed());
    emit statusMessage(QStringLiteral("[AT] 已发送: %1").arg(fullCmd.trimmed()));
}


