#include "leverarmestimator.h"

#include <QStringList>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr double kPi        = 3.14159265358979323846;
constexpr double kDeg2Rad   = kPi / 180.0;
constexpr double kRad2Deg   = 180.0 / kPi;
constexpr double kMetersPerDeg = 111320.0;   // 纬度 1° 对应的米数

constexpr int    kMinSamples    = 30;    // 少于此帧数不出解
constexpr double kMinSpanDeg    = 30.0;  // 航向跨度太小则方程病态
constexpr double kEnoughSpanDeg = 180.0; // 参考解 / 可用解分界
constexpr double kFullSpanDeg   = 360.0; // 满一圈
constexpr double kMovingRms     = 0.05;  // 速度残差 >5cm/s 认为数据异常
constexpr double kFixRatioWarn  = 0.90;  // 固定解占比低于 90% 给提示

// n×n 高斯-约当求逆（n ≤ 4），失败返回 false
bool invertSquare(const double src[4][4], int n, double inv[4][4])
{
    double a[4][8] = {};
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            a[i][j] = src[i][j];
            a[i][n + j] = (i == j) ? 1.0 : 0.0;
        }
    for (int col = 0; col < n; ++col) {
        int piv = col;
        for (int rr = col + 1; rr < n; ++rr)
            if (std::fabs(a[rr][col]) > std::fabs(a[piv][col]))
                piv = rr;
        if (std::fabs(a[piv][col]) < 1e-18)
            return false;
        if (piv != col)
            for (int j = 0; j < 2 * n; ++j)
                std::swap(a[piv][j], a[col][j]);
        const double d = a[col][col];
        for (int j = 0; j < 2 * n; ++j)
            a[col][j] /= d;
        for (int rr = 0; rr < n; ++rr) {
            if (rr == col)
                continue;
            const double f = a[rr][col];
            if (f == 0.0)
                continue;
            for (int j = 0; j < 2 * n; ++j)
                a[rr][j] -= f * a[col][j];
        }
    }
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            inv[i][j] = a[i][n + j];
    return true;
}

} // namespace

void LeverArmEstimator::reset()
{
    m_samples.clear();
    m_lat0 = m_lon0 = 0.0;
    m_hasOrigin = false;
    m_lastYawRad = 0.0;
    m_hasYaw = false;
    m_yawAccumRad = 0.0;
    m_firstMs = m_lastMs = -1;
    m_totalFrames = 0;
    m_fixedFrames = 0;
}

void LeverArmEstimator::addFusion(const FusionResult &f)
{
    ++m_totalFrames;

    // 只用固定解（0x0166 定位质量码：4=固定解）
    if (f.rtkStatus != 4)
        return;
    ++m_fixedFrames;

    // 航向连续化：设备可能输出连续累计角（如 -1112°），也可能在 ±180° 跳变，
    // 逐帧把差值折到 (-π, π] 再累加，两种固件都能得到正确的圈数
    const double yawRaw = double(f.yaw) * kDeg2Rad;
    if (m_hasYaw) {
        double d = yawRaw - m_lastYawRad;
        while (d >  kPi) d -= 2.0 * kPi;
        while (d < -kPi) d += 2.0 * kPi;
        m_yawAccumRad += d;
    } else {
        m_yawAccumRad = 0.0;
    }
    m_lastYawRad = yawRaw;
    m_hasYaw = true;

    // 局部坐标：以第一帧固定解为原点，避免大绝对坐标参与最小二乘
    const double lat = double(f.lat) * 1e-7;
    const double lon = double(f.lon) * 1e-7;
    if (!m_hasOrigin) {
        m_lat0 = lat;
        m_lon0 = lon;
        m_hasOrigin = true;
    }
    const double nLocal = (lat - m_lat0) * kMetersPerDeg;
    const double eLocal = (lon - m_lon0) * kMetersPerDeg * std::cos(m_lat0 * kDeg2Rad);

    const qint64 ms = qint64(f.gpsWeek) * 604800000LL + qint64(f.gpsWeekSecond);
    if (m_firstMs < 0)
        m_firstMs = ms;
    m_lastMs = ms;

    // 静止检测：只用陀螺模长 + 加计模长判断，静止帧对速度方程无信息
    const double gx = double(f.gyroX), gy = double(f.gyroY), gz = double(f.gyroZ);
    const double ax = double(f.accelX), ay = double(f.accelY), az = double(f.accelZ);
    const double gmag = std::sqrt(gx * gx + gy * gy + gz * gz);
    const double amag = std::sqrt(ax * ax + ay * ay + az * az);
    const bool isStatic = (gmag < 0.5 && std::fabs(amag - 1.0) < 0.05);

    if (isStatic) {
        // 静止段 ω≈0、v≈0，对速度方程没有任何信息，跳过
        return;
    }

    Sample s;
    s.n = nLocal;
    s.e = eLocal;
    s.yawRad = yawRaw;
    s.vn = double(f.velN);
    s.ve = double(f.velE);
    s.wz = gz * kDeg2Rad;    // 0x0166 陀螺 Z 轴 deg/s → rad/s

    // 注意：0x0166 是补偿后的位置，绕圆心运动时它本来就几乎不动，
    // 位置字段分辨率只有 1e-7 度（约 1.1 cm），很多帧量化后数值相同。
    // 这不是"重复观测"，不能按位置去重——去重会丢掉绝大部分有效帧。
    m_samples.append(s);
}

