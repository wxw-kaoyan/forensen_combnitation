#ifndef PROTOCOLDEFS_H
#define PROTOCOLDEFS_H

#include <cstdint>
#include <QString>

// ── 帧 ID 枚举 ──
enum class ProtocolID : uint16_t {
    AHRS_Basic       = 0x0002,  // AHRS 基本模式 (payload 44B)
    AHRS_Extended    = 0x0022,  // AHRS 扩展模式 (payload 49B)
    Quaternion       = 0x0023,  // 四元数模式     (payload 53B)
    AG_Data          = 0x0156,  // AG 数据        (payload 50B)
    FusionResult     = 0x0166,  // 组合导航坐标系融合结果 (payload 94B)
    Nav_Append       = 0x0167,  // 组合导航附加数据 NAV_APPEND (每帧 148B, 数据区 6~143, 校验 144~147)
    TiltMeasure      = 0x8166,  // 倾斜测量      (payload 105B, 内部区分值, 线协议同0x0166)
    Laser            = 0x0146,  // 激光协议      (payload 76B)
    NanfangLaser     = 0x0140,  // 南方激光      (payload 73B, 累加和校验)
    ZhdLaser         = 0x0446,  // 中海达激光    (payload 82B, 累加和校验)
    Attitude         = 0x0168,  // 姿态协议      (payload 36B)
    Quat614P         = 0x0199,  // 614P-pro_四元数(payload 46B, 累加和校验)
    Laser_Calib      = 0x0200,  // 激光标定      (payload 377B, 累加和校验)
    // ── DEBUG 调试数据流 (FORSENSE 619, 0x51~0x56) ──
    Debug_IMU        = 0x0051,  // 组合导航+IMU原始 (payload 86B)
    Debug_GPS        = 0x0052,  // GNSS/RTK 定位    (payload 59B)
    Debug_DRTK       = 0x0053,  // 双天线 RTK       (payload 24B)
    Debug_OD         = 0x0054,  // 轮速/里程计      (payload 15B)
    Debug_KF         = 0x0055,  // 卡尔曼滤波调试量 (payload 172B)
    Debug_Param      = 0x0056,  // 固定参数         (payload 140B)
    // ── 614E-B 调试数据流 (0x0101/0x0111/0x0131) ──
    Eb614_IMU        = 0x0101,  // 614E-B IMU原始数据   (payload 51B)
    Eb614_GPS        = 0x0111,  // 614E-B GNSS定位      (payload 71B)
    Eb614_KF         = 0x0131,  // 614E-B KF在线调试量  (payload 208B)
    // ── 凯芯调试数据流 (0x0109~0x0114, 通过外部 Python 调用 decode_kaixin.mexw64 解析) ──
    Kaixin_Data      = 0x0109,  // 凯芯数据（综合解析，输出单一 CSV）
    Can_RTK          = 0xCD01,  // CAN RTK数据 (CAN ID=0x1CEBFF9A) 内部区分值
    // ── 联适 CAN 数据流 ──
    LsNav_Gps        = 0xCD02,  // 联适DEBUG数据流 0x1CFF9B9A 包4~11 (GNSS/导航 20Hz) 内部区分值
    LsNav_Imu        = 0xCD03,  // 联适DEBUG数据流 0x1CFF9B9A 包1~3  (IMU原始 100Hz) 内部区分值
    LsNav_Param      = 0xCD04,  // 联适DEBUG数据流 0x1CFF9B9A 包12~14(参数/版本) 内部区分值
    LsImuLog         = 0xCD05,  // 联适排查数据帧 0x1FFFFF9A (6包, 滤波器内部状态 1Hz)
    A3_Imu           = 0xCD06,  // A3 IMU 异常测试协议 (CANFD ID=0x272, 64B, 小端, 500Hz)
    Epson_Imu        = 0xCD07,  // 爱普生 IMU 串口协议 (固定38B帧: 同步2B+数据32B+累加和2B+帧尾2B)
    Ahrs_Aa5         = 0xCD08,  // AHRS 标准协议 (26B帧: AA A5+Index+9×int16+2校验+0D 0A)
    Tx_Att           = 0xCD09,  // 同星/原极 CAN 扩展帧 姿态  (0x19FFCCxx, deg*100)
    Tx_Gyro          = 0xCD0A,  // 同星/原极 CAN 扩展帧 陀螺  (0x19FFCDxx, deg/s*52.0127)
    Tx_Accel         = 0xCD0B,  // 同星/原极 CAN 扩展帧 加表  (0x19FFCExx, g*3276.8 + 状态位)
    Tx_Sec           = 0xCD0C,  // 同星/原极 CAN 扩展帧 周内秒+安装偏差角+速度方向 (0x19FFCE9B)
    Tx_Pos           = 0xCD0D,  // 同星/原极 CAN 扩展帧 经纬度 (0x19CCFFxx, deg/1e7, int32)
    Tx_Height        = 0xCD0E,  // 同星/原极 CAN 扩展帧 高程+周内秒 (0x19CCFF9B, /1e7, int32)
    Tx_Vel           = 0xCD0F,  // 同星/原极 CAN 扩展帧 速度 (0x19FFD2xx, cm/s)
    // ── 诺瓦泰 NovAtel OEM 二进制 (AA 44 12 + MsgID) ──
    Novatel_BestPos  = 0xE042,  // BESTPOSB 最佳位置 (MsgID=42,  payload 72B)
    Novatel_PsrVel   = 0xE064,  // PSRVELB  伪距速度 (MsgID=100, payload 44B)
    // ── AimRT（MCAP）录包 — 非帧格式，通过外部 Python 脚本导出 IMU 数据 ──
    Aimrt_ImuBag     = 0xE100,  // AimRT 录包 IMU (MCAP，每话题一个 CSV)
    Unknown          = 0xFFFF,
};

// 联适 DEBUG 数据流 0x1CFF9B9A 的三个分组（按包号拆分）
inline bool isLsNavProtocol(ProtocolID pid) {
    uint16_t v = static_cast<uint16_t>(pid);
    return v >= 0xCD02 && v <= 0xCD04;
}

// DEBUG 调试数据流是否属于 0x51~0x56
inline bool isDebugProtocol(ProtocolID pid) {
    uint16_t v = static_cast<uint16_t>(pid);
    return v >= 0x0051 && v <= 0x0056;
}

// 614E-B 调试数据流 0x0101/0x0111/0x0131
inline bool isEb614Protocol(ProtocolID pid) {
    uint16_t v = static_cast<uint16_t>(pid);
    return v == 0x0101 || v == 0x0111 || v == 0x0131;
}

// 凯芯数据流 0x0109~0x0114
inline bool isKaixinProtocol(ProtocolID pid) {
    uint16_t v = static_cast<uint16_t>(pid);
    return v >= 0x0109 && v <= 0x0114;
}

// AimRT（MCAP）录包 — 非帧格式，走外部 Python 脚本
inline bool isAimrtBagProtocol(ProtocolID pid) {
    return pid == ProtocolID::Aimrt_ImuBag;
}

// ════════════════════════════════════════════════════════════════
//  同星/原极 CAN 扩展帧 (《通讯协议》1.4 节 表9~表15)
//  29 位扩展帧 ID 拆解：前缀(19FF/19CC) | 数据组 | 设备地址
//  末字节为设备地址，手册写 9A/9B，实测设备为 C4（可随节点变化），
//  匹配时只按数据组比较，具体设备地址由解析器自动识别后过滤。
// ════════════════════════════════════════════════════════════════
static constexpr uint32_t kTxCanIdMask        = 0xFFFFFF00u;  // 只比高 24 位，忽略设备地址
static constexpr uint32_t kTxAttCanIdBase     = 0x19FFCC00u;  // 表9  姿态
static constexpr uint32_t kTxGyroCanIdBase    = 0x19FFCD00u;  // 表10 陀螺
static constexpr uint32_t kTxAccelCanIdBase   = 0x19FFCE00u;  // 表11/表12 加表 或 周内秒(按设备地址区分)
static constexpr uint32_t kTxVelCanIdBase     = 0x19FFD200u;  // 表15 速度
static constexpr uint32_t kTxPosCanIdBase     = 0x19CCFF00u;  // 表13/表14 经纬度 或 高程(按设备地址区分)

