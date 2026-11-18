# TASoftware 导航模块函数框架说明

> 路径：`forensen_combnitation/modules/nav/`
> 覆盖：`common/`、`gins/`、`ahrs/`、`rtk/` 四个自研模块
> 说明：本文档只描述**函数/类的职责、输入输出与调用关系**，不展开具体实现，便于快速建立代码地图。

---

## 0. 模块总览

```
modules/nav/
├── common/        # 基础库：角度、地球模型、旋转、共用类型    (无状态，纯工具)
├── ahrs/          # 姿态模块：四元数积分 + 6 维 EKF            (上电 yaw=0，相对航向)
├── gins/          # 松组合：INS 机械编排 + 21 维 EKF          (主定位链路)
├── rtk/           # RTK 定位：双差 EKF + LAMBDA 模糊度固定    (GNSS 精密定位)
└── third_party/   # Eigen 3.3.9（第三方，不在本文档范围）
```

**数据流方向**：

```
IMU 100Hz ──► GinsEngine.addImu() ──► step() ──► NavState (pos/vel/euler)
                       │
GNSS 10Hz ──► GinsEngine.addGnss()  (位置/速度/航向分别有效)
Odo  ───────► GinsEngine.addOdo()
Lidar ──────► GinsEngine.addLidar() (预留)

RTK 链路（独立于 GINS）：
RTCM/观测 ──► RtkEngine.processEpoch(rover, base, nav) ──► Sol (ECEF 位置)
                  └─ 内部调用 spp / ephemeris / atmosphere / lambda
```

**关键约定**：
- 所有 IMU 输入必须是**增量**（`dtheta`/`dvel` 已乘 `dt`），不是速率
- 角度单位：内部统一用 **rad**，输出欧拉角 `[roll, pitch, yaw]`，ZYX 顺序
- 位置：GINS 用 **BLH (rad, rad, m)**；RTK 用 **ECEF (m)**
- 命名空间：`nav::common` 类、`nav::gins`、`nav::ahrs`、`nav::rtk`

---

## 1. common/ — 基础库

### 1.1 `angle.hpp` — 角度常量与单位转换

| 符号 | 类型 | 职责 |
|------|------|------|
| `PI / D2R / R2D` | constexpr double | π、度→弧度、弧度→度 |
| `Angle::rad2deg / deg2rad` | 静态模板 | 标量/矩阵的弧度↔度互转（支持 float 和任意 Eigen 矩阵） |
| `normalizeAngle(angle)` | 自由函数 | 角度归一化到 `[-π, π)` |
| `normalizeHeading(heading)` | 自由函数 | 航向归一化到 `[0, 2π)` |

### 1.2 `nav_types.hpp` — 共用数据结构

| 结构 | 字段 | 说明 |
|------|------|------|
| `Attitude` | `qbn / cbn / euler` | 姿态的三种表示：四元数、方向余弦矩阵、欧拉角 |
| `PVA` | `pos / vel / att` | 位置(BLH)、速度(NED)、姿态的打包结构 |
| `Pose` | `R / t` | 局部坐标转换用的位姿（旋转 + 平移） |
| `ImuSample` | `time / dt / dtheta / dvel / temperature / valid` | 单周期 IMU 增量，`dtheta`/`dvel` 必须是增量 |
| `OdoSample` | `time / vel / std / valid` | 里程计前向速度观测 |

### 1.3 `earth.hpp` — WGS84 地球模型

`Earth` 类（全部静态方法）：

| 方法 | 输入 | 输出 | 职责 |
|------|------|------|------|
| `gravity(blh)` | BLH | `[m/s²]` | 正常重力（Torge 公式） |
| `meridianPrimeVerticalRadius(lat)` | lat | `[RM, RN]` | 子午圈/卯酉圈半径 |
| `RN(lat)` | lat | RN | 卯酉圈半径 |
| `cne(blh)` | BLH | 3×3 矩阵 | n 系→e 系 方向余弦矩阵 |
| `qne(blh)` | BLH | 四元数 | n 系→e 系 四元数 |
| `blh(qne, h)` | 四元数+高 | BLH | 由 n→e 四元数反算经纬度 |
| `blh2ecef(blh)` | BLH | ECEF | 大地→地心地固 |
| `ecef2blh(ecef)` | ECEF | BLH | 地心地固→大地（迭代） |
| `DRi(blh)` | BLH | 3×3 | n 系相对位置→大地坐标相对位置的变换矩阵 |
| `DR(blh)` | BLH | 3×3 | 大地坐标相对位置→n 系相对位置 |
| `local2global(origin, local/Pose)` | 原点+局部 | 大地 | 局部 ENU → 大地坐标（支持点和位姿两种重载） |
| `global2local(origin, global/Pose)` | 原点+大地 | 局部 | 大地 → 局部 ENU |
| `iewe()` | — | `[0,0,ωie]` | 地球自转角速度 e 系 |
| `iewn(lat)` | lat | 3×1 | 地球自转角速度 n 系 |
| `enwn(rmn, blh, vel)` | 半径+BLH+速度 | 3×1 | n 系相对 e 系转动角速度（运输率） |

