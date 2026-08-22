#include "modules/parse/parser/external/kaixinframeparser.h"

#include <QTextStream>
#include <QStringList>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QTemporaryFile>

// ════════════════════════════════════════════════════════════════
//  KaixinFrameParser — 凯芯数据 (0x0109~0x0114)
//  通过外部 Python 脚本调用 decode_kaixin.mexw64
// ════════════════════════════════════════════════════════════════

bool KaixinFrameParser::isKaixinFile(const QByteArray &head)
{
    // 凯芯文件可能有几十字节的文件头，然后是 AA 55 09 01 ...
    // 在前 512 字节内搜索帧头 AA55 + 小端ID=0x0109
    int searchLen = qMin(head.size(), 512);
    for (int i = 0; i <= searchLen - 6; ++i) {
        if (static_cast<unsigned char>(head[i])     == 0xAA &&
            static_cast<unsigned char>(head[i + 1]) == 0x55 &&
            static_cast<unsigned char>(head[i + 2]) == 0x09 &&
            static_cast<unsigned char>(head[i + 3]) == 0x01) {
            return true;
        }
    }
    return false;
}

KaixinResult KaixinFrameParser::runPython(const QString &datPath, const QString &outCsvPath)
{
    KaixinResult r;

    // 定位脚本路径，按优先级依次尝试：
    //   1. <exe_dir>/scripts/kaixin_parse.py         （部署/构建后拷贝）
    //   2. <cwd>/scripts/kaixin_parse.py             （从源码目录启动）
    //   3. <exe_dir>/../../scripts/kaixin_parse.py   （Qt Creator 构建: build/.../debug/../../）
    //   4. <exe_dir>/../scripts/kaixin_parse.py      （一层目录打包）
    //   5. 源码树绝对路径（开发兜底）
    QString appDir = QCoreApplication::applicationDirPath();
    QString cwd    = QDir::currentPath();
    const QString scriptName = QStringLiteral("kaixin_parse.py");

    QStringList candidates;
    candidates << appDir + QStringLiteral("/scripts/") + scriptName;
    candidates << cwd    + QStringLiteral("/scripts/") + scriptName;
    candidates << appDir + QStringLiteral("/../../scripts/") + scriptName;
    candidates << appDir + QStringLiteral("/../scripts/") + scriptName;
    candidates << appDir + QStringLiteral("/") + scriptName;       // 脚本直接放在 exe 旁

    QString scriptPath;
    for (const QString &p : candidates) {
        QString canonical = QDir::cleanPath(p);
        if (QFileInfo::exists(canonical)) {
            scriptPath = canonical;
            break;
        }
    }

    if (scriptPath.isEmpty()) {
        r.error = QStringLiteral("找不到解析脚本 kaixin_parse.py\n"
                                 "已搜索路径:\n  %1")
                      .arg(candidates.join("\n  "));
        return r;
    }

    // Python 解释器：优先系统 PATH 的 python
    QString pythonExe = QStringLiteral("python");

    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(pythonExe, QStringList() << scriptPath << datPath << outCsvPath);

    if (!proc.waitForStarted(5000)) {
        r.error = QStringLiteral("无法启动 Python 进程，请确认已安装 Python 并加入 PATH");
        return r;
    }
    if (!proc.waitForFinished(-1)) {  // 大文件可能需要较长时间，无限等待
        proc.kill();
        r.error = QStringLiteral("Python 解析超时/被终止");
        return r;
    }

    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    QString errOut = QString::fromUtf8(proc.readAllStandardError());
    int exitCode = proc.exitCode();

    // 解析 JSON 摘要（标记之间的部分）
    int begin = output.indexOf(QStringLiteral("KAIXIN_SUMMARY_BEGIN"));
    int end   = output.indexOf(QStringLiteral("KAIXIN_SUMMARY_END"));
    if (begin >= 0 && end > begin) {
        int jsonStart = begin + int(QStringLiteral("KAIXIN_SUMMARY_BEGIN").length());
        // 跳过换行
        while (jsonStart < output.length() &&
               (output[jsonStart] == '\n' || output[jsonStart] == '\r'))
            ++jsonStart;
        QByteArray jsonBytes = output.mid(jsonStart, end - jsonStart).trimmed().toUtf8();
        QJsonParseError jerr;
        QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &jerr);
        if (jerr.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            r.ok            = obj.value(QStringLiteral("ok")).toBool();
            r.fileSize      = static_cast<qint64>(obj.value(QStringLiteral("fileSize")).toDouble());
            r.validRows     = obj.value(QStringLiteral("validRows")).toInt();
            r.gpsRows       = obj.value(QStringLiteral("gpsRows")).toInt();
            r.rtkFixRows    = obj.value(QStringLiteral("rtkFixRows")).toInt();
            r.week          = obj.value(QStringLiteral("week")).toInt();
            r.incrementType = obj.value(QStringLiteral("incrementType")).toInt();
            r.version       = obj.value(QStringLiteral("version")).toInt();
            r.columns       = obj.value(QStringLiteral("columns")).toInt();
            r.csvPath       = outCsvPath;
            QJsonObject fidObj = obj.value(QStringLiteral("frameIds")).toObject();
            for (auto it = fidObj.begin(); it != fidObj.end(); ++it) {
                r.frameIds.insert(it.key(), it.value().toInt());
            }
        } else {
            r.ok = false;
            r.error = QStringLiteral("解析脚本输出 JSON 失败: ") + jerr.errorString();
        }
    } else {
        r.ok = false;
        r.error = QStringLiteral("脚本未返回有效摘要（exit=%1）\n%2")
                      .arg(exitCode).arg(output.left(2000));
        if (!errOut.isEmpty())
            r.error += QStringLiteral("\n[stderr] ") + errOut.left(1000);
    }

    return r;
}

