#include "modules/commtool/realtimeparsepanel.h"

#include "modules/parse/parser/single/protocolregistry.h"
#include "modules/parse/parser/debugstream/debugframeparser.h"
#include "modules/parse/parser/debugstream/eb614debugparser.h"
#include "modules/parse/parser/other/epsonparser.h"
#include "modules/parse/parser/other/ahrsaa5parser.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPair>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

// 显示刷新频率（Hz）——与数据到达率解耦，避免高频帧把显示区刷爆
static constexpr int kRefreshHz = 10;

// 等宽字体下按"显示宽度"对齐：全角/中日韩字符占 2 列
static int displayWidth(const QString &s)
{
    int w = 0;
    for (const QChar &c : s)
        w += (c.unicode() >= 0x1100) ? 2 : 1;
    return w;
}

static QString padTo(const QString &s, int width)
{
    const int pad = width - displayWidth(s);
    return pad > 0 ? s + QString(pad, QLatin1Char(' ')) : s;
}

// ════════════════════════════════════════════════════════════════
//  可实时解析的帧清单
//    · 标准帧协议：直接取协议注册表（含南方激光 / 中海达激光）
//    · 调试数据流：手工列出各帧，因为一条数据流由多个帧 ID 组成
//  凯芯 / RTCM / CAN 实时不可用（分别依赖外部 MEX、非 AA55 帧头、BLF 文件），
//  不列入清单。
// ════════════════════════════════════════════════════════════════
QList<RealtimeParsePanel::FrameEntry> RealtimeParsePanel::streamingFrames()
{
    QList<FrameEntry> list;

    for (const ProtocolDescriptor &d : protocolRegistry()) {
        list.append({d.pid, QStringLiteral("标准帧协议"), d.label});
    }

    const struct { ProtocolID pid; const char *label; } kDbg[] = {
        {ProtocolID::Debug_IMU,   "0x51 组合导航IMU"},
        {ProtocolID::Debug_GPS,   "0x52 GNSS定位"},
        {ProtocolID::Debug_DRTK,  "0x53 双天线RTK"},
        {ProtocolID::Debug_OD,    "0x54 轮速/里程计"},
        {ProtocolID::Debug_KF,    "0x55 卡尔曼滤波"},
        {ProtocolID::Debug_Param, "0x56 固定参数"},
    };
    for (const auto &e : kDbg)
        list.append({e.pid, QStringLiteral("AG调试数据流"), QString::fromUtf8(e.label)});

    const struct { ProtocolID pid; const char *label; } kEb[] = {
        {ProtocolID::Eb614_IMU, "0x0101 IMU原始"},
        {ProtocolID::Eb614_GPS, "0x0111 GNSS定位"},
        {ProtocolID::Eb614_KF,  "0x0131 KF调试量"},
    };
    for (const auto &e : kEb)
        list.append({e.pid, QStringLiteral("614E-B调试数据流"), QString::fromUtf8(e.label)});

    // 非 AA55 帧头的串口协议（切帧格式由 StreamFramer::FramingMode 选择）
    const struct { ProtocolID pid; const char *label; } kSerial[] = {
        {ProtocolID::Epson_Imu,        "爱普生IMU(80 01)"},
        {ProtocolID::Ahrs_Aa5,         "AHRS(AA A5)"},
        {ProtocolID::Novatel_BestPos,  "BESTPOS_诺瓦泰"},
        {ProtocolID::Novatel_PsrVel,   "PSRVEL_诺瓦泰"},
    };
    for (const auto &e : kSerial)
        list.append({e.pid, QStringLiteral("串口实时协议"), QString::fromUtf8(e.label)});

    return list;
}

// ── 字段名 / 字段值（标准帧协议走字段描述符，调试流走 CSV 表头/行） ──

static QStringList csvFields(const QString &csv)
{
    QString s = csv;
    while (s.endsWith(QLatin1Char('\n')) || s.endsWith(QLatin1Char('\r')))
        s.chop(1);
    return s.isEmpty() ? QStringList{} : s.split(QLatin1Char(','));
}

QStringList RealtimeParsePanel::fieldNames(ProtocolID pid)
{
    const QList<FieldDescriptor> fields = fieldsForProtocol(pid);
    if (!fields.isEmpty()) {
        QStringList names;
        names.reserve(fields.size());
        for (const FieldDescriptor &fd : fields)
            names << fd.name;
        return names;
    }
    if (isDebugProtocol(pid))
        return csvFields(DebugFrameParser::csvHeaderFor(pid));
    if (isEb614Protocol(pid))
        return csvFields(Eb614DebugParser::csvHeaderFor(pid));
    if (pid == ProtocolID::Epson_Imu) {
        static EpsonParser parser;
        return csvFields(parser.csvHeader());
    }
    if (pid == ProtocolID::Ahrs_Aa5) {
        static AhrsAa5Parser parser;
        return csvFields(parser.csvHeader());
    }
    return {};
}

