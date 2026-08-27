#include "rinexreader.h"

#include "common/rtklib/rtklib.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>

namespace Analysis {

QString RinexSummary::sysName(int mask)
{
    switch (mask) {
        case Sys::GPS: return QStringLiteral("GPS");
        case Sys::GLO: return QStringLiteral("GLONASS");
        case Sys::GAL: return QStringLiteral("Galileo");
        case Sys::QZS: return QStringLiteral("QZSS");
        case Sys::BDS: return QStringLiteral("BeiDou");
        default:       return QStringLiteral("?");
    }
}

QString RinexSummary::sysLetter(int mask)
{
    switch (mask) {
        case Sys::GPS: return QStringLiteral("G");
        case Sys::GLO: return QStringLiteral("R");
        case Sys::GAL: return QStringLiteral("E");
        case Sys::QZS: return QStringLiteral("J");
        case Sys::BDS: return QStringLiteral("C");
        default:       return QStringLiteral("?");
    }
}

// RINEX 3 信噪比 SSI(1~9) → dBHz 经验映射（RINEX 3.03+ 部分接收机输出 dBHz，多数仍为 SSI）
static const double SSI_TO_DBHZ[10] = {0.0, 12.0, 17.0, 22.0, 26.0, 30.0, 34.0, 38.0, 42.0, 46.0};

bool RinexReader::analyze(const QString &file, RinexSummary *out, QString *err)
{
    if (!QFileInfo::exists(file)) {
        if (err) *err = QStringLiteral("文件不存在: %1").arg(file);
        return false;
    }

    // 初始化观测结构（readrnx 要求先初始化）
    obs_t obs;
    obs.n = obs.nmax = 0;
    obs.data = nullptr;

    // RTKLIB 的 expath(WIN32) 只识别反斜杠 '\'，而 Qt 路径是正斜杠 '/'，
    // 必须转成原生分隔符，否则目录部分丢失、readrnx 打不开文件返回 0。
    const QByteArray path = QDir::toNativeSeparators(file).toLocal8Bit();

    // 检测 RINEX 版本：v3 信噪比为 SSI(1~9)，v2 为 dBHz（单位换算不同）
    bool isRinex3 = false;
    {
        QFile vf(file);
        if (vf.open(QIODevice::ReadOnly)) {
            isRinex3 = vf.readLine().left(9).trimmed().toDouble() > 2.99;
            vf.close();
        }
    }

   // qDebug().noquote() << "[RinexReader] analyze file =" << file;
   //qDebug().noquote() << "[RinexReader] toLocal8Bit hex =" << QString::fromLatin1(path.toHex());

    // 诊断：readrnx 的文件读取入口
    // 1) expath —— readrnxt() 里第一道关卡，通配符展开失败会返回 0
    {
        char *paths[MAXEXFILE] = {nullptr};
        for (int i = 0; i < MAXEXFILE; ++i) paths[i] = (char *)malloc(1024);
        const int n = expath(path.constData(), paths, MAXEXFILE);
        //qDebug().noquote() << "[RinexReader] expath n =" << n;
        // if (n > 0) qDebug().noquote() << "[RinexReader] expath[0] =" << paths[0];
        for (int i = 0; i < MAXEXFILE; ++i) free(paths[i]);
    }
    // 2) fopen —— readrnxfile() 里打开文件的关卡
    {
        FILE *dbgFp = fopen(path.constData(), "rb");
       //  qDebug().noquote() << "[RinexReader] fopen(rb) =" << (dbgFp ? "OK" : "FAIL");
        if (dbgFp) {
            char dbgLine[256] = {0};
            if (fgets(dbgLine, sizeof(dbgLine), dbgFp))
               // qDebug().noquote() << "[RinexReader] first line =" << dbgLine;
            fclose(dbgFp);
        }
    }

    const int stat = readrnx(path.constData(), 0, "", &obs, nullptr, nullptr);
   // qDebug().noquote() << "[RinexReader] readrnx stat =" << stat << " obs.n =" << obs.n;

    if (stat <= 0) {
        if (err) *err = QStringLiteral("RINEX 解析失败（不是观测文件或格式不支持）");
        return false;
    }
    if (obs.n <= 0) {
        freeobs(&obs);
        if (err) *err = QStringLiteral("文件中没有观测数据");
        return false;
    }

    // 按时间 + 卫星排序
    sortobs(&obs);

    RinexSummary s;
    s.totalObsRecords = obs.n;

    // ── 第一遍：历元 / 卫星 / SNR 统计 ──
    // 记录每个历元时间（去重），以及每颗卫星首末出现时间
    struct SatSpan { double first; double last; int epochs; };
    QMap<int, QMap<int, SatSpan>> satSpan;   // sys -> prn -> span

    std::vector<double> epochTimes;           // 去重后的历元时间（GPS 周秒）
    epochTimes.reserve(obs.n);

    // 每历元可见卫星数（按系统）
    int epochGps = 0, epochGlo = 0, epochGal = 0, epochQzs = 0, epochBds = 0;
    QSet<int> epochGpsSet, epochGloSet, epochGalSet, epochQzsSet, epochBdsSet;

    double prevEpoch = -1.0;
    int week = 0;

    // 每历元 SNR 均值累计（跨所有卫星所有频点）
    double curSnrSum = 0;
    long   curSnrCount = 0;

    for (int i = 0; i < obs.n; ++i) {
        const obsd_t &d = obs.data[i];

        int prn = 0;
        const int sys = satsys(d.sat, &prn);

        const double t = time2gpst(d.time, &week);

        // 历元去重：仅在遇到新历元时记录历元时间，并在新历元开始时保存上一历元的快照
        const bool newEpoch = (i == 0 || std::abs(t - prevEpoch) > 1e-6);
        if (newEpoch) {
            if (i > 0) {
                EpochStat es;
                es.time     = prevEpoch;
                es.gps      = epochGpsSet.count();
                es.glo      = epochGloSet.count();
                es.gal      = epochGalSet.count();
                es.qzs      = epochQzsSet.count();
                es.bds      = epochBdsSet.count();
                es.total    = es.gps + es.glo + es.gal + es.qzs + es.bds;
                s.epochStats.append(es);
                // 上一历元平均 SNR
                s.snrEpochMean.append({prevEpoch, curSnrCount > 0 ? curSnrSum / curSnrCount : 0.0});
                // 重置
                epochGpsSet.clear(); epochGloSet.clear(); epochGalSet.clear();
                epochQzsSet.clear(); epochBdsSet.clear();
                curSnrSum = 0; curSnrCount = 0;
            }
            epochTimes.push_back(t);
        }
        prevEpoch = t;

        s.systemMask |= sys;
        s.satsPerSys[sys].insert(prn);

        // 当前历元卫星计数
        if (sys == Sys::GPS)     epochGpsSet.insert(prn);
        else if (sys == Sys::GLO) epochGloSet.insert(prn);
        else if (sys == Sys::GAL) epochGalSet.insert(prn);
        else if (sys == Sys::QZS) epochQzsSet.insert(prn);
        else if (sys == Sys::BDS) epochBdsSet.insert(prn);

        // 卫星出现区间
        SatSpan &span = satSpan[sys][prn];
        if (span.epochs == 0) { span.first = span.last = t; }
        else {
            span.first = std::min(span.first, t);
            span.last  = std::max(span.last,  t);
        }
        span.epochs++;

        // SNR 统计（跳过无效值 0）
        for (int f = 0; f < NFREQ + NEXOBS; ++f) {
            if (d.SNR[f] == 0) continue;
            int freq = 0;
            char *codeStr = code2obs(d.code[f], &freq);
            if (!codeStr || !*codeStr) continue;

            const QString code = QString::fromLatin1(codeStr);
            SnrStat &st = s.snrPerSysCode[sys][code];
            const unsigned char snrRaw = d.SNR[f];
            const double snrDb = isRinex3
                ? SSI_TO_DBHZ[snrRaw > 9 ? 9 : snrRaw]   // RINEX3 SSI(1~9) → dBHz
                : snrRaw * 0.25;                         // RINEX2 0.25dBHz → dBHz
            if (st.count == 0) { st.min = st.max = snrDb; }
            else {
                st.min = std::min(st.min, snrDb);
                st.max = std::max(st.max, snrDb);
            }
            st.sum += snrDb;
            st.count++;

            // 每历元平均 SNR 累计
            curSnrSum += snrDb;
            curSnrCount++;

            // SNR 时间序列
            s.snrTimePerSysCode[sys][code].append({t, snrDb});
        }
    }

    // 保存最后一个历元快照
    {
        EpochStat es;
        es.time     = prevEpoch;
        es.gps      = epochGpsSet.count();
        es.glo      = epochGloSet.count();
        es.gal      = epochGalSet.count();
        es.qzs      = epochQzsSet.count();
        es.bds      = epochBdsSet.count();
        es.total    = es.gps + es.glo + es.gal + es.qzs + es.bds;
        s.epochStats.append(es);
        s.snrEpochMean.append({prevEpoch, curSnrCount > 0 ? curSnrSum / curSnrCount : 0.0});
    }

    s.totalEpochs = static_cast<int>(epochTimes.size());
    if (s.totalEpochs > 0) {
        s.firstTime = epochTimes.front();
        s.lastTime  = epochTimes.back();
        s.durationSec = s.lastTime - s.firstTime;
    }

    // ── 采样率：历元间隔中位数 ──
    if (epochTimes.size() >= 2) {
        std::vector<double> intervals;
        intervals.reserve(epochTimes.size() - 1);
        for (size_t i = 1; i < epochTimes.size(); ++i)
            intervals.push_back(epochTimes[i] - epochTimes[i - 1]);

        std::vector<double> sorted = intervals;
        std::sort(sorted.begin(), sorted.end());
        const double median = sorted[sorted.size() / 2];

        if (median > 0) s.sampleRate = 1.0 / median;

        // ── 丢历元：间隔 > 2×中位数 ──
        const double threshold = median * 2.0;
        for (double iv : intervals) {
            if (iv > threshold) {
                s.epochGapCount++;
                s.totalGapSec += iv;
            }
        }

        // ── 掉星：末次出现远早于末历元（中途消失），且不是后加入 ──
        const double lateJoining = s.firstTime + threshold;
        const double earlyDrop   = s.lastTime  - threshold;
        for (auto it = satSpan.constBegin(); it != satSpan.constEnd(); ++it) {
            const int sys = it.key();
            for (auto sit = it.value().constBegin(); sit != it.value().constEnd(); ++sit) {
                const SatSpan &sp = sit.value();
                // 首次出现接近首历元（不是中途加入），且末次出现早于末历元 → 掉星
                if (sp.first <= lateJoining && sp.last < earlyDrop)
                    s.missingSatsPerSys[sys].insert(sit.key());
            }
        }
    }

    freeobs(&obs);
    *out = s;
    return true;
}

} // namespace Analysis