LeverArmEstimator::Result LeverArmEstimator::solve(Method method) const
{
    Result r;
    r.method      = method;
    r.totalFrames = m_totalFrames;
    r.fixedFrames = m_fixedFrames;
    r.usedFrames  = m_samples.size();
    r.yawSpanDeg  = std::fabs(m_yawAccumRad) * kRad2Deg;
    r.circles     = r.yawSpanDeg / 360.0;
    if (m_firstMs >= 0 && m_lastMs >= m_firstMs)
        r.durationSec = double(m_lastMs - m_firstMs) / 1000.0;

    if (m_totalFrames == 0) {
        r.reason = QStringLiteral("等待 0x0166 数据…");
        return r;
    }
    if (m_samples.size() < kMinSamples) {
        r.reason = QStringLiteral("固定解样本不足（%1/%2）")
                       .arg(m_samples.size()).arg(kMinSamples);
        return r;
    }
    if (r.yawSpanDeg < kMinSpanDeg) {
        r.reason = QStringLiteral("转向不足（航向跨度 %1°，至少 30°）")
                       .arg(r.yawSpanDeg, 0, 'f', 0);
        return r;
    }

    auto rmsOf = [](const QVector<double> &v) {
        double acc = 0.0;
        for (double d : v)
            acc += d * d;
        return v.isEmpty() ? 0.0 : std::sqrt(acc / double(v.size()));
    };

    // 速度法用的设计矩阵（也供分段检查复用）
    auto buildVel = [&](const QVector<int> &idx,
                        std::vector<double> &A, std::vector<double> &Y) {
        A.assign(std::size_t(idx.size()) * 4, 0.0);
        Y.assign(std::size_t(idx.size()) * 2, 0.0);
        for (int k = 0; k < idx.size(); ++k) {
            const Sample &s = m_samples.at(idx.at(k));
            const double w  = s.wz;
            const double c  = std::cos(s.yawRad);
            const double si = std::sin(s.yawRad);
            const std::size_t r0 = std::size_t(2 * k) * 2;
            const std::size_t r1 = std::size_t(2 * k + 1) * 2;
            A[r0 + 0] = -w * si; A[r0 + 1] = -w * c;
            Y[std::size_t(2 * k)] = s.vn;
            A[r1 + 0] =  w * c;  A[r1 + 1] = -w * si;
            Y[std::size_t(2 * k + 1)] = s.ve;
        }
    };
    auto velResid = [&](const QVector<int> &idx, const std::vector<double> &ans,
                        QVector<double> &out) {
        out.clear();
        out.reserve(idx.size());
        for (int k = 0; k < idx.size(); ++k) {
            const Sample &s = m_samples.at(idx.at(k));
            const double w  = s.wz;
            const double c  = std::cos(s.yawRad);
            const double si = std::sin(s.yawRad);
            const double pn = -w * si * ans[0] - w * c * ans[1];
            const double pe =  w * c  * ans[0] - w * si * ans[1];
            out.append(std::hypot(s.vn - pn, s.ve - pe));
        }
    };

    int posRej = 0, velRej = 0;
    QVector<int> posIdx, velIdx;

    // ══════════ 位置法：4 参数（圆心 + 杆臂）══════════
    // N = C_N + lx·cosψ − ly·sinψ
    // E = C_E + lx·sinψ + ly·cosψ
    // 与 MATLAB calibrate_lever_arm.m 的 coef = A\Y 完全一致（QR 最小二乘）。
    {
        posIdx.reserve(m_samples.size());
        for (int i = 0; i < m_samples.size(); ++i)
            posIdx.append(i);

        double cN = 0.0, cE = 0.0, lx = 0.0, ly = 0.0;
        QVector<double> res;
        if (solveLeverArm(m_samples, posIdx, cN, cE, lx, ly, res)) {
            double rmsPos = rmsOf(res);

            // 一轮 3σ 野值剔除
            if (rmsPos > 1e-9) {
                const double limit = 3.0 * rmsPos;
                QVector<int> keep;
                keep.reserve(posIdx.size());
                for (int k = 0; k < posIdx.size(); ++k)
                    if (res.at(k) <= limit)
                        keep.append(posIdx.at(k));
                if (keep.size() < posIdx.size() && keep.size() >= kMinSamples) {
                    double cN2 = 0.0, cE2 = 0.0, lx2 = 0.0, ly2 = 0.0;
                    QVector<double> res2;
                    if (solveLeverArm(m_samples, keep, cN2, cE2, lx2, ly2, res2)) {
                        posRej = posIdx.size() - keep.size();
                        posIdx = keep;
                        cN = cN2; cE = cE2; lx = lx2; ly = ly2;
                        res = res2;
                        rmsPos = rmsOf(res);
                    }
                }
            }

            r.posValid       = true;
            r.centerN        = cN;
            r.centerE        = cE;
            r.posLx          = lx;
            r.posLy          = ly;
            r.posResidualRms = rmsPos;

            // 杆臂标准差 σ²·(AᵀA)⁻¹（4 参数：C_N, C_E, lx, ly）
            double ata[4][4] = {};
            for (int i : posIdx) {
                const Sample &s = m_samples.at(i);
                const double c  = std::cos(s.yawRad);
                const double si = std::sin(s.yawRad);
                const double r0[4] = {1.0, 0.0,  c, -si};
                const double r1[4] = {0.0, 1.0, si,  c};
                for (int a = 0; a < 4; ++a)
                    for (int b = 0; b < 4; ++b)
                        ata[a][b] += r0[a] * r0[b] + r1[a] * r1[b];
            }
            double inv[4][4];
            if (invertSquare(ata, 4, inv)) {
                r.posStdLx = std::sqrt(std::max(0.0, inv[2][2] * rmsPos * rmsPos));
                r.posStdLy = std::sqrt(std::max(0.0, inv[3][3] * rmsPos * rmsPos));
            }
        }
    }

    // ══════════ 速度法：2 参数（v = Ċ + ω·R·l）══════════
    // 位置公式求导后平移常量 Ċ 被抹掉，只剩杆臂两个未知量。
    // 实测 |v| < 0.2 m/s 的帧会把杆臂拉小，优先只取高速帧。
    double speedGate = 0.0;
    {
        const double gates[] = {0.20, 0.10, 0.0};
        for (double g : gates) {
            velIdx.clear();
            velIdx.reserve(m_samples.size());
            for (int i = 0; i < m_samples.size(); ++i) {
                const Sample &s = m_samples.at(i);
                if (std::hypot(s.vn, s.ve) >= g)
                    velIdx.append(i);
            }
            speedGate = g;
            if (velIdx.size() >= kMinSamples)
                break;
        }
        r.speedGate = speedGate;

        if (velIdx.size() >= kMinSamples) {
            std::vector<double> A, Y, sol;
            buildVel(velIdx, A, Y);
            if (solveQrLstsq(A, Y, int(velIdx.size()) * 2, 2, sol)) {
                QVector<double> residual;
                velResid(velIdx, sol, residual);
                double rmsVel = rmsOf(residual);

                // 一轮 3σ
                if (rmsVel > 1e-9) {
                    const double limit = 3.0 * rmsVel;
                    QVector<int> keep;
                    keep.reserve(velIdx.size());
                    for (int k = 0; k < velIdx.size(); ++k)
                        if (residual.at(k) <= limit)
                            keep.append(velIdx.at(k));
                    if (keep.size() < velIdx.size() && keep.size() >= kMinSamples) {
                        std::vector<double> A2, Y2, sol2;
                        buildVel(keep, A2, Y2);
                        if (solveQrLstsq(A2, Y2, int(keep.size()) * 2, 2, sol2)) {
                            velRej = velIdx.size() - keep.size();
                            velIdx = keep;
                            sol = sol2;
                            velResid(velIdx, sol, residual);
                            rmsVel = rmsOf(residual);
                        }
                    }
                }

                r.velValid       = true;
                r.velLx          = sol[0];
                r.velLy          = sol[1];
                r.velResidualRms = rmsVel;

                // 杆臂标准差 σ²·(AᵀA)⁻¹（速度方程 2×2）
                double ata[2][2] = {};
                for (int i : velIdx) {
                    const Sample &s = m_samples.at(i);
                    const double w  = s.wz;
                    const double c  = std::cos(s.yawRad);
                    const double si = std::sin(s.yawRad);
                    const double r0[2] = {-w * si, -w * c};
                    const double r1[2] = { w * c,  -w * si};
                    for (int a = 0; a < 2; ++a)
                        for (int b = 0; b < 2; ++b)
                            ata[a][b] += r0[a] * r0[b] + r1[a] * r1[b];
                }
                const double det = ata[0][0] * ata[1][1] - ata[0][1] * ata[1][0];
                if (std::fabs(det) > 1e-18) {
                    r.velStdLx = std::sqrt(std::max(0.0, ata[1][1] / det * rmsVel * rmsVel));
                    r.velStdLy = std::sqrt(std::max(0.0, ata[0][0] / det * rmsVel * rmsVel));
                }

                double speedSum = 0.0;
                for (int i : velIdx)
                    speedSum += std::hypot(m_samples.at(i).vn, m_samples.at(i).ve);
                r.meanSpeed = speedSum / double(velIdx.size());
            }
        }
    }

    // ══════════ 取所选方法的结果 ══════════
    const bool usePos = (method == Method::Position);
    if (usePos) {
        if (!r.posValid) {
            r.reason = QStringLiteral("位置方程无解（样本退化）");
            return r;
        }
        r.lx          = r.posLx;
        r.ly          = r.posLy;
        r.residualRms = r.posResidualRms;
        r.leverStdLx  = r.posStdLx;
        r.leverStdLy  = r.posStdLy;
        r.usedFrames     = posIdx.size();
        r.rejectedFrames = posRej;
    } else {
        if (!r.velValid) {
            r.reason = QStringLiteral("速度方程病态：转向不足或数据异常");
            return r;
        }
        r.lx          = r.velLx;
        r.ly          = r.velLy;
        r.residualRms = r.velResidualRms;
        r.leverStdLx  = r.velStdLx;
        r.leverStdLy  = r.velStdLy;
        r.usedFrames     = velIdx.size();
        r.rejectedFrames = velRej;
    }
    const QVector<int> &selIdx = usePos ? posIdx : velIdx;

    // ── 分段一致性：各段独立解出的杆臂应当接近 ──
    if (r.yawSpanDeg >= kFullSpanDeg && selIdx.size() >= 60) {
        const int segCount = std::min(4, std::max(2, int(r.yawSpanDeg / 360.0)));
        const int segLen   = int(selIdx.size()) / segCount;
        if (segLen >= 15) {
            double spread = 0.0;
            int segOk = 0;
            for (int k = 0; k < segCount; ++k) {
                const int s0 = k * segLen;
                const int s1 = (k == segCount - 1) ? selIdx.size() : (k + 1) * segLen;
                QVector<int> sub;
                sub.reserve(s1 - s0);
                for (int i = s0; i < s1; ++i)
                    sub.append(selIdx.at(i));

                double aLx = 0.0, aLy = 0.0;
                if (usePos) {
                    double aCN = 0.0, aCE = 0.0;
                    QVector<double> dummy;
                    if (!solveLeverArm(m_samples, sub, aCN, aCE, aLx, aLy, dummy))
                        continue;
                    r.centerDrift = std::max(r.centerDrift,
                                             std::hypot(aCN - r.centerN,
                                                        aCE - r.centerE));
                } else {
                    std::vector<double> A2, Y2, sol2;
                    buildVel(sub, A2, Y2);
                    if (!solveQrLstsq(A2, Y2, int(sub.size()) * 2, 2, sol2))
                        continue;
                    aLx = sol2[0];
                    aLy = sol2[1];
                }
                ++segOk;
                spread = std::max(spread,
                                  std::hypot(aLx - r.lx, aLy - r.ly));
            }
            if (segOk >= 2) {
                r.leverChecked = true;
                r.segCount     = segOk;
                r.leverSpread  = spread;
                r.leverStable  = spread <= std::max(0.02, 0.1 * std::hypot(r.lx, r.ly));
            }
        }
    }

    // ── 质量判定 ──
    r.valid      = true;
    r.enoughTurn = r.yawSpanDeg >= kEnoughSpanDeg;
    r.fullTurn   = r.yawSpanDeg >= kFullSpanDeg;
    r.moving     = r.residualRms > kMovingRms;

    QStringList warn;
    if (r.leverChecked && !r.leverStable)
        warn << QStringLiteral("各段杆臂不一致（最大差 %1 mm）")
                    .arg(r.leverSpread * 1000.0, 0, 'f', 0);
    if (r.moving)
        warn << (usePos ? QStringLiteral("位置残差偏大，车体在移动")
                        : QStringLiteral("速度残差偏大，车体可能在移动"));
    else if (!r.enoughTurn)
        warn << QStringLiteral("建议再转 %1 圈")
                    .arg((kEnoughSpanDeg - r.yawSpanDeg) / 360.0, 0, 'f', 2);
    if (!usePos && r.speedGate < 0.20)
        warn << QStringLiteral("转速偏慢（平均 %1 m/s），杆臂会偏小；"
                               "建议天线速度 ≥0.2 m/s 再转一圈")
                    .arg(r.meanSpeed, 0, 'f', 2);
    if (r.totalFrames > 0
        && double(r.fixedFrames) / double(r.totalFrames) < kFixRatioWarn) {
        warn << QStringLiteral("非固定解帧较多，结果可能不稳");
    }
    r.reason = warn.join(QStringLiteral("；"));
    return r;
}

