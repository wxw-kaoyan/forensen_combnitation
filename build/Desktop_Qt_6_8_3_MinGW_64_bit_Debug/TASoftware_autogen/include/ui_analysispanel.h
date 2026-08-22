/********************************************************************************
** Form generated from reading UI file 'analysispanel.ui'
**
** Created by: Qt User Interface Compiler version 6.8.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ANALYSISPANEL_H
#define UI_ANALYSISPANEL_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "modules/analysis/plotwidget.h"

QT_BEGIN_NAMESPACE

class Ui_AnalysisPanel
{
public:
    QHBoxLayout *mainLayout;
    QFrame *leftPanel;
    QVBoxLayout *leftLayout;
    QLabel *dsLabel;
    QListWidget *datasetList;
    QHBoxLayout *dsBtnLayout;
    QPushButton *btnImportFromParse;
    QPushButton *btnRemoveDataset;
    QFrame *separator;
    QLabel *sceneLabel;
    QComboBox *sceneCombo;
    QFormLayout *timeInputLayout;
    QLabel *startLabel;
    QLineEdit *startTimeEdit;
    QLabel *endLabel;
    QLineEdit *endTimeEdit;
    QHBoxLayout *sceneBtnLayout;
    QPushButton *btnAddScene;
    QPushButton *btnRemoveScene;
    QListWidget *sceneList;
    QPushButton *btnAnalyze;
    QSpacerItem *spacerItem;
    QTabWidget *chartTabs;
    QWidget *tabSummary;
    QVBoxLayout *summaryLayout;
    QTableWidget *summaryTable;
    QWidget *tabTimeline;
    QVBoxLayout *timelineLayout;
    Analysis::PlotWidget *plotInterval;
    QWidget *tabPosition;
    QVBoxLayout *positionLayout;
    Analysis::PlotWidget *plotPosition;
    Analysis::PlotWidget *plotVelocity;
    QWidget *tabAttitude;
    QVBoxLayout *attitudeLayout;
    Analysis::PlotWidget *plotAttitude;
    QWidget *tabImu;
    QVBoxLayout *imuLayout;
    Analysis::PlotWidget *plotAccel;
    Analysis::PlotWidget *plotGyro;
    QWidget *tabRtk;
    QVBoxLayout *rtkLayout;
    Analysis::PlotWidget *plotFixType;
    Analysis::PlotWidget *plotSatCnt;

    void setupUi(QWidget *AnalysisPanel)
    {
        if (AnalysisPanel->objectName().isEmpty())
            AnalysisPanel->setObjectName("AnalysisPanel");
        AnalysisPanel->resize(1100, 700);
        mainLayout = new QHBoxLayout(AnalysisPanel);
        mainLayout->setSpacing(8);
        mainLayout->setObjectName("mainLayout");
        mainLayout->setContentsMargins(8, 8, 8, 8);
        leftPanel = new QFrame(AnalysisPanel);
        leftPanel->setObjectName("leftPanel");
        leftPanel->setMinimumSize(QSize(220, 0));
        leftPanel->setMaximumSize(QSize(320, 16777215));
        leftPanel->setFrameShape(QFrame::StyledPanel);
        leftLayout = new QVBoxLayout(leftPanel);
        leftLayout->setSpacing(8);
        leftLayout->setObjectName("leftLayout");
        dsLabel = new QLabel(leftPanel);
        dsLabel->setObjectName("dsLabel");

        leftLayout->addWidget(dsLabel);

        datasetList = new QListWidget(leftPanel);
        datasetList->setObjectName("datasetList");
        datasetList->setMinimumSize(QSize(200, 120));
        datasetList->setSelectionMode(QAbstractItemView::SingleSelection);

        leftLayout->addWidget(datasetList);

        dsBtnLayout = new QHBoxLayout();
        dsBtnLayout->setObjectName("dsBtnLayout");
        btnImportFromParse = new QPushButton(leftPanel);
        btnImportFromParse->setObjectName("btnImportFromParse");

        dsBtnLayout->addWidget(btnImportFromParse);

        btnRemoveDataset = new QPushButton(leftPanel);
        btnRemoveDataset->setObjectName("btnRemoveDataset");

        dsBtnLayout->addWidget(btnRemoveDataset);


        leftLayout->addLayout(dsBtnLayout);

        separator = new QFrame(leftPanel);
        separator->setObjectName("separator");
        separator->setFrameShape(QFrame::HLine);
        separator->setMinimumSize(QSize(0, 16));

        leftLayout->addWidget(separator);

        sceneLabel = new QLabel(leftPanel);
        sceneLabel->setObjectName("sceneLabel");

        leftLayout->addWidget(sceneLabel);

        sceneCombo = new QComboBox(leftPanel);
        sceneCombo->setObjectName("sceneCombo");
        sceneCombo->setMinimumSize(QSize(180, 0));

        leftLayout->addWidget(sceneCombo);

        timeInputLayout = new QFormLayout();
        timeInputLayout->setSpacing(4);
        timeInputLayout->setObjectName("timeInputLayout");
        startLabel = new QLabel(leftPanel);
        startLabel->setObjectName("startLabel");

        timeInputLayout->setWidget(0, QFormLayout::LabelRole, startLabel);

        startTimeEdit = new QLineEdit(leftPanel);
        startTimeEdit->setObjectName("startTimeEdit");

        timeInputLayout->setWidget(0, QFormLayout::LabelRole, startTimeEdit);

        endLabel = new QLabel(leftPanel);
        endLabel->setObjectName("endLabel");

        timeInputLayout->setWidget(1, QFormLayout::LabelRole, endLabel);

        endTimeEdit = new QLineEdit(leftPanel);
        endTimeEdit->setObjectName("endTimeEdit");

        timeInputLayout->setWidget(1, QFormLayout::LabelRole, endTimeEdit);


        leftLayout->addLayout(timeInputLayout);

        sceneBtnLayout = new QHBoxLayout();
        sceneBtnLayout->setObjectName("sceneBtnLayout");
        btnAddScene = new QPushButton(leftPanel);
        btnAddScene->setObjectName("btnAddScene");

        sceneBtnLayout->addWidget(btnAddScene);

        btnRemoveScene = new QPushButton(leftPanel);
        btnRemoveScene->setObjectName("btnRemoveScene");

        sceneBtnLayout->addWidget(btnRemoveScene);


        leftLayout->addLayout(sceneBtnLayout);

        sceneList = new QListWidget(leftPanel);
        sceneList->setObjectName("sceneList");
        sceneList->setMinimumSize(QSize(200, 80));

        leftLayout->addWidget(sceneList);

        btnAnalyze = new QPushButton(leftPanel);
        btnAnalyze->setObjectName("btnAnalyze");

        leftLayout->addWidget(btnAnalyze);

        spacerItem = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        leftLayout->addItem(spacerItem);


        mainLayout->addWidget(leftPanel);

        chartTabs = new QTabWidget(AnalysisPanel);
        chartTabs->setObjectName("chartTabs");
        chartTabs->setTabPosition(QTabWidget::North);
        tabSummary = new QWidget();
        tabSummary->setObjectName("tabSummary");
        summaryLayout = new QVBoxLayout(tabSummary);
        summaryLayout->setObjectName("summaryLayout");
        summaryTable = new QTableWidget(tabSummary);
        summaryTable->setObjectName("summaryTable");
        summaryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

        summaryLayout->addWidget(summaryTable);

        chartTabs->addTab(tabSummary, QString());
        tabTimeline = new QWidget();
        tabTimeline->setObjectName("tabTimeline");
        timelineLayout = new QVBoxLayout(tabTimeline);
        timelineLayout->setObjectName("timelineLayout");
        plotInterval = new Analysis::PlotWidget(tabTimeline);
        plotInterval->setObjectName("plotInterval");

        timelineLayout->addWidget(plotInterval);

        chartTabs->addTab(tabTimeline, QString());
        tabPosition = new QWidget();
        tabPosition->setObjectName("tabPosition");
        positionLayout = new QVBoxLayout(tabPosition);
        positionLayout->setObjectName("positionLayout");
        plotPosition = new Analysis::PlotWidget(tabPosition);
        plotPosition->setObjectName("plotPosition");

        positionLayout->addWidget(plotPosition);

        plotVelocity = new Analysis::PlotWidget(tabPosition);
        plotVelocity->setObjectName("plotVelocity");

        positionLayout->addWidget(plotVelocity);

        chartTabs->addTab(tabPosition, QString());
        tabAttitude = new QWidget();
        tabAttitude->setObjectName("tabAttitude");
        attitudeLayout = new QVBoxLayout(tabAttitude);
        attitudeLayout->setObjectName("attitudeLayout");
        plotAttitude = new Analysis::PlotWidget(tabAttitude);
        plotAttitude->setObjectName("plotAttitude");

        attitudeLayout->addWidget(plotAttitude);

        chartTabs->addTab(tabAttitude, QString());
        tabImu = new QWidget();
        tabImu->setObjectName("tabImu");
        imuLayout = new QVBoxLayout(tabImu);
        imuLayout->setObjectName("imuLayout");
        plotAccel = new Analysis::PlotWidget(tabImu);
        plotAccel->setObjectName("plotAccel");

        imuLayout->addWidget(plotAccel);

        plotGyro = new Analysis::PlotWidget(tabImu);
        plotGyro->setObjectName("plotGyro");

        imuLayout->addWidget(plotGyro);

        chartTabs->addTab(tabImu, QString());
        tabRtk = new QWidget();
        tabRtk->setObjectName("tabRtk");
        rtkLayout = new QVBoxLayout(tabRtk);
        rtkLayout->setObjectName("rtkLayout");
        plotFixType = new Analysis::PlotWidget(tabRtk);
        plotFixType->setObjectName("plotFixType");

        rtkLayout->addWidget(plotFixType);

        plotSatCnt = new Analysis::PlotWidget(tabRtk);
        plotSatCnt->setObjectName("plotSatCnt");

        rtkLayout->addWidget(plotSatCnt);

        chartTabs->addTab(tabRtk, QString());

        mainLayout->addWidget(chartTabs);


        retranslateUi(AnalysisPanel);

        QMetaObject::connectSlotsByName(AnalysisPanel);
    } // setupUi

    void retranslateUi(QWidget *AnalysisPanel)
    {
        dsLabel->setText(QCoreApplication::translate("AnalysisPanel", "\346\225\260\346\215\256\351\233\206", nullptr));
        dsLabel->setStyleSheet(QCoreApplication::translate("AnalysisPanel", "font-size:13px;font-weight:700;color:#12263f;", nullptr));
        btnImportFromParse->setText(QCoreApplication::translate("AnalysisPanel", "\344\273\216\350\247\243\346\236\220\345\257\274\345\205\245", nullptr));
        btnRemoveDataset->setText(QCoreApplication::translate("AnalysisPanel", "\345\210\240\351\231\244", nullptr));
        sceneLabel->setText(QCoreApplication::translate("AnalysisPanel", "\345\234\272\346\231\257\345\210\207\346\256\265", nullptr));
        sceneLabel->setStyleSheet(QCoreApplication::translate("AnalysisPanel", "font-size:13px;font-weight:700;color:#12263f;", nullptr));
        startLabel->setText(QCoreApplication::translate("AnalysisPanel", "\350\265\267\345\247\213 (\347\247\222)", nullptr));
        startTimeEdit->setPlaceholderText(QCoreApplication::translate("AnalysisPanel", "\344\273\216\346\225\260\346\215\256\350\265\267\347\202\271", nullptr));
        endLabel->setText(QCoreApplication::translate("AnalysisPanel", "\347\273\223\346\235\237 (\347\247\222)", nullptr));
        endTimeEdit->setPlaceholderText(QCoreApplication::translate("AnalysisPanel", "\345\210\260\346\225\260\346\215\256\347\273\210\347\202\271", nullptr));
        btnAddScene->setText(QCoreApplication::translate("AnalysisPanel", "\346\267\273\345\212\240\345\234\272\346\231\257", nullptr));
        btnRemoveScene->setText(QCoreApplication::translate("AnalysisPanel", "\345\210\240\351\231\244", nullptr));
        btnAnalyze->setText(QCoreApplication::translate("AnalysisPanel", "\345\210\267\346\226\260\345\210\206\346\236\220", nullptr));
        btnAnalyze->setStyleSheet(QCoreApplication::translate("AnalysisPanel", "font-weight:600;color:#fff;background-color:#2c7be5;padding:6px;", nullptr));
        chartTabs->setTabText(chartTabs->indexOf(tabSummary), QCoreApplication::translate("AnalysisPanel", "\347\273\237\350\256\241\346\221\230\350\246\201", nullptr));
        chartTabs->setTabText(chartTabs->indexOf(tabTimeline), QCoreApplication::translate("AnalysisPanel", "\346\227\266\351\227\264\350\275\264", nullptr));
        chartTabs->setTabText(chartTabs->indexOf(tabPosition), QCoreApplication::translate("AnalysisPanel", "\344\275\215\347\275\256/\351\200\237\345\272\246", nullptr));
        chartTabs->setTabText(chartTabs->indexOf(tabAttitude), QCoreApplication::translate("AnalysisPanel", "\345\247\277\346\200\201", nullptr));
        chartTabs->setTabText(chartTabs->indexOf(tabImu), QCoreApplication::translate("AnalysisPanel", "IMU", nullptr));
        chartTabs->setTabText(chartTabs->indexOf(tabRtk), QCoreApplication::translate("AnalysisPanel", "RTK\347\212\266\346\200\201", nullptr));
        (void)AnalysisPanel;
    } // retranslateUi

};

namespace Ui {
    class AnalysisPanel: public Ui_AnalysisPanel {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ANALYSISPANEL_H
