# Witt — 组合导航测试数据分析平台

> 原名 `forensen_combnitation`。用于原极科技（FORSENSE）组合导航产品的一体化测试、数据解析与分析工具。

集「设备通讯 → 数据模拟 → 协议解析 → 数据分析 → 固件升级」于一体，覆盖研发调试、产线测试与现场问题定位的完整链路。

---

## 1. 功能总览

主界面为 8 个顶层页签，每个页签由独立模块实现：

| 页签 | 功能 | 代码模块 |
|---|---|---|
| **NTRIP转发工具** | 基于 NTRIP(HTTP1.0) 拉取差分数据，实时查看流量、维护数据池，并转发至多个 TCP / 串口目标；支持 FixGGA 坐标覆盖与原始数据落盘 | `modules/ntrip` |
| **TCP串口助手** | 通用串口 / TCP Client 调试终端，含「常用AT指令」与「实时解析」两个子页 | `modules/commtool` |
| **数据解析** | 离线二进制数据文件的协议解析，按协议分类下拉选择，结果可导出 CSV/TXT | `modules/parse` |
| **原始数据分析** | RINEX 观测文件质量检核，以及 IMU / 真值数据读取与统计 | `modules/analysis/raw` |
| **对比分析** | 多源数据时间对齐与比对，图表化展示 | `modules/analysis/compare` |
| **常用工具** | 外部工具（Converter、rtkplot 等）的进程管理 | `common/toolprocess*` |
| **PPS数据模拟** | 生成 PPS 脉冲与多种协议数据流，用于设备联试与上位机自测 | `modules/simulator` |
| **OTA升级** | 串口 / TCP 通道的 Bootloader 固件升级，含握手、擦除、烧写、CRC 校验、重启 | `modules/ota` |

---

## 2. 支持的协议

### 2.1 数据解析（`modules/parse`）

按类分组，下拉框选择：

| 分类 | 协议 |
|---|---|
| **标准帧协议** | AHRS 基本(0x0002) / AHRS 扩展(0x0022) / 四元数(0x0023) / AG 数据(0x0156) / 组合导航融合结果(0x0166, 94B) / 倾斜测量(0x0166, 105B) / 激光(0x0146) / 南方激光(0x0140) / 中海达激光(0x0446) / 姿态(0x0168) / 614P-pro 四元数(0x0199) |
| **RTCM** | RTCM 3.x 帧解析 + 深解码（输出 RINEX 观测值），基于内嵌 RTKLIB |
| **CAN RTK** | CAN BLF 文件的 CAN RTK 数据（CAN ID `0x1CEBFF9A`） |
| **AG调试数据流** | 0x51 IMU / 0x52 GNSS / 0x53 双天线RTK / 0x54 轮速 / 0x55 KF / 0x56 参数 |
| **614E-B调试数据流** | 0x0101 IMU / 0x0111 GNSS / 0x0131 KF |
| **凯芯调试数据流** | 0x0109~0x0114，经外部 Python 脚本（`scripts/kaixin_parse.py` + `decode_kaixin.mexw64`）综合解析 |

统一 AA55 帧格式：`AA 55 + id(u16) + length(u16) + payload + CRC32(初值 1)`。

### 2.2 数据模拟（`modules/simulator`）

| 编码器 | 用途 |
|---|---|
| `aa55encoder` | 原极 AA55 私有帧 |
| `nmeaencoder` | NMEA 0183（GGA / RMC 等） |
| `novatelencoder` | NovAtel OEM 二进制日志（BESTPOSB / PSRVELB） |

---

## 3. 工程结构

```
forensen_combnitation/
├── main.cpp                    程序入口（应用图标、启动 MainWindow）
├── mainwindow.{cpp,h,ui}       主窗口，挂载 8 个顶层页签
├── CMakeLists.txt              构建脚本（Qt6 + RTKLIB）
├── package.ps1                 一键打包脚本
├── res/                        图标资源（app.ico / app.png / icons.qrc / app_icon.rc）
├── scripts/                    凯芯解析 Python 脚本与 MEX 库
├── config.json                 应用配置
├── at_commands.json            AT 指令库
├── sim_protocols.json          模拟协议库
│
├── common/                     公共基础层
│   ├── serialconnection.*      串口连接封装
│   ├── tcpconnection.*         TCP 连接封装
│   ├── toolprocess*.{cpp,h}    外部工具进程管理
│   ├── uiinitializer.*         窗口尺寸与样式统一初始化
│   └── rtklib/                 内嵌 RTKLIB（C 源码，按 C++ 编译）
│
└── modules/                    业务模块（每个模块自包含）
    ├── ntrip/                  NTRIP 拉取 / 数据池 / 多目标转发
    ├── commtool/               串口助手 + AT 指令 + 实时解析
    ├── simulator/              PPS 与数据流模拟
    ├── parse/                  离线协议解析
    ├── analysis/               数据分析（原始 / 对比）
    └── ota/                    OTA 固件升级
```

