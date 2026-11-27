#include "tempdriftanalysis.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kRateWindowSec = 30.0;   // 温度变化率的观察窗
constexpr double kMinSoakSec    = 60.0;   // 保温平台最短时长

double sampleStd(const QVector<double> &v)
{
    const int n = v.size();
    if (n < 2) return 0.0;
    double mean = 0.0;
    for (double x : v) mean += x;
    mean /= n;
    double ss = 0.0;
    for (double x : v) ss += (x - mean) * (x - mean);
    return std::sqrt(ss / (n - 1));
}

double peakToPeak(const QVector<double> &v)
{
    if (v.isEmpty()) return 0.0;
    double lo = v.first(), hi = v.first();
    for (double x : v) { lo = std::min(lo, x); hi = std::max(hi, x); }
    return hi - lo;
}

} // namespace

QString TempDriftAnalysis::axisLabel(int i)
{
    static const char *kNames[6] = {"加计X", "加计Y", "加计Z",
                                     "陀螺X", "陀螺Y", "陀螺Z"};
    if (i < 0 || i > 5) return QStringLiteral("?");
    return QString::fromUtf8(kNames[i]);
}

TempDriftAnalysis::Result TempDriftAnalysis::compute(const QVector<QVector<double>> &cols,
                                                     double fs,
                                                     double rampThresholdCPerMin)
{
    Result r;
    r.fs = fs;
    r.rampThreshold = rampThresholdCPerMin;

    if (cols.size() != 7) {
        r.notes << QStringLiteral("需要 7 列数据（ax ay az gx gy gz temp），当前 %1 列").arg(cols.size());
        return r;
    }

    const int N = cols[0].size();
    for (int c = 1; c < 7; ++c) {
        if (cols[c].size() != N) {
            r.notes << QStringLiteral("第 %1 列长度与第 1 列不一致").arg(c + 1);
            return r;
        }
    }
    r.totalSamples = N;

    if (fs < 1.0) {
        r.notes << QStringLiteral("采样率过低（%1 Hz），至少需要 1 Hz").arg(fs);
        return r;
    }
    if (rampThresholdCPerMin <= 0.0) {
        r.notes << QStringLiteral("温度变化率阈值必须大于 0");
        return r;
    }

    // mv_filter：窗口 count+1 点，步长 floor(count/2)
    const int count = std::max(2, static_cast<int>(std::lround(fs * 10.0)));
    const int step  = std::max(1, count / 2);
    r.windowSamples = count + 1;
    r.stepSamples   = step;

    if (N < count + 1) {
        r.notes << QStringLiteral("数据太短（%1 点），平滑窗口需要至少 %2 点")
                       .arg(N).arg(count + 1);
        return r;
    }

    // ── 7 列一起做滑动平均（加计/陀螺/温度共用同一时间基准）──
    QVector<QVector<double>> sm(7);
    for (int i = 0; i + count < N; i += step) {
        double sum[7] = {0, 0, 0, 0, 0, 0, 0};
        for (int j = i; j <= i + count; ++j) {
            for (int c = 0; c < 7; ++c)
                sum[c] += cols[c][j];
        }
        const double inv = 1.0 / (count + 1);
        for (int c = 0; c < 7; ++c)
            sm[c].append(sum[c] * inv);
        r.timeSec.append((i + count / 2.0) / fs);
    }

    r.outSamples = r.timeSec.size();
    if (r.outSamples < 10) {
        r.notes << QStringLiteral("平滑后数据太少（%1 点），无法统计").arg(r.outSamples);
        return r;
    }

    r.dtSec = step / fs;
    r.temp  = sm[6];
    r.axes.resize(6);
    for (int k = 0; k < 6; ++k)
        r.axes[k] = sm[k];

    // 加计 g → mg
    for (int k = 0; k < 3; ++k)
        for (double &v : r.axes[k])
            v *= 1000.0;

    // ── 保温 / 斜坡判定：|dT/dt| < 阈值 ──
    const int kRate = std::max(1, static_cast<int>(std::lround(kRateWindowSec / r.dtSec)));
    r.soakMask.resize(r.outSamples);
    for (int k = 0; k < r.outSamples; ++k) {
        if (k < kRate) { r.soakMask[k] = 0; continue; }   // 开头没有足够历史，按斜坡处理
        const double rate = (r.temp[k] - r.temp[k - kRate]) / (kRate * r.dtSec) * 60.0; // ℃/min
        r.soakMask[k] = (std::fabs(rate) < rampThresholdCPerMin) ? 1 : 0;
    }

    // ── 每轴统计 ──
    int totalSoak = 0;
    for (int a = 0; a < 6; ++a) {
        AxisMetric m;
        m.ppAll  = peakToPeak(r.axes[a]);
        m.stdAll = sampleStd(r.axes[a]);

        QVector<double> soakVals, soakTemps;
        soakVals.reserve(r.outSamples);
        soakTemps.reserve(r.outSamples);
        for (int k = 0; k < r.outSamples; ++k) {
            if (!r.soakMask[k]) continue;
            soakVals.append(r.axes[a][k]);
            soakTemps.append(r.temp[k]);
        }
        m.soakPoints = soakVals.size();

        if (soakVals.size() >= 2) {
            m.ppSoak  = peakToPeak(soakVals);
            m.stdSoak = sampleStd(soakVals);

            // 值-温度 线性最小二乘：slope = cov(T,x)/var(T)
            double mt = 0.0, mx = 0.0;
            for (int i = 0; i < soakVals.size(); ++i) { mt += soakTemps[i]; mx += soakVals[i]; }
            mt /= soakVals.size();
            mx /= soakVals.size();
            double sxx = 0.0, sxy = 0.0;
            for (int i = 0; i < soakVals.size(); ++i) {
                const double dt = soakTemps[i] - mt;
                sxx += dt * dt;
                sxy += dt * (soakVals[i] - mx);
            }
            m.slopePerC = (sxx > 1e-12) ? (sxy / sxx) : 0.0;
            m.ok = true;
        }
        r.metrics[a] = m;
        if (a == 0) totalSoak = m.soakPoints;
    }

    // ── 保温平台分段 ──
    int k = 0;
    while (k < r.outSamples) {
        if (!r.soakMask[k]) { ++k; continue; }
        const int start = k;
        while (k < r.outSamples && r.soakMask[k]) ++k;
        const int len = k - start;
        const double dur = len * r.dtSec;
        if (dur + 1e-9 < kMinSoakSec)
            continue;
        SoakSegment s;
        s.startIndex  = start;
        s.count       = len;
        s.startSec    = r.timeSec[start];
        s.durationSec = dur;
        double tsum = 0.0;
        for (int i = start; i < start + len; ++i) tsum += r.temp[i];
        s.meanTemp = tsum / len;
        r.soakSegments.append(s);
    }

    // ── 诊断 ──
    if (totalSoak == 0)
        r.notes << QStringLiteral("未检出保温平台（|dT/dt| < %1 ℃/min），可调大阈值或确认温度列")
                       .arg(rampThresholdCPerMin);
    else if (totalSoak < r.outSamples / 20)
        r.notes << QStringLiteral("保温点仅占 %1%，阈值可能偏小（当前 %2 ℃/min）")
                       .arg(100.0 * totalSoak / r.outSamples, 0, 'f', 1)
                       .arg(rampThresholdCPerMin);

    if (r.soakSegments.isEmpty() && totalSoak > 0)
        r.notes << QStringLiteral("保温点不足一段连续 %1 s 的平台，未输出平台列表").arg(kMinSoakSec);

    r.ok = true;
    return r;
}
