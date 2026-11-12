// ============================================================================
//  nav/rtk/core/rtk_engine.hpp
//  RTK 主引擎：双差 EKF + 模糊度固定 + 主调度
//
//  状态：x = [ p_ecef(3)  v_ecef(3)  N_sd(sat,f) ]
//        N 为单差模糊度（周），双差在观测方程中形成
//        维度随可见卫星数在运行期变化（每历元重建）
//
//  来源：RTKLIB rtkpos.c 的 relpos / zdres / ddres / udstate（裁剪分支）
// ============================================================================
#ifndef NAV_RTK_RTK_ENGINE_HPP
#define NAV_RTK_RTK_ENGINE_HPP

#include <Eigen/Dense>
#include <map>
#include <utility>
#include <vector>

#include "rtk_types.hpp"

namespace nav {
namespace rtk {

class RtkEngine {
public:
    explicit RtkEngine(const RtkConfig &cfg = RtkConfig());

    // 已知基站坐标（ECEF）。不设置时用基站的伪距单点解
    void setBasePosition(const Vector3d &baseEcef);
    bool basePositionKnown() const { return baseKnown_; }
    Vector3d basePosition() const { return basePos_; }

    // 处理一个历元：流动站观测 + 基站观测 + 星历
    // 返回本历元解（无解时 stat = SOLQ_NONE）
    Sol processEpoch(const std::vector<Obs> &roverObs,
                     const std::vector<Obs> &baseObs,
                     const Nav &nav);

    // 只跑单点（无基站数据时的降级路径）
    Sol processSingle(const std::vector<Obs> &roverObs, const Nav &nav);

    // 结果与调试
    const Sol &solution() const { return sol_; }
    const Eigen::VectorXd &state() const { return x_; }
    const Eigen::MatrixXd &covariance() const { return P_; }
    const std::vector<SatStat> &satStats() const { return ssat_; }
    double ratio() const { return sol_.ratio; }

    void reset();

private:
    // 单差模糊度键：(sat, freq)
    using AmbKey = std::pair<int, int>;

    // 按本历元可见卫星重建状态向量，保留已有模糊度
    void syncAmbiguities(const std::vector<int> &visibleSats,
                         const std::vector<double> &initCycle,
                         int nf);

    void temporalUpdate(double dt);

    int  ambIndex(int sat, int f) const {
        auto it = ambIdx_.find(AmbKey(sat, f));
        return (it == ambIdx_.end()) ? -1 : it->second;
    }

private:
    RtkConfig cfg_;

    bool     baseKnown_ = false;
    Vector3d basePos_   = Vector3d::Zero();

    // 状态
    Eigen::VectorXd x_;     // 6 + nN
    Eigen::MatrixXd P_;     // (6+nN) × (6+nN)
    std::map<AmbKey, int> ambIdx_;
    std::vector<AmbKey>   ambKeys_;

    GTime           lastTime_;
    bool            hasLast_ = false;
    Vector3d        lastPos_ = Vector3d::Zero();

    Sol             sol_;
    std::vector<SatStat> ssat_;

    // 上一历元的模糊度备份（重建状态时按 key 迁移）
    std::map<AmbKey, double>      ambValue_;
    std::map<AmbKey, double>      ambVar_;
};

} // namespace rtk
} // namespace nav

#endif // NAV_RTK_RTK_ENGINE_HPP
