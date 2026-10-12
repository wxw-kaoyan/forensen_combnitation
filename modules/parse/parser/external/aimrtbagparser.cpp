#include "modules/parse/parser/external/aimrtbagparser.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTextStream>
#include <QCoreApplication>

namespace {

// 定位解析脚本：与凯芯脚本同样的多路径查找（部署目录 / 源码目录 / 构建目录）
QString locateScript(const QString &scriptName)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd    = QDir::currentPath();

    QStringList candidates;
    candidates << appDir + QStringLiteral("/scripts/") + scriptName;
    candidates << cwd    + QStringLiteral("/scripts/") + scriptName;
    candidates << appDir + QStringLiteral("/../../scripts/") + scriptName;
    candidates << appDir + QStringLiteral("/../scripts/") + scriptName;
    candidates << appDir + QStringLiteral("/") + scriptName;

    for (const QString &p : candidates) {
        const QString canonical = QDir::cleanPath(p);
        if (QFileInfo::exists(canonical))
            return canonical;
    }
    return {};
}

// header.stamp 为 Unix 纪元纳秒 → "HH:mm:ss.fff"（UTC）
QString formatStamp(qint64 ns)
{
    if (ns <= 0)
        return QStringLiteral("-");
    return QDateTime::fromMSecsSinceEpoch(ns / 1000000, Qt::UTC)
        .toString(QStringLiteral("HH:mm:ss.zzz"));
}

QString formatBytes(qint64 bytes)
{
    if (bytes >= 1024LL * 1024 * 1024)
        return QStringLiteral("%1 GB").arg(bytes / 1024.0 / 1024 / 1024, 0, 'f', 2);
    if (bytes >= 1024 * 1024)
        return QStringLiteral("%1 MB").arg(bytes / 1024.0 / 1024, 0, 'f', 1);
    return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
}

} // namespace

