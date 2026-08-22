#ifndef ANALYSIS_STATISTICS_H
#define ANALYSIS_STATISTICS_H

#include <cmath>
#include <algorithm>
#include <QVector>
#include <numeric>

namespace Analysis {

// ════════════════════════════════════════════════════════════════
//  统计工具（对标 MATLAB sigma_value / rms / std）
// ════════════════════════════════════════════════════════════════

inline double mean(const QVector<double> &data)
{
    if (data.isEmpty()) return 0.0;
    return std::accumulate(data.begin(), data.end(), 0.0) / data.size();
}

inline double rms(const QVector<double> &data)
{
    if (data.isEmpty()) return 0.0;
    double sum = 0.0;
    for (auto v : data) sum += v * v;
    return std::sqrt(sum / data.size());
}

inline double stdDev(const QVector<double> &data, bool population = false)
{
    if (data.size() < 2) return 0.0;
    double m = mean(data);
    double sum = 0.0;
    for (auto v : data) sum += (v - m) * (v - m);
    double divisor = population ? data.size() : (data.size() - 1);
    return std::sqrt(sum / divisor);
}

inline double maxAbs(const QVector<double> &data)
{
    if (data.isEmpty()) return 0.0;
    double m = 0.0;
    for (auto v : data) m = qMax(m, std::abs(v));
    return m;
}

inline double minVal(const QVector<double> &data)
{
    if (data.isEmpty()) return 0.0;
    return *std::min_element(data.begin(), data.end());
}

inline double maxVal(const QVector<double> &data)
{
    if (data.isEmpty()) return 0.0;
    return *std::max_element(data.begin(), data.end());
}

// ════════════════════════════════════════════════════════════════
//  percentile(data, ratio) — 对标 MATLAB sigma_value(in, ratio)
//  返回第 ratio*100 百分位的值
//  e.g. percentile(data, 0.95) = CEP95
// ════════════════════════════════════════════════════════════════
inline double percentile(QVector<double> data, double ratio)
{
    if (data.isEmpty()) return 0.0;
    std::sort(data.begin(), data.end());
    int idx = static_cast<int>(std::floor(data.size() * ratio));
    if (idx < 0) idx = 0;
    if (idx >= data.size()) idx = data.size() - 1;
    return data[idx];
}

// 水平误差 (北/东方向)
inline QVector<double> horizontalError(const QVector<double> &dNorth, const QVector<double> &dEast)
{
    QVector<double> out(dNorth.size());
    for (int i = 0; i < out.size(); ++i)
        out[i] = std::sqrt(dNorth[i] * dNorth[i] + dEast[i] * dEast[i]);
    return out;
}

// 3D 误差
inline QVector<double> error3D(const QVector<double> &dNorth, const QVector<double> &dEast, const QVector<double> &dUp)
{
    QVector<double> out(dNorth.size());
    for (int i = 0; i < out.size(); ++i)
        out[i] = std::sqrt(dNorth[i] * dNorth[i] + dEast[i] * dEast[i] + dUp[i] * dUp[i]);
    return out;
}

} // namespace Analysis

#endif // ANALYSIS_STATISTICS_H