QStringList RealtimeParsePanel::fieldValues(ProtocolID pid, const ParsedFrame &f)
{
    const QList<FieldDescriptor> fields = fieldsForProtocol(pid);
    if (!fields.isEmpty()) {
        QStringList vals;
        vals.reserve(fields.size());
        for (const FieldDescriptor &fd : fields)
            vals << fd.extract(f);
        return vals;
    }
    if (isDebugProtocol(pid))
        return csvFields(DebugFrameParser::formatFrameFor(pid, f));
    if (isEb614Protocol(pid))
        return csvFields(Eb614DebugParser::formatFrameFor(pid, f));
    if (pid == ProtocolID::Epson_Imu) {
        static EpsonParser parser;
        return csvFields(parser.formatFrame(f));
    }
    if (pid == ProtocolID::Ahrs_Aa5) {
        static AhrsAa5Parser parser;
        return csvFields(parser.formatFrame(f));
    }
    return {};
}

// ════════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════════
RealtimeParsePanel::RealtimeParsePanel(QWidget *parent)
    : QWidget(parent)
{
    // ── 协议选择 + 操作按钮 ──
    auto *protoLabel = new QLabel(QStringLiteral("解析协议："), this);
    protoLabel->setStyleSheet(QStringLiteral("color:#70849a; font-size:12px;"));

    m_protocolCombo = new QComboBox(this);
    m_protocolCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_protocolCombo->setMinimumContentsLength(10);
    buildProtocolCombo();

    m_startButton = new QPushButton(QStringLiteral("开始解析"), this);
    m_pauseButton = new QPushButton(QStringLiteral("暂停"), this);
    m_pauseButton->setEnabled(false);
    m_clearButton = new QPushButton(QStringLiteral("清空"), this);

    // CRC 校验开关：默认关闭，与离线解析（crcCheckBox 默认不勾）保持一致
    m_crcCheck = new QCheckBox(QStringLiteral("CRC"), this);
    m_crcCheck->setChecked(false);
    m_crcCheck->setToolTip(QStringLiteral(
        "勾选后严格校验 CRC32。默认关闭——离线解析默认也不校验，\n"
        "若开启后帧全被丢弃，说明数据的 CRC 算法与本地实现不一致。"));

    // 逐帧刷新：默认关闭（10Hz 节流，显示稳定）；勾选后每帧到达立即刷新，
    // 用于查看原始数据流节奏（如 100Hz 爱普生数据的时间列连续性）
    m_frameByFrameCheck = new QCheckBox(QStringLiteral("逐帧刷新"), this);
    m_frameByFrameCheck->setChecked(false);
    m_frameByFrameCheck->setToolTip(QStringLiteral(
        "默认按 10Hz 节流刷新显示。勾选后每收到一帧就立即刷新，\n"
        "帧率越高刷新越频繁（100Hz 数据会以 100Hz 刷新界面）。"));
    connect(m_frameByFrameCheck, &QCheckBox::toggled,
            this, &RealtimeParsePanel::frameByFrameChanged);

    // 左栏较窄，分两行排：第一行协议选择，第二行操作按钮
    auto *protoRow = new QHBoxLayout;
    protoRow->setContentsMargins(0, 0, 0, 0);
    protoRow->setSpacing(6);
    protoRow->addWidget(protoLabel);
    protoRow->addWidget(m_protocolCombo, 1);

    auto *btnLayout = new QHBoxLayout;
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(6);
    btnLayout->addWidget(m_startButton);
    btnLayout->addWidget(m_pauseButton);
    btnLayout->addWidget(m_clearButton);
    btnLayout->addWidget(m_crcCheck);
    btnLayout->addWidget(m_frameByFrameCheck);
    btnLayout->addStretch();

    // 状态行：一眼看出"有没有收到帧 / 当前协议有没有命中"
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#617487; font-size:11px;"));
    m_statusLabel->setWordWrap(true);
    updateStatus();

    auto *hint = new QLabel(
        QStringLiteral("解析结果实时刷新在右侧「接收消息」框内（名称 + 数值）"), this);
    hint->setStyleSheet(QStringLiteral("color:#8496a8; font-size:11px;"));
    hint->setWordWrap(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addLayout(protoRow);
    layout->addLayout(btnLayout);
    layout->addWidget(m_statusLabel);
    layout->addWidget(hint);
    layout->addStretch();

    // ── 交互 ──
    connect(m_protocolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        const int data = m_protocolCombo->itemData(idx).toInt();
        if (data < 0)
            return;   // 分类分隔项
        m_pid = static_cast<ProtocolID>(data);
        m_pendingValues.clear();
        m_frameHit = 0;
        m_dirty = true;      // 立刻刷新一次表头
        m_statusDirty = true;
    });

    connect(m_startButton, &QPushButton::clicked, this, [this]() {
        m_parsing = !m_parsing;
        m_startButton->setText(m_parsing ? QStringLiteral("停止解析")
                                         : QStringLiteral("开始解析"));
        m_pauseButton->setEnabled(m_parsing);
        if (!m_parsing) {
            m_paused = false;
            m_pauseButton->setText(QStringLiteral("暂停"));
        }
        if (m_parsing) {
            resetCounters();   // 新一轮采集，重新计数
        } else {
            updateStatus();
            emit displayTextReady(QString());   // 停止时清掉实时显示
        }
        emit parseToggled(m_parsing);
    });

    connect(m_pauseButton, &QPushButton::clicked, this, [this]() {
        m_paused = !m_paused;
        m_pauseButton->setText(m_paused ? QStringLiteral("继续") : QStringLiteral("暂停"));
        m_dirty = true;   // 让表头立刻反映"暂停中"
    });

    connect(m_clearButton, &QPushButton::clicked, this, [this]() { clearValues(); });

    // ── 节流刷新定时器 ──
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(1000 / kRefreshHz);
    connect(m_refreshTimer, &QTimer::timeout, this, &RealtimeParsePanel::flushDisplay);
    m_refreshTimer->start();

    m_pid = currentProtocol();
}

