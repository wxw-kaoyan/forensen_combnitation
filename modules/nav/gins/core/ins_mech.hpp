// ============================================================================
//  nav/gins/core/ins_mech.hpp
//  INS 机械编排：速度 / 位置 / 姿态更新
//
//  来源：KF-GINS (i2Nav Group, Wuhan University), GPL-3.0
//  重构：改用 nav::ImuSample / nav::PVA；加 namespace；
//        全部补偿项原样保留（二阶圆锥、二阶划桨、旋转效应、哥氏、重力、地球自转）
// ============================================================================
#ifndef NAV_GINS_INS_MECH_HPP
#define NAV_GINS_INS_MECH_HPP

#include "../../common/nav_types.hpp"

namespace nav {
namespace gins {

class InsMech {
public:
    // 依次进行速度更新、位置更新、姿态更新（顺序不可调换）
    // 前提：dtheta / dvel 为增量（已乘 dt），相邻采样间隔均匀
    static void insMech(const PVA &pvapre, PVA &pvacur,
                        const ImuSample &imupre, const ImuSample &imucur);

private:
    static void velUpdate(const PVA &pvapre, PVA &pvacur,
                          const ImuSample &imupre, const ImuSample &imucur);
    static void posUpdate(const PVA &pvapre, PVA &pvacur,
                          const ImuSample &imupre, const ImuSample &imucur);
    static void attUpdate(const PVA &pvapre, PVA &pvacur,
                          const ImuSample &imupre, const ImuSample &imucur);
};

} // namespace gins
} // namespace nav

#endif // NAV_GINS_INS_MECH_HPP
