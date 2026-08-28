#include "modules/simulator/simprotocolregistry.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

#include "modules/simulator/protocols/gprmc.h"
#include "modules/simulator/protocols/gpgga.h"
#include "modules/simulator/protocols/bestposb.h"
#include "modules/simulator/protocols/psrvelb.h"

// ────────────────────────────────────────────────────────────
//  协议工厂：加协议 = include 头文件 + 加一个 else-if 分支
// ────────────────────────────────────────────────────────────
SimProtocol *createSimProtocol(const QJsonObject &cfg)
{
    const QString name = cfg.value("name").toString();

    SimProtocol *p = nullptr;
    if      (name == "GPRMC")    p = new Gprmc;
    else if (name == "GPGGA")    p = new Gpgga;
    else if (name == "BESTPOSB") p = new BestPosb;
    else if (name == "PSRVELB")  p = new Psrvelb;

    if (!p)
        return nullptr;

    p->name       = name;
    p->format     = cfg.value("format").toString();
    p->rateHz     = cfg.value("rateHz").toInt(1);
    p->hasGpsTime = cfg.value("hasGpsTime").toBool(true);
    p->enabled    = cfg.value("enabled").toBool(true);
    p->params     = cfg.value("params").toObject();
    return p;
}

QList<SimProtocol *> loadSimProtocols(const QString &jsonPath)
{
    QList<SimProtocol *> result;

    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly))
        return result;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    const QJsonArray arr = doc.object().value("sim_protocols").toArray();
    for (const QJsonValue &v : arr) {
        SimProtocol *p = createSimProtocol(v.toObject());
        if (p)
            result.append(p);
    }
    return result;
}
