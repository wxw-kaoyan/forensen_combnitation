#ifndef PARSERFACTORY_H
#define PARSERFACTORY_H

#include "modules/parse/parser/parserbase.h"
#include "modules/parse/parser/single/frameprotocolparser.h"
#include "modules/parse/parser/other/rtcmframeparser.h"
#include "modules/parse/parser/debugstream/debugframeparser.h"
#include "modules/parse/parser/debugstream/eb614debugparser.h"
#include "modules/parse/parser/external/kaixinframeparser.h"
#include "modules/parse/parser/single/protocolregistry.h"
#include "modules/parse/parser/can/canrtkparser.h"
#include "modules/parse/parser/can/lsnavparser.h"
#include "modules/parse/parser/can/lsimulogparser.h"
#include "modules/parse/parser/can/a3parser.h"
#include "modules/parse/parser/other/novatelparser.h"
#include <QList>

// ════════════════════════════════════════════════════════════════
//  解析器工厂 — 从协议注册表 + 特殊解析器创建全部
// ════════════════════════════════════════════════════════════════
class ParserFactory
{
public:
    static QList<ParserBase*> createAll()
    {
        QList<ParserBase*> result;
        // 标准帧协议 — 从注册表驱动
        for (const auto &desc : protocolRegistry())
            result.append(new FrameProtocolParser(desc));
        // 特殊协议
        result.append(new RtcmFrameParser);
        result.append(new CanRtkParser);
        result.append(new LsNavParser);
        result.append(new LsImuLogParser);
        result.append(new DebugFrameParser);
        result.append(new Eb614DebugParser);
        result.append(new KaixinFrameParser);
        result.append(new NovatelBestPosParser);
        result.append(new NovatelPsrVelParser);
        result.append(new A3Parser);
        return result;
    }
};

#endif // PARSERFACTORY_H
