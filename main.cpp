#include "mainwindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // 应用图标：窗口标题栏 / 任务栏 / 启动菜单等统一使用
    a.setWindowIcon(QIcon(":/icons/app.png"));
    MainWindow w;
    w.show();
    return QApplication::exec();
}
