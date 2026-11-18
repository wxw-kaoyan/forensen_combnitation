// ============================================================================
//  nav/gins/core/ins_mech.cpp
//  INS 机械编排（Strapdown Inertial Navigation Mechanization）
//
//  来源：KF-GINS (i2Nav Group, Wuhan University), GPL-3.0
//  算法逐行保留，仅做接口与命名空间适配
//
//  ── 机械编排是干什么的 ──
//  输入：上一时刻的姿态/速度/位置 + 这一采样周期内 IMU 输出的角增量 Δθ、速度增量 Δv
//  输出：这一时刻的姿态/速度/位置
//  它是惯导的"物理引擎"，负责把 IMU 的增量积分成轨迹。
//
//  ── 它与滤波器的关系（最容易误解的地方）──
//  本文件里确实没有一行滤波逻辑，但这不代表滤波器不影响轨迹。
//  gins_engine 在调用 insMech 前后各做了一件事：
//      调用前 imuCompensate()：用当前零偏/刻度估计先扣掉 IMU 增量的偏差
//      调用后 stateFeedback()：把 EKF 估出的位置/速度/姿态误差回灌到状态
//  也就是说：
//      · 零偏估计 → 影响本函数的输入（增量是否干净）
//      · 误差反馈 → 直接改写本函数的输出（pvacur_ 被拉回）
//  准确的说法是：本文件只负责"给定 IMU 增量，积分出轨迹"；
//  增量干不干净、算完要不要修正，由调用方 gins_engine 决定。
//
//  另注意：EKF 的"预测"传播的是协方差（不确定性），
//  而标称状态的数值推进由下面的机械编排完成，两者是不同层次。
//
//  ── 三个更新的顺序不能换 ──
//      velUpdate → posUpdate → attUpdate
//  位置更新要用刚算出的新速度；姿态更新要用新的位置和速度去算中间时刻的
//  地球参数。所以速度必须最先、位置其次、姿态最后。
//
//  ── 坐标系约定 ──
//      n 系：导航系 NED（北-东-地），z 轴指向地心方向（向下为正）
//      b 系：机体系 前-右-下，与 IMU 三个敏感轴对齐
//      e 系：地心地固系 ECEF
//      cbn ：b 系 → n 系的旋转矩阵，对应四元数 qbn
//      qne ：n 系 → e 系的旋转四元数，由经纬度唯一确定
//
//  ── 为什么每一步都要"中间时刻" ──
//  IMU 输出的是整个采样周期的积分量，但地球自转、运输率、重力在周期内
//  都在变化（随纬度/高度/速度变）。用周期起点或终点的值都会引入偏差，
//  所以先用一次近似外推到中点，在中点重算所有地理参数，再算第二步。
//  这是二阶精度（中点法），也是 P 型（速度）误差能控制在 mm/s 级的前提。
// ============================================================================
#include "ins_mech.hpp"

#include <cmath>

#include "../../common/earth.hpp"
#include "../../common/rotation.hpp"

