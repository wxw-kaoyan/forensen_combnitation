#include "toolprocessmanager.h"

#include <QWidget>
#include <QPushButton>
#include <QLabel>

ToolProcessManager::ToolProcessManager(QObject *parent)
    : QObject(parent)
{
    registerTools({
        {"Converter3.0.7.exe", "和芯解析", "converterButton", "converterStatusLabel"},
        {"rtkplot.exe",         "RTKPLOT",  "rtkplotButton",    "rtkplotStatusLabel"},
    });
}

void ToolProcessManager::registerTools(std::initializer_list<ToolSpec> specs)
{
    QWidget *host = qobject_cast<QWidget *>(parent());
    for (const ToolSpec &s : specs) {
        QPushButton *btn = host ? host->findChild<QPushButton *>(s.buttonName) : nullptr;
        QLabel      *lbl = host ? host->findChild<QLabel *>(s.statusLabelName) : nullptr;
        m_tools.append(ToolProcess::create({
            .exeName     = s.exeName,
            .displayName = s.displayName,
            .button      = btn,
            .statusLabel = lbl,
        }, this));
    }
}