inline bool isTxAttCanId(uint32_t canId)   { return (canId & kTxCanIdMask) == kTxAttCanIdBase; }
inline bool isTxGyroCanId(uint32_t canId)  { return (canId & kTxCanIdMask) == kTxGyroCanIdBase; }
inline bool isTxAccelCanId(uint32_t canId) { return (canId & kTxCanIdMask) == kTxAccelCanIdBase; }
inline bool isTxVelCanId(uint32_t canId)   { return (canId & kTxCanIdMask) == kTxVelCanIdBase; }
inline bool isTxPosCanId(uint32_t canId)   { return (canId & kTxCanIdMask) == kTxPosCanIdBase; }

// 同星/原极 CAN 七组（姿态/陀螺/加表/秒标志/经纬度/高程/速度）
inline bool isTxProtocol(ProtocolID pid) {
    uint16_t v = static_cast<uint16_t>(pid);
    return v >= 0xCD09 && v <= 0xCD0F;
}

inline bool isFusionResult(uint16_t id, uint16_t len) {
    return id == 0x0166 && len == 0x005E;  // 94
}
inline bool isTiltMeasure(uint16_t id, uint16_t len) {
    return id == 0x0166 && len == 0x0069;  // 105
}
inline ProtocolID resolveProtocolID(uint16_t id, uint16_t len) {
    if (id == 0x0166) {
        if (len == 0x005E) return ProtocolID::FusionResult;
        if (len == 0x0069) return ProtocolID::TiltMeasure;
    }
    switch (id) {
        case 0x0002: return ProtocolID::AHRS_Basic;
        case 0x0022: return ProtocolID::AHRS_Extended;
        case 0x0023: return ProtocolID::Quaternion;
        case 0x0156: return ProtocolID::AG_Data;
        case 0x0167: return ProtocolID::Nav_Append;
        case 0x0146: return ProtocolID::Laser;
        case 0x0140: return ProtocolID::NanfangLaser;
        case 0x0446: return ProtocolID::ZhdLaser;
        case 0x0168: return ProtocolID::Attitude;
        case 0x0199: return ProtocolID::Quat614P;
        case 0x0200: return ProtocolID::Laser_Calib;
        case 0x0051: return ProtocolID::Debug_IMU;
        case 0x0052: return ProtocolID::Debug_GPS;
        case 0x0053: return ProtocolID::Debug_DRTK;
        case 0x0054: return ProtocolID::Debug_OD;
        case 0x0055: return ProtocolID::Debug_KF;
        case 0x0056: return ProtocolID::Debug_Param;
        case 0x0101: return ProtocolID::Eb614_IMU;
        case 0x0111: return ProtocolID::Eb614_GPS;
        case 0x0131: return ProtocolID::Eb614_KF;
        default:     return ProtocolID::Unknown;
    }
}

inline const char* protocolName(ProtocolID pid) {
    switch (pid) {
        case ProtocolID::AHRS_Basic:    return "AHRS基本模式";
        case ProtocolID::AHRS_Extended: return "AHRS扩展模式";
        case ProtocolID::Quaternion:    return "四元数模式";
        case ProtocolID::AG_Data:       return "AG数据";
        case ProtocolID::FusionResult:  return "组合导航(融合)";
        case ProtocolID::Nav_Append:    return "组合导航附加(NAV_APPEND)";
        case ProtocolID::TiltMeasure:   return "倾斜测量";
        case ProtocolID::Laser:         return "激光";
        case ProtocolID::NanfangLaser:  return "南方激光";
        case ProtocolID::ZhdLaser:      return "中海达激光";
        case ProtocolID::Attitude:      return "姿态";
        case ProtocolID::Quat614P:      return "614P-pro_四元数";
        case ProtocolID::Laser_Calib:   return "激光标定(0x0200)";
        case ProtocolID::Debug_IMU:     return "调试-组合导航IMU(0x51)";
        case ProtocolID::Debug_GPS:     return "调试-GNSS定位(0x52)";
        case ProtocolID::Debug_DRTK:    return "调试-双天线RTK(0x53)";
        case ProtocolID::Debug_OD:      return "调试-轮速(0x54)";
        case ProtocolID::Debug_KF:      return "调试-卡尔曼滤波(0x55)";
        case ProtocolID::Debug_Param:   return "调试-参数(0x56)";
        case ProtocolID::Eb614_IMU:     return "614E-B-IMU(0x0101)";
        case ProtocolID::Eb614_GPS:     return "614E-B-GNSS(0x0111)";
        case ProtocolID::Eb614_KF:      return "614E-B-KF(0x0131)";
        case ProtocolID::Kaixin_Data:   return "凯芯数据(0x0109~0x0114)";
        case ProtocolID::Can_RTK:       return "联试_CAN981";
        case ProtocolID::LsNav_Gps:     return "NAVDATA_联适-GNSS";
        case ProtocolID::LsNav_Imu:     return "NAVDATA_联适-IMU";
        case ProtocolID::LsNav_Param:   return "NAVDATA_联适-参数";
        case ProtocolID::LsImuLog:      return "IMULOG_联适";
        case ProtocolID::Novatel_BestPos: return "BESTPOS_诺瓦泰(42)";
        case ProtocolID::Aimrt_ImuBag:  return "AimRT IMU 录包(MCAP)";
        case ProtocolID::Novatel_PsrVel:  return "PSRVEL_诺瓦泰(100)";
        case ProtocolID::Epson_Imu:       return "爱普生IMU";
        case ProtocolID::Ahrs_Aa5:        return "AHRS(AA A5)";
        case ProtocolID::Tx_Att:          return "同星CAN-姿态(0x19FFCCxx)";
        case ProtocolID::Tx_Gyro:         return "同星CAN-陀螺(0x19FFCDxx)";
        case ProtocolID::Tx_Accel:        return "同星CAN-加表(0x19FFCExx)";
        case ProtocolID::Tx_Sec:          return "同星CAN-周内秒(0x19FFCE9B)";
        case ProtocolID::Tx_Pos:          return "同星CAN-经纬度(0x19CCFFxx)";
        case ProtocolID::Tx_Height:       return "同星CAN-高程(0x19CCFF9B)";
        case ProtocolID::Tx_Vel:          return "同星CAN-速度(0x19FFD2xx)";
        default:                        return "未知";
    }
}

// ════════════════════════════════════════════════════════════════
//  通用帧头 (固定 6 字节: 0xAA 0x55 + ID + Length)
// ════════════════════════════════════════════════════════════════
struct FrameHeader {
    uint8_t  sync1;
    uint8_t  sync2;
    uint16_t id;
    uint16_t length;  // payload 字节数
};

static constexpr uint8_t  kFrameSync1 = 0xAA;
static constexpr uint8_t  kFrameSync2 = 0x55;
static constexpr int      kHeaderSize = 6;  // sync1+sync2+id+length
static constexpr int      kCrcSize    = 4;

// ════════════════════════════════════════════════════════════════
//  AHRS 基本模式 (ID=0x0002, payload=44B)
// ════════════════════════════════════════════════════════════════
struct AhRsBasic {
    uint32_t timer;   // us
    float    pitch;   // deg
    float    roll;    // deg
    float    yaw;     // deg
    float    ax;      // g
    float    ay;      // g
    float    az;      // g
    float    gx;      // deg/s
    float    gy;      // deg/s
    float    gz;      // deg/s
    float    temp;    // C
};

// ════════════════════════════════════════════════════════════════
//  AHRS 扩展模式 (ID=0x0022, payload=49B)
// ════════════════════════════════════════════════════════════════
struct AhRsExtended {
    AhRsBasic basic;
    uint8_t   flag;    // 0=未扣除偏 1=已扣除偏
    uint32_t  status;  // 状态位
};

