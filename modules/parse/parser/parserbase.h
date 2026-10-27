#ifndef PARSERBASE_H
#define PARSERBASE_H

#include "modules/parse/parser/core/protocoldefs.h"
#include <QByteArray>
#include <QList>
#include <QString>

// ════════════════════════════════════════════════════════════════
//  统一解析结果 — 所有解析分支的最终产物
// ════════════════════════════════════════════════════════════════
struct ParseResult {
    QString    displayText;     // 界面显示内容
    QString    fullText;        // 完整文本（导出用）
    QString    infoText;        // 状态栏摘要
    ProtocolID protocolId = ProtocolID::Unknown;
    QList<ParsedFrame> frames;  // 帧类分支缓存（供按帧导出）
    QString    externalCsvPath; // 外部脚本/进程生成的临时 CSV 路径（凯芯等）
    bool       hasCsvHeader = false;  // 输出为 CSV（决定导出默认扩展名 .csv/.txt）
};

// ════════════════════════════════════════════════════════════════
//  解析器基类 — 每个协议一个子类，按钮驱动解析
//  工厂负责创建，多态负责分发；parseToResult 是唯一切入点
// ════════════════════════════════════════════════════════════════
class ParserBase
{
public:
    virtual ~ParserBase() = default;

    // 按钮标签
    virtual QString buttonLabel() const = 0;

    // 协议分类（用于 UI 按类分组下拉框）
    virtual QString category() const { return QStringLiteral("其他"); }

    // 主协议 ID
    virtual ProtocolID protocolId() const = 0;

    // 解析原始二进制数据 → 帧列表
    virtual QList<ParsedFrame> parse(const QByteArray &rawData) = 0;

    // 格式化单帧
    virtual QString formatFrame(const ParsedFrame &f) const = 0;

    // CSV 表头（空 = 非 CSV 协议）
    virtual QString csvHeader() const { return {}; }

    // 统一入口：原始数据 → 结构化结果。
    // 默认实现走「帧解析」路径（标准帧 / CAN / DEBUG 流）。
    // 外部脚本（凯芯）与深解码（RTCM）子类重写此方法。
    virtual ParseResult parseToResult(const QByteArray &rawData,
                                      const QString &filePath);

    // CRC 开关（默认不校验）
    void setSkipCrc(bool skip) { m_skipCrc = skip; }
    bool skipCrc() const { return m_skipCrc; }

    // 协议名
    static QString protocolNameStatic(ProtocolID pid);
    static QString rtkStatusName(uint8_t s);
    static QString formatStatusFlags(uint32_t flags);
    static QString formatAttValid(uint8_t v);

protected:
    bool m_skipCrc = true;  // 默认不校验 CRC
};

#endif // PARSERBASE_H