### 1.4 `rotation.hpp` — 旋转表示转换

`Rotation` 类（全部静态方法）：

| 方法 | 职责 |
|------|------|
| `matrix2quaternion / quaternion2matrix` | DCM ↔ 四元数 |
| `matrix2euler(dcm)` | DCM → `[roll, pitch, yaw]`，ZYX 顺序；万向锁时 roll 置 0 静默处理；yaw 归一化到 `[0,2π)` |
| `quaternion2euler(q)` | 四元数 → 欧拉角 |
| `rotvec2quaternion(rotvec)` | 旋转矢量 → 四元数（零旋转返回单位四元数） |
| `coningDeltaQuaternion(dthetaPre, dthetaCur)` | **二阶圆锥补偿**后的 b 系姿态增量四元数，GINS 与 AHRS 共用 |
| `quaternion2vector(q)` | 四元数 → 旋转矢量 |
| `euler2matrix / euler2quaternion` | `[r,p,y]` → DCM / 四元数 |
| `skewSymmetric(v)` | 向量的反对称矩阵 |
| `quaternionleft(q) / quaternionright(p)` | 四元数左右乘矩阵（4×4） |

---

## 2. gins/ — 松组合导航

### 2.1 `gins_types.hpp` — 类型与配置

**状态维度**：基础 21 维，扩 SLAM 27 维（预留）。

| 结构/枚举 | 内容 |
|-----------|------|
| `ImuError` | `gyrbias / accbias / gyrscale / accscale`（零偏 + 刻度因数） |
| `NavState` | `pos / vel / euler / imuerror`（真值量，非误差量） |
| `ImuNoise` | `gyr_arw / acc_vrw / gyrbias_std / accbias_std / gyrscale_std / accscale_std / corr_time` |
| `StateID` | `P_ID(0) / V_ID(3) / PHI_ID(6) / BG_ID(9) / BA_ID(12) / SG_ID(15) / SA_ID(18)` + SLAM 扩维槽位 |
| `NoiseID` | 18 维噪声索引 |
| `MotionMode` | `UNKNOWN / STATIC / STRAIGHT / TURNING` |
| `GnssObservation` | `time / hasPos+blh+stdPos / hasVel+vel+stdVel / hasHeading+heading+stdHeading / isValid` — **位置/速度/航向可分别有效** |
| `LidarObservation` | 激光先验地图定位（预留） |
| `GinsConfig` | 状态维度、初始状态、IMU 噪声、杆臂、各观测/约束开关、ZUPT/ZARU/NHC 阈值、卡方门限、**卡方锁死逃逸参数** |
| `GinsDebug` | 每历元新息、卡方值、各观测是否被接受、运动模式、健康标志、累计计数 |

### 2.2 `ins_mech.hpp` — INS 机械编排

`InsMech` 类（全部静态）：

| 方法 | 职责 |
|------|------|
| `insMech(pvapre, pvacur, imupre, imucur)` | **主入口**：依次调用 速度→位置→姿态 更新（顺序不可调换） |
| `velUpdate(...)` | 速度更新：含二阶划桨补偿、旋转效应、哥氏、重力、地球自转 |
| `posUpdate(...)` | 位置更新：速度积分到 BLH |
| `attUpdate(...)` | 姿态更新：四元数 + 圆锥补偿 + 地球自转/运输率 |

### 2.3 `gins_engine.hpp` — 松组合 EKF 引擎

**`GinsEngine` 公开接口**：

| 方法 | 职责 |
|------|------|
| `GinsEngine(cfg)` | 构造，按配置初始化滤波器 |
| `addImu(imu)` | 注入 100 Hz IMU 增量 |
| `addGnss(obs)` | 注入 GNSS 观测（位置/速度/航向各自有效位） |
| `addOdo(odo)` | 注入轮速（`useOd` 打开时生效） |
| `addLidar(obs)` | 注入激光定位（预留） |
| `setStationary(on)` | 强制静止状态 |
| `setMotionMode(m)` | 强制运动模式 |
| `step()` | **推进一个 IMU 周期**：内部完成传播 + 所有观测更新 + 反馈 |
| `state()` | 返回 `NavState` |
| `covariance() / timestamp() / healthy() / debug() / attitude()` | 结果查询 |
| `reset(cfg)` | 用新配置重新初始化 |

