#include "leverarmestimator.h"

#include <QStringList>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kPi        = 3.14159265358979323846;
constexpr double kDeg2Rad   = kPi / 180.0;
constexpr double kRad2Deg   = 180.0 / kPi;
constexpr double kMetersPerDeg = 111320.0;   // 纬度 1° 对应的米数

constexpr int    kMinSamples    = 30;    // 少于此帧数不出解
constexpr double kMinSpanDeg    = 30.0;  // 航向跨度太小则方程病态
constexpr double kEnoughSpanDeg = 180.0; // 参考解 / 可用解分界
constexpr double kFullSpanDeg   = 360.0; // 满一圈
constexpr double kMovingRms     = 0.05;  // 残差 >5cm 认为车体在移动
constexpr double kFixRatioWarn  = 0.90;  // 固定解占比低于 90% 给提示

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

    Sample s;
    s.n = (lat - m_lat0) * kMetersPerDeg;
    s.e = (lon - m_lon0) * kMetersPerDeg * std::cos(m_lat0 * kDeg2Rad);
    s.yawRad = yawRaw;
    m_samples.append(s);

    const qint64 ms = qint64(f.gpsWeek) * 604800000LL + qint64(f.gpsWeekSecond);
    if (m_firstMs < 0)
        m_firstMs = ms;
    m_lastMs = ms;
}