// ════════════════════════════════════════════════════════════════
//  四元数模式 (ID=0x0023, payload=53B)
// ════════════════════════════════════════════════════════════════
struct QuaternionData {
    uint32_t timer;   // us
    float    qw;
    float    qx;
    float    qy;
    float    qz;
    float    ax;      // g
    float    ay;      // g
    float    az;      // g
    float    gx;      // deg/s
    float    gy;      // deg/s
    float    gz;      // deg/s
    float    temp;    // C
    uint8_t  flag;    // 0=未扣除偏 1=已扣除偏
    uint32_t status;  // 状态位
};

// ════════════════════════════════════════════════════════════════
//  AG 数据 (ID=0x0156, payload=50B)
// ════════════════════════════════════════════════════════════════
struct AGData {
    uint32_t gpsTime;  // ms
    float    roll;     // deg
    float    pitch;    // deg
    float    yaw;      // deg
    float    reserved;
    float    gyroX;    // deg/s 前
    float    gyroY;    // deg/s 右
    float    gyroZ;    // deg/s 下
    float    accelX;   // g 前
    float    accelY;   // g 右
    float    accelZ;   // g 下
    uint8_t  rtkStatus;
    uint8_t  attValid;
    uint32_t statusFlag;
};

// ════════════════════════════════════════════════════════════════
//  组合导航 — 坐标系融合结果 (ID=0x0166, payload=94B)
// ════════════════════════════════════════════════════════════════
struct FusionResult {
    uint32_t gpsWeekSecond;  // ms
    uint16_t gpsWeek;
    int32_t  lat;            // deg*1e7
    int32_t  lon;            // deg*1e7
    int32_t  alt;            // mm
    float    velN;           // m/s 北向速度
    float    velE;           // m/s 东向速度
    float    velD;           // m/s 地向速度
    float    roll;           // deg
    float    pitch;          // deg
    float    yaw;            // deg
    float    heading;        // deg (AHRS/RTK双天线/前轮)
    float    wheelAngle;     // deg (G200时=前轮转角)
    float    accelX;         // g
    float    accelY;         // g
    float    accelZ;         // g
    float    gyroX;          // deg/s
    float    gyroY;          // deg/s
    float    gyroZ;          // deg/s
    float    temp;           // C
    uint8_t  rtkStatus;      // 0=未定位 1=单点 2=伪距差分 4=固定 5=浮点
    uint8_t  numSatellites;
    uint8_t  diffAge;        // s
    uint8_t  dualAntStatus;  // 50=已定向
    uint16_t posStd;         // cm
    uint16_t statusFlag;     // bit0=RTK有效 bit1=PPS有效 bit2=初始化 bit3=前轮转角 ...
    uint32_t reserved1;
    uint32_t reserved2;
};

// ════════════════════════════════════════════════════════════════
//  NAV_APPEND — 组合导航附加数据 (ID=0x0167)
//  实测帧结构：每帧 148B，10 Hz
//    [0]0xAA [1]0x55 [2..3]ID=0x0167 [4..5]帧长=0x008A(138)
//    数据区 6~143（138B）  [144..147] 校验（低字节在前）
//  数据区偏移（0 基，已用 NAV680D.dat 实测校准）：
//     6   GPS周计数(u16)      8   GPS周内秒(u32,ms)
//    12   纬度(double,deg)    20  经度(double,deg)
//    28   椭球高(f32,m)
//    32   北向速度 / 36 东向速度 / 40 地向速度 (m/s)
//    44   横滚角 / 48 俯仰角 / 52 航向角 (deg)
//    56   纬度标准差 / 60 经度标准差 / 64 椭球高标准差 (m)
//    68   北向速度标准差 / 72 东向速度标准差 / 76 地向速度标准差 (m/s)
//    80   横滚角标准差 / 84 俯仰角标准差 / 88 航向角标准差 (deg)
//    92   加速度计 X/Y/Z (g)  104 陀螺仪 X/Y/Z (deg/s)  116 IMU温度(C)
//   120   GNSS定位状态(u8)    121 GNSS航迹角(f32)        125 主天线卫星数(u8)
//   126   GNSS HDOP(u8)       127 差分龄期(u8)           128 双天线定向周内毫秒(u32)
//   132   双天线定位状态(u8)  133 副天线卫星数(u8)       134 双天线航向(f32)
//   138   状态位(u16)         140 导航状态(u8)           141 预留(3B)
//   144   帧级校验(u32，位于 payload 之外) = CRC32(初值1, 多项式0xEDB88320, 无最终异或, 覆盖 0~143)
// ════════════════════════════════════════════════════════════════
struct NavAppendData {
    uint16_t gpsWeek;          // GPS 周计数
    uint32_t gpsWeekMs;        // GPS 周内秒 (ms)
    double   lat;              // 纬度 (deg)
    double   lon;              // 经度 (deg)
    float    height;           // 椭球高 (m)
    float    vn;               // 北向速度 (m/s)
    float    ve;               // 东向速度 (m/s)
    float    vd;               // 地向速度 (m/s)
    float    roll;             // 横滚角 (deg)
    float    pitch;            // 俯仰角 (deg)
    float    yaw;              // 航向角 (deg)
    float    latStd;           // 纬度标准差 (m)
    float    lonStd;           // 经度标准差 (m)
    float    heightStd;        // 椭球高标准差 (m)
    float    vnStd;            // 北向速度标准差 (m/s)
    float    veStd;            // 东向速度标准差 (m/s)
    float    vdStd;            // 地向速度标准差 (m/s)
    float    rollStd;          // 横滚角标准差 (deg)
    float    pitchStd;         // 俯仰角标准差 (deg)
    float    yawStd;           // 航向角标准差 (deg)
    float    accel[3];         // 加速度计 X/Y/Z (g)
    float    gyro[3];          // 陀螺仪 X/Y/Z (deg/s)
    float    imuTemp;          // IMU 温度 (C)
    uint8_t  gnssPosType;      // GNSS 定位状态 0=未定位 16=单点 17=伪距差分 34=浮点 50=固定
    float    gnssTrackAngle;   // GNSS 航迹角 (deg)
    uint8_t  numSVsMain;       // 主天线卫星数
    uint8_t  hdop;             // GNSS HDOP
    uint8_t  diffAge;          // GNSS 差分龄期
    uint32_t dualWeekMs;       // 双天线 GNSS 定向周内毫秒
    uint8_t  dualPosType;      // 双天线 GNSS 定位状态
    uint8_t  numSVsSlave;      // 副天线卫星数
    float    dualHeading;      // 双天线航向 (deg)
    uint16_t statusBits;       // bit0=RTK数据有效 bit1=PPS有效 bit2=组合导航已初始化
    uint8_t  naviStatus;       // 导航状态 0=未初始化 1=收敛中 2=精度优 3=INS推算 4=精度差
    uint8_t  reserved[3];      // 预留
};

// ════════════════════════════════════════════════════════════════
//  倾斜测量 (ID=0x0166, payload=105B)
// ════════════════════════════════════════════════════════════════
struct TiltMeasure {
    double   lat;          // deg
    double   lon;          // deg
    double   elev;         // m
    int16_t  laserBias[2]; // deg*1000
    uint16_t laserDist;    // mm
    uint8_t  sampleCnt;    // 采点数（收到一帧激光数据则加1）
    uint8_t  coordFlag;    // POS_FLAG: 0=杆底 6=激光点 8=下视光斑
    int16_t  temp;         // 温度 C*100
    uint8_t  rollStd;      // 横滚角标准差 deg*100
    uint8_t  pitchStd;     // 俯仰角标准差 deg*100
    float    tiltRoll;     // 对中杆倾角 deg
    float    tiltPitch;    // 精度因子 deg
    float    gyroRaw[3];   // deg/s
    float    accelRaw[3];  // g
    uint32_t timestamp;    // NMEA=UTC日内毫秒 / Novatel=GPS周内毫秒
    uint32_t sysStatus;    // 系统状态(见表8): 1初始化 3杆长 5杆臂 6标定 9保存 20旋转角 21诺瓦泰 22NMEA
    uint16_t poleHeight;   // 当前杆长 mm（天线相位中心至杆底）
    float    solutionRate; // 标定进度 百分比(-99=未校准)
    uint8_t  gnssFix;      // GNSS定位: 0未定位 1单点 2伪距差分 4固定解 5浮点解
    uint8_t  gnssSatCnt;   // GNSS卫星数量
    uint8_t  rtkAge;       // RTK差分延时 s
    int16_t  poleBias[2];  // 对中杆安装偏差角 deg*1000
    float    azimuth;      // 方位角 deg
    float    rollDeg;      // 横滚角 deg
    float    pitchDeg;     // 俯仰角 deg
    float    yawDeg;       // 航向角 deg
    uint32_t checksum;
};