---

## 4. 环境依赖

| 项 | 版本 / 说明 |
|---|---|
| Qt | 6.8.3（MinGW 64-bit 构建） |
| 编译器 | MinGW 13.1.0（`B:/qt/Tools/mingw1310_64`） |
| CMake | ≥ 3.19（`B:/qt/Tools/CMake_64`） |
| Qt 模块 | Core、Widgets、Network、SerialPort、Concurrent、Charts |
| 系统库 | `winmm` |
| 第三方 | RTKLIB（已内嵌于 `common/rtklib`） |
| 运行环境 | Python 3.12（仅凯芯数据解析需要） |

---

## 5. 构建与打包

### 5.1 一键打包（推荐）

```powershell
cd c:\Users\wxw\Desktop\QT\TASoftware\forensen_combnitation
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass   # 首次需要
.\package.ps1
```

脚本依次执行：清理并配置 CMake(Release) → 编译 → 复制产物到 `package/` → `windeployqt` 部署 Qt 运行时 → 补齐 MinGW 运行时 DLL → 复制 `config.json`、`at_commands.json`、`sim_protocols.json`、`scripts/`。

输出：`package/Witt.exe`

### 5.2 手动构建

```powershell
cmake -S . -B build/release -G "MinGW Makefiles" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH="B:/qt/6.8.3/mingw_64"
cmake --build build/release --config Release
```

### 5.3 说明

- 工程 target 名仍为 `TASoftware`，通过 `set_target_properties(... OUTPUT_NAME "Witt")` 将产物改名为 `Witt.exe`；
- `scripts/`、`at_commands.json`、`sim_protocols.json` 由 CMake `POST_BUILD` 自动复制到可执行文件目录；
- RTKLIB 的 `.c` 文件以 C++ 方式编译（`LANGUAGE CXX`）。

---

## 6. 配置文件

| 文件 | 用途 |
|---|---|
| `config.json` | 应用级配置 |
| `at_commands.json` | AT 指令库，供「常用AT指令」页加载 |
| `sim_protocols.json` | 模拟协议库，供「PPS数据模拟」页加载 |
| `指令集汇总.txt` | 指令速查 |

---

## 7. 关键设计

- **模块自治**：每个页签对应一个独立模块目录，模块间不交叉依赖，便于单独维护与扩展。
- **协议注册表驱动**：解析模块通过 `ProtocolDescriptor` 注册表 + 工厂（`ParserFactory::createAll()`）统一创建解析器，新增标准帧协议只需在注册表加一条描述符，UI 下拉框自动出现新项。
- **统一帧解析内核**：`FrameParser` 负责 AA55 帧的同步、长度校验、CRC32 与 payload 分发；各协议只关心自己的字段偏移。
- **异步不阻塞 UI**：文件加载与解析在后台线程执行（`QtConcurrent` / `QFutureWatcher`），OTA 升级在独立 `QThread` 中运行，均可随时取消。
- **统一连接抽象**：串口与 TCP 均封装为 `SerialConnection` / `TcpConnection`，NTRIP 转发与 OTA 升级复用同一套帧协议，链路切换对上层透明。
- **资源内嵌**：应用图标经 `res/icons.qrc`（运行时）与 `res/app_icon.rc`（可执行文件）双路径集成，窗口、任务栏、启动菜单图标统一。

---

## 8. 模块文档索引

| 模块 | 文档 |
|---|---|
| 数据解析 | `modules/parse/架构说明.md` |
| 数据分析 | `modules/analysis/架构说明.md` |
| 数据模拟 | `modules/simulator/README.md` |
| OTA 升级 | `modules/ota/OTA升级模块设计.md` |
