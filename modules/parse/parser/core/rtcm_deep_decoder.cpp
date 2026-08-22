#include "modules/parse/parser/core/rtcm_deep_decoder.h"
#include <QTextStream>
#include <QDebug>
#include <cstring>

// ════════════════════════════════════════════════════════════════
RtcmDeepDecoder::RtcmDeepDecoder()
{
    std::memset(&m_rtcm, 0, sizeof(m_rtcm));
}

RtcmDeepDecoder::~RtcmDeepDecoder()
{
    if (m_initialized) {
        free_rtcm(&m_rtcm);
    }
}

void RtcmDeepDecoder::reset()
{
    if (m_initialized) {
        free_rtcm(&m_rtcm);
    }
    std::memset(&m_rtcm, 0, sizeof(m_rtcm));
    m_epochs.clear();
    m_initialized = false;
}

// ════════════════════════════════════════════════════════════════
//  主解码：逐字节喂入 input_rtcm3()
// ════════════════════════════════════════════════════════════════
QList<RtcmObsEpoch> RtcmDeepDecoder::decode(const QByteArray &rawData)
{
    reset();

    if (!init_rtcm(&m_rtcm)) {
        qWarning() << "[RtcmDeepDecoder] init_rtcm failed";
        return m_epochs;
    }
    m_initialized = true;

    const int size = rawData.size();
    int msgCount = 0;
    int obsCount = 0;

    for (int i = 0; i < size; ++i) {
        int ret = input_rtcm3(&m_rtcm, static_cast<unsigned char>(rawData[i]));

        if (ret == 1) {
            // 观测值数据就绪 — 深拷贝当前历元
            int nobs = m_rtcm.obs.n;
            if (nobs > 0 && nobs <= MAXOBS) {
                RtcmObsEpoch epoch;
                epoch.time = m_rtcm.obs.data[0].time;

                for (int j = 0; j < nobs; ++j) {
                    epoch.observations.append(m_rtcm.obs.data[j]);
                }

                m_epochs.append(epoch);
                obsCount += nobs;
                msgCount++;
            }
        }
    }

    qDebug() << "[RtcmDeepDecoder] decoded" << msgCount << "epochs,"
             << obsCount << "total observations";
    return m_epochs;
}

// ════════════════════════════════════════════════════════════════
//  卫星编号 → RINEX ID 字符串
// ════════════════════════════════════════════════════════════════
QString RtcmDeepDecoder::satIdStr(int sat)
{
    int prn = 0;
    int sys = satsys(sat, &prn);

    char id[16] = {0};
    switch (sys) {
        case SYS_GPS: snprintf(id, sizeof(id), "G%02d", prn); break;
        case SYS_GLO: snprintf(id, sizeof(id), "R%02d", prn); break;
        case SYS_GAL: snprintf(id, sizeof(id), "E%02d", prn); break;
        case SYS_QZS: snprintf(id, sizeof(id), "J%02d", prn); break;
        case SYS_CMP: snprintf(id, sizeof(id), "C%02d", prn); break;
        case SYS_SBS: snprintf(id, sizeof(id), "S%02d", prn - 100); break;
        case SYS_IRN: snprintf(id, sizeof(id), "I%02d", prn); break;
        default:      snprintf(id, sizeof(id), "?%02d", sat); break;
    }
    return QString::fromLatin1(id);
}

// ════════════════════════════════════════════════════════════════
//  观测码 → RINEX 字符串
// ════════════════════════════════════════════════════════════════
QString RtcmDeepDecoder::obsCodeStr(unsigned char code)
{
    int freq = 0;
    const char *s = code2obs(code, &freq);
    return QString::fromLatin1(s ? s : "  ");
}

