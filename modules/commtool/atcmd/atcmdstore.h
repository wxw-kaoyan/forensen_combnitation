#ifndef ATCMDSTORE_H
#define ATCMDSTORE_H

#include <QString>
#include <QList>

class QJsonObject;

// 单条 AT 指令描述
struct AtCmdInfo
{
    QString command;       // 指令名，如 "AT+SET_BAUD="
    QString description;   // 功能说明
    QString paramHint;     // 参数提示，空串表示无参
};

// 指令板块
struct AtCmdCategory
{
    QString name;
    QList<AtCmdInfo> commands;
};

// AT 指令库管理器 — at_commands.json 读写
class AtCmdStore
{
public:
    // 内置板块（通用/芯片/导航）
    static QList<AtCmdCategory> loadCategories();

    // 常用指令（自定义区）
    static QList<AtCmdInfo> loadCustom();

    // 添加一条常用指令（去重：全库已存在则失败）
    static bool addCustom(const AtCmdInfo &cmd);

    // 删除一条常用指令
    static bool removeCustom(const QString &command);

    // 全库查重（内置 + 自定义）
    static bool exists(const QString &command);

    // 配置文件路径
    static QString storePath();

private:
    static QJsonObject toJson(const AtCmdInfo &cmd);
    static AtCmdInfo fromJson(const QJsonObject &obj);
};

#endif // ATCMDSTORE_H
