# simulator 模拟器模块说明

本模块实现 **PPS 脉冲 + GNSS 数据流模拟**：占用两个串口，一个输出秒脉冲（PPS 上升沿），另一个按协议输出 GNSS 定位数据帧，用于在无真实接收机时调试上位机软件。

## 1. 目录结构

```
modules/simulator/
├── simulatorpanel.h / .cpp     # 模拟面板（UI：串口选择、协议勾选、启停）
├── ppsgenerator.h / .cpp      # PPS 发生器（DTR 秒脉冲，QPC 高精度线程，µs 级）
├── datasimulator.h / .cpp     # 数据模拟器（按 PPS 节奏驱动协议发帧）
├── simclock.h                 # 模拟时间基准（UTC -> GPS 时间换算）
├── simprotocol.h              # 模拟协议抽象基类
├── simprotocolregistry.h / .cpp  # 协议工厂 + JSON 加载
├── protocols/                 # 具体协议（每种一个头文件）
│   ├── gprmc.h                #   $GPRMC  NMEA 文本
│   ├── gpgga.h                #   $GPGGA  NMEA 文本
│   ├── bestposb.h             #   NovAtel BESTPOSB（msgId=42）
│   └── psrvelb.h              #   NovAtel PSRVELB（msgId=100）
└── encoders/                  # 帧编码器（协议 -> 字节流）
    ├── nmeaencoder.h / .cpp   #   NMEA 0183 文本打包（$...+校验和+\r\n）
    ├── novatelencoder.h / .cpp#   NovAtel OEM 二进制打包（AA 44 12 头）
    ├── aa55encoder.h / .cpp   #   本工程 AA55 自定义帧打包（AA 55 头）
    └── bytewriter.h           #   小端字节写入工具
```

## 2. 整体时序逻辑

模拟器由 **PPS 脉冲作为节拍源**，数据流与之整秒对齐：

```
SimulatorPanel
   │  启动时创建两个对象并连线
   │  connect(PpsGenerator::ppsTick, DataSimulator::onPpsTick)
   ▼
PpsGenerator（PPS 串口，高精度线程）
   │  start() 启动独立 std::thread，首拍对齐下一个 UTC 整秒
   │  每拍（绝对节拍网格，间隔精确 +1s）：
   │  1) 睡眠到目标前 2ms，再用 QElapsedTimer(QPC) 忙等至精确时刻
   │  2) 拉高 DTR（上升沿，µs 级）
   │  3) 5ms 后拉低（同样 睡眠+忙等 控制下降沿）
   │  4) emit ppsTick()（排队投递到主线程）
   ▼
DataSimulator（数据串口）
   │  onPpsTick(): 刷新时间基准 m_clock（取系统 UTC 整秒 -> GPS 时间）
   │                发送第 0 帧（与 PPS 上升沿对齐）
   │                若 rateHz > 1，启动 m_subTimer(1000/rateHz ms)
   │  onSubTick():  发送第 1..N-1 帧，发满 rateHz 帧后停表，等下一个整秒
   ▼
sendAll(): 遍历所有启用协议，p->encode(m_clock) 得到字节流写入串口
```

关键点：

- **电气脉冲 µs 级对齐**：上升沿由高精度线程忙等自旋确定（QPC 分辨率 ~100ns），与整秒边界对齐，单拍过冲不向下一拍累积漂移。
- **帧对齐为亚 ms 级**：`ppsTick` 经 Qt 队列投递到主线程后再发第 0 帧，相对电气沿有一拍事件投递延迟（µs~ms 级）；GPS 时间戳按整秒取整不受影响。
- **秒内均分**：`m_subTimer` 按 `1000/rateHz` ms 间隔发剩余帧，实现 1~100Hz 均匀输出。
- **每协议一帧**：`sendAll()` 每 tick 对每个协议编码一帧，即 N 个协议 = 每秒 N×rateHz 帧。
- 频率统一由面板全局 `rateHz`（1~100Hz）控制，覆盖 JSON 中各协议默认频率。

## 3. 时间基准 SimClock

[simclock.h](simclock.h) 定义模拟时钟结构，各协议编码时从中取时间戳：

| 字段 | 含义 |
|------|------|
| `gpsWeek` | GPS 周 |
| `gpsWeekMs` | 周内毫秒（PPS 整秒对齐时毫秒部分=0） |
| `gpsWeekSec` | 周内秒（整秒） |
| `utcTimeOfDayMs` | UTC 日内毫秒（NMEA 时间字段用） |
| `utcDate` | UTC 日期（GPRMC 日期字段用） |
| `timerUs` | 上电计时（us），保留字段 |

换算关系（`simClockFromUtc`）：

```
GPS秒 = UTC秒 - GPS纪元(1980-01-06) + 闰秒(18)
GPS周 = GPS秒 / 604800
周内毫秒 = (GPS秒 % 604800) * 1000 + UTC毫秒
```

- 闰秒固定为 **18s**（2017-01-01 起）；未来再插闰秒需同步修改 `simclock.h` 中的常量。
- `onPpsTick` 里将系统 UTC 时间**向下取整到秒**再换算，使 GPS 周内毫秒整秒对齐 PPS。

## 4. PPS 发生器（ppsgenerator.cpp）

