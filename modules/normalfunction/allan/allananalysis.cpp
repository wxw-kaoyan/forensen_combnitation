#include "allananalysis.h"

#include <cmath>
#include <algorithm>
#include <QtMath>

/* ============================================================
 *  τ 生成，与 MATLAB allan.m 第 7 行逐字对应：
 *    tau = logspace(0, floor(log10(N/freq)), ...
 *                   (floor(log10(N/freq/4)) - ceil(log10(1/freq)))*200);
 * ============================================================ */
QVector<double> AllanAnalysis::generateTau(double fs, int N, int density)
{
    QVector<double> out;
    if (fs <= 0 || N < 2) return out;

    const double loExp = 0.0;                                   // logspace 起点 = 10^0 = 1 s
    const double hiExp = std::floor(std::log10(double(N) / fs));
    int n = int((std::floor(std::log10(double(N) / fs / 4.0))
                 - std::ceil(std::log10(1.0 / fs))) * density);
    if (n < 1) n = 1;

    out.reserve(n);
    if (n == 1) {
        out.append(std::pow(10.0, hiExp));                      // MATLAB logspace(a,b,1) = 10^b
    } else {
        for (int i = 0; i < n; ++i)
            out.append(std::pow(10.0, loExp + (hiExp - loExp) * i / double(n - 1)));
    }
    return out;
}

/* ============================================================
 *  单轴 Allan 标准差 (non-overlapping)
 *  对应 MATLAB allan_variance.m:
 *    W = floor(fs*tau);  K = floor(N/W);
 *    Omega = mean(reshape(data, W, K), 1);     % 每窗均值  (1×K)
 *    Sigma = sqrt(0.5 * mean( diff(Omega).^2 ))
 *    Err   = 1 / sqrt(2*(K-1))
 * ============================================================ */
AllanAnalysis::AxisResult AllanAnalysis::compute(const QVector<double> &data,
                                                 double fs,
                                                 const QVector<double> &tauIn)
{
    AxisResult r;
    if (fs <= 0 || data.size() < 4) return r;

    QVector<double> tau = tauIn.isEmpty() ? generateTau(fs, data.size()) : tauIn;
    r.tau = tau;

    const int N = data.size();
    r.sigma.resize(tau.size());
    r.sigmaErr.resize(tau.size());
    r.K.resize(tau.size());

    for (int i = 0; i < tau.size(); ++i) {
        double Wf = tau[i] * fs;
        if (Wf < 1.0) {
            r.sigma[i] = 0; r.sigmaErr[i] = 1.0; r.K[i] = 0; continue;
        }
        int W = int(Wf);                 // = floor(fs*tau)
        int K = N / W;                   // = floor(N/W)
        if (K < 2) {                     // K=1 时 diff 为空（MATLAB 得 NaN），防护除零
            r.sigma[i] = 0; r.sigmaErr[i] = 1.0; r.K[i] = K; continue;
        }

        // 每窗均值
        double sum1 = 0, sum2 = 0;
        int    cnt = 0;
        for (int k = 0; k < K - 1; ++k) {
            // Omega_k = mean(data[k*W .. (k+1)*W-1])
            double o1 = 0;
            for (int j = 0; j < W; ++j) o1 += data[k * W + j];
            o1 /= W;
            double o2 = 0;
            for (int j = 0; j < W; ++j) o2 += data[(k + 1) * W + j];
            o2 /= W;
            double d = o2 - o1;
            sum1 += d * d;
            sum2 += d;
            ++cnt;
        }
        double meanSq = sum1 / cnt;
        r.sigma[i]   = std::sqrt(0.5 * meanSq);
        r.sigmaErr[i] = 1.0 / std::sqrt(2.0 * (cnt - 1));
        r.K[i]       = K;
    }
    return r;
}

/* ============================================================
 *  6 轴批量计算
 * ============================================================ */
