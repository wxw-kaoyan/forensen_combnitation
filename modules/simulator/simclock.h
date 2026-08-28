#ifndef SIMCLOCK_H
#define SIMCLOCK_H

#include <QDateTime>
#include <cstdint>

// ────────────────────────────────────────────────────────────
//  模拟时间基准
//  由系统 UTC 时间换算而来，PPS 每到整秒对齐刷新一次。
//  各协议编码时从 SimClock 取值填时间戳字段。
// ────────────────────────────────────────────────────────────
struct SimClock {
    uint16_t gpsWeek      = 0;     // GPS 周
    uint32_t gpsWeekMs    = 0;     // 周内毫秒
    uint32_t gpsWeekSec   = 0;     // 周内秒（整秒，PPS 对齐）
    uint32_t utcTimeOfDayMs = 0;   // UTC 日内毫秒（NMEA 用）
    QDate    utcDate;              // UTC 日期（NMEA GPRMC 日期字段用）
    uint32_t timerUs      = 0;     // 上电计时(us)，无 GPS 时间协议用
};

// 系统 UTC 时间 -> 模拟时间基准（GPS 周 / 周内毫秒 / UTC 日内毫秒）。
// 闰秒：GPS = UTC + 18s（2017-01-01 起；若未来再插闰秒需同步修改此值）。
inline SimClock simClockFromUtc(const QDateTime &utc)
{
    SimClock c;
    static const qint64 gpsEpochSecs =
        QDateTime(QDate(1980, 1, 6), QTime(0, 0, 0), Qt::UTC).toSecsSinceEpoch();
    constexpr int leapSeconds = 18;

    const qint64 gpsSecs = utc.toSecsSinceEpoch() - gpsEpochSecs + leapSeconds;

    c.gpsWeek        = static_cast<uint16_t>(gpsSecs / 604800);
    c.gpsWeekSec     = static_cast<uint32_t>(gpsSecs % 604800);
    c.gpsWeekMs      = c.gpsWeekSec * 1000u + static_cast<uint32_t>(utc.time().msec());
    c.utcTimeOfDayMs = static_cast<uint32_t>(utc.time().msecsSinceStartOfDay());
    c.utcDate        = utc.date();
    c.timerUs        = 0;
    return c;
}

#endif // SIMCLOCK_H