**`GinsEngine` 私有流程**（被 `step()` 调用）：

```
step()
├─ imuCompensate(imu)            # 扣除零偏/刻度因数
├─ insPropagation(pre, cur)      # InsMech.insMech 传播 PVA
├─ EKFPredict(Phi, Qd)           # 误差状态预测 + 协方差传播
│
├─ 对队列中所有时间对齐的观测：
│   ├─ gnssUpdate(obs)           # 位置/速度/航向（带卡方门限）
│   ├─ lidarUpdate(obs)          # （预留）
│   ├─ odUpdate(odo)             # 轮速前向速度
│   └─ zuptZaruUpdate(imu)       # 静止时 ZUPT(3) + ZARU(3)
│   └─ nhcUpdate()               # 行驶时 NHC(2)
│
├─ stateFeedback()               # 误差状态反馈到真值
└─ checkCov()                    # 协方差对角线钳位防发散
```

| 私有方法 | 职责 |
|----------|------|
| `initialize(initstate, initstateStd)` | 由初始标准差构造协方差矩阵 |
| `imuCompensate(imu)` | 用当前估计的零偏/刻度修正 IMU 增量 |
| `isToUpdate(t1, t2, updatetime)` | 判断观测时间是否落在两个 IMU 历元之间（时间对齐容差 `timeAlignErr`） |
| `insPropagation(pre, cur)` | 调 `InsMech::insMech` 并构造状态转移矩阵 Phi |
| `gnssUpdate(obs)` | 位置/速度/航向观测，各走 `EKFUpdateGated` |
| `lidarUpdate(obs)` / `odUpdate(odo)` / `zuptZaruUpdate(imu)` / `nhcUpdate()` | 各类观测/约束更新 |
| `EKFPredict(Phi, Qd)` | 标准 EKF 预测：`P = Φ P Φᵀ + Q` |
| `EKFUpdate(dz, H, R)` | 标准 EKF 更新 |
| `EKFUpdateGated(dz, H, R, threshold, chi2Out)` | **带卡方门限的更新**；超门限则拒绝，连续拒绝 `maxRejectRun` 次后膨胀对应状态块协方差（防锁死） |
| `stateFeedback()` | 误差状态反馈到 PVA 和 ImuError |
| `checkCov()` | 对角线超过 `covMaxDiagonal` 则钳位并标记不健康 |
| `toNavState()` | 把内部 `pvacur_ + imuerror_` 打包成 `NavState` |

---

## 3. ahrs/ — 姿态模块

### 3.1 `ahrs_types.hpp` — 类型与配置

| 结构/枚举 | 内容 |
|-----------|------|
| `AhrsStateID` | `ATT_ID(0)` 姿态误差(3) + `GB_ID(3)` 陀螺零偏，共 6 维 |
| `AhrsConfig` | 初始姿态、加计水平对准开关、陀螺噪声、倾斜/ZARU 观测参数、地球自转补偿（默认关） |
| `AhrsDebug` | 倾斜新息、ZARU 是否应用、静止标志、当前零偏估计 |

### 3.2 `ahrs_engine.hpp` — 姿态引擎

**`AhrsEngine` 公开接口**：

| 方法 | 职责 |
|------|------|
| `AhrsEngine(cfg)` | 构造 |
| `addImu(imu)` | 注入 IMU 增量 |
| `step()` | 推进一个 IMU 周期：姿态积分 + 观测 |
| `attitude() / euler() / yaw()` | 查询姿态（四元数/欧拉角/航向） |
| `gyroBias() / covariance() / timestamp() / healthy() / debug()` | 状态查询 |
| `resetYaw(yawRad)` | 用外部航向（双天线/航迹）重置航向 |
| `resetYawToZero()` | 上电清零航向 |
| `alignLevelFromAccel()` | 用当前加计重新对准水平姿态 |
| `reset(cfg)` | 重新初始化 |

**`AhrsEngine` 私有流程**：

```
step()
├─ 圆锥补偿积分姿态 (Rotation::coningDeltaQuaternion)
├─ applyObservations(imuRaw, dthetaComp)
│   ├─ tiltUpdate(fBody)        # 加计辅助水平姿态(2维)，|‖f‖-g|<阈值才用
│   └─ zaruUpdate(omega)        # 静止时零角速率(3维)
├─ stateFeedback()
└─ checkCov()
```

