#ifndef SIMPROTOCOLREGISTRY_H
#define SIMPROTOCOLREGISTRY_H

#include <QJsonObject>
#include <QList>
#include <QString>

#include "modules/simulator/simprotocol.h"

// 单个协议工厂：根据 JSON 配置创建对应协议实例，找不到返回 nullptr
SimProtocol *createSimProtocol(const QJsonObject &cfg);

// 从 JSON 文件加载所有模拟协议（"sim_protocols" 数组）
QList<SimProtocol *> loadSimProtocols(const QString &jsonPath);

#endif // SIMPROTOCOLREGISTRY_H
