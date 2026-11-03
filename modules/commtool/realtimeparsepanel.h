#ifndef REALTIMEPARSEPANEL_H
#define REALTIMEPARSEPANEL_H

#include <QWidget>
#include <QList>
#include <QMap>
#include <QStringList>

#include "modules/parse/parser/core/protocoldefs.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTimer;

// ════════════════════════════════════════════════════════════════
//  实时解析面板 — 只负责"选协议 + 开关 + 统计"
//
//  解析结果不在这里显示：面板每 100ms 把当前字段值拼成文本，
//  通过 displayTextReady() 交给 CommTool 写进右侧「接收消息」大框，
//  由那个大框原地刷新（名称 + 值），看得清楚。
//
//  · 一次只解析一种协议（下拉框切换）
//  · 刷新默认 10Hz 节流；勾选「逐帧刷新」后每帧到达立即刷新（原始数据流）
//  · 可暂停冻结读数
// ════════════════════════════════════════════════════════════════
class RealtimeParsePanel : public QWidget
{
    Q_OBJECT
public:
    explicit RealtimeParsePanel(QWidget *parent = nullptr);

    bool isParsing() const { return m_parsing; }

    // 是否启用严格 CRC 校验（默认关闭，与离线解析的默认行为一致）
    bool strictCrc() const;

    // 是否逐帧刷新（勾选时每帧到达立即刷新接收区，并需串口侧关闭 30ms 合并）
    bool frameByFrame() const;

    // 当前选中的协议
    ProtocolID currentProtocol() const;

    // 收到一帧：只更新缓存值，不直接改界面
    void submitFrame(const ParsedFrame &f);

    // CommTool 回传帧解析统计，用于状态行诊断
    void setFramerStats(qint64 found, qint64 crcRejected);

    // 帧 → 字段名列表 / 字段值列表（两者列数严格一致）
    static QStringList fieldNames(ProtocolID pid);
    static QStringList fieldValues(ProtocolID pid, const ParsedFrame &f);

signals:
    void parseToggled(bool enabled);

    // 逐帧刷新勾选变化（CommTool 据此让串口侧关闭/恢复 30ms 合并）
    void frameByFrameChanged(bool enabled);

    // 待显示的字段文本（空串 = 清空显示区）
    void displayTextReady(const QString &text);

private:
    // 可实时解析的一帧（含所属分类，用于下拉框分组）
    struct FrameEntry {
        ProtocolID pid;
        QString    group;
        QString    label;
    };
    static QList<FrameEntry> streamingFrames();

    void buildProtocolCombo();
    void flushDisplay();         // 定时器回调：拼文本并发出
    void updateStatus();
    void clearValues();
    void resetCounters();
    QString buildDisplayText() const;

    QComboBox   *m_protocolCombo = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_pauseButton = nullptr;
    QPushButton *m_clearButton = nullptr;
    QCheckBox   *m_crcCheck = nullptr;
    QCheckBox   *m_frameByFrameCheck = nullptr;   // 逐帧刷新（原始数据流）
    QLabel      *m_statusLabel = nullptr;
    QTimer      *m_refreshTimer = nullptr;

    ProtocolID m_pid = ProtocolID::Debug_IMU;   // 当前解析的协议

    bool m_parsing = false;   // 是否在采集
    bool m_paused  = false;   // 是否冻结显示

    QStringList m_pendingValues;   // 最新一帧的字段值
    bool        m_dirty = false;

    // 状态行计数
    qint64 m_frameTotal = 0;             // 解析期间收到的帧总数
    qint64 m_frameHit   = 0;             // 命中当前协议的帧数
    qint64 m_framerFound = 0;            // framer 找到的完整帧数
    qint64 m_framerCrcRejected = 0;      // 被 CRC 丢弃的帧数
    QMap<ProtocolID, qint64> m_seenCount;// 流中各帧计数
    bool m_statusDirty = false;
};

#endif // REALTIMEPARSEPANEL_H