// ════════════════════════════════════════════════════════════════
//  激光协议 (ID=0x0146, payload=76B)
// ════════════════════════════════════════════════════════════════
struct LaserData {
    float    laserDist;      // m
    float    laserBias[2];   // deg
    double   targetLat;      // deg
    double   targetLon;      // deg
    double   targetElev;     // m
    double   poleLat;        // deg
    double   poleLon;        // deg
    double   poleElev;       // m
    uint8_t  solutionType;   // 1=弱信号 2=浮点解 3=强信号 4=LD欠佳 5=过强 6=硬件 9=未知
    uint32_t timestamp;
    uint8_t  latStd;         // mm
    uint8_t  lonStd;         // mm
    uint8_t  elevStd;        // mm
    uint8_t  reserved[4];
    float    tiltAngle;      // deg
    uint32_t checksum;
};

// ════════════════════════════════════════════════════════════════
//  南方激光 (ID=0x0140, payload=73B, 累加和校验)
// ════════════════════════════════════════════════════════════════
struct NanfangLaserData {
    uint32_t timestamp;       // NMEA=UTC日内毫秒 / Novatel=GPS周内毫秒
    float    laserDist;       // 激光距离 (mm)
    float    roll;            // IMU 横滚 (deg)
    float    pitch;           // IMU 俯仰 (deg)
    float    yaw;             // IMU 航向 (deg)
    double   emitLat;         // 激光发射点纬度 (deg)
    double   emitLon;         // 激光发射点经度 (deg)
    double   emitAlt;         // 激光发射点高程 (m)
    double   centerLat;       // 激光光心纬度 (deg)
    double   centerLon;       // 激光光心经度 (deg)
    double   centerAlt;       // 激光光心高程 (m)
    uint8_t  faultCode;       // 激光故障码
    float    precisionFactor; // 精度因子 (deg)
};

// ════════════════════════════════════════════════════════════════
//  中海达激光 (ID=0x0446, payload=82B, 累加和校验)
// ════════════════════════════════════════════════════════════════
struct ZhdLaserData {
    uint32_t timestamp;       // ms
    float    laserDist;       // 激光距离 (m)
    float    roll;            // IMU 横滚 (deg)
    float    pitch;           // IMU 俯仰 (deg)
    float    yaw;             // IMU 航向 (deg)
    double   laserLat;        // 激光点纬度 (deg)
    double   laserLon;        // 激光点经度 (deg)
    double   laserAlt;        // 激光点高程 (m)
    double   imuLat;          // IMU 纬度 (deg)
    double   imuLon;          // IMU 经度 (deg)
    double   imuAlt;          // IMU 高程 (m)
    uint8_t  laserLatStd;     // 激光点纬度中误差 (mm)
    uint8_t  laserLonStd;     // 激光点经度中误差 (mm)
    uint8_t  laserAltStd;     // 激光点高程中误差 (mm)
    uint8_t  rollStd;         // 横滚中误差 (deg*100)
    uint8_t  pitchStd;        // 俯仰中误差 (deg*100)
    uint8_t  yawStd;          // 航向中误差 (deg*100)
    uint8_t  imuLatStd;       // IMU纬度中误差 (mm)
    uint8_t  imuLonStd;       // IMU经度中误差 (mm)
    uint8_t  imuAltStd;       // IMU高度中误差 (mm)
    uint8_t  laserError;      // 激光错误类型
    float    precisionFactor; // 精度因子 (deg)
};

// ════════════════════════════════════════════════════════════════
//  姿态协议 (ID=0x0168, payload=36B)
// ════════════════════════════════════════════════════════════════
struct AttitudeData {
    uint32_t timestamp;
    float    roll;        // deg
    float    pitch;       // deg
    float    yaw;         // deg
    float    rollStd;     // deg
    float    pitchStd;    // deg
    float    yawStd;      // deg
    float    reserved[2];
    uint32_t checksum;
};

// ════════════════════════════════════════════════════════════════
//  激光标定 (ID=0x0200, payload=377B, 累加和校验)
//  offset: 0 rev(u32) 4 dist_now(u16,mm) 6 dist_time_ms(u32)
//          10 rev(4B) 14 is_calib_point_valid(u8)
//          15 base_pos(f64[3], 纬度/经度/高度, deg/deg/m — 协议文档写的"经纬高"与实测不符)
//          39 calib_point_pos(f32[10][3], NED m)
//          159 calib_point_att(i16[10][3], rad×100, roll/pitch/yaw)
//          219 rev(u16[10]) 239 calib_point_dis(u16[10], mm)
//          259 calib_residual(i16[10][3], NED mm)
//          319 laser_install_att(i16[2], deg×1000, pitch/yaw)
//          323 point_num(u8) 324 rev(53B)
// ════════════════════════════════════════════════════════════════
struct LaserCalibData {
    uint16_t distNow;              // 当前激光距离 (mm)
    uint32_t distTimeMs;           // 激光距离时间戳 (ms)
    uint8_t  calibPointValid;      // 采集点有效性 0=无效 1=有效
    double   baseLon;              // 已知点经度 (deg)
    double   baseLat;              // 已知点纬度 (deg)
    double   baseAlt;              // 已知点高度 (m)
    float    calibPos[10][3];      // 采集点激光 NED 坐标 (m)
    float    calibAtt[10][3];      // 采集点姿态 roll/pitch/yaw (deg; 帧内为 rad×100, 解析时已换算)
    uint16_t calibDist[10];        // 采集点激光距离 (mm)
    int16_t  calibResidual[10][3]; // 采集点残差 NED (mm)
    float    installPitch;         // 激光安装偏差角 pitch (deg, 原始/1000)
    float    installYaw;           // 激光安装偏差角 yaw   (deg, 原始/1000)
    uint8_t  pointNum;             // 实际采集点数 0~10
};

// ════════════════════════════════════════════════════════════════
//  614P-pro 四元数 (ID=0x0199, payload=46B, 累加和校验)
// ════════════════════════════════════════════════════════════════
struct Quat614PData {
    uint32_t gpsWeekMs;      // ms
    double   insLat;         // deg, INS纬度
    double   insLon;         // deg, INS经度
    float    insAlt;         // m,   INS高程
    float    quatW;          // 四元数 w
    float    quatX;          // 四元数 x
    float    quatY;          // 四元数 y
    float    quatZ;          // 四元数 z
    float    imuPrecision;   // IMU精度因子
    uint16_t systemStatus;   // 系统状态
};

// ════════════════════════════════════════════════════════════════
//  DEBUG 调试数据流结构体 (FORSENSE 619, 0x51~0x56)
//  字段布局与 FORSENSE_LIB/sensor.h 严格一致 (小端)
// ════════════════════════════════════════════════════════════════

