#include "atcmdstore.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

QString AtCmdStore::storePath()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/at_commands.json");
}

QList<AtCmdCategory> AtCmdStore::loadCategories()
{
    QFile f(storePath());
    if (!f.open(QIODevice::ReadOnly))
        return {};

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    QList<AtCmdCategory> result;
    const QJsonArray arr = doc.object().value(QStringLiteral("categories")).toArray();
    for (const auto &v : arr) {
        const QJsonObject obj = v.toObject();
        AtCmdCategory cat;
        cat.name = obj.value(QStringLiteral("name")).toString();
        const QJsonArray cmds = obj.value(QStringLiteral("commands")).toArray();
        for (const auto &c : cmds)
            cat.commands.append(fromJson(c.toObject()));
        result.append(cat);
    }
    return result;
}

QList<AtCmdInfo> AtCmdStore::loadCustom()
{
    QFile f(storePath());
    if (!f.open(QIODevice::ReadOnly))
        return {};

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    QList<AtCmdInfo> result;
    const QJsonArray arr = doc.object().value(QStringLiteral("custom")).toArray();
    for (const auto &v : arr)
        result.append(fromJson(v.toObject()));
    return result;
}

bool AtCmdStore::exists(const QString &command)
{
    if (command.isEmpty())
        return false;

    for (const auto &cat : loadCategories())
        for (const auto &c : cat.commands)
            if (c.command == command)
                return true;

    for (const auto &c : loadCustom())
        if (c.command == command)
            return true;

    return false;
}

bool AtCmdStore::addCustom(const AtCmdInfo &cmd)
{
    if (cmd.command.isEmpty() || exists(cmd.command))
        return false;

    QFile f(storePath());
    if (!f.open(QIODevice::ReadOnly))
        return false;

    QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    QList<AtCmdInfo> custom = loadCustom();
    custom.append(cmd);

    QJsonArray arr;
    for (const auto &c : custom)
        arr.append(toJson(c));
    root[QStringLiteral("custom")] = arr;

    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(root).toJson());
    f.close();
    return true;
}

bool AtCmdStore::removeCustom(const QString &command)
{
    QFile f(storePath());
    if (!f.open(QIODevice::ReadOnly))
        return false;

    QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    QJsonArray arr;
    bool removed = false;
    for (const auto &c : loadCustom()) {
        if (c.command == command) {
            removed = true;
            continue;
        }
        arr.append(toJson(c));
    }
    if (!removed)
        return false;

    root[QStringLiteral("custom")] = arr;

    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(root).toJson());
    f.close();
    return true;
}

QJsonObject AtCmdStore::toJson(const AtCmdInfo &cmd)
{
    QJsonObject o;
    o[QStringLiteral("command")]     = cmd.command;
    o[QStringLiteral("description")] = cmd.description;
    o[QStringLiteral("paramHint")]   = cmd.paramHint;
    return o;
}

AtCmdInfo AtCmdStore::fromJson(const QJsonObject &obj)
{
    AtCmdInfo c;
    c.command     = obj.value(QStringLiteral("command")).toString();
    c.description = obj.value(QStringLiteral("description")).toString();
    c.paramHint   = obj.value(QStringLiteral("paramHint")).toString();
    return c;
}