// ════════════════════════════════════════════════════════════════
//  通用 Householder QR 最小二乘
//  a: rows×cols 列主序；y: 右端项；输出 x: cols 个未知量
//  与 MATLAB 的 A\Y 同一算法（QR 分解，不做正规方程的平方条件数）
// ════════════════════════════════════════════════════════════════
bool LeverArmEstimator::solveQrLstsq(std::vector<double> &a, std::vector<double> &y,
                                     int rows, int cols, std::vector<double> &x)
{
    std::vector<double> hv(static_cast<std::size_t>(rows));
    for (int k = 0; k < cols; ++k) {
        double norm = 0.0;
        for (int i = k; i < rows; ++i) {
            const double v = a[static_cast<std::size_t>(i) * cols + k];
            norm += v * v;
        }
        norm = std::sqrt(norm);
        if (!(norm > 0.0))
            return false;

        const double akk = a[static_cast<std::size_t>(k) * cols + k];
        const double alpha = (akk >= 0.0) ? -norm : norm;   // 取反避免相消
        const double v0 = akk - alpha;
        double vnorm2 = v0 * v0;
        hv[0] = v0;
        for (int i = k + 1; i < rows; ++i) {
            const double v = a[static_cast<std::size_t>(i) * cols + k];
            hv[static_cast<std::size_t>(i - k)] = v;
            vnorm2 += v * v;
        }
        if (!(vnorm2 > 0.0))
            continue;
        const double beta = 2.0 / vnorm2;

        for (int j = k; j < cols; ++j) {
            double acc = 0.0;
            for (int i = k; i < rows; ++i)
                acc += hv[static_cast<std::size_t>(i - k)]
                       * a[static_cast<std::size_t>(i) * cols + j];
            acc *= beta;
            for (int i = k; i < rows; ++i)
                a[static_cast<std::size_t>(i) * cols + j] -=
                    acc * hv[static_cast<std::size_t>(i - k)];
        }
        double acc = 0.0;
        for (int i = k; i < rows; ++i)
            acc += hv[static_cast<std::size_t>(i - k)] * y[static_cast<std::size_t>(i)];
        acc *= beta;
        for (int i = k; i < rows; ++i)
            y[static_cast<std::size_t>(i)] -= acc * hv[static_cast<std::size_t>(i - k)];
    }

    x.assign(static_cast<std::size_t>(cols), 0.0);
    for (int i = cols - 1; i >= 0; --i) {
        double acc = y[static_cast<std::size_t>(i)];
        for (int j = i + 1; j < cols; ++j)
            acc -= a[static_cast<std::size_t>(i) * cols + j]
                   * x[static_cast<std::size_t>(j)];
        const double diag = a[static_cast<std::size_t>(i) * cols + i];
        if (std::fabs(diag) < 1e-300)
            return false;   // 秩亏：航向几乎没变化
        x[static_cast<std::size_t>(i)] = acc / diag;
    }
    return true;
}