// 0x51 组合导航 + IMU 原始 (86B)
struct DebugImuData {
    uint32_t time_gps_ms;   // GPS 周内秒(ms)
    uint16_t week_num;      // GPS 周
    float    gyro[3];       // 陀螺 X/Y/Z (deg/s)
    float    accel[3];      // 加表 X/Y/Z (g)
    float    temperature;   // IMU 温度 (C)
    uint32_t status;        // 初始化状态
    float    att_ahrs[3];   // AHRS 姿态 (横滚/俯仰/航向, deg)
    float    att[3];        // 组合导航姿态 (横滚/俯仰/航向, deg)
    float    vel[3];        // 组合导航速度 (北/东/地, m/s)
    int32_t  pos[3];        // 组合导航位置 (纬度×1e7, 经度×1e7, 高度mm)
};

// 0x52 GNSS/RTK 定位 (59B)
struct DebugGpsData {
    uint32_t itow;          // GPS 周内秒(ms)
    uint16_t week_num;      // GPS 周
    uint32_t pps_time_us;   // PPS 触发时刻(us)
    uint32_t revc_time_ms;  // 数据接收时刻(ms)
    uint8_t  fixtype;       // 定位状态
    uint8_t  numSV;         // 参与解算星数
    uint8_t  multiSVs;      // 双频星数
    int32_t  lat;           // 纬度 deg*1e7
    int32_t  lon;           // 经度 deg*1e7
    int32_t  alt;           // 海拔 mm
    float    vn;            // 北向速度 m/s
    float    ve;            // 东向速度 m/s
    float    vd;            // 地向速度 m/s
    float    gspeed;        // 地速 m/s
    float    headMot;       // 运动航向 deg
    int16_t  p_hacc;        // 水平位置精度
    int16_t  p_vacc;        // 垂直位置精度
    int16_t  v_hacc;        // 水平速度精度
    int16_t  v_vacc;        // 垂直速度精度
    uint16_t diff_age;      // 差分龄期
};

// 0x53 双天线 RTK (24B)
struct DebugDrtkData {
    uint32_t itow;          // GPS 周内秒(ms)
    uint8_t  fixtype;       // 定位状态
    float    heading;       // 航向 (deg)
    float    heading_std;   // 航向标准差
    float    pitch;         // 俯仰 (deg)
    float    pitch_std;     // 俯仰标准差
    uint16_t length;        // 基线长度
    uint8_t  svnum;         // 星数
};

// 0x54 轮速/里程计 (15B)
struct DebugOdData {
    uint32_t time_gps_ms;   // GPS 周内秒(ms)
    int16_t  vel_fl_cm_s;   // 前左轮速 (cm/s)
    int16_t  vel_fr_cm_s;   // 前右轮速 (cm/s)
    int16_t  vel_bl_cm_s;   // 后左轮速 (cm/s)
    int16_t  vel_br_cm_s;   // 后右轮速 (cm/s)
    uint8_t  gear;          // 挡位
    int16_t  steer_angle;   // 转向角
};

// 0x55 卡尔曼滤波调试量 (172B)
struct DebugKfData {
    float    pk[18];        // 协方差对角线
    float    imu_bias[6];   // 陀螺/加表零偏
    float    innov[6];      // 量测新息
    uint16_t od_k;          // 里程计刻度
    int16_t  inst_pitch;    // 安装俯仰
    int16_t  inst_yaw;      // 安装航向
    float    time_consume;  // 解算耗时
    uint32_t task_period;   // 任务周期
    uint32_t cpu_usage;     // CPU 占用
    uint16_t RK_p[3];       // 位置量测噪声
    uint16_t Rk_v[3];       // 速度量测噪声
    uint16_t Rk_b[3];       // 零偏量测噪声
    uint32_t rev[4];        // 版本/预留
};

// 0x56 固定参数 (140B)
struct DebugParamData {
    float SETTING;
    float GPS_HACC_FIX;
    float GPS_HACC_SINGLE;
    float GPS_SACC;
    float GPS_HEADING_ACC;
    float GYRO_STD;
    float ACCEL_STD;
    float GYRO_ZUPT_VALUE;
    float MEAS_CONSTRAIN_AHRS;
    float ALIGN_VEL;
    float PARAM_ANTENNA_INST_ANGLE;
    float PARAM_ANTENNA_MODIFY_ANGLE;
    float ANTENNA_LENGTH;
    float PARAM_OD_K;
    float PARAM_INST_ROLL;
    float PARAM_INST_PITCH;
    float PARAM_INST_YAW;
    float PARAM_GPS_POS_X;
    float PARAM_GPS_POS_Y;
    float PARAM_GPS_POS_Z;
    float PARAM_OD_POS_X;
    float PARAM_OD_POS_Y;
    float PARAM_OD_POS_Z;
    float ODR;
    float rev[10];
    float VERSION;
};

// ════════════════════════════════════════════════════════════════
//  614E-B 调试数据流结构体 (0x0101/0x0111/0x0131, 小端, 与固件一致)
// ════════════════════════════════════════════════════════════════

// 0x0101 IMU 原始数据 (51B)
struct Eb614ImuData {
    uint32_t time_gps_ms;   // GPS 周内秒(ms)
    uint16_t week_num;      // GPS 周
    float    gyro[3];       // 陀螺 X/Y/Z (deg/s)
    float    accel[3];      // 加表 X/Y/Z (g)
    float    temperature;   // IMU 温度 (C)
    int16_t  temp_vel;      // 温度变化率
    uint8_t  imu_type;      // 52=614P, 88=614B, 68=614P_PRO
    uint8_t  rtk_type;      // 0=UM980, 1=SINAN K803
    uint8_t  zupt;          // 零速检测标志
    int16_t  qekf_att[3];   // QEKF姿态 roll/pitch/yaw (deg*100)
    int16_t  kf_att[3];     // KF姿态   roll/pitch/yaw (deg*100)
};

// 0x0111 GNSS 定位 (71B)
struct Eb614GpsData {
    uint32_t itow_pos;      // 位置解 ITOW(ms)
    uint16_t week_num;      // GPS 周
    uint32_t pps_time_us;   // PPS 触发 MCU 时刻(us)
    uint32_t revc_time_ms;  // 接收时刻(ms)
    uint8_t  fixtype;       // 定位类型
    uint8_t  numSV;         // 参与解算星数
    uint8_t  multiSVs;      // 多频星数
    double   lat;           // 纬度(deg)
    double   lon;           // 经度(deg)
    int32_t  alt;           // 海拔(m*1e5)
    uint16_t p_hacc;        // 水平位置精度(mm)
    uint16_t p_vacc;        // 垂直位置精度(mm)
    uint16_t diff_age;      // 差分龄期
    uint32_t itow_vel;      // 速度解 ITOW(ms)
    float    vn;            // 北向速度(m/s)
    float    ve;            // 东向速度(m/s)
    float    vd;            // 地向速度(m/s)
    float    gspeed;        // 地速(m/s)
    float    headMot;       // 运动航向(deg)
    uint16_t v_hacc;        // 水平速度精度(mm/s)
    uint16_t v_vacc;        // 垂直速度精度(mm/s)
};

// 0x0131 KF 在线调试量 (208B)
struct Eb614KfData {
    float    pk[21];           // 协方差对角线(21状态)
    float    innov[6];         // 量测新息
    float    att[3];           // 姿态 (roll/pitch/yaw, deg)
    float    vel[3];           // 速度 (N/E/D, m/s)
    double   pos[3];           // 位置 (lat/lon/alt)
    float    gyro_bias[3];     // 陀螺零偏 (deg/s)
    float    acceel_bias[3];   // 加表零偏 (g)
    float    gyro_scale[3];    // 陀螺标度因数 (ppm)
    float    est_lever_arm[3]; // 估计杆臂 (mm)
    uint32_t itow;             // GPS 周内秒(ms)
};

// ════════════════════════════════════════════════════════════════
//  CAN RTK 数据 (CAN ID=0x1CEBFF9A, 5帧×8字节=40字节)
//  所有多字节字段为大端序 (Big Endian)
// ════════════════════════════════════════════════════════════════
struct CanRtkData {
    // 帧序号验证
    uint8_t  frameIdx1, frameIdx2, frameIdx3, frameIdx4, frameIdx5;

