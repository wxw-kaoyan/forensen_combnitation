#ifndef REALTIMEPARSEPANEL_H
#define REALTIMEPARSEPANEL_H

#include <QWidget>
#include "modules/parse/parser/core/protocoldefs.h"

class QComboBox;
class QPushButton;

// ════════════════════════════════════════════════════════════════
//  实时解析面板 — 解析协议下拉框 + 开始/停止解析按钮
//  解析结果由 CommTool 输出到「接收消息」框，本面板只负责选协议与开关。
// ════════════════════════════════════════════════════════════════
class RealtimeParsePanel : public QWidget
{
    Q_OBJECT
public:
    explicit RealtimeParsePanel(QWidget *parent = nullptr);

    // 当前选中的解析协议
    ProtocolID currentProtocol() const;

    // 单帧 → 一行显示文本（复用注册表 / Debug / Eb614 formatter）
    static QString frameToText(const ParsedFrame &f);

    // 协议 → 一行表头（CSV 协议返回 csvHeader，原生文本协议返回空）
    static QString headerLine(ProtocolID pid);

signals:
    // 开始/停止解析切换
    void parseToggled(bool enabled);

private:
    void setupProtocolList();

    QComboBox   *m_protocolCombo = nullptr;
    QPushButton *m_startButton = nullptr;
    bool         m_parsing = false;
};

#endif // REALTIMEPARSEPANEL_H
