#ifndef FRAMEPROTOCOLPARSER_H
#define FRAMEPROTOCOLPARSER_H

#include "modules/parse/parser/parserbase.h"
#include "modules/parse/parser/single/protocolregistry.h"

#include <QString>

// ════════════════════════════════════════════════════════════════
//  通用帧协议解析器 — 通过 ProtocolDescriptor 驱动
//  取代了原来 8 个手写子类
// ════════════════════════════════════════════════════════════════
class FrameProtocolParser : public ParserBase
{
public:
    explicit FrameProtocolParser(const ProtocolDescriptor &desc)
        : m_desc(desc) {}

    QString buttonLabel() const override { return m_desc.label; }
    ProtocolID protocolId() const override { return m_desc.pid; }

    QList<ParsedFrame> parse(const QByteArray &rawData) override
    {
        return m_desc.parseFn(rawData, m_skipCrc);
    }

    QString formatFrame(const ParsedFrame &f) const override
    {
        return m_desc.formatFn(f);
    }

    QString csvHeader() const override { return m_desc.csvHeader; }

    const ProtocolDescriptor &descriptor() const { return m_desc; }

private:
    ProtocolDescriptor m_desc;
};

#endif // FRAMEPROTOCOLPARSER_H