bool LeverArmEstimator::solveLeverArm(const QVector<Sample> &samples,
                                      const QVector<int> &used,
                                      double &centerN, double &centerE,
                                      double &lx, double &ly,
                                      QVector<double> &residualDist)
{
    const int n = used.size();
    if (n < 4)
        return false;

    // 去中心化：绝对坐标可能是万米级，减去均值后与杆臂同量级，
    // 避免 dN/dE 的小量被大数吃掉；圆心最后再加回均值
    double meanN = 0.0, meanE = 0.0;
    for (int i : used) {
        meanN += samples.at(i).n;
        meanE += samples.at(i).e;
    }
    meanN /= double(n);
    meanE /= double(n);

    // 与 MATLAB calibrate_lever_arm.m 的 coef = A\Y 严格对齐：
    // 构造 2n×4 设计矩阵，用 Householder QR 求最小二乘解。
    const int rows = 2 * n;
    const int cols = 4;
    std::vector<double> aMat(static_cast<std::size_t>(rows) * cols);
    std::vector<double> rhs(static_cast<std::size_t>(rows));
    for (int k = 0; k < n; ++k) {
        const Sample &s = samples.at(used.at(k));
        const double c  = std::cos(s.yawRad);
        const double si = std::sin(s.yawRad);
        const std::size_t r0 = static_cast<std::size_t>(2 * k) * cols;
        const std::size_t r1 = r0 + cols;
        aMat[r0 + 0] = 1.0; aMat[r0 + 1] = 0.0; aMat[r0 + 2] =  c; aMat[r0 + 3] = -si;
        aMat[r1 + 0] = 0.0; aMat[r1 + 1] = 1.0; aMat[r1 + 2] =  si; aMat[r1 + 3] =  c;
        rhs[static_cast<std::size_t>(2 * k)]     = s.n - meanN;
        rhs[static_cast<std::size_t>(2 * k) + 1] = s.e - meanE;
    }

    std::vector<double> x;
    if (!solveQrLstsq(aMat, rhs, rows, cols, x))
        return false;

    centerN = x[0] + meanN;
    centerE = x[1] + meanE;
    lx      = x[2];
    ly      = x[3];

    // 残差：实测位置 − 模型预测位置
    residualDist.clear();
    residualDist.reserve(n);
    for (int i : used) {
        const Sample &s = samples.at(i);
        const double c  = std::cos(s.yawRad);
        const double si = std::sin(s.yawRad);
        const double pn = centerN + lx * c - ly * si;
        const double pe = centerE + lx * si + ly * c;
        residualDist.append(std::hypot(s.n - pn, s.e - pe));
    }
    return true;
}