---

## 4. rtk/ — RTK 定位

### 4.1 `rtk_types.hpp` — 常量与数据结构

| 类别 | 内容 |
|------|------|
| 物理常量 | `CLIGHT / FREQ1 / FREQ2 / LAM1 / LAM2 / MU_* / OMGE_* / RE_WGS84 / FE_WGS84 / HION` |
| 枚举 | `SatSys`（GPS/GLO/GAL/CMP/QZS/SBS 位标志）、`SolStat`（FIX/FLOAT/SBAS/DGPS/SINGLE）、`PosMode`、`IonoOpt`、`TropOpt` |
| `GTime` | GPS 周 + 周内秒 |
| `Eph` | 广播星历（16 个开普勒参数 + 钟差 f0/f1/f2 + 健康标志） |
| `Obs` | 单星观测：`L[2]` 载波(周)、`P[2]` 伪距(m)、`D[2]` 多普勒、`SNR[2]`、`LLI[2]` |
| `Nav` | 星历集合 + Klobuchar 参数 + 波长表；含 `updateWavelengths()` / `findEph()` |
| `Sol` | 解：`time / stat / ns / rr(ECEF) / qr / ratio / age` |
| `RtkConfig` | 定位模式、高度角门限、噪声模型、LAMBDA 比率门限、初始协方差等 |

### 4.2 `rtk_util.hpp` — 时间/坐标/几何工具

| 函数 | 职责 |
|------|------|
| `satsys(sat, &prn)` / `satName(sat)` / `satId(sys, prn)` | 卫星系统判定、名称、编号构造 |
| `timeAdd / timeDiff / time2gpst / gpst2time / epoch2time` | 时间系统运算（GPS 周+周内秒） |
| `ecef2pos / pos2ecef` | ECEF ↔ BLH |
| `ecef2enu / ecef2enuv / cofactor` | ECEF → ENU 位置/速度矢量/协方差 |
| `geodist(rs, rr, &e)` | 卫星到接收机几何距离（含地球自转/Sagnac 改正），输出视线单位矢量 |
| `satazel(pos, e, &azel)` | 由视线矢量算方位角、高度角 |
| `baseline(ru, rb, &dr)` | 基线向量与长度 |

### 4.3 `atmosphere.hpp` — 大气改正

| 函数 | 职责 |
|------|------|
| `kIonDefault[8]` | 默认 Klobuchar 参数（无广播时兜底） |
| `ionmodel(time, ion, pos, azel)` | Klobuchar 电离层延迟 [m]（L1） |
| `ionmapf(pos, azel)` | 电离层单层投影函数 |
| `tropmodel(time, pos, azel, humi=0.7)` | Saastamoinen 对流层延迟 [m] |

### 4.4 `ephemeris.hpp` — 星历解算

| 函数 | 职责 |
|------|------|
| `eph2clk(time, eph)` | 卫星钟差多项式部分 [s] |
| `eph2pos(time, eph, &rs, &dts, &var)` | 卫星位置(ECEF) + 钟差(含相对论改正) + 方差 |
| `satpos(time, sat, nav, &rs, &dts, &var, &svh)` | 单颗卫星：查表星历 → 位置/钟差/健康 |
| `satposs(teph, obs, nav, &out)` | 批量：按观测列表算所有卫星位置（`SatPossResult`） |

### 4.5 `ambiguity.hpp` — LAMBDA 模糊度固定

| 符号 | 职责 |
|------|------|
| `LambdaResult` | `fixed0` 最优整数解、`fixed1` 次优、`s0/s1` 残差、`ratio=s1/s0` |
| `lambda(n, m, a, Q)` | 整数最小二乘搜索（MLAMBDA，逐行对照 RTKLIB 移植） |

### 4.6 `spp_solver.hpp` — 单点定位

| 符号 | 职责 |
|------|------|
| `SppResult` | `rr(ECEF) / dtr(钟差) / qr / ns / vsat / azel / resp` |
| `spp(obs, nav, cfg, initPos)` | 迭代加权最小二乘单点定位（含高度角加权、电离层/对流层改正、地球自转改正） |
| `obsSigma(cfg, el, phase)` | 按高度角计算观测噪声标准差 [m] |

### 4.7 `rtk_engine.hpp` — RTK 主引擎

**状态**：`x = [p_ecef(3) v_ecef(3) N_sd(sat,f)...]`，维度随可见卫星数运行期变化。

