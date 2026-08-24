#ifndef ANALYSIS_DATAMODEL_H
#define ANALYSIS_DATAMODEL_H

#include <QString>
#include <QVector>
#include <cstdint>

namespace Analysis {

// ════════════════════════════════════════════════════════════════
//  统一数据点 —— 所有协议解析结果的共同表示
// ════════════════════════════════════════════════════════════════
struct DataPoint {
    double  gpstMs = 0;         // GPS 周秒 (ms)  — 统一时间轴

    // ── 位置 ──
    double  lat = 0;            // 纬度 (deg)
    double  lon = 0;            // 经度 (deg)
    double  height = 0;         // 椭球高 (m)
    double  heightAnomaly = 0;  // 高程异常 (m)

    // ── 速度 ──
    double  velN = 0;           // 北向速度 (m/s)
    double  velE = 0;           // 东向速度 (m/s)
    double  velD = 0;           // 天向速度 (m/s)

    // ── 姿态 ──
    double  roll  = 0;          // deg
    double  pitch = 0;          // deg
    double  yaw   = 0;          // deg
    double  heading = 0;        // deg (航向，区别于yaw)

    // ── IMU ──
    double  accelX = 0, accelY = 0, accelZ = 0; // g
    double  gyroX  = 0, gyroY  = 0, gyroZ  = 0; // deg/s
    double  temp = 0;                            // degC

    // ── 质量指标 ──
    uint8_t fixType = 0;        // 0未定位 1单点 4固定 5浮点 50=RTK_FIXED(内部)
    uint8_t numSatellites = 0;
    uint16_t diffAge = 0;       // 差分龄期 (s)
    uint16_t statusFlag = 0;
    double   posStd = 0;        // 位置标准差 (cm)

    // ── 字段可用性标志 ──
    bool hasPosition  = false;
    bool hasVelocity  = false;
    bool hasAttitude  = false;
    bool hasImu       = false;
};

// ════════════════════════════════════════════════════════════════
//  场景段 —— 跑车路线按场景切段
// ════════════════════════════════════════════════════════════════
struct SceneSegment {
    QString name;           // "全路段" / "开阔天空" / "隧道" / "城市峡谷" ...
    double  startGpstMs;    // 起止时间 (ms)
    double  endGpstMs;
};

// ════════════════════════════════════════════════════════════════
//  数据集
// ════════════════════════════════════════════════════════════════
struct DataSet {
    QString name;                   // 显示名称
    QString sourceType;             // "组合导航"/"AHRS"/"AG"/"CAN-RTK"/"RTCM"/"RINEX"/"IE真值"/...
    QString sourcePath;             // 原始文件路径

    QVector<DataPoint> points;      // 按时间排序
    QVector<SceneSegment> scenes;   // 场景切段列表

    // 预计算的完整性摘要
    double  startGpstMs  = 0;
    double  endGpstMs    = 0;
    double  durationSec  = 0;
    double  avgDataRate  = 0;       // Hz
    int     totalPoints  = 0;
    int     gapCount     = 0;       // 时间断点数
    double  totalGapSec  = 0;       // 总间断时长 (s)
    int     fixCount     = 0;       // RTK 固定解帧数
    int     floatCount   = 0;       // RTK 浮点解帧数

    // 数据按场景筛选
    QVector<DataPoint> pointsInScene(const SceneSegment &seg) const
    {
        QVector<DataPoint> result;
        for (const auto &p : points) {
            if (p.gpstMs >= seg.startGpstMs && p.gpstMs <= seg.endGpstMs)
                result.append(p);
        }
        return result;
    }

    // 数据按全局时间范围筛选
    QVector<DataPoint> pointsInRange(double tStart, double tEnd) const
    {
        QVector<DataPoint> result;
        for (const auto &p : points) {
            if (p.gpstMs >= tStart && p.gpstMs <= tEnd)
                result.append(p);
        }
        return result;
    }
};

// 场景预设列表
inline QVector<SceneSegment> presetScenes()
{
    return {
        {"全路段",     0, 0},       // 特殊：起止时间为0表示使用全量数据
        {"开阔天空",   0, 0},
        {"城市峡谷",   0, 0},
        {"林荫路",     0, 0},
        {"隧道",       0, 0},
        {"高架桥",     0, 0},
        {"地下车库",   0, 0},
    };
}

} // namespace Analysis

#endif // ANALYSIS_DATAMODEL_H
