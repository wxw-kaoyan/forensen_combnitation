/********************************************************************************
** Form generated from reading UI file 'atcmdpanel.ui'
**
** Created by: Qt User Interface Compiler version 6.8.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ATCMDPANEL_H
#define UI_ATCMDPANEL_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_AtCmdPanel
{
public:
    QVBoxLayout *rootLayout;
    QTabWidget *cmdTabWidget;
    QWidget *commonAtPage;
    QVBoxLayout *commonPageLayout;
    QScrollArea *commonScrollArea;
    QWidget *commonScrollContent;
    QWidget *chipAtPage;
    QVBoxLayout *chipPageLayout;
    QScrollArea *chipScrollArea;
    QWidget *chipScrollContent;
    QWidget *navAtPage;
    QVBoxLayout *navPageLayout;
    QScrollArea *navScrollArea;
    QWidget *navScrollContent;

    void setupUi(QWidget *AtCmdPanel)
    {
        if (AtCmdPanel->objectName().isEmpty())
            AtCmdPanel->setObjectName("AtCmdPanel");
        AtCmdPanel->resize(320, 400);
        AtCmdPanel->setStyleSheet(QString::fromUtf8("QWidget#AtCmdPanel { background: transparent; }"));
        rootLayout = new QVBoxLayout(AtCmdPanel);
        rootLayout->setSpacing(0);
        rootLayout->setObjectName("rootLayout");
        rootLayout->setContentsMargins(0, 0, 0, 0);
        cmdTabWidget = new QTabWidget(AtCmdPanel);
        cmdTabWidget->setObjectName("cmdTabWidget");
        cmdTabWidget->setStyleSheet(QString::fromUtf8("QTabBar::tab { font-size: 12px; padding: 4px 12px; }\n"
"QTabBar::tab:selected { font-weight: 600; }"));
        commonAtPage = new QWidget();
        commonAtPage->setObjectName("commonAtPage");
        commonPageLayout = new QVBoxLayout(commonAtPage);
        commonPageLayout->setSpacing(0);
        commonPageLayout->setObjectName("commonPageLayout");
        commonPageLayout->setContentsMargins(0, 2, 0, 0);
        commonScrollArea = new QScrollArea(commonAtPage);
        commonScrollArea->setObjectName("commonScrollArea");
        commonScrollArea->setFrameShape(QFrame::Shape::NoFrame);
        commonScrollArea->setWidgetResizable(true);
        commonScrollContent = new QWidget();
        commonScrollContent->setObjectName("commonScrollContent");
        commonScrollContent->setGeometry(QRect(0, 0, 314, 369));
        commonScrollArea->setWidget(commonScrollContent);

        commonPageLayout->addWidget(commonScrollArea);

        cmdTabWidget->addTab(commonAtPage, QString());
        chipAtPage = new QWidget();
        chipAtPage->setObjectName("chipAtPage");
        chipPageLayout = new QVBoxLayout(chipAtPage);
        chipPageLayout->setSpacing(0);
        chipPageLayout->setObjectName("chipPageLayout");
        chipPageLayout->setContentsMargins(0, 2, 0, 0);
        chipScrollArea = new QScrollArea(chipAtPage);
        chipScrollArea->setObjectName("chipScrollArea");
        chipScrollArea->setFrameShape(QFrame::Shape::NoFrame);
        chipScrollArea->setWidgetResizable(true);
        chipScrollContent = new QWidget();
        chipScrollContent->setObjectName("chipScrollContent");
        chipScrollContent->setGeometry(QRect(0, 0, 314, 369));
        chipScrollArea->setWidget(chipScrollContent);

        chipPageLayout->addWidget(chipScrollArea);

        cmdTabWidget->addTab(chipAtPage, QString());
        navAtPage = new QWidget();
        navAtPage->setObjectName("navAtPage");
        navPageLayout = new QVBoxLayout(navAtPage);
        navPageLayout->setSpacing(0);
        navPageLayout->setObjectName("navPageLayout");
        navPageLayout->setContentsMargins(0, 2, 0, 0);
        navScrollArea = new QScrollArea(navAtPage);
        navScrollArea->setObjectName("navScrollArea");
        navScrollArea->setFrameShape(QFrame::Shape::NoFrame);
        navScrollArea->setWidgetResizable(true);
        navScrollContent = new QWidget();
        navScrollContent->setObjectName("navScrollContent");
        navScrollContent->setGeometry(QRect(0, 0, 314, 369));
        navScrollArea->setWidget(navScrollContent);

        navPageLayout->addWidget(navScrollArea);

        cmdTabWidget->addTab(navAtPage, QString());

        rootLayout->addWidget(cmdTabWidget);


        retranslateUi(AtCmdPanel);

        cmdTabWidget->setCurrentIndex(2);


        QMetaObject::connectSlotsByName(AtCmdPanel);
    } // setupUi

    void retranslateUi(QWidget *AtCmdPanel)
    {
        cmdTabWidget->setTabText(cmdTabWidget->indexOf(commonAtPage), QCoreApplication::translate("AtCmdPanel", "\351\200\232\347\224\250AT", nullptr));
        cmdTabWidget->setTabText(cmdTabWidget->indexOf(chipAtPage), QCoreApplication::translate("AtCmdPanel", "\350\212\257\347\211\207AT", nullptr));
        cmdTabWidget->setTabText(cmdTabWidget->indexOf(navAtPage), QCoreApplication::translate("AtCmdPanel", "\347\273\204\345\220\210\345\257\274\350\210\252AT", nullptr));
        (void)AtCmdPanel;
    } // retranslateUi

};

namespace Ui {
    class AtCmdPanel: public Ui_AtCmdPanel {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ATCMDPANEL_H
