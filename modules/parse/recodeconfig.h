#ifndef RECODECONFIG_H
#define RECODECONFIG_H

#include <QString>
#include <QStringList>
#include <QJsonArray>
#include <QJsonObject>

// ════════════════════════════════════════════════════════════════
//  New Code 配置项 — 一套字段勾选 + 备注
// ════════════════════════════════════════════════════════════════
struct RecodeConfig
{
    QString     note;        // 备注，如 "AG只保留陀螺"
    QString     protocolId;  // ProtocolID 枚举名，如 "FusionResult"
    QStringList fields;      // 勾选的 fieldKey 列表
};

// ════════════════════════════════════════════════════════════════
//  New Code 配置管理器 — config.json 读写
// ════════════════════════════════════════════════════════════════
class RecodeConfigManager
{
public:
    // 读取所有配置
    static QList<RecodeConfig> loadAll();

    // 保存一条配置（若已存在同 protocolId+note 则覆盖）
    static void saveConfig(const RecodeConfig &cfg);

    // 删除一条配置
    static void deleteConfig(const QString &protocolId, const QString &note);

    // 根据 protocolId 过滤
    static QList<RecodeConfig> loadForProtocol(const QString &protocolId);

private:
    static QString configFilePath();
    static QJsonObject toJson(const RecodeConfig &cfg);
    static RecodeConfig fromJson(const QJsonObject &obj);
};

#endif // RECODECONFIG_H
