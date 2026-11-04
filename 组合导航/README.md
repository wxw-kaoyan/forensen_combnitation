# KF-GINS

[[官方原版中文说明]](./README_CN.md)

> **说明**：本文档针对本仓库的**二次开发版本**编写。本仓库基于武汉大学 i2Nav 团队开源的 KF-GINS，在原版松组合算法之上扩展了 **GNSS 速度量测**、**双天线航向量测** 与 **ZUPT 零速修正**，官方原版说明见 [README_CN.md](./README_CN.md)。

## 基于扩展卡尔曼滤波的 GNSS/INS 组合导航软件

## 简介

KF-GINS[^1] 是一套基于扩展卡尔曼滤波的 GNSS/INS 组合导航软件，实现了经典的 GNSS 定位结果与 IMU 数据的组合导航解算。算法实现参考牛小骥教授和陈起金博士的《惯性导航原理与 GNSS/INS 组合导航》课程讲义，是该课程的配套资源。软件采用 C++ 语言编写，使用 CMake 管理项目。

主要特点：

- 扩展卡尔曼滤波架构（误差状态向量）的 GNSS/INS 松组合算法，包括 IMU 误差补偿、惯性导航解算、Kalman 滤波、误差反馈等环节
- 采用 21 维系统误差状态：位置误差、速度误差、姿态误差、IMU 零偏误差和 IMU 比例因子误差
- 姿态误差采用 Phi 角模型，速度、位置误差定义在导航坐标系下
- 惯性导航解算采用基于线性变化假设的双子样机械编排算法，补偿了姿态圆锥效应、速度的旋转效应和划桨效应

**本仓库相对官方原版的扩展：**

- **GNSS 速度量测更新**：GNSS 文件含速度（北-东-地）及其标准差时，在位置更新后追加速度量测更新；更新前对速度新息做**卡方检验**（3 自由度、99% 置信、阈值 11.34），GNSS 失锁导致速度被冻结或输出哨兵值时自动跳过本次更新，避免野值灌入滤波器
- **双天线航向量测更新**：GNSS 文件含双天线航向（deg）及其标准差时，追加航向角量测更新；航向新息做了 ±180° 回绕处理，观测矩阵作用于航向误差状态（PHI 第 3 分量）
- **ZUPT 零速修正**：以 1 Hz 频度做静止判定，三条件同时满足才触发——角速度模长 < 1 °/s、比力模长与当地重力之差的绝对值 < 0.5 m/s²、当前速度估计模长 < 0.5 m/s；触发后以速度零值为观测（R = 0.01 m/s）进行量测更新。增加速度约束是为了避免与"直线匀速行驶"（陀螺≈0、比力≈g）混淆导致行驶中误触发
- **GNSS 观测文件格式扩展**：兼容 7 列（仅位置）、13 列（位置+速度）、15 列（位置+速度+航向）三种格式，加载时按实际列数自动识别

