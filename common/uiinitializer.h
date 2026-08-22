#ifndef UIINITIALIZER_H
#define UIINITIALIZER_H

#include <QMainWindow>

// 一次性初始化：窗口尺寸 + 全局样式
class UiInitializer
{
public:
    static void apply(QMainWindow *window);
};

#endif // UIINITIALIZER_H
