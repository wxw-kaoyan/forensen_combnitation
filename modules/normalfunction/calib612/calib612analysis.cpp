#include "calib612analysis.h"

#include <algorithm>
#include <cmath>

namespace {

// ── 判据阈值（与 MATLAB 版一致）──
constexpr double kZuptStdThr  = 0.3;   // deg/s，1 s 窗三轴 std 的模
constexpr double kZuptMeanThr = 5.0;   // deg/s，1 s 窗三轴均值的模
constexpr double kAccelTolG   = 0.1;   // g，静态姿态需 |a∓1g| < 0.1
constexpr double kRotStdThr   = 0.2;   // deg/s，1 s 窗 ‖ω‖ 的 std
constexpr double kRotTolRate  = 10.0;  // deg/s，‖ω‖ 与标定速率之差
constexpr int    kAccelRounds = 2;     // 加计每个方向采集两段（12 位置）
constexpr double kPi          = 3.14159265358979323846;

inline double norm3(const double v[3])
{
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

inline double norm3(const QVector<QVector<double>> &cols, int i)
{
    const double v[3] = {cols[3][i], cols[4][i], cols[5][i]};
    return norm3(v);
}

// NaveGo euler2dcm：[roll pitch yaw] (rad) → C3*(C2*C1)
void euler2dcm(const double e[3], double C[3][3])
{
    const double cp = std::cos(e[0]), sp = std::sin(e[0]);
    const double ct = std::cos(e[1]), st = std::sin(e[1]);
    const double cy = std::cos(e[2]), sy = std::sin(e[2]);

    const double C1[3][3] = {{ cy,  sy, 0.0},
                             {-sy,  cy, 0.0},
                             {0.0, 0.0, 1.0}};
    const double C2[3][3] = {{ ct, 0.0, -st},
                             {0.0, 1.0, 0.0},
                             { st, 0.0,  ct}};
    const double C3[3][3] = {{1.0, 0.0, 0.0},
                             {0.0,  cp,  sp},
                             {0.0, -sp,  cp}};

    double T[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            T[i][j] = C2[i][0] * C1[0][j] + C2[i][1] * C1[1][j] + C2[i][2] * C1[2][j];

    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            C[i][j] = C3[i][0] * T[0][j] + C3[i][1] * T[1][j] + C3[i][2] * T[2][j];
}

// 1 s 滑窗静态判定（陀螺 std/mean）
QVector<quint8> computeZupt(const QVector<QVector<double>> &cols, int W, int N)
{
    QVector<quint8> flag(N, 0);
    const int n = W + 1;
    double s[3] = {0, 0, 0};
    double ss[3] = {0, 0, 0};

    for (int i = 0; i < N; ++i) {
        const double g[3] = {cols[3][i], cols[4][i], cols[5][i]};
        for (int k = 0; k < 3; ++k) { s[k] += g[k]; ss[k] += g[k] * g[k]; }

        if (i - n >= 0) {
            const double o[3] = {cols[3][i - n], cols[4][i - n], cols[5][i - n]};
            for (int k = 0; k < 3; ++k) { s[k] -= o[k]; ss[k] -= o[k] * o[k]; }
        }

        if (i >= W) {
            double mean[3], sd[3];
            for (int k = 0; k < 3; ++k) {
                mean[k] = s[k] / n;
                const double var = (ss[k] - s[k] * s[k] / n) / (n - 1);
                sd[k] = std::sqrt(std::max(0.0, var));
            }
            flag[i] = (norm3(sd) < kZuptStdThr && norm3(mean) < kZuptMeanThr) ? 1 : 0;
        }
    }
    return flag;
}

// 1 s 滑窗稳定转动判定
QVector<quint8> computeRotate(const QVector<QVector<double>> &cols, int W, int N,
                              double gyroRate)
{
    QVector<quint8> flag(N, 0);
    const int n = W + 1;
    double sw = 0.0, sww = 0.0;
    int stableCount = 0;

    for (int i = 0; i < N; ++i) {
        const double w = norm3(cols, i);
        sw += w;
        sww += w * w;

        if (i - n >= 0) {
            const double ow = norm3(cols, i - n);
            sw -= ow;
            sww -= ow * ow;
        }

        if (i >= W) {
            const double meanw = sw / n;
            const double varw = (sww - sw * sw / n) / (n - 1);
            const double sdw = std::sqrt(std::max(0.0, varw));
            if (sdw < kRotStdThr && std::fabs(w - gyroRate) < kRotTolRate) {
                if (++stableCount > W)
                    flag[i] = 1;
            } else {
                stableCount = 0;
            }
        }
    }
    return flag;
}

Calib612Analysis::Segment makeSegment(int dir, const QVector<QVector<double>> &cols,
                                      int axisBase, int start, int len)
{
    Calib612Analysis::Segment seg;
    seg.dir         = dir;
    seg.startIndex  = start;
    seg.sampleCount = len;
    for (int k = 0; k < 3; ++k) {
        double sum = 0.0;
        const QVector<double> &v = cols[axisBase + k];
        for (int i = start; i < start + len; ++i)
            sum += v[i];
        seg.mean[k] = sum / len;
    }
    return seg;
}

int countDir(const QVector<Calib612Analysis::Segment> &segs, int dir)
{
    int n = 0;
    for (const auto &s : segs)
        if (s.dir == dir) ++n;
    return n;
}

// 加计 12 位置：与 accel_calib.m 的状态机一致
QVector<Calib612Analysis::Segment> detectAccel(const QVector<QVector<double>> &cols,
                                               const QVector<quint8> &zupt,
                                               int W, int segN, int N)
{
    QVector<Calib612Analysis::Segment> segs;
    int flag[6] = {0, 0, 0, 0, 0, 0};
    bool first6 = false;
    int i = 0;

    while (i < N) {
        if (!zupt[i]) {
            // 六个方向都采集到后，等下一次“动起来”再开始第二轮（与 MATLAB 一致）
            if (flag[0] == 1 && flag[1] == 1 && flag[2] == 1 &&
                flag[3] == 1 && flag[4] == 1 && flag[5] == 1) {
                first6 = true;
                i += 3 * W;
            }
            ++i;
            continue;
        }

        for (int d = 0; d < 6; ++d) {
            const int axis = d / 2;
            const double target = (d % 2 == 0) ? 1.0 : -1.0;
            if (std::fabs(cols[axis][i] - target) >= kAccelTolG)
                continue;
            if (i + segN > N)
                break;

            if (flag[d] == 0) {
                segs.append(makeSegment(d, cols, 0, i, segN));
                flag[d] = 1;
                i += segN;
                break;
            }
            if (first6 && flag[d] == 1) {
                bool allStatic = true;
                for (int j = i; j < i + segN; ++j) {
                    if (!zupt[j]) { allStatic = false; break; }
                }
                if (!allStatic)
                    continue;
                segs.append(makeSegment(d, cols, 0, i, segN));
                flag[d] = 2;
                i += segN;
                break;
            }
        }
        ++i;
    }
    return segs;
}

// 陀螺 6 位置：与 gyro_calib.m 的状态机一致
QVector<Calib612Analysis::Segment> detectGyro(const QVector<QVector<double>> &cols,
                                              const QVector<quint8> &rot,
                                              int segN, int N, double gyroRate)
{
    QVector<Calib612Analysis::Segment> segs;
    int flag[6] = {0, 0, 0, 0, 0, 0};
    int i = 0;

    while (i < N) {
        if (!rot[i]) { ++i; continue; }

        for (int d = 0; d < 6; ++d) {
            const int axis = d / 2;
            const double target = (d % 2 == 0) ? gyroRate : -gyroRate;
            if (flag[d] == 0 && std::fabs(cols[3 + axis][i] - target) < kRotTolRate) {
                if (i + segN > N)
                    break;
                segs.append(makeSegment(d, cols, 3, i, segN));
                flag[d] = 1;
                i += segN;
                break;
            }
        }
        ++i;
    }
    return segs;
}

// M = L·Aᵀ·(A·Aᵀ)⁻¹，A 行结构固定 ⇒ A·Aᵀ = diag(2s²,2s²,2s²,6)
void solveM(const double L[3][6], double s, double M[3][4])
{
    for (int r = 0; r < 3; ++r) {
        M[r][0] = (L[r][0] - L[r][1]) / (2.0 * s);
        M[r][1] = (L[r][2] - L[r][3]) / (2.0 * s);
        M[r][2] = (L[r][4] - L[r][5]) / (2.0 * s);
        M[r][3] = (L[r][0] + L[r][1] + L[r][2] + L[r][3] + L[r][4] + L[r][5]) / 6.0;
    }
}

// 把 6 个方向的段均值平均成 3×6 的 L（行=测量轴，列=方向）
bool buildL(const QVector<Calib612Analysis::Segment> &segs, double L[3][6])
{
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 6; ++c)
            L[r][c] = 0.0;

    for (int d = 0; d < 6; ++d) {
        const int cnt = countDir(segs, d);
        if (cnt == 0)
            return false;
        for (const auto &s : segs) {
            if (s.dir != d) continue;
            for (int k = 0; k < 3; ++k)
                L[k][d] += s.mean[k];
        }
        for (int k = 0; k < 3; ++k)
            L[k][d] /= cnt;
    }
    return true;
}

// 非正交角（rad）：± 两方向交叉耦合的平均
void nonOrthoAngles(const double L[3][6], double scale, double e[3])
{
    e[0] = (L[1][5] - L[1][4] + L[2][2] - L[2][3]) / (4.0 * scale);  // alpha
    e[1] = (L[0][4] - L[0][5] + L[2][1] - L[2][0]) / (4.0 * scale);  // beta
    e[2] = (L[0][3] - L[0][2] + L[1][0] - L[1][1]) / (4.0 * scale);  // gamma
}

double maxNonOrthoDeg(const double M[3][4], const double e[3])
{
    double C[3][3];
    euler2dcm(e, C);

    // S_T = C * M(:,1:3)
    double ST[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            ST[i][j] = C[i][0] * M[0][j] + C[i][1] * M[1][j] + C[i][2] * M[2][j];

    const double off[6] = {ST[0][1], ST[0][2], ST[1][0],
                           ST[1][2], ST[2][0], ST[2][1]};
    double m = 0.0;
    for (double v : off)
        m = std::max(m, std::fabs(v));
    m = std::min(1.0, std::max(-1.0, m));
    return std::asin(m) / kPi * 180.0;
}

void appendMissingNotes(QStringList &notes, const QVector<Calib612Analysis::Segment> &segs,
                        bool accel)
{
    const QString prefix = accel ? QStringLiteral("加计") : QStringLiteral("陀螺");
    for (int d = 0; d < 6; ++d) {
        const int cnt = countDir(segs, d);
        if (cnt == 0) {
            notes << (accel
                      ? QStringLiteral("%1 %2 位置未检出（需要该轴 |a∓1 g|<0.1 的静止段）")
                            .arg(prefix, Calib612Analysis::dirLabel(d))
                      : QStringLiteral("%1 %2 位置未检出（需要 ‖ω‖≈标定速率、稳定旋转 >1 s）")
                            .arg(prefix, Calib612Analysis::dirLabel(d)));
        } else if (accel && cnt < kAccelRounds) {
            notes << QStringLiteral("%1 %2 只检出 %3 段（12 位置应为 2 段，按已有段计算）")
                     .arg(prefix, Calib612Analysis::dirLabel(d)).arg(cnt);
        }
    }
}

} // namespace

QString Calib612Analysis::dirLabel(int dir)
{
    static const char *kNames[6] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
    if (dir < 0 || dir > 5) return QStringLiteral("?");
    return QString::fromLatin1(kNames[dir]);
}

Calib612Analysis::Result Calib612Analysis::compute(const QVector<QVector<double>> &cols,
                                                   double fs, double gyroRate,
                                                   double segmentSeconds)
{
    Result r;
    r.fs = fs;
    r.gyroRate = gyroRate;

    if (cols.size() != 6) {
        r.notes << QStringLiteral("需要 6 列数据（ax ay az gx gy gz），当前 %1 列").arg(cols.size());
        return r;
    }

    const int N = cols[0].size();
    for (int c = 1; c < 6; ++c) {
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

    const int W    = std::max(1, static_cast<int>(std::lround(fs)));                 // 1 s 窗
    const int segN = std::max(2, static_cast<int>(std::lround(fs * segmentSeconds))); // 每段点数
    r.segmentSamples = segN;

    if (N < W + segN + 1) {
        r.notes << QStringLiteral("数据太短（%1 点），至少需要 %2 点").arg(N).arg(W + segN + 1);
        return r;
    }

    // ── 加计 12 位置 ──
    const QVector<quint8> zupt = computeZupt(cols, W, N);
    r.accelSegments = detectAccel(cols, zupt, W, segN, N);

    double L[3][6];
    if (buildL(r.accelSegments, L)) {
        double M[3][4];
        solveM(L, 1.0, M);                      // 加计以 g 为单位，标称值 s = 1 g
        double e[3];
        nonOrthoAngles(L, 1.0, e);
        for (int k = 0; k < 3; ++k) {
            r.accelScale[k]   = (M[k][k] - 1.0) * 1000.0;   // 千分比
            r.accelBias[k]    = M[k][3] * 1000.0;           // mg
            r.accelInstall[k] = e[k] / kPi * 180.0;        // deg
        }
        r.accelMaxNonOrtho = maxNonOrthoDeg(M, e);
        r.accelOk = true;
    }
    appendMissingNotes(r.notes, r.accelSegments, true);

    // ── 陀螺 6 位置 ──
    const QVector<quint8> rot = computeRotate(cols, W, N, gyroRate);
    r.gyroSegments = detectGyro(cols, rot, segN, N, gyroRate);

    if (buildL(r.gyroSegments, L)) {
        double M[3][4];
        solveM(L, gyroRate, M);                 // 陀螺以 deg/s 为单位
        double e[3];
        nonOrthoAngles(L, gyroRate, e);
        for (int k = 0; k < 3; ++k) {
            r.gyroScale[k] = (M[k][k] - 1.0) * 1000.0;      // 千分比
            r.gyroBias[k]  = M[k][3];                       // deg/s
        }
        r.gyroMaxNonOrtho = maxNonOrthoDeg(M, e);
        r.gyroOk = true;
    }
    appendMissingNotes(r.notes, r.gyroSegments, false);

    return r;
}