AllanAnalysis::MultiResult AllanAnalysis::compute6(
        const QVector<QVector<double>> &cols, double fs, const QVector<double> &tauIn)
{
    MultiResult mr;
    mr.axisCount = cols.size();
    mr.fs = fs;
    if (cols.isEmpty()) return mr;
    mr.totalLen = cols.first().size();

    QVector<double> tau = tauIn.isEmpty() ? generateTau(fs, mr.totalLen) : tauIn;

    mr.axes.reserve(mr.axisCount);
    for (const auto &col : cols)
        mr.axes.append(compute(col, fs, tau));

    return mr;
}

/* ============================================================
 *  最近邻 tau 索引，对应 MATLAB 的 [~,idx]=min(abs(tau-target))
 * ============================================================ */
static int nearestTauIndex(const QVector<double> &tau, double target)
{
    if (tau.isEmpty()) return -1;
    int best = 0;
    double bestD = std::abs(tau[0] - target);
    for (int i = 1; i < tau.size(); ++i) {
        const double d = std::abs(tau[i] - target);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

/* ============================================================
 *  MATLAB mv_filter.m 同款：步长 floor(count/2)，窗口 count+1 点
 *    for i=1:floor(count/2):len-count
 *        output(index,:) = mean(x(i:i+count,:));
 * ============================================================ */
static QVector<double> mvFilter(const QVector<double> &x, int count)
{
    QVector<double> out;
    const int len = x.size();
    if (count <= 0 || len <= count) return out;

    const int stride = count / 2;
    if (stride < 1) return out;

    for (int i = 0; i < len - count; i += stride) {
        double s = 0;
        for (int j = i; j <= i + count; ++j) s += x[j];   // count+1 个点
        out.append(s / double(count + 1));
    }
    return out;
}

/* MATLAB std 默认 N-1 归一化 */
static double movingStd(const QVector<double> &v)
{
    if (v.size() < 2) return 0;
    double mean = 0;
    for (double x : v) mean += x;
    mean /= v.size();
    double s = 0;
    for (double x : v) { const double d = x - mean; s += d * d; }
    return std::sqrt(s / (v.size() - 1));
}

/* ============================================================
 *  加计噪声参数提取（与 MATLAB allan.m 第 22-39 行逐项对应）
 *
 *  输入单位: g
 *  输出:
 *    RW             = σ(τ 最近邻 1s)  × 9.8 × 60      单位 m/s/√h
 *    BI @10s        = σ(τ 最近邻 10s) × 1e6           单位 μg
 *    BI @60s        = mean(σ(τ=51..70s)) × 1e6        单位 μg
 *    BI @200s       = mean(σ(τ=191..210s)) × 1e6      单位 μg
 *    静态偏置稳定    = std(mv_filter(data, fs*10)) × 1e6  单位 μg
 * ============================================================ */
QVector<AllanAnalysis::AccelParams> AllanAnalysis::extractAccel3(
        const MultiResult &mr, const QVector<QVector<double>> &cols, double fs)
{
    QVector<AccelParams> out(3);
    for (int axis = 0; axis < 3 && axis < mr.axes.size() && axis < cols.size(); ++axis) {
        const AxisResult &ax = mr.axes[axis];
        if (ax.tau.isEmpty()) continue;
        AccelParams p;

        // RW: [~,idx]=min(abs(tau-1)); rw = astd(idx) * 9.8 * 60
        const int i1 = nearestTauIndex(ax.tau, 1.0);
        p.randomWalk = (i1 >= 0) ? ax.sigma[i1] * 9.8 * 60.0 : 0.0;   // m/s/√h

        // [~,idx]=min(abs(tau-10)); BI_10s = astd(idx)*1e6
        const int i10 = nearestTauIndex(ax.tau, 10.0);
        p.biasInstability_10s = (i10 >= 0) ? ax.sigma[i10] * 1e6 : 0.0;

        // for i=1:20, idx=min(abs(tau-(50+i))); BI_60s = mean(...)
        {
            double sum = 0; int cnt = 0;
            for (int i = 1; i <= 20; ++i) {
                const int idx = nearestTauIndex(ax.tau, 50.0 + i);
                if (idx >= 0) { sum += ax.sigma[idx] * 1e6; ++cnt; }
            }
            p.biasInstability_60s = cnt ? sum / cnt : 0.0;
        }
        // for i=1:20, idx=min(abs(tau-(190+i))); BI_200s = mean(...)
        {
            double sum = 0; int cnt = 0;
            for (int i = 1; i <= 20; ++i) {
                const int idx = nearestTauIndex(ax.tau, 190.0 + i);
                if (idx >= 0) { sum += ax.sigma[idx] * 1e6; ++cnt; }
            }
            p.biasInstability_200s = cnt ? sum / cnt : 0.0;
        }

        // Bias_stability = std(mv_filter(IMU(:,i), freq*10)) * 1e6
        p.biasStabilityStatic = movingStd(mvFilter(cols[axis], int(fs * 10.0))) * 1e6;

        p.ok = true;
        out[axis] = p;
    }
    return out;
}

/* ============================================================
 *  陀螺噪声参数提取（与 MATLAB allan.m 第 22-39 行逐项对应）
 *
 *  输入单位: deg/s
 *  输出:
 *    RW             = σ(τ 最近邻 1s)  × 60        单位 deg/√h
 *    BI @10s        = σ(τ 最近邻 10s) × 3600      单位 deg/h
 *    BI @60s        = mean(σ(τ=51..70s)) × 3600   单位 deg/h
 *    BI @200s       = mean(σ(τ=191..210s)) × 3600 单位 deg/h
 *    静态偏置稳定    = std(mv_filter(data, fs*10)) × 3600  单位 deg/h
 * ============================================================ */
QVector<AllanAnalysis::GyroParams> AllanAnalysis::extractGyro3(
        const MultiResult &mr, const QVector<QVector<double>> &cols, double fs)
{
    QVector<GyroParams> out(3);
    for (int axis = 3; axis < 6 && axis < mr.axes.size() && axis < cols.size(); ++axis) {
        const AxisResult &ax = mr.axes[axis];
        if (ax.tau.isEmpty()) continue;
        GyroParams p;

        // [~,idx]=min(abs(tau-1)); rw = astd(idx) * 60
        const int i1 = nearestTauIndex(ax.tau, 1.0);
        p.randomWalk = (i1 >= 0) ? ax.sigma[i1] * 60.0 : 0.0;   // deg/√h

        // [~,idx]=min(abs(tau-10)); BI_10s = astd(idx)*3600
        const int i10 = nearestTauIndex(ax.tau, 10.0);
        p.biasInstability_10s = (i10 >= 0) ? ax.sigma[i10] * 3600.0 : 0.0;

        // for i=1:20, idx=min(abs(tau-(50+i))); BI_60s = mean(...)
        {
            double sum = 0; int cnt = 0;
            for (int i = 1; i <= 20; ++i) {
                const int idx = nearestTauIndex(ax.tau, 50.0 + i);
                if (idx >= 0) { sum += ax.sigma[idx] * 3600.0; ++cnt; }
            }
            p.biasInstability_60s = cnt ? sum / cnt : 0.0;
        }
        // for i=1:20, idx=min(abs(tau-(190+i))); BI_200s = mean(...)
        {
            double sum = 0; int cnt = 0;
            for (int i = 1; i <= 20; ++i) {
                const int idx = nearestTauIndex(ax.tau, 190.0 + i);
                if (idx >= 0) { sum += ax.sigma[idx] * 3600.0; ++cnt; }
            }
            p.biasInstability_200s = cnt ? sum / cnt : 0.0;
        }

        // Bias_stability = std(mv_filter(IMU(:,i), freq*10)) * 3600
        p.biasStabilityStatic = movingStd(mvFilter(cols[axis], int(fs * 10.0))) * 3600.0;

        p.ok = true;
        out[axis - 3] = p;
    }
    return out;
}