- 占用一个串口，**用 DTR 线**输出秒脉冲：上升沿 µs 级对齐，脉宽 5ms。
- **高精度实现**（原 QTimer 方案已废弃）：独立 `std::thread` + `QElapsedTimer`（Windows 底层即 QPC，分辨率 ~100ns）。
  - **首拍对齐到下一个 UTC 整秒**，之后每拍在绝对节拍网格上精确 +1s——单拍过冲只影响本拍，不向下一拍累积漂移；
  - 每拍先睡眠到目标前 2ms，剩余时间**忙等自旋**到精确时刻再拉 DTR，规避系统睡眠精度（Windows 默认 ~15.6ms，`timeBeginPeriod(1)` 后 ~1ms）造成的抖动；
  - Windows 下线程提权 `THREAD_PRIORITY_TIME_CRITICAL`，DTR 直调原生 `EscapeCommFunction` 绕过 Qt 串口对象、降低切换延迟；
  - 非 Windows 平台自动回退到 `setDataTerminalReady`，精度为尽力而为。
- **精度边界**：取决于主机调度负载。正常负载下抖动 µs 级；系统过载（CPU 被占满）时仍可能超标，若要 ns 级必须改用硬件 PPS。

## 5. 数据模拟器（datasimulator.cpp）

- `start()`：打开数据串口（8N1，无流控），取第一个协议的 `rateHz` 作为全局频率。
- `onPpsTick()`：**重置时间基准 + 发第 0 帧**，`m_framesSent=1`；rateHz>1 时启动子定时器。
- `onSubTick()`：继续发帧直到本秒发满 `rateHz` 帧，然后停表等待下一个 PPS。
- `sendAll()`：对所有协议调用 `encode(m_clock)`，非空则写入串口。
- 协议对象所有权归 `DataSimulator`（析构时 `qDeleteAll`）。

## 6. 协议与编码器

### 协议抽象基类 `SimProtocol`

```cpp
virtual QByteArray encode(const SimClock &clock) = 0;
QString name;      // 协议名，如 "GPRMC"
QString format;    // "nmea" / "novatel" / "aa55"
int     rateHz;    // 输出频率
bool    hasGpsTime;
bool    enabled;   // 是否启用
QJsonObject params; // 模板固定参数（运行时由 JSON 提供）
```

### 编码器

| 编码器 | 帧格式 | 校验 |
|--------|--------|------|
| `NmeaEncoder` | `$sentence,body*CS\r\n`，经纬度 `ddmm.mmmm` 格式，时间 `hhmmss.sss` | `$` 与 `*` 之间异或 |
| `NovatelEncoder` | `AA 44 12` + 28 字节头 + payload + CRC32，小端 | CRC32 初值 **0**，不取反（与 rtklib rtk_crc32 一致） |
| `Aa55Encoder` | `AA 55` + ID(u16) + len(u16) + payload + CRC32 | CRC32 初值 **1**，不取反（与 FrameParser::computeCrc32 一致） |
| `ByteWriter` | 小端 appendU8/U16/U32/I32/Float/Double | — |

### 现有协议

| 协议 | 帧类型 | 说明 |
|------|--------|------|
| GPRMC | NMEA 文本 | 推荐最小定位信息（时间/经纬度/速度/航向/日期/磁偏角） |
| GPGGA | NMEA 文本 | 定位固定数据（质量/星数/HDOP/高程/大地水准面差） |
| BESTPOSB | NovAtel 二进制 (42) | 最佳位置：解状态/定位类型/经纬高/精度/星数 |
| PSRVELB | NovAtel 二进制 (100) | 伪距/载波速度：水平速度/地面航向/垂直速度 |

各协议所有可变参数都从 `params` 取（带默认值），例如 `params.value("lat").toDouble(39.9042)`。

## 7. 配置 JSON（sim_protocols.json）

面板启动时从 `sim_protocols.json` 加载协议列表，文件位于**程序运行目录**（优先）或**当前工作目录**（兜底）。

```json
{
  "sim_protocols": [
    {
      "name": "GPRMC",
      "format": "nmea",
      "rateHz": 1,
      "hasGpsTime": true,
      "enabled": true,
      "params": { "status": "A", "lat": 39.9042, "lon": 116.4074 }
    }
  ]
}
```

- `name` 决定工厂创建哪个协议实例，**必须匹配** `createSimProtocol` 中的分支。
- `params` 为协议模板的固定值，改 JSON 即可调整，无需重新编译。

## 8. 面板交互逻辑（simulatorpanel.cpp）

1. **串口配置**：PPS 串口 / 数据串口 + 各自波特率（9600~921600），两串口**不能相同**。
2. **协议选择**：从 JSON 加载后以表格展示（启用勾选/协议名/格式/有无 GPS 时间），支持全选/全不选/重新加载。
3. **启动流程** `onStartStop()`：
   - 重新从 JSON 加载协议 → 按表格勾选过滤 → 未勾选者删除；
   - 面板 `rateHz` 覆盖所有协议默认频率；
   - 先 `m_pps->start()` 再 `m_data->start()`；任一失败则统一停止。
4. **停止**：两个发生器一起 stop，按钮/状态复位。

## 9. 如何新增一个模拟协议

1. 在 `protocols/` 下新建 `xxx.h`，继承 `SimProtocol` 实现 `encode()`（参考 `bestposb.h`）。
2. 如为新帧格式，在 `encoders/` 新增/复用编码器。
3. 在 `simprotocolregistry.cpp` 中 `#include` 该头文件，并在 `createSimProtocol` 加一个 `else if` 分支。
4. 在 `sim_protocols.json` 中登记配置（`name` 与工厂分支一致）。

## 10. 注意事项

- **闰秒 18s 为硬编码**，未来 GPS 闰秒变更需更新 `simclock.h`。
- NovAtel 与 AA55 的 CRC32 **初值不同**（0 vs 1），实现必须与各自解析端（rtklib 风格 vs FrameParser::computeCrc32）保持一致，不能混用。
- PPS 与数据两个串口物理上必须分开；PPS 只依赖 DTR 线，数据依赖 TX 数据线。
- 面板全局频率会覆盖 JSON 中每个协议的 `rateHz`，JSON 内该字段实际仅在工厂加载时记录。