ParseResult KaixinFrameParser::parseToResult(const QByteArray &rawData, const QString &filePath)
{
    ParseResult result;
    result.protocolId = protocolId();

    if (filePath.isEmpty()) {
        result.displayText = QStringLiteral("（凯芯数据需要从文件加载，请先加载 .dat 文件）\n");
        result.infoText = QStringLiteral("错误");
        return result;
    }

    // 快速校验文件头（可能有文件头，检查前 512 字节内是否有 AA55 0901）
    int headLen = qMin((int)rawData.size(), 512);
    if (headLen < 6 || !isKaixinFile(rawData.left(headLen))) {
        result.displayText = QStringLiteral("（未找到凯芯帧头 AA55 0901，可能不是凯芯数据）\n");
        result.infoText = QStringLiteral("格式不匹配");
        return result;
    }

    // 临时输出 CSV（唯一文件名）
    QTemporaryFile tmpFile(QDir::tempPath() + QStringLiteral("/kaixin_XXXXXX.csv"));
    tmpFile.setAutoRemove(false);  // 保留以便导出和显示
    if (!tmpFile.open()) {
        result.displayText = QStringLiteral("无法创建临时 CSV 文件");
        result.infoText = QStringLiteral("错误");
        return result;
    }
    QString tmpCsv = tmpFile.fileName();
    tmpFile.close();

    KaixinResult kr = runPython(filePath, tmpCsv);
    if (!kr.ok) {
        result.displayText = QStringLiteral("凯芯解析失败:\n%1").arg(kr.error);
        result.infoText = QStringLiteral("失败");
        return result;
    }

    // 读取 CSV 内容
    QFile csv(tmpCsv);
    QString csvText;
    if (csv.open(QIODevice::ReadOnly | QIODevice::Text)) {
        csvText = QString::fromUtf8(csv.readAll());
        csv.close();
    }

    result.fullText = csvText;
    result.externalCsvPath = tmpCsv;

    // 组装显示文本：摘要 + 数据预览（截取前后部分）
    QString summary;
    QTextStream ss(&summary);
    ss << QStringLiteral("=== 凯芯数据解析结果 ===\n");
    ss << QStringLiteral("文件大小: %1 KB\n").arg(kr.fileSize / 1024.0, 0, 'f', 1);
    ss << QStringLiteral("有效行数: %1  (200Hz, 约 %2 分钟)\n")
          .arg(kr.validRows).arg(kr.validRows / 200 / 60.0, 0, 'f', 1);
    ss << QStringLiteral("GPS定位行: %1  (10Hz)\n").arg(kr.gpsRows);
    ss << QStringLiteral("固定解行: %1\n").arg(kr.rtkFixRows);
    ss << QStringLiteral("GPS周: %1\n").arg(kr.week);
    ss << QStringLiteral("增量类型: %1\n").arg(kr.incrementType);
    ss << QStringLiteral("版本号: %1 (0x%2)\n").arg(kr.version).arg(kr.version, 0, 16);
    ss << QStringLiteral("列数: %1\n\n").arg(kr.columns);
    ss << QStringLiteral("帧ID统计:\n");
    for (auto it = kr.frameIds.constBegin(); it != kr.frameIds.constEnd(); ++it) {
        ss << QStringLiteral("  %1 : %2 帧\n").arg(it.key()).arg(it.value());
    }
    ss << QStringLiteral("\n=== 数据预览（前 100 行 + 后 20 行） ===\n");

    QStringList lines = csvText.split('\n');
    const int dataStart = 1;  // 第0行是表头
    int dataLines = lines.size() > dataStart ? lines.size() - dataStart - 1 : 0;
    // 去掉末尾空行
    while (dataLines > 0 && lines[dataStart + dataLines - 1].trimmed().isEmpty())
        --dataLines;

    if (lines.size() > 0) {
        ss << lines[0] << '\n';  // 表头
        int previewHead = qMin(100, dataLines);
        for (int i = 0; i < previewHead; ++i)
            ss << lines[dataStart + i] << '\n';
        if (dataLines > 120) {
            ss << QStringLiteral("\n  ... (省略 %1 行) ...\n\n").arg(dataLines - 120);
            int tailStart = dataStart + dataLines - 20;
            for (int i = tailStart; i < dataStart + dataLines; ++i)
                ss << lines[i] << '\n';
        }
    }
    result.displayText = summary;

    result.infoText = QStringLiteral("凯芯:%1行(GPS:%2,固定:%3)")
                          .arg(kr.validRows).arg(kr.gpsRows).arg(kr.rtkFixRows);
    return result;
}
