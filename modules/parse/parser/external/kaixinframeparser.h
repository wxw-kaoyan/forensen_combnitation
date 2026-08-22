#ifndef KAIXINFRAMEPARSER_H
#define KAIXINFRAMEPARSER_H

#include "modules/parse/parser/parserbase.h"

#include <QMap>
#include <QString>

// ════════════════════════════════════════════════════════════════
//  凯芯调试数据流解析器 (0x0109~0x0114) — 与 DEBUG 并列
//  通过外部 Python 脚本调用 decode_kaixin.mexw64，输出一个综合 CSV
// ════════════════════════════════════════════════════════════════
struct KaixinResult {
    bool    ok = false;
    QString error;
    qint64  fileSize = 0;
    int     validRows = 0;
    int     gpsRows = 0;
    int     rtkFixRows = 0;
    int     week = 0;
    int     incrementType = 0;
    int     version = 0;
    int     columns = 0;
    QString csvPath;         // 临时 CSV 路径（脚本输出）
    QMap<QString, int> frameIds;
};

class KaixinFrameParser : public ParserBase
{
public:
    QString buttonLabel() const override { return QStringLiteral("611-AG调试数据流"); }
    ProtocolID protocolId() const override { return ProtocolID::Kaixin_Data; }

    // 空实现（解析不走帧解析路径，而是通过 runPython 直接输出 CSV）
    QList<ParsedFrame> parse(const QByteArray &) override { return {}; }
    QString formatFrame(const ParsedFrame &) const override { return {}; }

    // 外部脚本路径（非帧解析），重写统一入口
    ParseResult parseToResult(const QByteArray &rawData, const QString &filePath) override;

    // 检测文件头是否为凯芯 AA55 0901
    static bool isKaixinFile(const QByteArray &head);

    // 通过 Python 脚本解析文件，返回结果；结果 CSV 在 out.csvPath
    // 在调用线程中同步执行（可能耗时，应在工作线程中调用）
    static KaixinResult runPython(const QString &datPath, const QString &outCsvPath);
};

#endif // KAIXINFRAMEPARSER_H
