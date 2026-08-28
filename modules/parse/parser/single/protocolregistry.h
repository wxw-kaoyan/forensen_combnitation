#ifndef PROTOCOLREGISTRY_H
#define PROTOCOLREGISTRY_H

#include "modules/parse/parser/core/protocoldefs.h"
#include <QByteArray>
#include <QList>
#include <QString>
#include <functional>

// ════════════════════════════════════════════════════════════════
//  字段描述符 — 协议字段（显示名 + 格式化提取）
// ════════════════════════════════════════════════════════════════
struct FieldDescriptor {
    QString name;       // 显示名，如 "GPS周"
    // 从 ParsedFrame 中提取该字段的格式化字符串
    std::function<QString(const ParsedFrame &)> extract;
};

// ════════════════════════════════════════════════════════════════
//  协议描述符 — 一个协议的所有元信息
// ════════════════════════════════════════════════════════════════
struct ProtocolDescriptor {
    QString     label;      // 按钮标签
    ProtocolID  pid;        // 协议 ID
    bool        hasCrc = false;

    // CSV 表头（空字符串 = 原生文本格式，非 CSV）
    QString csvHeader;

    // 解析函数: (原始数据, skipCrc) → 帧列表
    std::function<QList<ParsedFrame>(const QByteArray &, bool)> parseFn;

    // 格式化单帧（CSV 或 原生文本）
    std::function<QString(const ParsedFrame &)> formatFn;

    // 协议字段列表
    QList<FieldDescriptor> fields;
};

// 获取所有标准帧协议描述符
const QList<ProtocolDescriptor> &protocolRegistry();

// 根据 ProtocolID 查找描述符，找不到返回 nullptr
const ProtocolDescriptor *findProtocol(ProtocolID pid);

// 获取指定协议的所有字段（含 CAN-RTK 等特殊协议）
QList<FieldDescriptor> fieldsForProtocol(ProtocolID pid);

#endif // PROTOCOLREGISTRY_H
