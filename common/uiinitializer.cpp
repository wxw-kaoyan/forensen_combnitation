#include "uiinitializer.h"

#include <QScreen>
#include <QGuiApplication>
#include <QStatusBar>

void UiInitializer::apply(QMainWindow *window)
{
    // ---- 窗口尺寸：屏宽 50%、屏高 80%、居中 ----
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect r = screen->availableGeometry();
        int w = r.width()  * 50 / 100;
        int h = r.height() * 80 / 100;
        window->setMinimumSize(900, 600);
        window->resize(w, h);
        window->move((r.width() - w) / 2, (r.height() - h) / 2);
    } else {
        window->setMinimumSize(1280, 840);
    }

    window->statusBar()->showMessage(QStringLiteral("Ready | UI prototype loaded"));

    // ---- 全局样式 ----
    window->setStyleSheet(
        "QMainWindow { background: #eef3f8; }"
        "QFrame#headerFrame, QFrame#ntripFrame, QFrame#serialTcpToolFrame, "
        "QFrame#parseFrame, QFrame#bottomFrame, QGroupBox {"
        "background: #ffffff; border: 1px solid #d8e2ec; border-radius: 10px; }"
        "QPushButton { background: #17324d; color: white; border-radius: 8px; "
        "padding: 8px 14px; font-weight: 600; }"
        "QPushButton:hover { background: #24476b; }"
        "QPushButton:checked { background: #2a5a8c; }"
        "QLineEdit, QComboBox, QPlainTextEdit, QTableWidget {"
        "background: #fbfdff; border: 1px solid #ccd7e2; border-radius: 8px; padding: 6px; }"
        "QTabWidget::pane { border: 1px solid #d8e2ec; background: #ffffff; border-radius: 10px; }"
        "QTabBar::tab { background: #dfe9f3; color: #17324d; padding: 10px 18px; margin-right: 4px;"
        "border-top-left-radius: 8px; border-top-right-radius: 8px; }"
        "QTabBar::tab:selected { background: #17324d; color: white; }"
        "QGroupBox { font-weight: 700; margin-top: 10px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }"
    );
}
