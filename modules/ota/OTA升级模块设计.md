# OTA升级模块设计文档

> 参照工程：`D:\工作目录\测试工具\Linux_UATR_OTA`（Linux C 串口 IMU 固件升级工具）
> 目标：在 Witt 工程中新增**独立模块** `modules/ota/`，主窗口单独一个 Tab「OTA升级」。
> 支持**串口**与 **TCP 网络**两种升级链路，协议帧内容完全相同，仅传输层不同。
>
> **一期范围（本次实现）**：升级功能（协议层 + 升级状态机 + 升级面板）。
> 解析/分析为后续扩展，不在本次范围内；本模块**不包含任何仿真器**，也不依赖 `modules/simulator`。

---

## 1. 参考工程协议分析

### 1.1 帧格式

| 方向 | 帧头 | 说明 |
|---|---|---|
| PC → 设备（下行） | `55 AA` | 参考代码发送结构体 `header1=0x55, header2=0xAA` |
| 设备 → PC（上行） | `AA 55` | 参考代码接收状态机先等 `0xAA` 再等 `0x55` |

帧结构（小端）：

```
┌────────┬────────┬──────────┬──────────┬─────────┬──────────┐
│ sync1  │ sync2  │ id u16LE │ len u16LE│ payload │ crc32 u32│
│ 1 byte │ 1 byte │ 2 bytes  │ 2 bytes  │ N bytes │ 4 bytes  │
└────────┴────────┴──────────┴──────────┴─────────┴──────────┘
```

- `len` = payload 字节数（不含帧头 6 字节和 CRC 4 字节）。
- **CRC32**：IEEE 802.3 表驱动，**初值 = 1**，计算范围 = sync 到 payload 末尾（与工程现有 `FrameParser::computeCrc32` 算法一致；本模块自带独立实现，不依赖 parse 模块）。

### 1.2 两种下行帧

**长命令帧**（握手/擦除/校验/重启等无数据命令），总长 36 字节，payload = 24 字节（6 个 u32 参数）：

```cpp
struct OtaLongCmdFrame {        // 36 字节
    uint8_t  header1, header2;  // 55 AA
    uint16_t id;                // 命令字
    uint16_t length;            // = 26 (0x001A)
    uint32_t param[6];          // 命令参数，默认全 0
    uint32_t crc;
};
```

**数据帧**（烧写固件块），总长 81 字节，payload = 71 字节：

```cpp
struct OtaDataFrame {           // 81 字节
    uint8_t  header1, header2;  // 55 AA
    uint16_t id;                // 0x0027 PROG_MULTI
    uint16_t length;            // = 71 (0x0047)
    uint8_t  data[64];          // 固件块（必须 4 字节对齐）
    uint32_t offset;            // 块在固件中的偏移
    uint16_t size;              // 块有效字节数（≤64）
    uint8_t  cmd;               // 子命令（参考代码填 0）
    uint32_t crc;
};
```

**硬复位魔术帧**：固定 34 字节，`id = 0x88BB`（字节序 `BB 88`），`len = 0x0018(24)`，用于强制 MCU 跳 Bootloader，参考代码连发 3 次：

```
55 AA BB 88 18 00 00 00 C8 42 00 00 48 43 2C 01
00 00 90 01 00 00 F4 01 00 00 58 02 00 00 40 97 46 6A
```

### 1.3 命令字表

| 命令 | 值 | 方向 | payload | 说明 |
|---|---|---|---|---|
| NOP | `0x00` | 上行 | — | 无应答/初始态 |
| OK | `0x10` | 上行 | — | ACK 成功标志 |
| GET_SYNC | `0x21` | 下行 | 长命令 | Bootloader 握手 |
| GET_DEVICE | `0x22` | 下行 | 长命令 | 读设备信息 |
| CHIP_ERASE | `0x23` | 下行 | 长命令 | 擦除 APP 区 Flash（超时 30s） |
| CHIP_VERIFY | `0x24` | 下行 | 长命令 | 校验（参考代码未使用） |
| PROG_MULTI | `0x27` | 下行 | 数据帧 | 64B/块烧写 |
| GET_CRC | `0x29` | 下行 | 长命令 | 让芯片回算固件 CRC32 |
| REBOOT | `0x30` | 下行 | 长命令 | 重启进入新固件 |
| 复位魔术 | `0x88BB` | 下行 | 24B 固定 | 强制进 Bootloader |

### 1.4 上行 ACK 帧