// ════════════════════════════════════════════════════════════════
//  协议下拉框
// ════════════════════════════════════════════════════════════════
void RealtimeParsePanel::buildProtocolCombo()
{
    m_protocolCombo->clear();

    QString lastGroup;
    for (const FrameEntry &fe : streamingFrames()) {
        if (fe.group != lastGroup) {
            m_protocolCombo->addItem(QStringLiteral("—— %1 ——").arg(fe.group), -1);
            lastGroup = fe.group;
        }
        m_protocolCombo->addItem(fe.label, static_cast<int>(fe.pid));
    }

    // 分类分隔项不可选
    if (auto *model = qobject_cast<QStandardItemModel *>(m_protocolCombo->model())) {
        for (int i = 0; i < m_protocolCombo->count(); ++i)
            if (m_protocolCombo->itemData(i).toInt() < 0 && model->item(i))
                model->item(i)->setEnabled(false);
    }

    // 默认选中 AG 调试流的 0x51 主轴
    const int idx = m_protocolCombo->findData(static_cast<int>(ProtocolID::Debug_IMU));
    if (idx >= 0)
        m_protocolCombo->setCurrentIndex(idx);
}

ProtocolID RealtimeParsePanel::currentProtocol() const
{
    const int idx = m_protocolCombo->currentIndex();
    if (idx < 0)
        return ProtocolID::Debug_IMU;
    const int data = m_protocolCombo->itemData(idx).toInt();
    return data < 0 ? ProtocolID::Debug_IMU : static_cast<ProtocolID>(data);
}

bool RealtimeParsePanel::strictCrc() const
{
    return m_crcCheck && m_crcCheck->isChecked();
}

bool RealtimeParsePanel::frameByFrame() const
{
    return m_frameByFrameCheck && m_frameByFrameCheck->isChecked();
}

// ════════════════════════════════════════════════════════════════
//  数据入口 + 节流刷新
// ════════════════════════════════════════════════════════════════
void RealtimeParsePanel::submitFrame(const ParsedFrame &f)
{
    if (!m_parsing)
        return;

    ++m_frameTotal;
    m_seenCount[f.protocol] += 1;
    m_statusDirty = true;

    if (f.protocol != m_pid)
        return;   // 一次只解析一种协议，其它帧只计数

    ++m_frameHit;
    m_pendingValues = fieldValues(m_pid, f);
    m_dirty = true;   // 只标记，不拼文本

    // 逐帧刷新：勾选后不等 10Hz 定时器，收到即拼文本发出（原始数据流节奏）
    // 状态行统计不走这里，仍由定时器低频刷新
    if (m_frameByFrameCheck && m_frameByFrameCheck->isChecked() && !m_paused) {
        emit displayTextReady(buildDisplayText());
        m_dirty = false;
    }
}

void RealtimeParsePanel::setFramerStats(qint64 found, qint64 crcRejected)
{
    if (m_framerFound == found && m_framerCrcRejected == crcRejected)
        return;
    m_framerFound = found;
    m_framerCrcRejected = crcRejected;
    m_statusDirty = true;
}

void RealtimeParsePanel::flushDisplay()
{
    if (m_statusDirty) {
        updateStatus();
        m_statusDirty = false;
    }

    if (!m_parsing || !m_dirty)
        return;

    emit displayTextReady(buildDisplayText());
    m_dirty = false;
}

