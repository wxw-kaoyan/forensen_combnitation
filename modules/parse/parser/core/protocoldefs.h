#ifndef PROTOCOLDEFS_H
#define PROTOCOLDEFS_H

#include <cstdint>

// ── 帧 ID 枚举 ──
enum class ProtocolID : uint16_t {
    AHRS_Basic       = 0x0002,  // AHRS 基本模式 (payload 44B)
    AHRS_Extended    = 0x0022,  // AHRS 扩展模式 (payload 49B)
    Quaternion       = 0x0023,  // 四元数模式     (payload 53B)
    AG_Data          = 0x0156,  // AG 数据        (payload 50B)
    FusionResult     = 0x0166,  // 组合导航坐标系融合结果 (payload 94B)
    TiltMeasure      = 0x8166,  // 倾斜测量      (payload 105B, 内部区分值, 线协议同0x0166)
    Laser            = 0x0146,  // 激光协议      (payload 76B)
    NanfangLaser     = 0x0140,  // 南方激光      (payload 73B, 累加和校验)
    ZhdLaser         = 0x0446,  // 中海达激光    (payload 82B, 累加和校验)
    Attitude         = 0x0168,  // 姿态协议      (payload 36B)
    Quat614P         = 0x0199,  // 614P-pro_四元数(payload 46B, 累加和校验)
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
    Unknown          = 0xFFFF,
};

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
        case 0x0146: return ProtocolID::Laser;
        case 0x0140: return ProtocolID::NanfangLaser;
        case 0x0446: return ProtocolID::ZhdLaser;
        case 0x0168: return ProtocolID::Attitude;
        case 0x0199: return ProtocolID::Quat614P;
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
        case ProtocolID::TiltMeasure:   return "倾斜测量";
        case ProtocolID::Laser:         return "激光";
        case ProtocolID::NanfangLaser:  return "南方激光";
        case ProtocolID::ZhdLaser:      return "中海达激光";
        case ProtocolID::Attitude:      return "姿态";
        case ProtocolID::Quat614P:      return "614P-pro_四元数";
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
//  倾斜测量 (ID=0x0166, payload=105B)
// ════════════════════════════════════════════════════════════════
struct TiltMeasure {
    double   lat;          // deg
    double   lon;          // deg
    double   elev;         // m
    int16_t  laserBias[2]; // deg*1000
    uint16_t laserDist;    // mm
    uint8_t  sampleCnt;    // 采点数
    uint8_t  coordFlag;    // 0=尾坐标 6=杆底 8=手持
    int16_t  temp;         // C*100
    uint8_t  rollStd;      // deg*100
    uint8_t  pitchStd;     // deg*100
    float    tiltRoll;     // deg*100
    float    tiltPitch;    // deg
    float    gyroRaw[3];   // deg/s
    float    accelRaw[3];  // g
    uint32_t timestamp;
    uint32_t sysStatus;
    uint16_t poleHeight;   // mm
    float    solutionRate; // 百分比
    uint8_t  gnssFix;
    uint8_t  gnssSatCnt;
    uint8_t  rtkAge;       // s
    int16_t  poleBias[2];  // deg*1000
    float    rollDeg;      // deg
    float    pitchDeg;     // deg
    float    yawDeg;       // deg
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
        TiltMeasure    tilt;
        LaserData      laser;
        NanfangLaserData nanfangLaser;
        ZhdLaserData   zhdLaser;
        AttitudeData   attitude;
        Quat614PData   quat614p;
        CanRtkData     canRtk;
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