**单位：** 武汉大学卫星导航定位技术研究中心[多源智能导航实验室](http://www.i2nav.com/)

**相关资料：**

- X. Niu, L. Wang, Q. Chen, H. Tang, Q. Zhang, and T. Zhang, "KF-GINS: an open-sourced software for GNSS/INS integrated navigation," GPS Solut, vol. 29, no. 4, p. 202, Oct. 2025, doi: 10.1007/s10291-025-01967-w.
- 牛小骥, 陈起金, 张全, "惯性导航原理与算法设计", 高等学校导航工程专业规划教材. 武汉: 武汉大学出版社, 2025.
- 牛小骥, 陈起金, "[惯性导航原理与GNSS/INS组合导航课程讲义](http://www.i2nav.com/index/newListDetail_zw.do?newskind_id=13a8654e060c40c69e5f3d4c13069078&newsinfo_id=40f3c65b158742c099ba3b600c983aa1)", 武汉大学多源智能导航实验室, 2022.
- 牛小骥, 陈起金, "[惯性导航原理与GNSS/INS组合导航课程视频](https://www.bilibili.com/video/BV1na411Z7rQ?spm_id_from=333.999.0.0&vd_source=a417ebe0768fc96919fe8e34c55ed591)", 武汉大学多源智能导航实验室, 2022.
- X. Niu, Q. Zhang, L. Gong, C. Liu, H. Zhang, C. Shi, J. Wang and M. Coleman (2014). "Development and evaluation of GNSS/INS data processing software for position and orientation systems." Survey Review 2014; 47(341), 87-98.
- 严恭敏, 翁浚, 捷联惯导算法与组合导航原理. 西北工业大学出版社, 2019.
- E.-H. Shin, "Estimation techniques for low-cost inertial navigation," Ph.D. dissertation, Dept. Geomatics Eng., Univ. of Calary, AB, Cabada, 2005
- [Savage, P.G.](http://www.strapdownassociates.com/), "Strapdown analytics, Part I", Maple Plain, MN: Strapdown Associates, 2000.
- Titterton David, JohnL. Weston, and John Weston, "Strapdown inertial navigation technology", The Institution of Electrical Engineers, 2004.
- P. D. Groves, "Principles of GNSS, Inertial, and Multi-sensor Integrated Navigation Systems", 2nd ed., vol. 39. Artech House, 2013.

**如果你使用这个软件进行学术研究，请添加如下致谢，并引用我们的[相关文档和论文](./ref.bib)**

```
中文模板：“本文作者感谢武汉大学卫星导航定位技术研究中心多源智能导航实验室(i2Nav)牛小骥教授团队开源的KF-GINS软件平台。”
English version: “The authors would like to acknowledge the team of Prof. Xiaoji Niu of the Integrated and Intelligent Navigation (i2Nav) group from GNSS Research Center of Wuhan University for providing the open-source KF-GINS software that was used in the paper.”
```

**联系：**

- 有任何相关的技术问题，可以向王立强(wlq@whu.edu.cn)或唐海亮博士(thl@whu.edu.cn)发送邮件。
- 中国使用者可以加入QQ交流群(481173293)进行讨论，进群需要提供所在机构和真实姓名。

## 1. 程序编译与运行

### 1.1 编译环境

KF-GINS 项目使用 CMake 管理，支持在 Linux、MacOS 和 Windows 环境下编译，建议优先选择 Linux 环境。

编译成功后需以配置文件作为参数运行；程序调试时同样需要添加配置文件作为命令行参数。

### 1.2 依赖库

除 C++ 标准库之外，KF-GINS 依赖 Eigen3、abseil-cpp 和 yaml-cpp 三个库。这三个库已作为三方库随附在 **ThirdParty** 目录中，不需要使用者单独安装。

### 1.3 在 Linux 内编译

建议使用 Ubuntu 18.04 或 Ubuntu 20.04 系统下的 g++ 编译工具，编译前需安装必要的库：

```shell
sudo apt-get install cmake
sudo apt-get install build-essential
```

配置好编译环境之后，将仓库克隆到本地并按如下操作编译：

```shell
# 克隆仓库
git clone https://github.com/i2Nav-WHU/KF-GINS.git ~/

# 编译KF-GINS
cd ~/KF-GINS
mkdir build && cd build

cmake ../ -DCMAKE_BUILD_TYPE=Release 
make -j8

# 运行测试数据集
cd ~/KF-GINS
./bin/KF-GINS ./dataset/kf-gins.yaml

# 等待程序运行结束
```

### 1.4 在 Windows 内编译

Windows 环境下一般使用 MSVC（Microsoft Visual C/C++）编译器，建议在 VSCode 软件下编译。

先通过 Visual Studio Installer 安装 MSVC 编译器，并安装 VSCode 及必要插件（C/C++、C/C++ Extension Pack、CMake、CMake Tools）。

克隆仓库到本地，用 VSCode 打开仓库文件夹：

- 选择编译器：打开命令面板（Ctrl+Shift+P）输入 "CMake: Select a Kit"，选择 MSVC 编译器
- 设置编译参数：命令面板输入 "CMake: Select Variant"，选择 Release
- 配置 CMake：命令面板输入 "CMake: Configure"
- 编译工程：命令面板输入 "CMake: Build"

在工程目录下打开 PowerShell 或 CMD 终端，运行测试数据集：

```shell
.\bin\KF-GINS.exe .\dataset\kf-gins.yaml
# 可执行文件也可能生成在 .\bin\Release 文件夹下，则执行命令为:
# .\bin\Release\KF-GINS.exe .\dataset\kf-gins.yaml
```

> 注意：Windows 下配置文件中的数据路径**不能包含中文**。

### 1.5 在 MacOS 内编译

MacOS 中需要 xcode-select 和 cmake，按如下命令安装：

```shell
xcode-select --install
brew install cmake
```

配置好编译环境之后，将仓库克隆到本地并按如下操作编译：

```shell
# 克隆仓库
git clone https://github.com/i2Nav-WHU/KF-GINS.git ~/

# 编译KF-GINS
cd ~/KF-GINS
mkdir build && cd build

cmake ../ -DCMAKE_BUILD_TYPE=Release 
make -j8

# 运行测试数据集
cd ~/KF-GINS
./bin/KF-GINS ./dataset/kf-gins.yaml

# 等待程序运行结束
```

### 1.6 在 VSCode 内调试

Linux 下调试需要安装 gdb：

```shell
sudo apt-get install gdb
```

调试操作如下：

- 选择编译器：命令面板（Ctrl+Shift+P）输入 "CMake: Select a Kit"，Linux 选择 gcc，Windows 选择 MSVC，MacOS M 系列芯片选择 "clang 15.0.0 arm64-apple-darwin23.5.0" 或更高版本
- 设置编译参数：命令面板输入 "CMake: Select Variant"，选择 **Debug**
- 配置并编译：命令面板依次输入 "CMake: Configure"、"CMake: Build"
- 选择调试启动选项：点击 VSCode 左侧第四个按钮打开"运行和调试"菜单（或 Ctrl+Shift+D），在上方选择 "Windows 启动"、"Linux gdb 启动" 或 "Mac lldb 启动"
- 点击绿色三角符号（或按 F5）开始调试

## 2. KF-GINS 使用

### 2.1 基础知识

**坐标系定义：**

- IMU 坐标系：原点为 IMU 测量中心，三轴指向前-右-下
- 导航参考坐标系：原点与 IMU 坐标系一致，三轴指向北-东-地

**导航状态：**

- 位置：IMU 在地球坐标系下的大地坐标（纬度-经度-椭球高）
- 速度：IMU 相对地球的速度在导航参考坐标系下的投影（北向速度-东向速度-地向速度）
- 姿态：IMU 相对导航参考坐标系的姿态（四元数、方向余弦矩阵或欧拉角；欧拉角定义为航向角-俯仰角-横滚角，ZYX 旋转顺序）

**IMU 误差建模：**

- IMU 测量噪声建模为高斯白噪声
- IMU 零偏误差建模为一阶高斯-马尔科夫过程
- IMU 比例因子误差建模为一阶高斯-马尔科夫过程

更多算法细节请参考[惯性导航原理与GNSS/INS组合导航课程讲义](http://www.i2nav.com/index/newListDetail_zw.do?newskind_id=13a8654e060c40c69e5f3d4c13069078&newsinfo_id=40f3c65b158742c099ba3b600c983aa1)，docs 目录下另附有算法文档与论文 PDF。

### 2.2 数据格式

- IMU 文本文件格式：

| 列数 | 数据描述         | 单位    |
| ---- | ---------------- | ------- |
| 1    | GNSS 周内秒      | s       |
| 2~4  | X-Y-Z 轴角度增量 | rad     |
| 5~7  | X-Y-Z 轴速度增量 | m/s     |

- GNSS 观测文本文件格式（本仓库支持三种列数，按实际列数自动识别量测类型）：

**7 列（仅位置量测，官方原版格式）：**

| 列数 | 数据描述                    | 单位 |
| ---- | --------------------------- | ---- |
| 1    | GNSS 周内秒                 | s    |
| 2    | 纬度                        | deg  |
| 3    | 经度                        | deg  |
| 4    | 椭球高                      | m    |
| 5~7  | 位置标准差（北向-东向-地向）| m    |

**13 列（位置+速度量测）：**

| 列数  | 数据描述                     | 单位 |
| ----- | ---------------------------- | ---- |
| 1     | GNSS 周内秒                  | s    |
| 2~4   | 纬度 / 经度 / 椭球高         | deg/deg/m |
| 5~7   | GNSS 速度（北向-东向-地向）  | m/s  |
| 8~10  | 位置标准差（北向-东向-地向） | m    |
| 11~13 | 速度标准差（北向-东向-地向） | m/s  |

**15 列（位置+速度+双天线航向量测）：** 在 13 列基础上追加

| 列数 | 数据描述       | 单位 |
| ---- | -------------- | ---- |
| 14   | 航向角         | deg  |
| 15   | 航向角标准差   | deg  |

> 航向需已折算到本机航向基准（IMU 偏航角参考系）。速度/航向不可用时只需提供对应较少列数的文件即可，无需修改代码。

- 导航结果和参考真值文本文件格式：

| 列数 | 数据描述                  | 单位 |
| ---- | ------------------------- | ---- |
| 1    | GNSS 周                   | -    |
| 2    | GNSS 周内秒               | s    |
| 3    | 纬度                      | deg  |
| 4    | 经度                      | deg  |
| 5    | 椭球高                    | m    |
| 6~8  | 三维速度（北向-东向-地向）| m/s  |
| 9~11 | 姿态角（横滚-俯仰-航向）  | deg  |

- IMU 误差文件格式（`KF_GINS_IMU_ERR.txt`，均为 double 类型）：

| 列数  | 数据描述                 | 单位  |
| ----- | ------------------------ | ----- |
| 1     | GNSS 周内秒              | s     |
| 2~4   | X-Y-Z 轴陀螺仪零偏       | deg/h |
| 5~7   | X-Y-Z 轴加速度计零偏     | mGal  |
| 8~10  | X-Y-Z 轴陀螺仪比例因子   | ppm   |
| 11~13 | X-Y-Z 轴加速度计比例因子 | ppm   |

- 系统状态标准差文件格式（`KF_GINS_STD.txt`，均为 double 类型）：

| 列数  | 数据描述                        | 单位  |
| ----- | ------------------------------- | ----- |
| 1     | GNSS 周内秒                     | s     |
| 2~4   | 三维位置标准差（北向-东向-地向）| m     |
| 5~7   | 三维速度标准差（北向-东向-地向）| m/s   |
| 8~10  | 三维姿态标准差（横滚-俯仰-航向）| deg   |
| 11~13 | X-Y-Z 轴陀螺仪零偏标准差        | deg/h |
| 14~16 | X-Y-Z 轴加速度计零偏标准差      | mGal  |
| 17~19 | X-Y-Z 轴陀螺仪比例因子标准差    | ppm   |
| 20~22 | X-Y-Z 轴加速度计比例因子标准差  | ppm   |

### 2.3 初始对准

KF-GINS 目前只支持给定全部初始状态进行初始对准，程序运行前需在配置文件（kf-gins.yaml）中配置初始位置、初始速度和初始姿态。实测数据集建议：初始位置取首个 GNSS 历元，初始姿态的航向角可取双天线 RTK 航向（参考 cmp_data 下的配置示例）。

### 2.4 配置文件说明

配置文件（kf-gins.yaml）主要字段：

| 字段 | 说明 |
| ---- | ---- |
| imupath / gnsspath / outputpath | IMU 文件、GNSS 文件路径与输出文件夹路径 |
| imudatalen / imudatarate | IMU 文件列数（只取前 7 列）与采样率 [Hz] |
| starttime / endtime | 处理时间段 [s]，endtime 设为 -1 表示处理至 IMU 文件结束 |
| initpos / initvel / initatt | 初始位置 [deg,deg,m]、初始速度 [m/s]、初始姿态（横滚/俯仰/航向）[deg] |
| initgyrbias / initaccbias / initgyrscale / initaccscale | 初始 IMU 零偏 [deg/h, mGal] 与比例因子 [ppm] |
| initposstd / initvelstd / initattstd | 初始位置/速度/姿态标准差 |
| initbgstd / initbastd / initsgstd / initsastd | 初始 IMU 零偏与比例因子标准差（不指定时默认取 imunoise 中的对应值） |
| imunoise | IMU 噪声参数：arw（角度随机游走）、vrw（速度随机游走）、gbstd/abstd（零偏不稳定性）、gsstd/asstd（比例因子）、corrtime（相关时间） |
| antlever | GNSS 天线杆臂，IMU 坐标系前-右-下方向 [m] |

## 3. 数据集

### 3.1 官方示例数据

`dataset/` 目录下保存官方示例数据集（Leador-A15 采集的 IMU 数据、GNSS-RTK 定位结果及参考真值 truth.nav），并附配套配置文件 kf-gins.yaml。

### 3.2 本仓库实测数据

`cmp_data/` 目录下保存两套车载实测数据，均已附带调好的配置文件（kf-gins.yaml）及输出结果（KF_GINS_STD.txt、KF_GINS_IMU_ERR.txt）：

- `cmp_data/1633/`：MEMS IMU（ADIS16488 级，100 Hz），上海车载数据
- `cmp_data/680D/`：较高精度 IMU 对比数据

### 3.3 开源数据集

可在 **[awesome-gins-datasets](https://github.com/i2Nav-WHU/awesome-gins-datasets)** 下载更多开源数据集。

### 3.4 使用自己的数据

数据格式与 2.2 节定义一致（与 [awesome-gins-datasets](https://github.com/i2Nav-WHU/awesome-gins-datasets) 的格式定义兼容），按格式准备好 IMU 与 GNSS 文本文件，并修改配置文件中的路径、初始状态与 IMU 噪声参数即可。

## 4. 结果绘图

`scripts/plot_navresult.py` 提供导航结果绘图功能，可对输出的 `KF_GINS_Navresult.nav`、`KF_GINS_STD.txt` 进行轨迹、速度、姿态及各状态标准差的可视化，用于评估组合导航精度与滤波收敛情况。

## 5. 许可

KF-GINS 源代码在 GPLv3 许可下发布。

我们仍在努力提高代码的可靠性。如有任何技术问题，请联系王立强(wlq@whu.edu.cn)或唐海亮(thl@whu.edu.cn)，或在仓库中开启一个议题。

商业用途请联系牛小骥教授(xjniu@whu.edu.cn)。

[^1]: i2Nav 团队另开源了基于图优化的 GNSS/INS 组合导航算法 [OB_GINS](https://github.com/i2Nav-WHU/OB_GINS)。