// ════════════════════════════════════════════════════════════════
//  调用外部 Python 脚本导出 CSV 并取回 JSON 摘要
// ════════════════════════════════════════════════════════════════
AimrtResult AimrtBagParser::runPython(const QString &mcapPath, const QString &outDir,
                                      double checkRateHz)
{
    AimrtResult r;

    const QString scriptPath = locateScript(QStringLiteral("aimrt_imu_export.py"));
    if (scriptPath.isEmpty()) {
        r.error = QStringLiteral("找不到解析脚本 aimrt_imu_export.py\n"
                                 "请确认它位于 exe 同级的 scripts/ 目录下。");
        return r;
    }

    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    // 固定脚本输出为 UTF-8：Windows 控制台默认 GBK，中文路径经 stdout 回传会乱码
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    proc.setProcessEnvironment(env);
    proc.start(QStringLiteral("python"),
               QStringList() << scriptPath
                             << mcapPath
                             << QStringLiteral("-o") << outDir
                             << QStringLiteral("--check-rate")
                             << QString::number(checkRateHz, 'f', 1));
    if (!proc.waitForStarted(5000)) {
        r.error = QStringLiteral("无法启动 Python 进程，请确认已安装 Python 并加入 PATH");
        return r;
    }
    if (!proc.waitForFinished(-1)) {   // 百兆包需数十秒，无限等待
        proc.kill();
        r.error = QStringLiteral("解析超时/被终止");
        return r;
    }

    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    const QString errOut = QString::fromUtf8(proc.readAllStandardError());
    const int exitCode = proc.exitCode();

    // 摘要在一对标记之间（JSON）
    const QString beginTag = QStringLiteral("AIMRT_SUMMARY_BEGIN");
    const QString endTag   = QStringLiteral("AIMRT_SUMMARY_END");
    const int begin = output.indexOf(beginTag);
    const int end   = output.indexOf(endTag);
    if (begin < 0 || end <= begin) {
        r.error = QStringLiteral("脚本未返回有效摘要（exit=%1）\n%2")
                      .arg(exitCode).arg(output.right(2000));
        if (!errOut.isEmpty())
            r.error += QStringLiteral("\n[stderr] ") + errOut.right(1000);
        return r;
    }

    int jsonStart = begin + beginTag.length();
    const QByteArray jsonBytes =
        output.mid(jsonStart, end - jsonStart).trimmed().toUtf8();

    QJsonParseError jerr;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &jerr);
    if (jerr.error != QJsonParseError::NoError || !doc.isObject()) {
        r.error = QStringLiteral("解析脚本输出 JSON 失败: ") + jerr.errorString();
        return r;
    }

    const QJsonObject obj = doc.object();
    r.ok         = obj.value(QStringLiteral("ok")).toBool();
    r.fileSize   = static_cast<qint64>(obj.value(QStringLiteral("fileSize")).toDouble());
    r.stem       = obj.value(QStringLiteral("stem")).toString();
    r.outDir     = obj.value(QStringLiteral("outDir")).toString();
    r.checkRate  = obj.value(QStringLiteral("checkRate")).toDouble();
    r.totalRows  = obj.value(QStringLiteral("totalRows")).toInt();
    r.totalMissed = obj.value(QStringLiteral("totalMissed")).toInt();

    for (const QJsonValue &v : obj.value(QStringLiteral("topics")).toArray()) {
        const QJsonObject o = v.toObject();
        AimrtTopicStat st;
        st.name             = o.value(QStringLiteral("name")).toString();
        st.topic            = o.value(QStringLiteral("topic")).toString();
        st.csv              = o.value(QStringLiteral("csv")).toString();
        st.rows             = o.value(QStringLiteral("rows")).toInt();
        st.hz               = o.value(QStringLiteral("hz")).toDouble();
        st.durationS        = o.value(QStringLiteral("durationS")).toDouble();
        st.intervalMedianUs = o.value(QStringLiteral("intervalMedianUs")).toDouble();
        st.intervalMinUs    = o.value(QStringLiteral("intervalMinUs")).toDouble();
        st.intervalMaxUs    = o.value(QStringLiteral("intervalMaxUs")).toDouble();
        st.devMinUs         = o.value(QStringLiteral("devMinUs")).toDouble();
        st.devMaxUs         = o.value(QStringLiteral("devMaxUs")).toDouble();
        st.maxGapUs         = o.value(QStringLiteral("maxGapUs")).toDouble();
        st.maxGapAtNs       = static_cast<qint64>(o.value(QStringLiteral("maxGapAtNs")).toDouble());
        st.missed           = o.value(QStringLiteral("missed")).toInt();
        st.firstStampNs     = static_cast<qint64>(o.value(QStringLiteral("firstStampNs")).toDouble());
        st.lastStampNs      = static_cast<qint64>(o.value(QStringLiteral("lastStampNs")).toDouble());
        r.topics.append(st);
    }

    if (!r.ok)
        r.error = QStringLiteral("脚本返回失败状态");
    return r;
}