| id | payload | 含义 |
|---|---|---|
| `0x0064` | `command(u32) + result(u32)` | 通用命令应答；`result == 0x10` 表示 OK |
| `0x753C` | `command(u32) + result(u32)` | `result` = 芯片侧计算的**固件 CRC32** |
| `0x753D` | `command(u32) + result(u32)` | `result` = 已确认烧写的 **offset**（块确认） |

### 1.5 升级状态机（参考 `do_upload()` 流程）

```
[1] （自动模式）发硬复位魔术帧 ×3，等待 2s（MCU 复位进 BL）
    （手动模式）跳过本步，由用户先给设备上电/按键进 BL
[2] 串口链路：波特率切换为 115200（BL 固定速率）；TCP 链路无此步
[3] 发握手字符串 "HC32MCU_FORSENSE" ×10（ASCII 裸字符串，非帧）
[4] GET_SYNC(0x21) ×3 → 等 0x0064/OK；失败则重复 [3]+[4]
[5] 打开 *.firmware，校验扩展名，取文件大小，提取文件名 CRC
[6] 最多 3 轮：
      ├─ CHIP_ERASE(0x23) → 等 2s → 等 ACK（30s 超时，失败重试 3 次）
      ├─ PROG_MULTI(0x27) 循环：64B/块，等 0x753D offset 应答
      │     └─ offset 不匹配：重同步 + 重发本块，单块最多重试 100 次
      │     └─ 每块上报进度百分比
      └─ GET_CRC(0x29) ×10 → 收 0x753C 的 CRC，与文件名中 CRC 比对
[7] REBOOT(0x30) ×10，升级完成
```

### 1.6 固件文件约定

- 扩展名必须为 `.firmware`。
- 文件名中嵌入预期 CRC32（十进制）：`#CRC<数值>`，例如
  `imu614E_B_出货14240657#CRC813432377.firmware` → 预期 CRC = `813432377`。
- 烧写块大小 64 字节，且每块长度必须为 **4 的倍数**。
- 软件加载固件时本地也算一遍 CRC32，与文件名 CRC 交叉提示（不阻断升级）。

---

## 2. 模块总体架构

独立目录 `modules/ota/`，**不依赖** `modules/parse`、`modules/simulator`；传输层复用
`common/serialconnection`（串口）与 `common/tcpconnection`（TCP），二者信号槽接口一致
（`send(QByteArray)` / `dataReceived(QByteArray)` / `connected()` / `errorOccurred()`）。

```
modules/ota/
├── OTA升级模块设计.md        ← 本文档
├── core/
│   ├── otaprotocol.h/.cpp    ← 协议层：命令枚举、帧结构体、CRC32、组帧、文件名CRC提取
│   └── otaworker.h/.cpp      ← 升级控制状态机（工作线程，串口/TCP 两种传输）
└── ui/
    ├── otapanel.h/.cpp/.ui   ← 「OTA升级」面板（链路选择/固件/进度/日志）
```

线程模型：`OtaWorker` + `SerialConnection`/`TcpConnection` 放入独立工作线程；
面板通过信号槽交互，不阻塞界面。

---

## 3. 类设计

### 3.1 `OtaProtocol`（协议层，无状态纯函数）

```cpp
namespace OtaProtocol {
    enum Cmd : uint16_t {
        NOP=0x00, OK=0x10, GET_SYNC=0x21, GET_DEVICE=0x22,
        CHIP_ERASE=0x23, CHIP_VERIFY=0x24, PROG_MULTI=0x27,
        GET_CRC=0x29, REBOOT=0x30, RESET_MAGIC=0x88BB,
        ACK_CMD=0x0064, ACK_CRC=0x753C, ACK_OFFSET=0x753D
    };
    constexpr int kBlockSize = 64;      // 烧写块大小
    constexpr int kBootBaud = 115200;   // Bootloader 固定波特率

    uint32_t crc32(const uint8_t *data, int len, uint32_t init = 1);
    uint32_t crc32File(const QString &path, QString *err = nullptr);

    QByteArray buildLongCmd(uint16_t cmd);                       // 36B，参数全 0
    QByteArray buildDataFrame(uint32_t offset, uint16_t size,
                              const uint8_t *data);              // 81B
    QByteArray buildResetMagic();                                // 34B 固定帧
    QByteArray handshakeString();                                // "HC32MCU_FORSENSE"
    uint32_t extractCrcFromFileName(const QString &fileName);    // 解析 #CRC<十进制>，0=未找到
}
```

### 3.2 `OtaWorker`（升级控制，核心类）