    // 时间 (UTC)
    uint16_t utcDays;       // 距1970-01-01的天数
    double   secondsOfDay;  // 天内秒

    // 定位
    uint8_t  fixType;       // 定位类型 0=未定位 1=单点 4=固定解 5=浮点解
    uint8_t  diffAge;       // 差分龄期 (秒)
    uint8_t  numSatellites; // 卫星数

    // 位置
    double   latitude;      // 纬度 (deg)
    double   longitude;     // 经度 (deg)
    double   altitude;      // 海拔 (m)

    // 速度
    double   forwardSpeed;  // 前向速度 (m/s)
    double   speedDirection;// 速度方向/航向 (deg), 0xFFFF=无效

    // 姿态 (0xFFFF原始值=无效)
    bool     rollValid;
    double   roll;          // 横滚角 (deg)
    bool     pitchValid;
    double   pitch;         // 俯仰角 (deg)
    bool     yawValid;
    double   yaw;           // 航向角 (deg)

    // 原始值
    uint64_t latRaw;
    uint64_t lonRaw;
    uint32_t altRaw;
    uint16_t velRaw;
    uint16_t sdRaw;
    uint16_t rollRaw;
    uint16_t pitchRaw;
    uint16_t yawRaw;
};

// ════════════════════════════════════════════════════════════════
//  联适 DEBUG 数据流 (CAN ID=0x1CFF9B9A) — 14 包，多字节大端
//  包1~3: IMU 原始 (100Hz) / 包4~11: GNSS+导航 (20Hz) / 包12~14: 参数 (0.1Hz)
// ════════════════════════════════════════════════════════════════
struct LsNavImuData {
    uint32_t timeGpsMs;     // GPS 周内毫秒
    uint16_t weekNumImu;    // IMU GPS 周
    double   accel[3];      // 加表 (g)     = raw / 3276.8
    double   gyro[3];       // 陀螺 (deg/s) = raw / 52.0127
    double   temperature;   // IMU 温度 (C) = raw / 10
    uint8_t  kfInitDone;
};

struct LsNavGpsData {
    uint32_t itow;          // GPS 周内毫秒
    uint16_t weekNumGps;
    uint8_t  fixType;       // 50=窄巷固定
    uint32_t revcTimeMs;
    uint8_t  numSV;
    uint8_t  multiSVs;
    double   lat;           // 纬度 (deg)   = raw / 1e7
    double   lon;           // 经度 (deg)   = raw / 1e7
    double   alt;           // 海拔 (m)     = raw / 1000
    double   vn;            // 北向速度 (m/s) = raw / 1000
    double   ve;            // 东向速度 (m/s)
    double   vd;            // 地向速度 (m/s)
    double   gspeed;        // 地速 (m/s)
    double   headMot;       // 运动航向 (deg) = raw / 1000
    int16_t  pHacc;
    int16_t  pVacc;
    int16_t  vHacc;
    int16_t  llExtend;
    uint16_t diffAge;
    uint8_t  kfIndexGet;
};

struct LsNavParamData {
    int16_t  gpsPosX, gpsPosY, gpsPosZ;  // 主天线杆臂 (m) = raw / 100
    uint8_t  rtkFlag;
    int16_t  odPosX, odPosY, odPosZ;     // 后轮中心杆臂 (m) = raw / 100
    uint32_t version;
    uint32_t resetReason;                // 单片机重启原因保存结果
};

// ════════════════════════════════════════════════════════════════
//  联适排查数据帧 (CAN ID=0x1FFFFF9A, 6×8B, 1Hz) — 大端
//  滤波器/IMU 内部状态，用于排查航向、零偏、静态检测等问题
// ════════════════════════════════════════════════════════════════
struct LsImuLogData {
    uint8_t  initStatus;        // 初始化状态
    uint8_t  filterSelect;      // 滤波器筛选值
    double   initHeading;       // 初始化的航向角 (deg) = raw/100
    uint8_t  deviceStatus;      // 设备状态
    uint8_t  betposGapCnt;      // 1s 内 betpos 间隔>200ms 的数量
    uint8_t  psrvelGapCnt;      // 1s 内 psrvel 间隔>200ms 的数量
    uint32_t zuptCnt;           // ZUPT 计数值
    uint8_t  staticFlag;        // 动静态标记
    uint32_t straightCnt;       // 直行计数值
    double   yawInstallBias;    // 航向安装偏差角 (deg) = raw/100
    uint8_t  betposCnt;         // 1s 内解析的 betpos 数量
    double   gyroZBias;         // 陀螺 z 轴零偏 (deg/h)
    uint16_t delayMs;           // 延迟值 (ms)
    uint8_t  psrvelCnt;         // 1s 内解析的 psrvel 数量
    uint32_t imuStaticCnt;      // IMU 静态计数值
    uint8_t  filterState;       // 滤波器状态
    uint8_t  resetFlag;         // 重置标记位
    double   pitchInstallBias;  // 俯仰安装偏差角 (deg) = raw/100
    double   resN;              // 北向残差 (m) = raw/100
    double   resE;              // 东向残差 (m)
    double   resU;              // 天向残差 (m)
};

// ════════════════════════════════════════════════════════════════
//  诺瓦泰 NovAtel OEM 二进制 (同步头 AA 44 12, 28B 头 + payload + CRC32)
// ════════════════════════════════════════════════════════════════
struct NovatelBestPosData {
    uint16_t gpsWeek;
    uint32_t gpsWeekMs;
    int32_t  solStatus;      // 解状态
    int32_t  posType;        // 定位类型
    double   lat;            // 纬度 (deg)
    double   lon;            // 经度 (deg)
    double   height;         // 椭球高 (m)
    double   undulation;     // 高程异常 (m)
    int32_t  datumId;        // 61 = WGS84
    double   latStd;         // 纬度标准差 (m)
    double   lonStd;         // 经度标准差 (m)
    double   heightStd;      // 高程标准差 (m)
    double   diffAge;        // 差分龄期 (s)
    double   solAge;         // 解龄期 (s)
    uint8_t  numSVs;         // 可见星数
    uint8_t  numSolnSVs;     // 参与解算星数
    uint8_t  numSolnL1SVs;   // L1 参与解算星数
    uint8_t  numSolnMultiSVs;// 多频参与解算星数
};

struct NovatelPsrVelData {
    uint16_t gpsWeek;
    uint32_t gpsWeekMs;
    int32_t  solStatus;      // 解状态
    int32_t  velType;        // 速度类型 8=DOPPLER_VELOCITY
    double   latency;        // 延迟 (s)
    double   diffAge;        // 差分龄期 (s)
    double   horSpeed;       // 水平速度 (m/s)
    double   trackGround;    // 地面航向 (deg)
    double   verSpeed;       // 垂直速度 (m/s)
};

// ════════════════════════════════════════════════════════════════
//  A3 IMU 异常测试协议 (CANFD ID=0x272, 64B, 小端, 500Hz)
//  帧 17 字段：Cycle / GPSWeek / GPSWeekMs / GYR[3] / ACC[3] / Temp /
//              Q[4] / RunStatus / 10 监控标志位 / CRC
// ════════════════════════════════════════════════════════════════
struct A3ImuData {
    uint8_t  cycle;          // 循环计数 0-255
    uint16_t gpsWeek;        // GPS 周
    uint32_t gpsWeekMs;      // GPS 周内毫秒
    float    gyro[3];        // 陀螺 X/Y/Z (deg/s)
    float    accel[3];       // 加表 X/Y/Z (m/s²)
    float    temperature;    // 温度 (C)（已 ×0.01 解码）
    float    quat[4];        // 四元数 Q0/Q1/Q2/Q3
    uint8_t  runStatus;      // 运行状态位 (bit0~5)
    uint8_t  flags[10];      // 10 监控标志位 (byte50~59)
    uint32_t crc;            // CRC（接收值）
};