// ════════════════════════════════════════════════════════════════
//  统一入口：解析 + 丢包统计摘要
// ════════════════════════════════════════════════════════════════
ParseResult AimrtBagParser::parseToResult(const QByteArray &rawData, const QString &filePath)
{
    Q_UNUSED(rawData);   // 只需文件路径，数据由 Python 脚本直接读取

    ParseResult result;
    result.protocolId = protocolId();

    if (filePath.isEmpty() || !filePath.endsWith(QStringLiteral(".mcap"), Qt::CaseInsensitive)) {
        result.displayText = QStringLiteral("（AimRT 录包解析需要 .mcap 文件，请先加载 mcap 文件）\n");
        result.infoText = QStringLiteral("请先加载 .mcap");
        return result;
    }

    // CSV 落到临时目录：单包两个话题约 60MB，不适合放工程目录
    const QString stem = QFileInfo(filePath).completeBaseName();
    const QString outDir = QDir::tempPath() + QStringLiteral("/aimrt_imu_") + stem;
    QDir().mkpath(outDir);

    const AimrtResult ar = runPython(filePath, outDir);
    if (!ar.ok) {
        result.displayText = QStringLiteral("AimRT 录包解析失败:\n%1").arg(ar.error);
        result.infoText = QStringLiteral("失败");
        return result;
    }

    // ── 摘要文本（不放 CSV 内容，体积太大）──
    QString text;
    QTextStream s(&text);
    s << QStringLiteral("=== AimRT IMU 录包解析（MCAP）===\n");
    s << QStringLiteral("文件：%1　%2\n").arg(QFileInfo(filePath).fileName())
                                            .arg(formatBytes(ar.fileSize));
    s << QStringLiteral("话题：%1 个　合计 %2 条\n").arg(ar.topics.size()).arg(ar.totalRows);
    s << QStringLiteral("CSV 输出：%1\n").arg(ar.outDir);

    s << QStringLiteral("\n── 丢包统计（按 %1 Hz 检查采样连续性）──\n")
             .arg(ar.checkRate, 0, 'f', 0);
    int idx = 1;
    for (const AimrtTopicStat &t : ar.topics) {
        const double lostPct = (t.rows + t.missed) > 0
                                   ? 100.0 * t.missed / (t.rows + t.missed) : 0.0;
        s << QStringLiteral("[%1] %2　%3\n").arg(idx++).arg(t.name, t.topic);
        if (t.rows < 2) {
            s << QStringLiteral("    数据不足（%1 条），无法判断连续性\n").arg(t.rows);
            continue;
        }
        s << QStringLiteral("    条数 %1　实测 %2 Hz　跨度 %3 s\n")
                 .arg(t.rows).arg(t.hz, 0, 'f', 2).arg(t.durationS, 0, 'f', 1);
        s << QStringLiteral("    header 时间 %1 → %2（UTC）\n")
                 .arg(formatStamp(t.firstStampNs), formatStamp(t.lastStampNs));
        s << QStringLiteral("    间隔 中位 %1 ms　最小 %2 ms　最大 %3 ms\n")
                 .arg(t.intervalMedianUs / 1000.0, 0, 'f', 5)
                 .arg(t.intervalMinUs / 1000.0, 0, 'f', 5)
                 .arg(t.intervalMaxUs / 1000.0, 0, 'f', 5);
        s << QStringLiteral("    相对期望间隔偏差 %1 ~ %2 us\n")
                 .arg(t.devMinUs, 0, 'f', 1).arg(t.devMaxUs, 0, 'f', 1);
        s << QStringLiteral("    最大间隔 %1 ms @ %2\n")
                 .arg(t.maxGapUs / 1000.0, 0, 'f', 5).arg(formatStamp(t.maxGapAtNs));
        s << QStringLiteral("    丢帧 %1 帧（%2%）\n")
                 .arg(t.missed).arg(lostPct, 0, 'f', 3);
    }

    // ── 丢包汇总 ──
    s << QStringLiteral("\n── 丢包汇总 ──\n");
    s << QStringLiteral("合计 %1 条　估算丢帧 %2 帧\n").arg(ar.totalRows).arg(ar.totalMissed);

    if (ar.totalMissed == 0) {
        double maxDev = 0.0;
        for (const AimrtTopicStat &t : ar.topics) {
            maxDev = qMax(maxDev, qMax(qAbs(t.devMinUs), qAbs(t.devMaxUs)));
        }
        s << QStringLiteral("结论：未发现丢帧，采样连续（间隔偏差在 ±%1 us 内）\n")
                 .arg(maxDev, 0, 'f', 1);
    } else {
        QStringList bad;
        for (const AimrtTopicStat &t : ar.topics) {
            if (t.missed > 0)
                bad << QStringLiteral("%1(%2帧)").arg(t.name).arg(t.missed);
        }
        s << QStringLiteral("结论：以下话题存在丢帧：%1\n").arg(bad.join(QStringLiteral("、")));
    }
    s.flush();

    result.displayText = text;
    result.fullText = text;
    // 导出用：这里存的是 CSV 输出目录（导出按钮按目录处理）
    result.externalCsvPath = ar.outDir;
    result.infoText = QStringLiteral("MCAP:%1条 %2Hz 丢帧%3")
                          .arg(ar.totalRows)
                          .arg(ar.checkRate, 0, 'f', 0)
                          .arg(ar.totalMissed);
    return result;
}
