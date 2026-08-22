#ifndef TOOLPROCESSMANAGER_H
#define TOOLPROCESSMANAGER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <initializer_list>

#include "toolprocess.h"

// 外部工具进程批量管理器
// 通过 objectName 自动查找按钮/状态标签，无需向构造函数传递控件指针；
// 新增工具只需在构造函数里的列表加一行 {exe, 显示名, 按钮名, 状态标签名}。
class ToolProcessManager : public QObject
{
    Q_OBJECT

public:
    // 单个工具的声明（按钮/标签用 objectName 字符串定位）
    struct ToolSpec {
        QString exeName;
        QString displayName;
        QString buttonName;
        QString statusLabelName;
    };

    explicit ToolProcessManager(QObject *parent = nullptr);

    // 批量注册（内部通过 parent 递归 findChild 查找控件）
    void registerTools(std::initializer_list<ToolSpec> specs);

private:
    QVector<ToolProcess *> m_tools;
};

#endif // TOOLPROCESSMANAGER_H