**`RtkEngine` 公开接口**：

| 方法 | 职责 |
|------|------|
| `RtkEngine(cfg)` | 构造 |
| `setBasePosition(baseEcef)` | 设置基站已知坐标；不设则用基站伪距单点解 |
| `processEpoch(roverObs, baseObs, nav)` | **处理一个历元**：双差 EKF + 模糊度固定，返回 `Sol` |
| `processSingle(roverObs, nav)` | 降级路径：只跑单点（无基站） |
| `solution() / state() / covariance() / satStats() / ratio()` | 结果查询 |
| `reset()` | 重置状态 |

**`RtkEngine` 私有流程**：

```
processEpoch(rover, base, nav)
├─ satposs()                  # 算流动站/基站所有卫星位置
├─ spp()                      # 流动站单点解做初值
├─ syncAmbiguities()          # 按本历元可见星重建状态向量，保留已有模糊度
├─ temporalUpdate(dt)         # EKF 时间更新（位置/速度随机游走，模糊度随机游走）
├─ 双差观测方程 (zdres → ddres)
│   ├─ 伪距双差 + 载波双差
│   ├─ 电离层(Klobuchar) / 对流层(Saastamoinen) 改正
│   └─ 高度角加权 R
├─ EKF 更新（浮动解）
├─ 模糊度固定 (lambda)        # 比率 > ratioThreshold 则用固定解
└─ 输出 Sol
```

---

## 5. 跨模块调用关系图

```
                    ┌─────────────────────────────────┐
                    │            common/               │
                    │  angle · earth · rotation · types│
                    └──────────────┬──────────────────┘
                                   │
              ┌────────────────────┼────────────────────┐
              │                    │                    │
        ┌─────▼─────┐        ┌─────▼─────┐        ┌─────▼─────┐
        │   ahrs/   │        │   gins/   │        │    rtk/   │
        │ AhrsEngine│        │GinsEngine │        │ RtkEngine │
        │  6维 EKF  │        │ 21维 EKF  │        │ 双差 EKF  │
        │  相对航向 │        │ 松组合    │        │ + LAMBDA  │
        └───────────┘        │InsMech    │        └─────┬─────┘
                             └─────┬─────┘              │
                                   │                    │
                                   └──── 两者独立 ───────┘
                       (GINS 可接收 RTK 的位置作为 GNSS 观测)
```

---

## 6. 配置要点速查

| 模块 | 关键配置项 | 典型值 |
|------|-----------|--------|
| GINS | `antlever` | `(0.231, 0.564, -0.797)` m（天线相对 IMU） |
| GINS | `zuptGyroThreshold` | `1.0°/s` |
| GINS | `chi2Threshold` | `11.34`（χ²(3) 99%） |
| GINS | `maxRejectRun / posCovInflationStd` | `3 / 20.0` m（防卡方锁死） |
| AHRS | `tiltAccThreshold` | `0.3 m/s²` |
| RTK | `elmin / ratioThreshold` | `10° / 3.0` |
| RTK | `errPhaseBase` | `0.003` m |

---

## 7. 文件清单

| 文件 | 行数级 | 职责 |
|------|--------|------|
| `common/angle.hpp` | ~55 | 角度常量与单位转换 |
| `common/earth.hpp` | ~235 | WGS84 地球模型 |
| `common/nav_types.hpp` | ~65 | 共用数据结构 |
| `common/rotation.hpp` | ~128 | 旋转表示转换 |
| `gins/gins_types.hpp` | ~225 | GINS 类型/配置/调试 |
| `gins/ins_mech.hpp/.cpp` | — | INS 机械编排 |
| `gins/gins_engine.hpp/.cpp` | — | 松组合 EKF 主引擎 |
| `ahrs/ahrs_types.hpp` | ~90 | AHRS 类型/配置 |
| `ahrs/ahrs_engine.hpp/.cpp` | — | 姿态 EKF 引擎 |
| `rtk/rtk_types.hpp` | ~225 | RTK 常量/结构/配置 |
| `rtk/rtk_util.hpp/.cpp` | — | 时间/坐标/几何工具 |
| `rtk/atmosphere.hpp/.cpp` | — | 电离层+对流层改正 |
| `rtk/ephemeris.hpp/.cpp` | — | 广播星历解算 |
| `rtk/ambiguity.hpp/.cpp` | — | LAMBDA 模糊度固定 |
| `rtk/spp_solver.hpp/.cpp` | — | 单点定位 |
| `rtk/rtk_engine.hpp/.cpp` | — | RTK 主引擎 |
