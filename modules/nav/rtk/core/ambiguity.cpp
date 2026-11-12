// ============================================================================
//  nav/rtk/core/ambiguity.cpp
//  LAMBDA 实现（LD 分解 → 整数高斯变换 → 置换 → 归约 → MLAMBDA 搜索）
//
//  说明：算法与 RTKLIB lambda.c 完全一致，只把列主序的裸指针换成 Eigen。
//        RTKLIB 的 L[i+j*n] 即 Eigen 的 L(i,j)。
// ============================================================================
#include "ambiguity.hpp"

#include <cmath>
#include <vector>

namespace nav {
namespace rtk {

namespace {

constexpr int LOOPMAX = 10000;

inline double sgn(double x)   { return x <= 0.0 ? -1.0 : 1.0; }
inline double roundd(double x) { return std::floor(x + 0.5); }

// LD 分解 Q = L' * diag(D) * L
bool ldFactor(int n, const Eigen::MatrixXd &Q,
              Eigen::MatrixXd &L, Eigen::VectorXd &D)
{
    Eigen::MatrixXd A = Q;
    L = Eigen::MatrixXd::Zero(n, n);
    D = Eigen::VectorXd::Zero(n);

    for (int i = n - 1; i >= 0; --i) {
        D[i] = A(i, i);
        if (D[i] <= 0.0)
            return false;

        const double a = std::sqrt(D[i]);
        for (int j = 0; j <= i; ++j)
            L(i, j) = A(i, j) / a;
        for (int j = 0; j <= i - 1; ++j)
            for (int k = 0; k <= j; ++k)
                A(j, k) -= L(i, k) * L(i, j);
        for (int j = 0; j <= i; ++j)
            L(i, j) /= L(i, i);
    }
    return true;
}

// 整数高斯变换
void gauss(int n, Eigen::MatrixXd &L, Eigen::MatrixXd &Z, int i, int j)
{
    const int mu = static_cast<int>(roundd(L(i, j)));
    if (mu == 0)
        return;

    for (int k = i; k < n; ++k)
        L(k, j) -= static_cast<double>(mu) * L(k, i);
    for (int k = 0; k < n; ++k)
        Z(k, j) -= static_cast<double>(mu) * Z(k, i);
}

// 置换
void perm(int n, Eigen::MatrixXd &L, Eigen::VectorXd &D, int j, double del,
          Eigen::MatrixXd &Z)
{
    const double eta = D[j] / del;
    const double lam = D[j + 1] * L(j + 1, j) / del;

    D[j]     = eta * D[j + 1];
    D[j + 1] = del;

    for (int k = 0; k <= j - 1; ++k) {
        const double a0 = L(j, k);
        const double a1 = L(j + 1, k);
        L(j, k)     = -L(j + 1, j) * a0 + a1;
        L(j + 1, k) = eta * a0 + lam * a1;
    }
    L(j + 1, j) = lam;

    for (int k = j + 2; k < n; ++k)
        std::swap(L(k, j), L(k, j + 1));
    for (int k = 0; k < n; ++k)
        std::swap(Z(k, j), Z(k, j + 1));
}

// Lambda 归约（z = Z'·a，Qz = Z'·Q·Z = L'·diag(D)·L）
void reduction(int n, Eigen::MatrixXd &L, Eigen::VectorXd &D, Eigen::MatrixXd &Z)
{
    int j = n - 2;
    int k = n - 2;

    while (j >= 0) {
        if (j <= k) {
            for (int i = j + 1; i < n; ++i)
                gauss(n, L, Z, i, j);
        }

        const double del = D[j] + L(j + 1, j) * L(j + 1, j) * D[j + 1];

        if (del + 1e-6 < D[j + 1]) {   // 数值误差容限
            perm(n, L, D, j, del, Z);
            k = j;
            j = n - 2;
        } else {
            --j;
        }
    }
}

// MLAMBDA 搜索
bool mlambdaSearch(int n, int m, const Eigen::MatrixXd &L, const Eigen::VectorXd &D,
                   const Eigen::VectorXd &zs, Eigen::MatrixXd &zn, Eigen::VectorXd &s)
{
    int nn = 0;
    int imax = 0;
    double maxdist = 1e99;

    Eigen::MatrixXd S    = Eigen::MatrixXd::Zero(n, n);
    Eigen::VectorXd dist = Eigen::VectorXd::Zero(n);
    Eigen::VectorXd zb   = Eigen::VectorXd::Zero(n);
    Eigen::VectorXd z    = Eigen::VectorXd::Zero(n);
    Eigen::VectorXd step = Eigen::VectorXd::Zero(n);

    int k = n - 1;
    dist[k] = 0.0;
    zb[k]   = zs[k];
    z[k]    = roundd(zb[k]);
    double y = zb[k] - z[k];
    step[k]  = sgn(y);

    int c = 0;
    for (; c < LOOPMAX; ++c) {
        const double newdist = dist[k] + y * y / D[k];

        if (newdist < maxdist) {
            if (k != 0) {
                --k;
                dist[k] = newdist;

                for (int i = 0; i <= k; ++i)
                    S(k, i) = S(k + 1, i) + (z[k + 1] - zb[k + 1]) * L(k + 1, i);

                zb[k]   = zs[k] + S(k, k);
                z[k]    = roundd(zb[k]);
                y       = zb[k] - z[k];
                step[k] = sgn(y);
            } else {
                if (nn < m) {
                    if (nn == 0 || newdist > s[imax])
                        imax = nn;
                    for (int i = 0; i < n; ++i)
                        zn(i, nn) = z[i];
                    s[nn++] = newdist;
                } else {
                    if (newdist < s[imax]) {
                        for (int i = 0; i < n; ++i)
                            zn(i, imax) = z[i];
                        s[imax] = newdist;
                        imax = 0;
                        for (int i = 0; i < m; ++i)
                            if (s[imax] < s[i])
                                imax = i;
                    }
                    maxdist = s[imax];
                }
                z[0] += step[0];
                y = zb[0] - z[0];
                step[0] = -step[0] - sgn(step[0]);
            }
        } else {
            if (k == n - 1) {
                break;
            } else {
                ++k;
                z[k] += step[k];
                y = zb[k] - z[k];
                step[k] = -step[k] - sgn(step[k]);
            }
        }
    }

    // 按平方残差升序排序
    for (int i = 0; i < m - 1; ++i) {
        for (int j = i + 1; j < m; ++j) {
            if (s[i] < s[j])
                continue;
            std::swap(s[i], s[j]);
            for (int kk = 0; kk < n; ++kk)
                std::swap(zn(kk, i), zn(kk, j));
        }
    }

    return c < LOOPMAX;
}

} // namespace

// ════════════════════════════════════════════════════════════════════
LambdaResult lambda(int n, int m, const Eigen::VectorXd &a, const Eigen::MatrixXd &Q)
{
    LambdaResult res;

    if (n <= 0 || m <= 0 || a.size() != n || Q.rows() != n || Q.cols() != n)
        return res;

    Eigen::MatrixXd L;
    Eigen::VectorXd D;

    if (!ldFactor(n, Q, L, D))
        return res;

    Eigen::MatrixXd Z = Eigen::MatrixXd::Identity(n, n);
    reduction(n, L, D, Z);

    Eigen::VectorXd z = Z.transpose() * a;      // z = Z'·a

    Eigen::MatrixXd E  = Eigen::MatrixXd::Zero(n, m);   // 候选整数解
    Eigen::VectorXd s  = Eigen::VectorXd::Zero(m);

    if (!mlambdaSearch(n, m, L, D, z, E, s))
        return res;

    // F = Z^-T · E
    Eigen::MatrixXd F = Z.transpose().partialPivLu().solve(E);

    res.ok     = true;
    res.nCand  = m;
    res.fixed0 = F.col(0);
    res.s0     = s[0];

    if (m >= 2) {
        res.fixed1 = F.col(1);
        res.s1     = s[1];
        // 最优解残差为 0（无噪声数据）时视为最高置信度，避免除数退化
        res.ratio  = (s[0] > 1e-9) ? s[1] / s[0] : 1e6;
    }
    return res;
}

} // namespace rtk
} // namespace nav