QVector<QPointF> LeverArmEstimator::rawTrack(int maxPoints) const
{
    QVector<QPointF> pts;
    pts.reserve(m_samples.size());
    for (const Sample &s : m_samples)
        pts.append(QPointF(s.e, s.n));   // x=东向, y=北向
    return decimate(pts, maxPoints);
}

QVector<QPointF> LeverArmEstimator::projectedTrack(double lx, double ly, int maxPoints) const
{
    QVector<QPointF> pts;
    pts.reserve(m_samples.size());
    for (const Sample &s : m_samples) {
        const double c  = std::cos(s.yawRad);
        const double si = std::sin(s.yawRad);
        pts.append(QPointF(s.e - (lx * si + ly * c),
                           s.n - (lx * c  - ly * si)));
    }
    return decimate(pts, maxPoints);
}

QVector<QPointF> LeverArmEstimator::decimate(const QVector<QPointF> &pts, int maxPoints)
{
    if (maxPoints <= 0 || pts.size() <= maxPoints)
        return pts;

    QVector<QPointF> out;
    out.reserve(maxPoints);
    const double step = double(pts.size() - 1) / double(maxPoints - 1);
    for (int k = 0; k < maxPoints; ++k)
        out.append(pts.at(int(k * step)));
    out.last() = pts.last();
    return out;
}
