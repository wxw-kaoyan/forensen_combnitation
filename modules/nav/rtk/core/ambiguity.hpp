// ============================================================================
//  nav/rtk/core/ambiguity.hpp
//  LAMBDA / MLAMBDA 整数最小二乘模糊度搜索
//
//  来源：RTKLIB lambda.c（T.TAKASU），逐行对照移植，不做算法改动
//  参考：
//    [1] P.J.G.Teunissen, J.Geodesy, Vol.70, 65-82, 1995
//    [2] X.-W.Chang et al., J.Geodesy, Vol.79, 552-565, 2005
// ============================================================================
#ifndef NAV_RTK_AMBIGUITY_HPP
#define NAV_RTK_AMBIGUITY_HPP

#include <Eigen/Dense>

namespace nav {
namespace rtk {

struct LambdaResult {
    bool     ok = false;
    int      nCand = 0;              // 有效候选数
    Eigen::VectorXd fixed0;          // 最优整数解
    Eigen::VectorXd fixed1;          // 次优整数解
    double   s0 = 0.0;               // 最优解的平方残差和
    double   s1 = 0.0;               // 次优解的平方残差和
    double   ratio = 0.0;            // s1 / s0
};

// 整数最小二乘
//   n : 浮点模糊度个数
//   m : 需要的候选解个数（≥2 才能算 ratio）
//   a : 浮点解 (n)
//   Q : 浮点解协方差 (n×n)
LambdaResult lambda(int n, int m, const Eigen::VectorXd &a,
                    const Eigen::MatrixXd &Q);

} // namespace rtk
} // namespace nav

#endif // NAV_RTK_AMBIGUITY_HPP
