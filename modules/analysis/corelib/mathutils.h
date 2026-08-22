#ifndef ANALYSIS_MATHUTILS_H
#define ANALYSIS_MATHUTILS_H

#include <cmath>
#include <algorithm>
#include <QVector>

namespace Analysis {

// π 常量
constexpr double PI = 3.14159265358979323846;
constexpr double DEG_TO_RAD = PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / PI;
constexpr double RE_WGS84 = 6378137.0;    // WGS84 长半轴 (m)

// ════════════════════════════════════════════════════════════════
//  角度归一化到 [-180, 180]
// ════════════════════════════════════════════════════════════════
inline double wrap180(double deg)
{
    while (deg > 180.0)  deg -= 360.0;
    while (deg < -180.0) deg += 360.0;
    return deg;
}

// ════════════════════════════════════════════════════════════════
//  LLH 差分 → ENU 北-东-天 (m)
//  pos1: 待测, pos2: 参考
// ════════════════════════════════════════════════════════════════
inline void llh2diff(double lat1, double lon1, double hgt1,
                     double lat2, double lon2, double hgt2,
                     double &dNorth, double &dEast, double &dUp)
{
    double cosLat = std::cos(lat1 * DEG_TO_RAD);
    dEast  = (lon1 - lon2) * DEG_TO_RAD * RE_WGS84 * cosLat;
    dNorth = (lat1 - lat2) * DEG_TO_RAD * RE_WGS84;
    dUp    = hgt1 - hgt2;
}

// ════════════════════════════════════════════════════════════════
//  LLH → 相对 ENU 坐标 (以 refLLH 为原点)
// ════════════════════════════════════════════════════════════════
inline void llh2enu(double lat, double lon, double hgt,
                    double refLat, double refLon, double refHgt,
                    double &e, double &n, double &u)
{
    llh2diff(lat, lon, hgt, refLat, refLon, refHgt, n, e, u);
}

// ════════════════════════════════════════════════════════════════
//  批量 LLH → 相对 ENU
// ════════════════════════════════════════════════════════════════
inline void batchLlh2enu(const QVector<double> &lat, const QVector<double> &lon,
                         const QVector<double> &hgt,
                         double refLat, double refLon, double refHgt,
                         QVector<double> &east, QVector<double> &north, QVector<double> &up)
{
    int n = std::min({lat.size(), lon.size(), hgt.size()});
    east.resize(n);
    north.resize(n);
    up.resize(n);
    for (int i = 0; i < n; ++i) {
        llh2diff(lat[i], lon[i], hgt[i], refLat, refLon, refHgt, north[i], east[i], up[i]);
    }
}

} // namespace Analysis

#endif // ANALYSIS_MATHUTILS_H
