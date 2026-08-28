#ifndef SIMPROTOCOL_H
#define SIMPROTOCOL_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include "modules/simulator/simclock.h"

// ────────────────────────────────────────────────────────────
//  模拟协议抽象基类
//  加协议 = 继承本类实现 encode()，再在 SimProtocolRegistry
//  工厂里登记一行。调度器按 rateHz 定时调用 encode() 输出。
// ────────────────────────────────────────────────────────────
class SimProtocol
{
public:
    virtual ~SimProtocol() = default;

    // 根据时间基准 + 模板参数编码一帧
    virtual QByteArray encode(const SimClock &clock) = 0;

    // 元信息（由工厂从 JSON 配置填充）
    QString    name;
    QString    format;      // "nmea" / "novatel" / "aa55"
    int        rateHz   = 1;
    bool       hasGpsTime = true;
    bool       enabled   = true;   // 默认启用（面板勾选可覆盖）
    QJsonObject params;     // 模板参数（固定值，运行时可改）
};

#endif // SIMPROTOCOL_H