LeverArmEstimator::Result LeverArmEstimator::solve() const
{
    Result r;
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

    QVector<int> used;
    used.reserve(m_samples.size());
    for (int i = 0; i < m_samples.size(); ++i)
        used.append(i);

    QVector<double> residual;
    if (!solveLeverArm(m_samples, used, r.centerN, r.centerE, r.lx, r.ly, residual)) {
        r.reason = QStringLiteral("方程病态：转向不足或数据异常");
        return r;
    }

    double rms = 0.0;
    for (double d : residual)
        rms += d * d;
    rms = std::sqrt(rms / double(residual.size()));

    // ── 3σ 野值剔除，只做一轮：坏帧（丢帧/重定位跳变）会把圆拟合拉偏 ──
    if (rms > 1e-9 && residual.size() == used.size()) {
        const double limit = 3.0 * rms;
        QVector<int> keep;
        keep.reserve(used.size());
        for (int k = 0; k < used.size(); ++k) {
            if (residual.at(k) <= limit)
                keep.append(used.at(k));
        }
        if (!keep.isEmpty() && keep.size() < used.size() && keep.size() >= kMinSamples) {
            QVector<double> residual2;
            double cN = 0.0, cE = 0.0, ax = 0.0, ay = 0.0;
            if (solveLeverArm(m_samples, keep, cN, cE, ax, ay, residual2)) {
                r.rejectedFrames = used.size() - keep.size();
                used = keep;
                residual = residual2;
                r.centerN = cN;
                r.centerE = cE;
                r.lx = ax;
                r.ly = ay;
                rms = 0.0;
                for (double d : residual)
                    rms += d * d;
                rms = std::sqrt(rms / double(residual.size()));
            }
        }
    }

    // ── 统计量 ──
    r.usedFrames   = used.size();
    r.residualRms  = rms;

    double meanN = 0.0, meanE = 0.0;
    for (int i : used) {
        meanN += m_samples.at(i).n;
        meanE += m_samples.at(i).e;
    }
    meanN /= double(used.size());
    meanE /= double(used.size());

    double varN = 0.0, varE = 0.0;
    for (int i : used) {
        const double dn = m_samples.at(i).n - meanN;
        const double de = m_samples.at(i).e - meanE;
        varN += dn * dn;
        varE += de * de;
    }
    r.srcSpreadN = std::sqrt(varN / double(used.size()));
    r.srcSpreadE = std::sqrt(varE / double(used.size()));

    // 补偿后点云：把每帧的杆臂减掉，正常应收缩成一个点
    double sumPN = 0.0, sumPE = 0.0;
    QVector<QPointF> proj;
    proj.reserve(used.size());
    for (int i : used) {
        const Sample &s = m_samples.at(i);
        const double c  = std::cos(s.yawRad);
        const double si = std::sin(s.yawRad);
        const double x = s.e - (r.lx * si + r.ly * c);
        const double y = s.n - (r.lx * c  - r.ly * si);
        proj.append(QPointF(x, y));
        sumPN += y;
        sumPE += x;
    }
    sumPN /= double(proj.size());
    sumPE /= double(proj.size());
    double varP = 0.0;
    for (const QPointF &p : proj) {
        const double dx = p.x() - sumPE;
        const double dy = p.y() - sumPN;
        varP += dx * dx + dy * dy;
    }
    r.projSpreadRms = std::sqrt(varP / double(proj.size()));

    // ── 质量判定 ──
    r.valid      = true;
    r.enoughTurn = r.yawSpanDeg >= kEnoughSpanDeg;
    r.fullTurn   = r.yawSpanDeg >= kFullSpanDeg;
    r.moving     = rms > kMovingRms;

    QStringList warn;
    if (r.moving)
        warn << QStringLiteral("残差偏大，车体可能在移动");
    else if (!r.enoughTurn)
        warn << QStringLiteral("建议再转 %1 圈")
                    .arg((kEnoughSpanDeg - r.yawSpanDeg) / 360.0, 0, 'f', 2);
    if (r.totalFrames > 0
        && double(r.fixedFrames) / double(r.totalFrames) < kFixRatioWarn) {
        warn << QStringLiteral("非固定解帧较多，结果可能不稳");
    }
    r.reason = warn.join(QStringLiteral("；"));
    return r;
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

    // 正规方程 (AᵀA)·x = Aᵀ·y，只有 4 个未知量
    double ata[4][4] = {};
    double aty[4]    = {};
    for (int i : used) {
        const Sample &s = samples.at(i);
        const double c  = std::cos(s.yawRad);
        const double si = std::sin(s.yawRad);
        const double yN = s.n - meanN;
        const double yE = s.e - meanE;

        const double rowN[4] = {1.0, 0.0,  c, -si};
        const double rowE[4] = {0.0, 1.0,  si,  c};
        for (int a = 0; a < 4; ++a) {
            for (int b = 0; b < 4; ++b)
                ata[a][b] += rowN[a] * rowN[b] + rowE[a] * rowE[b];
            aty[a] += rowN[a] * yN + rowE[a] * yE;
        }
    }

    // 4×4 高斯-约当消元（部分主元）
    double m[4][5];
    double maxAbs = 0.0;
    for (int a = 0; a < 4; ++a) {
        for (int b = 0; b < 4; ++b) {
            m[a][b] = ata[a][b];
            maxAbs = std::max(maxAbs, std::fabs(m[a][b]));
        }
        m[a][4] = aty[a];
    }
    if (maxAbs <= 0.0)
        return false;

    for (int col = 0; col < 4; ++col) {
        int piv = col;
        for (int row = col + 1; row < 4; ++row) {
            if (std::fabs(m[row][col]) > std::fabs(m[piv][col]))
                piv = row;
        }
        if (std::fabs(m[piv][col]) < maxAbs * 1e-12)
            return false;   // 病态：航向几乎没有变化
        if (piv != col) {
            for (int k = col; k < 5; ++k)
                std::swap(m[piv][k], m[col][k]);
        }
        const double d = m[col][col];
        for (int k = col; k < 5; ++k)
            m[col][k] /= d;
        for (int row = 0; row < 4; ++row) {
            if (row == col)
                continue;
            const double f = m[row][col];
            if (f == 0.0)
                continue;
            for (int k = col; k < 5; ++k)
                m[row][k] -= f * m[col][k];
        }
    }

    centerN = m[0][4] + meanN;
    centerE = m[1][4] + meanE;
    lx      = m[2][4];
    ly      = m[3][4];

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
