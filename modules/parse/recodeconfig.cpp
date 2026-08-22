#include "recodeconfig.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>

QString RecodeConfigManager::configFilePath()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/recode_config.json");
}

QList<RecodeConfig> RecodeConfigManager::loadAll()
{
    QFile f(configFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return {};

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    QList<RecodeConfig> result;
    const QJsonArray arr = doc.object().value(QStringLiteral("configs")).toArray();
    for (const auto &v : arr)
        result.append(fromJson(v.toObject()));
    return result;
}

QList<RecodeConfig> RecodeConfigManager::loadForProtocol(const QString &protocolId)
{
    QList<RecodeConfig> all = loadAll();
    QList<RecodeConfig> filtered;
    for (const auto &c : all) {
        if (c.protocolId == protocolId)
            filtered.append(c);
    }
    return filtered;
}

void RecodeConfigManager::saveConfig(const RecodeConfig &cfg)
{
    QFile f(configFilePath());
    QJsonArray arr;
    QList<RecodeConfig> all = loadAll();

    // 覆盖同 protocolId+note 的已有配置
    bool found = false;
    for (auto &existing : all) {
        if (existing.protocolId == cfg.protocolId && existing.note == cfg.note) {
            existing = cfg;
            found = true;
        }
        arr.append(toJson(existing));
    }
    if (!found)
        arr.append(toJson(cfg));

    QJsonObject root;
    root[QStringLiteral("configs")] = arr;

    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(QJsonDocument(root).toJson());
    f.close();
}

void RecodeConfigManager::deleteConfig(const QString &protocolId, const QString &note)
{
    QFile f(configFilePath());
    QJsonArray arr;
    QList<RecodeConfig> all = loadAll();

    for (const auto &c : all) {
        if (c.protocolId == protocolId && c.note == note)
            continue;
        arr.append(toJson(c));
    }

    QJsonObject root;
    root[QStringLiteral("configs")] = arr;

    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(QJsonDocument(root).toJson());
    f.close();
}

QJsonObject RecodeConfigManager::toJson(const RecodeConfig &cfg)
{
    QJsonObject obj;
    obj[QStringLiteral("note")]       = cfg.note;
    obj[QStringLiteral("protocolId")] = cfg.protocolId;
    QJsonArray fieldsArr;
    for (const auto &f : cfg.fields)
        fieldsArr.append(f);
    obj[QStringLiteral("fields")] = fieldsArr;
    return obj;
}

RecodeConfig RecodeConfigManager::fromJson(const QJsonObject &obj)
{
    RecodeConfig cfg;
    cfg.note       = obj.value(QStringLiteral("note")).toString();
    cfg.protocolId = obj.value(QStringLiteral("protocolId")).toString();
    for (const auto &v : obj.value(QStringLiteral("fields")).toArray())
        cfg.fields.append(v.toString());
    return cfg;
}