// ════════════════════════════════════════════════════════════════
//  爱普生 IMU 串口协议 (固定 38B 帧, 数据区 32B 小端 int16)
//  帧: SYNC1=0x80 SYNC2=0x01 | 数据区32B | 校验和2B(低字节在前) | END=0x0D 0x0A
//  数据区: [0..1]计数 [2..3]状态 [4..9]陀螺 [10..15]加表
//          [16..17]俯仰 [18..19]横滚 [20..21]航向 [22..23]温度 [24..31]四元数
// ════════════════════════════════════════════════════════════════
struct EpsonImuData {
    uint16_t counter;        // 帧计数/时间戳 (数据区[0..1], 实测步长 625)
    int16_t  status;         // 状态字 (数据区[2..3], 实测恒为 3)
    float    gyro[3];        // 陀螺 X/Y/Z (原始/64, deg/s)
    float    accel[3];       // 加表 X/Y/Z (原始/1000, g)
    float    pitch;          // 俯仰 (原始/100, deg)
    float    yaw;            // 航向 (原始/100, deg)
    float    roll;           // 横滚 (原始/100, deg)
    float    temperature;    // 温度 (原始/100, °C) — 数据区[22..23]
    float    quat[4];        // 四元数 W/X/Y/Z (原始/30000) — 数据区[24..31]
    uint16_t checksum;       // 接收校验和（低字节在前）
    uint16_t checksumCalc;   // 计算的 32B 累加和
    double   timeSec;        // 相对时间(s)：counter 解 16 位回绕后 × 16µs(625 tick = 10ms = 100Hz)
};

// ════════════════════════════════════════════════════════════════
//  AHRS 标准协议（《IMU规格标准》）— 26 字节定长帧
//  AA A5 | Index | Roll | Pitch | Yaw | RateX/Y/Z | AccX/Y/Z | CS_A | CS_B | 0D 0A
//  数值均为 int16 小端；角度 ×100(deg)、角速度 ×100(deg/s)、加速度 ×1000(g)
//  固件 1683 输出顺序：r,p,y,gy,-gx,gz,ay,-ax,az（1675 为 gx,gy,gz,ax,ay,az）
//  解析器已按 1683 还原为物理轴 gx/gy/gz、ax/ay/az
//  ChecksumA(u16 LE) = (int16)Yaw + (int16)RateZ
//  ChecksumB(u8)     = Index + Pitch + Roll + RateX + RateY + AccX + AccY + AccZ 各字节和
// ════════════════════════════════════════════════════════════════
struct AhrsAa5Data {
    uint8_t   index;          // 帧序号，每帧 +1（u8 回绕）
    uint32_t  globalIndex;    // 累计全局序号，从 1 开始不回绕
    float     roll;           // 横滚 (°，±180)
    float     pitch;          // 俯仰 (°，±90)
    float     yaw;            // 航向 (°，±180)
    float     rate[3];        // 角速度 X/Y/Z (deg/s)
    float     accel[3];       // 加速度 X/Y/Z (g)
    uint16_t  checksumA;      // 接收校验 A
    uint16_t  checksumACalc;  // 计算校验 A
    uint8_t   checksumB;      // 接收校验 B
    uint8_t   checksumBCalc;  // 计算校验 B
};

// ════════════════════════════════════════════════════════════════
//  同星 CAN 扩展帧 — 表9 姿态 (0x19FFCCxx, payload 8B)
//  字节布局: [1..2]横滚 [3..4]俯仰 [5..6]航向 [7..8]航迹, int16 小端
//  单位 deg*100（raw / 100 = deg）
// ════════════════════════════════════════════════════════════════
struct TxAttData {
    double   ascTime;    // ASC 行首时间戳 (s, 相对录制起点)
    double   roll;       // 横滚角 (deg)
    double   pitch;      // 俯仰角 (deg)
    double   yaw;        // 航向角 (deg)
    double   track;      // 航迹角 (deg)
    int16_t  raw[4];     // 原始值 (deg*100)
};

// ════════════════════════════════════════════════════════════════
//  同星 CAN 扩展帧 — 表10 陀螺 (0x19FFCDxx, payload 8B)
//  字节布局: [1..2]gyro_x [3..4]gyro_y [5..6]gyro_z [7..8]预留, int16 小端
//  单位 deg/s*52.0127（raw / 52.0127 = deg/s）
//  预留位：接 G200 时输出前轮角度
// ════════════════════════════════════════════════════════════════
struct TxGyroData {
    double   ascTime;    // ASC 行首时间戳 (s, 相对录制起点)
    double   gyro[3];    // 陀螺 X/Y/Z (deg/s)
    int16_t  reserved;   // 预留；接 G200 时为前轮转角
    int16_t  raw[3];     // 原始值 (deg/s*52.0127)
};

// ════════════════════════════════════════════════════════════════
//  同星 CAN 扩展帧 — 表11 加表 (0x19FFCExx, payload 8B)
//  字节布局: [1..2]acc_x [3..4]acc_y [5..6]acc_z [7..8]状态位, int16 小端
//  单位 g*3276.8（raw / 3276.8 = g）
//  状态位: bit0=RTK板卡数据有效 bit1=PPS有效 bit2=前轮转角有效
//          bit3=前轮陀螺有效 bit4=RTK定位固定解 bit5=RTK定向固定解
//          bit6=航向初始化标志(仅单天线车身陀螺)
// ════════════════════════════════════════════════════════════════
struct TxAccelData {
    double   ascTime;    // ASC 行首时间戳 (s, 相对录制起点)
    double   accel[3];   // 加表 X/Y/Z (g)
    uint16_t status;     // 状态位
    int16_t  raw[3];     // 原始值 (g*3276.8)
};

// ════════════════════════════════════════════════════════════════
//  同星/原极 CAN 扩展帧 — 表12 周内秒/安装偏差/速度方向 (0x19FFCE9B)
//  字节布局: [1..4]周内秒(ms,int32) [5..6]估计天线安装偏差角 [7]速度方向 [8]预留
//  备注：速度方向 1=前进 −1=后退；
//        安装偏差角文档未标注标度，此处保留原始 int16
// ════════════════════════════════════════════════════════════════
struct TxSecData {
    double   ascTime;       // ASC 行首时间戳 (s, 相对录制起点)
    uint32_t weekMs;        // 周内秒 (ms)
    int16_t  antBiasRaw;    // 估计天线安装偏差角 (原始值)
    int8_t   velDirection;  // 速度方向 1=前进 -1=后退
    uint8_t  reserved;      // 预留
};

// ════════════════════════════════════════════════════════════════
//  同星/原极 CAN 扩展帧 — 表13 经纬度 (0x19CCFFxx)
//  字节布局: [1..4]纬度(int32) [5..8]经度(int32)，单位 deg，raw/1e7
//  注：文档表头写作「经度 | 纬度」，但实测数据（31.26N / 121.62E 上海）
//      为「纬度 | 经度」，此处按实测顺序解码，raw 值同时输出供核对
//  坐标系 WGS84，默认输出主天线相位中心位置
// ════════════════════════════════════════════════════════════════
struct TxPosData {
    double   ascTime;    // ASC 行首时间戳 (s, 相对录制起点)
    double   lon;        // 经度 (deg)
    double   lat;        // 纬度 (deg)
    int32_t  lonRaw;     // 原始值 (deg*1e7)
    int32_t  latRaw;     // 原始值 (deg*1e7)
};

// ════════════════════════════════════════════════════════════════
//  同星/原极 CAN 扩展帧 — 表14 高程+周内秒 (0x19CCFF9B)
//  字节布局: [1..4]高程(int32, 椭球高) [5..8]周内秒(int32, ms)
//  高程单位 m，raw/1e7
// ════════════════════════════════════════════════════════════════
struct TxHeightData {
    double   ascTime;    // ASC 行首时间戳 (s, 相对录制起点)
    double   height;     // 椭球高 (m)
    uint32_t weekMs;     // 周内秒 (ms)
    int32_t  heightRaw;  // 原始值 (m*1e7)
};