// ════════════════════════════════════════════════════════════════
//  格式化为 RINEX 观测值文本
// ════════════════════════════════════════════════════════════════
QString RtcmDeepDecoder::toRinexText(const QList<RtcmObsEpoch> &epochs)
{
    if (epochs.isEmpty()) {
        return QStringLiteral("（无有效观测值）\n");
    }

    QString out;
    QTextStream s(&out);

    s << QStringLiteral("═══════════════════════════════════════════════════════════\n");
    s << QStringLiteral("  RTCM 3.x 深解码 — RINEX 观测值格式\n");
    s << QStringLiteral("  历元数: %1\n").arg(epochs.size());

    int totalObs = 0;
    for (const auto &ep : epochs) totalObs += ep.observations.size();
    s << QStringLiteral("  总观测值: %1\n").arg(totalObs);
    s << QStringLiteral("═══════════════════════════════════════════════════════════\n");

    for (const auto &ep : epochs) {
        // 历元头
        double t[6];
        time2epoch(ep.time, t);
        s << QStringLiteral("\n> %1 %2 %3 %4 %5 %6  %7\n")
            .arg((int)t[0], 4)
            .arg((int)t[1], 2, 10, QLatin1Char('0'))
            .arg((int)t[2], 2, 10, QLatin1Char('0'))
            .arg((int)t[3], 2, 10, QLatin1Char('0'))
            .arg((int)t[4], 2, 10, QLatin1Char('0'))
            .arg(t[5], 0, 'f', 7)
            .arg(ep.observations.size());

        // 每条观测记录
        for (const auto &obs : ep.observations) {
            s << satIdStr(obs.sat);

            for (int f = 0; f < NFREQ; ++f) {
                if (obs.P[f] != 0.0) {
                    s << QStringLiteral("  %1").arg(obs.P[f], 14, 'f', 3);
                    s << QStringLiteral("  %1").arg(obsCodeStr(obs.code[f]));
                } else {
                    s << QStringLiteral("                ");
                }
                if (obs.L[f] != 0.0) {
                    s << QStringLiteral("  %1").arg(obs.L[f], 14, 'f', 3);
                } else {
                    s << QStringLiteral("                ");
                }
                if (obs.D[f] != 0.0f) {
                    s << QStringLiteral("  %1").arg(obs.D[f], 14, 'f', 3);
                } else {
                    s << QStringLiteral("                ");
                }
                s << QStringLiteral("  %1").arg(obs.SNR[f]);
            }
            s << QStringLiteral("\n");
        }
    }

    s.flush();
    return out;
}

// ════════════════════════════════════════════════════════════════
//  格式化为简化 CSV 文本
// ════════════════════════════════════════════════════════════════
QString RtcmDeepDecoder::toCsvText(const QList<RtcmObsEpoch> &epochs)
{
    if (epochs.isEmpty()) {
        return QStringLiteral("（无有效观测值）\n");
    }

    QString out;
    QTextStream s(&out);

    s << QStringLiteral("═══════════════════════════════════════════════════════════\n");
    s << QStringLiteral("  RTCM 3.x 深解码 — 观测值列表\n");
    s << QStringLiteral("  历元数: %1  总观测值: %2\n")
        .arg(epochs.size())
        .arg([&]() { int n = 0; for (auto &e:epochs) n+=e.observations.size(); return n; }());
    s << QStringLiteral("═══════════════════════════════════════════════════════════\n");

    // CSV 表头
    s << QStringLiteral("GPS周秒,卫星,P1(m),码1,L1(cyc),P2(m),码2,L2(cyc),P5(m),码5,L5(cyc)\n");

    for (const auto &ep : epochs) {
        double ws = time2gpst(ep.time, nullptr);
        for (const auto &obs : ep.observations) {
            s << QStringLiteral("%1,%2")
                .arg(ws, 0, 'f', 3)
                .arg(satIdStr(obs.sat));

            for (int f = 0; f < NFREQ; ++f) {
                if (obs.P[f] != 0.0)
                    s << QStringLiteral(",%1,%2,%3")
                        .arg(obs.P[f], 0, 'f', 3)
                        .arg(obsCodeStr(obs.code[f]))
                        .arg(obs.L[f], 0, 'f', 3);
                else
                    s << QStringLiteral(",,,");
            }
            s << QStringLiteral("\n");
        }
    }

    s.flush();
    return out;
}
