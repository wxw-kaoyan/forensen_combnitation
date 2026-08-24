#include "modules/commtool/realtimeparsepanel.h"

#include "modules/parse/parser/single/protocolregistry.h"
#include "modules/parse/parser/debugstream/debugframeparser.h"
#include "modules/parse/parser/debugstream/eb614debugparser.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStringList>

// ════════════════════════════════════════════════════════════════
//  协议下拉框内容：StreamFramer 能解析出的全部 AA55 协议
// ════════════════════════════════════════════════════════════════
static QList<ProtocolID> streamingProtocols()
{
    return {
        ProtocolID::AHRS_Basic, ProtocolID::AHRS_Extended, ProtocolID::Quaternion,
        ProtocolID::AG_Data, ProtocolID::FusionResult, ProtocolID::TiltMeasure,
        ProtocolID::Laser, ProtocolID::Attitude, ProtocolID::Quat614P,
        ProtocolID::Debug_IMU, ProtocolID::Debug_GPS, ProtocolID::Debug_DRTK,
        ProtocolID::Debug_OD, ProtocolID::Debug_KF, ProtocolID::Debug_Param,
        ProtocolID::Eb614_IMU, ProtocolID::Eb614_GPS, ProtocolID::Eb614_KF,
    };
}

RealtimeParsePanel::RealtimeParsePanel(QWidget *parent)
    : QWidget(parent)
{
    auto *label = new QLabel(QStringLiteral("解析协议："), this);
    label->setStyleSheet(QStringLiteral("color: #70849a; font-size: 12px;"));

    m_protocolCombo = new QComboBox(this);
    m_protocolCombo->setMinimumWidth(220);
    setupProtocolList();

    m_startButton = new QPushButton(QStringLiteral("开始解析"), this);

    auto *protocolLayout = new QHBoxLayout;
    protocolLayout->setContentsMargins(0, 0, 0, 0);
    protocolLayout->addWidget(label);
    protocolLayout->addWidget(m_protocolCombo);
    protocolLayout->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(protocolLayout);
    layout->addWidget(m_startButton);
    layout->addStretch();

    connect(m_startButton, &QPushButton::clicked, this, [this]() {
        m_parsing = !m_parsing;
        m_startButton->setText(m_parsing ? QStringLiteral("停止解析")
                                         : QStringLiteral("开始解析"));
        m_protocolCombo->setEnabled(!m_parsing); // 解析中锁定协议，保证表头与列对齐
        emit parseToggled(m_parsing);
    });
}

void RealtimeParsePanel::setupProtocolList()
{
    for (ProtocolID pid : streamingProtocols())
        m_protocolCombo->addItem(QString::fromUtf8(protocolName(pid)),
                                 static_cast<int>(pid));

    // 初始选中 Debug_IMU（原 DEBUG 解析主轴）
    const int idx = m_protocolCombo->findData(static_cast<int>(ProtocolID::Debug_IMU));
    if (idx >= 0)
        m_protocolCombo->setCurrentIndex(idx);
}

ProtocolID RealtimeParsePanel::currentProtocol() const
{
    const int idx = m_protocolCombo->currentIndex();
    if (idx < 0)
        return ProtocolID::Debug_IMU;
    return static_cast<ProtocolID>(m_protocolCombo->itemData(idx).toInt());
}

QString RealtimeParsePanel::frameToText(const ParsedFrame &f)
{
    const ProtocolID pid = f.protocol;

    // 标准帧协议：用注册表字段列表（extract 已做单位换算），输出纯数据 CSV 行
    const QList<FieldDescriptor> fields = fieldsForProtocol(pid);
    if (!fields.isEmpty()) {
        QStringList vals;
        for (const auto &fd : fields)
            vals << fd.extract(f);
        return vals.join(QLatin1Char(',')) + QLatin1Char('\n');
    }
    // DEBUG 流 / 614E-B 流
    if (isDebugProtocol(pid))
        return DebugFrameParser::formatFrameFor(pid, f);
    if (isEb614Protocol(pid))
        return Eb614DebugParser::formatFrameFor(pid, f);
    return {};
}

QString RealtimeParsePanel::headerLine(ProtocolID pid)
{
    // 标准帧协议：用字段名生成表头（与 frameToText 列严格对齐）
    const QList<FieldDescriptor> fields = fieldsForProtocol(pid);
    if (!fields.isEmpty()) {
        QStringList names;
        for (const auto &fd : fields)
            names << fd.name;
        return names.join(QLatin1Char(',')) + QLatin1Char('\n');
    }
    // DEBUG 流 / 614E-B 流
    if (isDebugProtocol(pid))
        return DebugFrameParser::csvHeaderFor(pid); // 已带 \n
    if (isEb614Protocol(pid))
        return Eb614DebugParser::csvHeaderFor(pid); // 已带 \n
    return {};
}