// 拼"名称 + 值"文本：首行是协议与统计，每行 2 个字段对（利用宽度减少行数）
QString RealtimeParsePanel::buildDisplayText() const
{
    const QStringList names = fieldNames(m_pid);
    if (names.isEmpty())
        return QStringLiteral("该协议没有可显示的字段\n");

    QString out;
    out += QStringLiteral("%1　已解析 %2 帧%3\n")
               .arg(QString::fromUtf8(protocolName(m_pid)))
               .arg(m_frameHit)
               .arg(m_paused ? QStringLiteral("　[已暂停]") : QString());

    // 列宽：按字段名最大显示宽度对齐
    int w = 0;
    for (const QString &n : names)
        w = qMax(w, displayWidth(n));
    w = qMax(w, 10);   // 下限，窄协议也好看

    // 每 行 2 个字段对 → 垂直高度直接砍半，不需要往下拉太多
    const int kColsPerRow = 2;
    const int colSpan = w + 14;   // 每个字段对占的显示宽度

    out += QString(colSpan * kColsPerRow, QLatin1Char('-')) + QLatin1Char('\n');

    const QStringList &vals = m_pendingValues;
    for (int i = 0; i < names.size(); i += kColsPerRow) {
        QString line;
        for (int j = 0; j < kColsPerRow; ++j) {
            const int idx = i + j;
            if (idx >= names.size()) break;
            const QString v = (idx < vals.size()) ? vals.at(idx) : QStringLiteral("--");
            line += padTo(names.at(idx), w) + QStringLiteral("  ") + v;
            if (j + 1 < kColsPerRow)
                line += QString(6, QLatin1Char(' '));   // 两组之间留 6 空格分隔
        }
        out += line + QLatin1Char('\n');
    }
    return out;
}

void RealtimeParsePanel::clearValues()
{
    m_pendingValues.clear();
    m_dirty = false;
    resetCounters();
    emit displayTextReady(QString());
}

// ════════════════════════════════════════════════════════════════
//  状态行（诊断用：有没有收到帧、当前协议有没有命中）
// ════════════════════════════════════════════════════════════════
void RealtimeParsePanel::resetCounters()
{
    m_frameTotal = 0;
    m_frameHit   = 0;
    m_framerFound = 0;
    m_framerCrcRejected = 0;
    m_seenCount.clear();
    // NAV_APPEND 的丢数判定用全局状态存"上一帧 GPS 时间"，
    // 不复位的话上一轮/离线解析留下的 lastMs 会让新一轮首帧
    // 间隔被算成几分钟，累计丢帧数瞬间虚增
    resetNavGapStats();
    m_statusDirty = false;
    updateStatus();
}

void RealtimeParsePanel::updateStatus()
{
    if (!m_statusLabel)
        return;

    if (m_frameTotal == 0) {
        if (m_framerFound > 0) {
            // 帧头找到了但全被丢弃 —— 基本就是 CRC 不过
            m_statusLabel->setText(
                QStringLiteral("已找到 %1 个帧但全部被丢弃（CRC 不通过 %2 个）"
                               "　⚠ 取消勾选「CRC」试试")
                    .arg(m_framerFound).arg(m_framerCrcRejected));
        } else {
            m_statusLabel->setText(
                m_parsing
                    ? QStringLiteral("等待数据… 未在流里找到 AA55 帧头（检查协议/波特率）")
                    : QStringLiteral("选好协议后点「开始解析」"));
        }
        return;
    }

    // 流中帧按出现次数从多到少，最多列 6 种
    QList<QPair<qint64, ProtocolID>> seen;
    seen.reserve(m_seenCount.size());
    for (auto it = m_seenCount.constBegin(); it != m_seenCount.constEnd(); ++it)
        seen.append({it.value(), it.key()});
    std::sort(seen.begin(), seen.end(),
              [](const QPair<qint64, ProtocolID> &a, const QPair<qint64, ProtocolID> &b) {
                  return a.first > b.first;
              });

    QStringList parts;
    for (int i = 0; i < seen.size() && i < 6; ++i)
        parts << QStringLiteral("%1×%2")
                     .arg(QString::fromUtf8(protocolName(seen.at(i).second)))
                     .arg(seen.at(i).first);

    if (m_frameHit > 0) {
        m_statusLabel->setText(
            QStringLiteral("已收帧 %1 · 当前协议命中 %2　流中帧：%3")
                .arg(m_frameTotal).arg(m_frameHit).arg(parts.join(QStringLiteral("  "))));
    } else {
        // 收到了帧，但没有当前协议 —— 提示该切哪个
        m_statusLabel->setText(
            QStringLiteral("已收帧 %1，但流里没有「%2」　流中帧：%3　⚠ 请在左上角切换协议")
                .arg(m_frameTotal)
                .arg(QString::fromUtf8(protocolName(m_pid)))
                .arg(parts.join(QStringLiteral("  "))));
    }
}
