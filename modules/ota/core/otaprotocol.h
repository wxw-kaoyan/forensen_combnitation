#ifndef OTAPROTOCOL_H
#define OTAPROTOCOL_H

#include <QByteArray>
#include <QString>
#include <cstdint>

// ════════════════════════════════════════════════════════════════
//  OTA 升级协议层 — 帧定义 / CRC32 / 组帧 / 文件名 CRC 提取
//  参照 Linux_UATR_OTA 工具：
//    下行帧头 55 AA，上行帧头 AA 55；小端；
//    CRC32 为 IEEE802.3 表驱动，初值 = 1，覆盖 sync~payload。
//  本模块独立，不依赖 modules/parse。
// ════════════════════════════════════════════════════════════════
namespace OtaProtocol {

// ── 命令字 ──
enum Cmd : uint16_t {
    NOP          = 0x00,
    OK           = 0x10,   // ACK 成功标志
    GET_SYNC     = 0x21,   // Bootloader 握手
    GET_DEVICE   = 0x22,   // 读设备信息
    CHIP_ERASE   = 0x23,   // 擦除 APP 区
    CHIP_VERIFY  = 0x24,
    PROG_MULTI   = 0x27,   // 64B/块烧写
    GET_CRC      = 0x29,   // 回算固件 CRC32
    REBOOT       = 0x30,   // 重启
    RESET_MAGIC  = 0x88BB, // 进 Bootloader 指令帧的 id（见 buildEnterBootCmd）

    ACK_CMD      = 0x0064, // 通用命令应答: command(u32)+result(u32)
    ACK_CRC      = 0x753C, // result = 芯片侧固件 CRC32
    ACK_OFFSET   = 0x753D, // result = 已确认烧写 offset
};

constexpr int kBlockSize = 64;       // 烧写块大小（字节）
constexpr int kBootBaud  = 115200;   // Bootloader 兜底波特率（探测失败时用；正常按探测结果）

// ── AT / 文本指令（进 BL 前后用于压停/恢复设备主动输出、读版本）──
constexpr char kCmdSetNo[]   = "AT+SETNO\r\n";        // 让设备停止主动输出
constexpr char kCmdSetYes[]  = "AT+SETYES\r\n";       // 恢复设备主动输出
constexpr char kCmdVersion[] = "AT+VERSION\r\n";      // 读版本（应答原样返回，不解析）
constexpr char kCmdStay[]    = "HC32MCU_FORSENSE";    // Bootloader 停留指令

// ── CRC32（初值 1，与参考工具 crc_crc32 一致）──
uint32_t crc32(const uint8_t *data, int len, uint32_t init = 1);

// 计算整个固件文件的 CRC32（分块读取）；失败返回 0 并填 err
uint32_t crc32File(const QString &path, QString *err = nullptr);

// ── 组帧（下行，帧头 55 AA）──
QByteArray buildLongCmd(uint16_t cmd);                       // 34B 长命令，参数全 0
QByteArray buildDataFrame(uint32_t offset, uint16_t size,
                          const uint8_t *data);              // 81B 数据帧
QByteArray buildEnterBootCmd();                               // 34B 进 Bootloader 固定帧（id=0x88BB）

// 从固件文件名提取 "#CRC<十进制>" 中的 CRC32；未找到返回 0
uint32_t extractCrcFromFileName(const QString &fileName);

} // namespace OtaProtocol

#endif // OTAPROTOCOL_H