namespace nav {
namespace gins {

// ════════════════════════════════════════════════════════════════════
//  机械编排总入口
//  注意 pvapre 是 const，pvacur 是输出；三个子函数内部是"读前写后"，
//  即读 pvapre 的旧值、写 pvacur 的新值，不会互相覆盖。
// ════════════════════════════════════════════════════════════════════
void InsMech::insMech(const PVA &pvapre, PVA &pvacur,
                      const ImuSample &imupre, const ImuSample &imucur)
{
    // 依次进行速度更新、位置更新、姿态更新，不可调换顺序
    velUpdate(pvapre, pvacur, imupre, imucur);
    posUpdate(pvapre, pvacur, imupre, imucur);
    attUpdate(pvapre, pvacur, imupre, imucur);
}

// ════════════════════════════════════════════════════════════════════
//  速度更新
//
//  比力方程（n 系）：
//      dv^n/dt = C_b^n · f^b  −  (2·ω_ie^n + ω_en^n) × v^n  +  g^n
//                └─ 比力项 ─┘    └──── 哥氏 + 向心 ────┘    └重力┘
//
//  本函数就是把这个微分方程用一阶积分推进一个采样周期。
//  比力项里还要额外补偿两个"采样周期内的几何误差"：旋转效应和划桨效应。
// ════════════════════════════════════════════════════════════════════
void InsMech::velUpdate(const PVA &pvapre, PVA &pvacur,
                        const ImuSample &imupre, const ImuSample &imucur)
{
    Vector3d d_vfb, d_vfn, d_vgn, gl, midvel, midpos;
    Vector3d temp1, temp2, temp3;
    Matrix3d cnn;
    const Matrix3d I33 = Matrix3d::Identity();
    Quaterniond qne, qee, qnn;

    // ── ① 取上一时刻纬度下的地球曲率半径 ──
    //  rmrn[0] = 子午圈曲率半径 R_M（南北方向的曲率）
    //  rmrn[1] = 卯酉圈曲率半径 R_N（东西方向的曲率）
    //  两者随纬度变化，赤道与极地相差约 21 km，不能当常数处理。
    Eigen::Vector2d rmrn = Earth::meridianPrimeVerticalRadius(pvapre.pos(0));

    // ── ② 两个必须扣掉的"有害角速度"（都投影到 n 系）──
    //  wie_n：地球自转角速度在 n 系的分量
    //         北向 = ω·cosφ，天向 = −ω·sinφ（z 轴朝下，故取负号）
    //         物理含义：载体停在地球上不动，也会因为地球在转而"看到"这个转动
    //  wen_n：运输率，载体在地球表面移动引起的 n 系相对 e 系的转动
    //         东向速度使 n 系绕北轴转 → 北向分量 v_E/(R_N+h)
    //         北向速度使 n 系绕东轴转 → 东向分量 −v_N/(R_M+h)
    //         东向速度在高纬度还会产生天向分量 −v_E·tanφ/(R_N+h)
    //         物理含义：车往北开，n 系的"北"方向本身在漂移
    Vector3d wie_n, wen_n;
    wie_n << WGS84_WIE * std::cos(pvapre.pos[0]), 0.0, -WGS84_WIE * std::sin(pvapre.pos[0]);
    wen_n << pvapre.vel[1] / (rmrn[1] + pvapre.pos[2]),
            -pvapre.vel[0] / (rmrn[0] + pvapre.pos[2]),
            -pvapre.vel[1] * std::tan(pvapre.pos[0]) / (rmrn[1] + pvapre.pos[2]);

    const double gravity = Earth::gravity(pvapre.pos);

    // ── ③ 采样周期内的几何误差补偿 ──
    //  两个采样周期（pre 和 cur）组成一个"双子样"，用来补偿高频误差：
    //
    //  temp1：旋转效应（rotation effect）
    //    周期内载体的转动会让加速度在 b 系中的方向连续变化，
    //    一阶近似后多出半个 Δθ×Δv，除以 2。
    //
    //  temp2 / temp3：划桨效应（sculling effect）
    //    载体同时做"转动"和"振动/加速"时，两种运动的耦合会产生
    //    一个虚假的速度增量，方向垂直于两者所在平面。
    //    双子样补偿公式： (Δθ₁×Δv₂ + Δv₁×Δθ₂) / 12
    temp1 = imucur.dtheta.cross(imucur.dvel) / 2.0;
    temp2 = imupre.dtheta.cross(imucur.dvel) / 12.0;
    temp3 = imupre.dvel.cross(imucur.dtheta) / 12.0;

    // ── ④ b 系下的比力速度增量 ──
    //  即 ∫f^b dt，加上上面的几何补偿项
    d_vfb = imucur.dvel + temp1 + temp2 + temp3;

    // ── ⑤ 第一次投影：用上一时刻的姿态和半周期的 n 系转动做近似 ──
    //  temp1 取半个周期的 (ω_ie + ω_en)·dt/2，构造 n 系转过的小角度
    //  cnn = I − [temp1×] 是一阶近似：n 系在积分区间内转过 θ，
    //  用它把 b 系积分量校正到 n 系，抵消"投影时刻不一致"带来的方向偏差。
    temp1 = (wie_n + wen_n) * imucur.dt / 2.0;
    cnn   = I33 - Rotation::skewSymmetric(temp1);
    d_vfn = cnn * pvapre.att.cbn * d_vfb;

    // ── ⑥ 重力 / 哥氏力 / 向心力积分项（用周期起点的速度）──
    //  gl：重力矢量，n 系下只有天向分量（向下为正）
    //  (2·ω_ie + ω_en) × v 就是比力方程里的有害加速度项
    gl << 0.0, 0.0, gravity;
    d_vgn = (gl - (2.0 * wie_n + wen_n).cross(pvapre.vel)) * imucur.dt;

    // ── ⑦ 算出"中间时刻"的速度，为二次迭代做准备 ──
    midvel = pvapre.vel + (d_vfn + d_vgn) / 2.0;

    // ── ⑧ 把位置外推到中间时刻 ──
    //  qnn：n 系在半个周期内转过的角度（四元数）
    //  qee：e 系在半个周期内转过的角度（地球自转，只有 z 轴分量）
    //  qne = qee · qne_pre · qnn：先把 n 系随载体转到新朝向，再叠加地球自转
    //  这样得到的 qne 对应"半个周期后的 n 系"，再由它反算经纬度。
    //  为什么要绕这一圈：直接对经纬度做线性插值，在高纬度/极区误差很大。
    qnn = Rotation::rotvec2quaternion(temp1);
    temp2 << 0.0, 0.0, -WGS84_WIE * imucur.dt / 2.0;
    qee = Rotation::rotvec2quaternion(temp2);
    qne = Earth::qne(pvapre.pos);
    qne = (qee * qne * qnn).normalized();

    //  天向单独积分：h 向下为正，载体上升时 h 减小
    midpos[2] = pvapre.pos[2] - midvel[2] * imucur.dt / 2.0;
    midpos    = Earth::blh(qne, midpos[2]);   // 由 qne 反解纬度/经度，并塞回高度

    // ── ⑨ 在中点重新计算全部地理参数，做第二次（也是最终）计算 ──
    //  第一次算的时候用的是起点纬度和起点速度，中点法把它修正为
    //  "区间中点处的值"，误差从 O(dt) 降到 O(dt²)。
    rmrn = Earth::meridianPrimeVerticalRadius(midpos[0]);
    wie_n << WGS84_WIE * std::cos(midpos[0]), 0.0, -WGS84_WIE * std::sin(midpos[0]);
    wen_n << midvel[1] / (rmrn[1] + midpos[2]),
            -midvel[0] / (rmrn[0] + midpos[2]),
            -midvel[1] * std::tan(midpos[0]) / (rmrn[1] + midpos[2]);

    //  用中点处的 n 系转动重新投影比力项
    temp3 = (wie_n + wen_n) * imucur.dt / 2.0;
    cnn   = I33 - Rotation::skewSymmetric(temp3);
    d_vfn = cnn * pvapre.att.cbn * d_vfb;

    //  用中点处的重力、哥氏项、以及中点速度重新积分
    gl << 0.0, 0.0, Earth::gravity(midpos);
    d_vgn = (gl - (2.0 * wie_n + wen_n).cross(midvel)) * imucur.dt;

    // ── ⑩ 得到本周期结束时的速度 ──
    pvacur.vel = pvapre.vel + d_vfn + d_vgn;
}

// ════════════════════════════════════════════════════════════════════
//  位置更新
//
//  位置方程：dp/dt = D_R⁻¹ · v^n
//  其中 D_R 把经纬高变化率（rad/s, rad/s, m/s）映射成 NED 速度（m/s）。
//  D_R⁻¹ 是它的逆，把速度映射回经纬高变化率。
//
//  但这里没有直接积分经纬度，而是走"四元数"这条路，原因见下面注释。
// ════════════════════════════════════════════════════════════════════
void InsMech::posUpdate(const PVA &pvapre, PVA &pvacur,
                        const ImuSample &imupre, const ImuSample &imucur)
{
    (void)imupre;   // 位置更新不使用 imupre

    Vector3d temp1, temp2, midvel, midpos;
    Quaterniond qne, qee, qnn;

    // ── ① 梯形积分：用周期首尾速度的平均值作为等效速度 ──
    midvel = (pvacur.vel + pvapre.vel) / 2.0;

    // ── ② 先把位置外推到中间时刻（数值上仍是 /2）──
    //  DRi 是 D_R⁻¹，把 m/s 的速度转成 rad/s 的经纬度变化率
    midpos = pvapre.pos + Earth::DRi(pvapre.pos) * midvel * imucur.dt / 2.0;

    // ── ③ 中点处重算运输率 ──
    Eigen::Vector2d rmrn = Earth::meridianPrimeVerticalRadius(midpos[0]);
    Vector3d wie_n, wen_n;
    wie_n << WGS84_WIE * std::cos(midpos[0]), 0.0, -WGS84_WIE * std::sin(midpos[0]);
    wen_n << midvel[1] / (rmrn[1] + midpos[2]),
            -midvel[0] / (rmrn[0] + midpos[2]),
            -midvel[1] * std::tan(midpos[0]) / (rmrn[1] + midpos[2]);

    // ── ④ 构造两个"整周期"的转动四元数 ──
    //  temp1 = 整个周期 n 系的转动角度；(wie+wen)·dt
    //  qnn   = 该角度对应的四元数
    //  temp2 = 整个周期地球自转的角度，注意取负号
    //    因为 qee 是 e→n 方向的变化，而地球实际是绕 z 轴正向转，
    //    在 NED（z 向下）里表现为 −ω·dt
    temp1 = (wie_n + wen_n) * imucur.dt;
    qnn   = Rotation::rotvec2quaternion(temp1);
    temp2 << 0.0, 0.0, -WGS84_WIE * imucur.dt;
    qee = Rotation::rotvec2quaternion(temp2);

    // ── ⑤ 用四元数链式更新 n→e 的朝向 ──
    //  qne_new = qee · qne_old · qnn
    //  右乘 qnn：n 系随载体移动而转动
    //  左乘 qee：叠加上地球自身的自转
    //  ⚠ 为什么不直接积分经纬度：
    //     经度在极区会剧烈变化（甚至不连续），tanφ 在 φ→90° 时发散；
    //     四元数没有奇异点，全球通用。
    qne           = (qee * Earth::qne(pvapre.pos) * qnn).normalized();

    // ── ⑥ 高度单独积分，再由 qne 反解经纬度 ──
    //  高度相对独立，直接标量积分即可；h 向下为正，故用减号
    pvacur.pos[2] = pvapre.pos[2] - midvel[2] * imucur.dt;
    pvacur.pos    = Earth::blh(qne, pvacur.pos[2]);
}

// ════════════════════════════════════════════════════════════════════
//  姿态更新
//
//  姿态方程：q̇_b^n = 0.5 · q_b^n ⊗ ω_nb^b
//  离散化后的等价形式（乘性更新）：
//      q_b^n(k) = q_n(k-1)^n(k) ⊗ q_b^n(k-1) ⊗ q_b(k-1)^b(k)
//
//  三个因子：
//      qnn —— n 系自己在区间内的转动（左乘）
//      qbn —— 上一时刻的姿态
//      qbb —— b 系相对 b 系的转动，即 IMU 测到的角增量（右乘）
//
//  为什么左乘 qnn、右乘 qbb：矩阵/四元数连乘的顺序就是"先作用谁"。
//      右乘 qbb：在 b 系内部先转，是载体自身的转动（body frame）
//      左乘 qnn：再整体把结果放到新的 n 系里去（navigation frame）
//  顺序搞反会导致姿态在高动态下明显漂移。
// ════════════════════════════════════════════════════════════════════
void InsMech::attUpdate(const PVA &pvapre, PVA &pvacur,
                        const ImuSample &imupre, const ImuSample &imucur)
{
    Quaterniond qne_pre, qne_cur, qne_mid, qnn, qee, qbb;
    Vector3d temp1, midpos, midvel;

    // ── ① 构造"中间时刻"的 qne ──
    //  这里不用 (pre + cur)/2 插值经纬度，而是用四元数求两个朝向之间的
    //  测地线中点，避免经纬度插值在高纬度的非线性误差。
    midvel  = (pvapre.vel + pvacur.vel) / 2.0;
    qne_pre = Earth::qne(pvapre.pos);
    qne_cur = Earth::qne(pvacur.pos);

    //  整个周期地球自转的四元数
    qee     = Rotation::rotvec2quaternion(Vector3d(0.0, 0.0, -WGS84_WIE * imucur.dt));

    //  求 n 系在这一个周期内的净转动（已扣掉地球自转）：
    //      qne_cur = qee · qne_pre · qnn
    //  反解：qnn = qne_pre⁻¹ · qee⁻¹ · qne_cur
    //  quaternion2vector 把"四元数形式的转动"变回"旋转矢量"，
    //  以便后面取一半做中点。
    temp1   = Rotation::quaternion2vector((qne_pre.inverse() * qee.inverse() * qne_cur).normalized());

    //  用半周期地球自转 + 半个 net 转动，拼出中点朝向
    qee     = Rotation::rotvec2quaternion(Vector3d(0.0, 0.0, -WGS84_WIE * imucur.dt / 2.0));
    qne_mid = (qee * qne_pre * Rotation::rotvec2quaternion(temp1 / 2.0)).normalized();

    //  高度取首尾平均，再反解中点经纬度
    midpos[2] = (pvacur.pos[2] + pvapre.pos[2]) / 2.0;
    midpos    = Earth::blh(qne_mid, midpos[2]);

    // ── ② 中点处的地理参数 ──
    Eigen::Vector2d rmrn = Earth::meridianPrimeVerticalRadius(midpos[0]);
    Vector3d wie_n, wen_n;
    wie_n << WGS84_WIE * std::cos(midpos[0]), 0.0, -WGS84_WIE * std::sin(midpos[0]);
    wen_n << midvel[1] / (rmrn[1] + midpos[2]),
            -midvel[0] / (rmrn[0] + midpos[2]),
            -midvel[1] * std::tan(midpos[0]) / (rmrn[1] + midpos[2]);

    // ── ③ n 系在整周期内的转动 ──
    //  取负号：qnn 描述的是 n(k-1) → n(k)，而姿态更新需要的是把旧的
    //  姿态"搬运"到新 n 系，两者方向相反。
    temp1 = -(wie_n + wen_n) * imucur.dt;
    qnn   = Rotation::rotvec2quaternion(temp1);

    // ── ④ b 系转动 + 圆锥误差补偿 ──
    //  IMU 测到的是两个采样周期的角增量。如果载体在做高频角振动，
    //  两个方向不同的 Δθ 先后作用，不能简单相加——这就是圆锥误差。
    //  coningDeltaQuaternion 做二阶补偿：
    //      Δθ_k = θ_k + 1/12 · θ_{k-1} × θ_k
    //  与 AHRS 模块共用同一份实现，保证两个模块输出的姿态一致。
    qbb = Rotation::coningDeltaQuaternion(imupre.dtheta, imucur.dtheta);

    // ── ⑤ 乘性更新 ──
    //  qbn_new = qnn ⊗ qbn_old ⊗ qbb
    //  左乘 qnn 进新导航系，右乘 qbb 叠加机体自身转动
    pvacur.att.qbn   = (qnn * pvapre.att.qbn * qbb).normalized();

    // ── ⑥ 同步维护矩阵和欧拉角 ──
    //  cbn 供 F 矩阵和观测方程用；euler 只是给人看的输出，
    //  内部递推一律用四元数，避免欧拉角奇异。
    pvacur.att.cbn   = Rotation::quaternion2matrix(pvacur.att.qbn);
    pvacur.att.euler = Rotation::matrix2euler(pvacur.att.cbn);
}

} // namespace gins
} // namespace nav