// ════════════════════════════════════════════════════════════════
//  同星/原极 CAN 扩展帧 — 表15 速度 (0x19FFD2xx)
//  字节布局: [1..2]北向 [3..4]东向 [5..6]地向 [7..8]地速(平面), int16
//  单位 cm/s，raw/100 = m/s
// ════════════════════════════════════════════════════════════════
struct TxVelData {
    double   ascTime;    // ASC 行首时间戳 (s, 相对录制起点)
    double   vn;         // 北向速度 (m/s)
    double   ve;         // 东向速度 (m/s)
    double   vd;         // 地向速度 (m/s)
    double   gspeed;     // 地速/平面速度 (m/s)
    int16_t  raw[4];     // 原始值 (cm/s)
};

// 诺瓦泰定位类型名称（posType，联适 DEBUG 流与本机 OEM 二进制共用同一张表）
inline QString novatelPosTypeName(int32_t v)
{
    switch (v) {
        case 0:  return QStringLiteral("NONE");
        case 1:  return QStringLiteral("FIXEDPOS");
        case 2:  return QStringLiteral("FIXEDHEIGHT");
        case 16: return QStringLiteral("单点(SINGLE)");
        case 17: return QStringLiteral("伪距差分(PSRDIFF)");
        case 18: return QStringLiteral("SBAS(WAAS)");
        case 32: return QStringLiteral("L1浮点(L1_FLOAT)");
        case 33: return QStringLiteral("无电离层浮点(IONOFREE_FLOAT)");
        case 34: return QStringLiteral("窄巷浮点(NARROW_FLOAT)");
        case 48: return QStringLiteral("L1固定(L1_INT)");
        case 49: return QStringLiteral("宽巷固定(WIDE_INT)");
        case 50: return QStringLiteral("窄巷固定(NARROW_INT)");
        case 69: return QStringLiteral("PPP");
        default: return QString::number(v);
    }
}

// 诺瓦泰解状态名称（solStatus）
inline QString novatelSolStatusName(int32_t v)
{
    switch (v) {
        case 0:  return QStringLiteral("SOL_COMPUTED");
        case 1:  return QStringLiteral("INSUFFICIENT_OBS");
        case 2:  return QStringLiteral("NO_CONVERGENCE");
        case 3:  return QStringLiteral("SINGULARITY");
        case 4:  return QStringLiteral("COV_TRACE");
        case 5:  return QStringLiteral("TEST_DIST");
        case 6:  return QStringLiteral("COLD_START");
        case 7:  return QStringLiteral("V_H_LIMIT");
        case 8:  return QStringLiteral("VARIANCE");
        case 9:  return QStringLiteral("RESIDUALS");
        case 10: return QStringLiteral("INTEGRITY_WARNING");
        case 13: return QStringLiteral("PENDING");
        case 18: return QStringLiteral("INVALID_FIX");
        default: return QString::number(v);
    }
}

// 诺瓦泰速度类型名称（velType）
inline QString novatelVelTypeName(int32_t v)
{
    switch (v) {
        case 0: return QStringLiteral("NO_VELOCITY");
        case 8: return QStringLiteral("DOPPLER_VELOCITY");
        default: return QString::number(v);
    }
}

// ════════════════════════════════════════════════════════════════
//  通用解析结果
// ════════════════════════════════════════════════════════════════
struct ParsedFrame {
    ProtocolID protocol;
    uint16_t   frameId;
    uint16_t   payloadLength;
    uint32_t   crc32;           // 接收到的 CRC
    uint32_t   crc32Expected;   // 计算出的 CRC
    bool       crcOk;
    int        offsetInFile;    // 帧在文件中的起始位置

    union {
        AhRsBasic      ahrsBasic;
        AhRsExtended   ahrsExtended;
        QuaternionData quat;
        AGData         agData;
        FusionResult   fusion;
        NavAppendData  navAppend;
        TiltMeasure    tilt;
        LaserData      laser;
        NanfangLaserData nanfangLaser;
        ZhdLaserData   zhdLaser;
        AttitudeData   attitude;
        Quat614PData   quat614p;
        LaserCalibData laserCalib;
        CanRtkData     canRtk;
        LsNavImuData   lsNavImu;
        LsNavGpsData   lsNavGps;
        LsNavParamData lsNavParam;
        LsImuLogData   lsImuLog;
        NovatelBestPosData novatelBestPos;
        NovatelPsrVelData  novatelPsrVel;
        A3ImuData          a3Imu;
        EpsonImuData       epsonImu;
        AhrsAa5Data        ahrsAa5;
        TxAttData          txAtt;
        TxGyroData         txGyro;
        TxAccelData        txAccel;
        TxSecData          txSec;
        TxPosData          txPos;
        TxHeightData       txHeight;
        TxVelData          txVel;
        DebugImuData   debugImu;
        DebugGpsData   debugGps;
        DebugDrtkData  debugDrtk;
        DebugOdData    debugOd;
        DebugKfData    debugKf;
        DebugParamData debugParam;
        Eb614ImuData   eb614Imu;
        Eb614GpsData   eb614Gps;
        Eb614KfData    eb614Kf;
    };
};

// ════════════════════════════════════════════════════════════════
//  RTCM 3.x 帧定义
// ════════════════════════════════════════════════════════════════
static constexpr uint8_t  kRtcmPreamble = 0xD3;
static constexpr int      kRtcmHeaderSize = 3;   // preamble + reserved + length
static constexpr int      kRtcmCrcSize    = 3;   // CRC-24Q (3 bytes)

struct RtcmHeader {
    uint8_t  preamble;        // 0xD3
    uint16_t payloadLength;   // 10-bit, 0-1023, 不含帧头和CRC
};

struct RtcmFrame {
    int      offsetInFile;
    uint16_t messageType;     // 12-bit 消息类型 (e.g. 1004, 1005, 1077, MSM4...)
    uint16_t payloadLength;   // 变长数据字节数
    uint32_t crcReceived;     // 接收到的 CRC-24Q
    uint32_t crcExpected;     // 计算出的 CRC-24Q
    bool     crcOk;
};

// CRC-24Q 参数 (多项式 0x864CFB)
uint32_t computeRtcmCrc24Q(const uint8_t *data, int len);

// 常见 RTCM 消息类型名称
inline const char* rtcmMessageName(uint16_t msgType) {
    switch (msgType) {
        case 1001: return "GPS L1观测值";
        case 1002: return "GPS L1扩展观测值";
        case 1003: return "GPS L1/L2观测值";
        case 1004: return "GPS L1/L2扩展观测值";
        case 1005: return "基准站坐标(无高度)";
        case 1006: return "基准站坐标(含天线高)";
        case 1007: return "天线描述符";
        case 1008: return "天线描述符+序列号";
        case 1010: return "GLONASS L1观测值";
        case 1011: return "GLONASS L1扩展";
        case 1012: return "GLONASS L1/L2观测值";
        case 1013: return "GLONASS L1/L2扩展";
        case 1019: return "GPS星历";
        case 1020: return "GLONASS星历";
        case 1029: return "Unicode文本";
        case 1033: return "接收机/天线描述";
        case 1042: return "BDS星历";
        case 1045: return "Galileo F/NAV星历";
        case 1046: return "Galileo I/NAV星历";
        case 1074: return "GPS MSM4(全周期)";
        case 1075: return "GPS MSM5(全周期+扩展)";
        case 1077: return "GPS MSM7(全周期+高精度)";
        case 1084: return "GLONASS MSM4";
        case 1085: return "GLONASS MSM5";
        case 1087: return "GLONASS MSM7";
        case 1094: return "Galileo MSM4";
        case 1097: return "Galileo MSM7";
        case 1104: return "SBAS MSM4";
        case 1114: return "QZSS MSM4";
        case 1124: return "BDS MSM4";
        case 1125: return "BDS MSM5";
        case 1127: return "BDS MSM7";
        case 1230: return "GLONASS码相位偏差";
        default:   return "未知";
    }
}

#endif // PROTOCOLDEFS_H