```cpp
class OtaWorker : public QObject {
    Q_OBJECT
public:
    enum class Transport { Serial, Tcp };
    enum class Phase { Idle, Connecting, Resetting, Handshaking, Erasing,
                       Programming, Verifying, Rebooting, Done, Failed };

    struct Config {
        Transport transport = Transport::Serial;
        QString   port;        int baud = 115200;   // 串口
        QString   host;        int tcpPort = 0;     // TCP
        QString   firmwarePath;
        bool      autoReset = true;                 // true=发魔术帧自动进BL；false=手动已进BL
    };

public slots:
    void start(const OtaWorker::Config &cfg);
    void cancel();

signals:
    void phaseChanged(OtaWorker::Phase phase, const QString &text);
    void progress(int percent);
    void logMessage(const QString &msg);
    void finished(bool success, const QString &summary);

    // 内部：传输层控制（跨线程排队到工作线程）
    void openSerialRequested(const QString &port, int baud);
    void openTcpRequested(const QString &host, int port);
    void sendRequested(const QByteArray &data);
    void closeRequested();

private:
    // ACK 状态机：逐字节喂入，完整帧解析出 cmdAck/resultAck/crcAck/offsetAck
    void feedRx(const QByteArray &bytes);
    bool waitAck(uint32_t &resultOut, OtaProtocol::Cmd expectAck, int timeoutMs);
    bool doSyncRound();
    bool eraseChip();
    bool programFirmware(const QByteArray &fw);
    bool verifyCrc(uint32_t expectCrc);
};
```

- 状态机严格按 §1.5 执行；参考代码硬编码的 sleep/重试次数提取为常量
  （握手轮次上限、擦除超时 30s、块重试 100 次、整轮 3 次），并支持 `cancel()` 随时中断。
- 串口链路：自动模式下先以用户选择波特率发魔术帧，再重开为 115200；
  手动模式下直接以 115200 打开（界面提示用户确认设备已在 BL）。
- TCP 链路：无波特率概念，魔术帧/握手/升级流程不变。

### 3.3 `OtaPanel`（界面）

```
┌─ OTA升级 ────────────────────────────────────────────────┐
│ 链路: (◉串口 ○TCP)                                        │
│  串口: [COM3 ▾][刷新] 波特:[115200 ▾]                     │
│  TCP : [192.168.1.100      ] 端口:[____]                  │
│ 固件: [...........................] [浏览...] (.firmware) │
│       文件名CRC:813432377  本地CRC:813432377  大小:xx KB  │
│ 进BL: [✓]自动发复位帧进Bootloader（取消勾选=已手动进入BL） │
│       [开始升级]  [取消]                                   │
│ 进度: [██████████████░░░░░░] 62%   阶段: 烧写中            │
│ 日志窗口（实时滚动）……                                     │
└───────────────────────────────────────────────────────────┘
```

---

## 4. 工程集成

1. `CMakeLists.txt`：源文件列表加入 `modules/ota/core/otaprotocol.cpp`、
   `modules/ota/core/otaworker.cpp`、`modules/ota/ui/otapanel.cpp`；
   头文件包含路径已有工程根目录（`modules/...` 风格 include）。
2. `mainwindow.ui`：主 TabWidget 新增页「OTA升级」（`otaTab` + `verticalLayout_ota`）。
3. `mainwindow.cpp`：
   ```cpp
   auto *otaPanel = new OtaPanel(ui->otaTab);
   ui->verticalLayout_ota->addWidget(otaPanel);
   ```

---

## 5. 实施步骤（一期）

1. **协议层** `otaprotocol.h/.cpp`：组帧/CRC/文件名解析；
2. **升级控制** `otaworker.h/.cpp`：ACK 状态机 + 升级状态机 + 串口/TCP 传输；
3. **界面** `otapanel.h/.cpp/.ui`：接线、固件选择、进度/日志；
4. **集成**：CMake + mainwindow Tab；
5. 用户自行编译，真实硬件/TCP 链路联调。

后续扩展（不在本期）：OTA 抓包帧解析器、升级会话时间线/重传统计/失败点分析。

---

## 6. 已确认决策

1. 模块完全独立，不做 Bootloader 仿真器，不与 `modules/simulator` 耦合。
2. 支持**串口**与 **TCP** 两种升级链路，协议帧一致。
3. 进 Bootloader 保留两种方式：界面勾选「自动发复位帧」（默认）或取消勾选走手动。
4. 一期只实现升级功能；解析/分析以后再说。
5. 待真机确认项：GET_DEVICE(0x22) 回包 payload 格式参考代码未解析，本期不展示设备信息。
