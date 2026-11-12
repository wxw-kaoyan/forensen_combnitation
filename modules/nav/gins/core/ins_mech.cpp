// ============================================================================
//  nav/gins/core/ins_mech.cpp
//  INS 机械编排实现
//
//  来源：KF-GINS (i2Nav Group, Wuhan University), GPL-3.0
//  算法逐行保留，仅做接口与命名空间适配
// ============================================================================
#include "ins_mech.hpp"

#include <cmath>

#include "../../common/earth.hpp"
#include "../../common/rotation.hpp"

namespace nav {
namespace gins {

void InsMech::insMech(const PVA &pvapre, PVA &pvacur,
                      const ImuSample &imupre, const ImuSample &imucur)
{
    // 依次进行速度更新、位置更新、姿态更新，不可调换顺序
    velUpdate(pvapre, pvacur, imupre, imucur);
    posUpdate(pvapre, pvacur, imupre, imucur);
    attUpdate(pvapre, pvacur, imupre, imucur);
}

void InsMech::velUpdate(const PVA &pvapre, PVA &pvacur,
                        const ImuSample &imupre, const ImuSample &imucur)
{
    Vector3d d_vfb, d_vfn, d_vgn, gl, midvel, midpos;
    Vector3d temp1, temp2, temp3;
    Matrix3d cnn;
    const Matrix3d I33 = Matrix3d::Identity();
    Quaterniond qne, qee, qnn;

    // 地理参数：子午/卯酉圈半径、地球自转投影、运输率、重力
    Eigen::Vector2d rmrn = Earth::meridianPrimeVerticalRadius(pvapre.pos(0));
    Vector3d wie_n, wen_n;
    wie_n << WGS84_WIE * std::cos(pvapre.pos[0]), 0.0, -WGS84_WIE * std::sin(pvapre.pos[0]);
    wen_n << pvapre.vel[1] / (rmrn[1] + pvapre.pos[2]),
            -pvapre.vel[0] / (rmrn[0] + pvapre.pos[2]),
            -pvapre.vel[1] * std::tan(pvapre.pos[0]) / (rmrn[1] + pvapre.pos[2]);
    const double gravity = Earth::gravity(pvapre.pos);

    // 旋转效应 + 双子样划桨效应（基于相同采样间隔推导）
    temp1 = imucur.dtheta.cross(imucur.dvel) / 2.0;
    temp2 = imupre.dtheta.cross(imucur.dvel) / 12.0;
    temp3 = imupre.dvel.cross(imucur.dtheta) / 12.0;

    // b 系比力积分项
    d_vfb = imucur.dvel + temp1 + temp2 + temp3;

    // 比力积分项投影到 n 系
    temp1 = (wie_n + wen_n) * imucur.dt / 2.0;
    cnn   = I33 - Rotation::skewSymmetric(temp1);
    d_vfn = cnn * pvapre.att.cbn * d_vfb;

    // 重力 / 哥氏力积分项
    gl << 0.0, 0.0, gravity;
    d_vgn = (gl - (2.0 * wie_n + wen_n).cross(pvapre.vel)) * imucur.dt;

    // 中间时刻速度
    midvel = pvapre.vel + (d_vfn + d_vgn) / 2.0;

    // 外推到中间时刻位置
    qnn = Rotation::rotvec2quaternion(temp1);
    temp2 << 0.0, 0.0, -WGS84_WIE * imucur.dt / 2.0;
    qee = Rotation::rotvec2quaternion(temp2);
    qne = Earth::qne(pvapre.pos);
    qne = (qee * qne * qnn).normalized();
    midpos[2] = pvapre.pos[2] - midvel[2] * imucur.dt / 2.0;
    midpos    = Earth::blh(qne, midpos[2]);

    // 在中间时刻重算地理参数与比力投影（中点法二次迭代，提高精度）
    rmrn = Earth::meridianPrimeVerticalRadius(midpos[0]);
    wie_n << WGS84_WIE * std::cos(midpos[0]), 0.0, -WGS84_WIE * std::sin(midpos[0]);
    wen_n << midvel[1] / (rmrn[1] + midpos[2]),
            -midvel[0] / (rmrn[0] + midpos[2]),
            -midvel[1] * std::tan(midpos[0]) / (rmrn[1] + midpos[2]);

    temp3 = (wie_n + wen_n) * imucur.dt / 2.0;
    cnn   = I33 - Rotation::skewSymmetric(temp3);
    d_vfn = cnn * pvapre.att.cbn * d_vfb;

    gl << 0.0, 0.0, Earth::gravity(midpos);
    d_vgn = (gl - (2.0 * wie_n + wen_n).cross(midvel)) * imucur.dt;

    pvacur.vel = pvapre.vel + d_vfn + d_vgn;
}

void InsMech::posUpdate(const PVA &pvapre, PVA &pvacur,
                        const ImuSample &imupre, const ImuSample &imucur)
{
    (void)imupre;   // 位置更新不使用 imupre

    Vector3d temp1, temp2, midvel, midpos;
    Quaterniond qne, qee, qnn;

    // 中间时刻速度与位置
    midvel = (pvacur.vel + pvapre.vel) / 2.0;
    midpos = pvapre.pos + Earth::DRi(pvapre.pos) * midvel * imucur.dt / 2.0;

    // 中间时刻地理参数
    Eigen::Vector2d rmrn = Earth::meridianPrimeVerticalRadius(midpos[0]);
    Vector3d wie_n, wen_n;
    wie_n << WGS84_WIE * std::cos(midpos[0]), 0.0, -WGS84_WIE * std::sin(midpos[0]);
    wen_n << midvel[1] / (rmrn[1] + midpos[2]),
            -midvel[0] / (rmrn[0] + midpos[2]),
            -midvel[1] * std::tan(midpos[0]) / (rmrn[1] + midpos[2]);

    // n 系 k-1 -> k 的旋转矢量
    temp1 = (wie_n + wen_n) * imucur.dt;
    qnn   = Rotation::rotvec2quaternion(temp1);
    // e 系转动等效旋转矢量（k-1 相对 k，取负号）
    temp2 << 0.0, 0.0, -WGS84_WIE * imucur.dt;
    qee = Rotation::rotvec2quaternion(temp2);

    qne           = (qee * Earth::qne(pvapre.pos) * qnn).normalized();
    pvacur.pos[2] = pvapre.pos[2] - midvel[2] * imucur.dt;
    pvacur.pos    = Earth::blh(qne, pvacur.pos[2]);
}

void InsMech::attUpdate(const PVA &pvapre, PVA &pvacur,
                        const ImuSample &imupre, const ImuSample &imucur)
{
    Quaterniond qne_pre, qne_cur, qne_mid, qnn, qee, qbb;
    Vector3d temp1, midpos, midvel;

    // 中间时刻位置：用 qne 的中值构造，避免经纬度直接插值的非线性误差
    midvel  = (pvapre.vel + pvacur.vel) / 2.0;
    qne_pre = Earth::qne(pvapre.pos);
    qne_cur = Earth::qne(pvacur.pos);
    qee     = Rotation::rotvec2quaternion(Vector3d(0.0, 0.0, -WGS84_WIE * imucur.dt));
    temp1   = Rotation::quaternion2vector((qne_pre.inverse() * qee.inverse() * qne_cur).normalized());
    qee     = Rotation::rotvec2quaternion(Vector3d(0.0, 0.0, -WGS84_WIE * imucur.dt / 2.0));
    qne_mid = (qee * qne_pre * Rotation::rotvec2quaternion(temp1 / 2.0)).normalized();
    midpos[2] = (pvacur.pos[2] + pvapre.pos[2]) / 2.0;
    midpos    = Earth::blh(qne_mid, midpos[2]);

    // 中间时刻地理参数
    Eigen::Vector2d rmrn = Earth::meridianPrimeVerticalRadius(midpos[0]);
    Vector3d wie_n, wen_n;
    wie_n << WGS84_WIE * std::cos(midpos[0]), 0.0, -WGS84_WIE * std::sin(midpos[0]);
    wen_n << midvel[1] / (rmrn[1] + midpos[2]),
            -midvel[0] / (rmrn[0] + midpos[2]),
            -midvel[1] * std::tan(midpos[0]) / (rmrn[1] + midpos[2]);

    // n 系旋转四元数
    temp1 = -(wie_n + wen_n) * imucur.dt;
    qnn   = Rotation::rotvec2quaternion(temp1);

    // b 系旋转四元数，补偿二阶圆锥误差（均匀采样假设）
    //   Δθ_k = θ_k + 1/12 · θ_{k-1} × θ_k
    // 与 AHRS 模块共用 Rotation::coningDeltaQuaternion
    qbb = Rotation::coningDeltaQuaternion(imupre.dtheta, imucur.dtheta);

    pvacur.att.qbn   = (qnn * pvapre.att.qbn * qbb).normalized();
    pvacur.att.cbn   = Rotation::quaternion2matrix(pvacur.att.qbn);
    pvacur.att.euler = Rotation::matrix2euler(pvacur.att.cbn);
}

} // namespace gins
} // namespace nav
