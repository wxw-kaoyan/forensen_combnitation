/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.8.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "modules/atcmd/atcmdpanel.h"

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout_8;
    QFrame *headerFrame;
    QHBoxLayout *horizontalLayout_6;
    QVBoxLayout *verticalLayout_9;
    QLabel *titleLabel;
    QLabel *subTitleLabel;
    QSpacerItem *horizontalSpacer_3;
    QLabel *headerStatusLabel;
    QTabWidget *topModuleTabWidget;
    QWidget *toolTab;
    QVBoxLayout *verticalLayout;
    QFrame *toolSelectorFrame;
    QVBoxLayout *verticalLayout_18;
    QHBoxLayout *toolButtonLayout;
    QPushButton *ntripToolButton;
    QPushButton *commToolButton;
    QSpacerItem *toolBtnSpacer;
    QStackedWidget *toolStackedWidget;
    QWidget *ntripPage;
    QVBoxLayout *verticalLayout_2;
    QFrame *ntripFrame;
    QVBoxLayout *verticalLayout_11;
    QLabel *ntripTitleLabel;
    QLabel *ntripDescLabel;
    QHBoxLayout *horizontalLayout_7;
    QPushButton *ntripConfigButton;
    QFrame *ntripStatusCard1;
    QVBoxLayout *verticalLayout_13;
    QLabel *label_14;
    QLabel *ntripConnectStatusLabel;
    QFrame *ntripStatusCard2;
    QVBoxLayout *verticalLayout_14;
    QLabel *label_15;
    QLabel *ntripRateLabel;
    QGroupBox *ntripPoolGroupBox;
    QVBoxLayout *verticalLayout_17;
    QLabel *ntripPoolDescLabel;
    QHBoxLayout *horizontalLayout_8;
    QPushButton *addTcpTargetButton;
    QPushButton *addSerialTargetButton;
    QCheckBox *fixGgaCheckBox;
    QSpacerItem *horizontalSpacer_4;
    QFrame *fixGgaFrame;
    QHBoxLayout *fixGgaLayout;
    QLabel *fixLonLabel;
    QLineEdit *fixLonEdit;
    QLabel *fixLatLabel;
    QLineEdit *fixLatEdit;
    QLabel *fixHeightLabel;
    QLineEdit *fixHeightEdit;
    QTableWidget *ntripForwardTableWidget;
    QHBoxLayout *horizontalLayout_3;
    QPlainTextEdit *ntripLogEdit;
    QWidget *commPage;
    QVBoxLayout *verticalLayout_3;
    QFrame *serialTcpToolFrame;
    QVBoxLayout *verticalLayout_4;
    QLabel *serialNetTitleLabel;
    QHBoxLayout *commMainHLayout;
    QFrame *commConfigFrame;
    QVBoxLayout *commConfigVLayout;
    QGroupBox *connectionModeGroupBox;
    QVBoxLayout *connParamVLayout;
    QHBoxLayout *connModeHLayout;
    QLabel *connModeLabel;
    QComboBox *connectionModeComboBox;
    QHBoxLayout *addrHLayout;
    QLabel *addrLabel;
    QLineEdit *toolAddressEdit;
    QComboBox *commPortComboBox;
    QHBoxLayout *portHLayout;
    QLabel *portLabel;
    QLineEdit *toolPortEdit;
    QHBoxLayout *baudHLayout;
    QLabel *baudLabel;
    QComboBox *baudComboBox;
    QFrame *serialParamsFrame;
    QHBoxLayout *serialParamsHLayout;
    QLabel *paramLabel;
    QComboBox *dataBitsComboBox;
    QComboBox *parityComboBox;
    QComboBox *stopBitsComboBox;
    QPushButton *toolConnectButton;
    QGroupBox *extraFuncGroupBox;
    QVBoxLayout *extraFuncVLayout;
    QTabWidget *extraFuncTabWidget;
    QWidget *atCmdTab;
    QVBoxLayout *atCmdTabVLayout;
    AtCmdPanel *atCmdPanel;
    QWidget *calibTab;
    QVBoxLayout *calibTabVLayout;
    QLabel *calibPlaceholderLabel;
    QSpacerItem *calibTabSpacer;
    QWidget *debugDataTab;
    QVBoxLayout *debugDataTabVLayout;
    QLabel *debugPlaceholderLabel;
    QSpacerItem *debugTabSpacer;
    QSpacerItem *commConfigSpacer;
    QFrame *commRecvFrame;
    QVBoxLayout *commRecvVLayout;
    QHBoxLayout *recvHeaderHLayout;
    QLabel *recvLabel;
    QLabel *recvCountLabel;
    QSpacerItem *recvHeaderSpacer;
    QCheckBox *hexDisplayCheckBox;
    QPlainTextEdit *toolConsoleEdit;
    QFrame *commSendFrame;
    QVBoxLayout *commSendVLayout;
    QHBoxLayout *sendHeaderHLayout;
    QLabel *sendLabel;
    QSpacerItem *sendHeaderSpcacer;
    QCheckBox *sendNewlineCheckBox;
    QCheckBox *sendHexCheckBox;
    QPlainTextEdit *commandEdit;
    QHBoxLayout *sendBtnHLayout;
    QPushButton *sendCommandButton;
    QPushButton *saveConsoleButton;
    QSpacerItem *sendBtnSpacer;
    QPushButton *clearConsoleButton;
    QWidget *parseTab;
    QVBoxLayout *verticalLayout_5;
    QWidget *analysisTab;
    QVBoxLayout *verticalLayout_7;
    QFrame *analysisFrame;
    QVBoxLayout *verticalLayout_10;
    QLabel *analysisTitleLabel;
    QLabel *analysisInfoLabel;
    QSpacerItem *analysisSpacer1;
    QGroupBox *singleDeviceGroup;
    QVBoxLayout *singleDeviceLayout;
    QLabel *singleDeviceDescLabel;
    QHBoxLayout *singleDeviceFeatureLayout;
    QFrame *singleCard1;
    QVBoxLayout *verticalLayout_21;
    QLabel *singleLabel1;
    QLabel *singleStatus1;
    QFrame *singleCard2;
    QVBoxLayout *verticalLayout_22;
    QLabel *singleLabel2;
    QLabel *singleStatus2;
    QFrame *singleCard3;
    QVBoxLayout *verticalLayout_23;
    QLabel *singleLabel3;
    QLabel *singleStatus3;
    QFrame *singleCard4;
    QVBoxLayout *verticalLayout_24;
    QLabel *singleLabel4;
    QLabel *singleStatus4;
    QSpacerItem *singleDeviceSpacer;
    QLabel *singleDeviceDetailLabel;
    QGroupBox *multiDeviceGroup;
    QVBoxLayout *multiDeviceLayout;
    QLabel *multiDeviceDescLabel;
    QTableWidget *compareTableWidget;
    QSpacerItem *analysisBottomSpacer;
    QWidget *toolsTab;
    QVBoxLayout *verticalLayout_20;
    QLabel *toolsTitleLabel;
    QHBoxLayout *toolsLayout;
    QPushButton *converterButton;
    QLabel *converterStatusLabel;
    QSpacerItem *toolsSpacer;
    QHBoxLayout *toolsLayout2;
    QPushButton *rtkplotButton;
    QLabel *rtkplotStatusLabel;
    QSpacerItem *toolsSpacer2;
    QSpacerItem *toolsBottomSpacer;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(1440, 988);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        verticalLayout_8 = new QVBoxLayout(centralwidget);
        verticalLayout_8->setSpacing(12);
        verticalLayout_8->setObjectName("verticalLayout_8");
        verticalLayout_8->setContentsMargins(12, 12, 12, 12);
        headerFrame = new QFrame(centralwidget);
        headerFrame->setObjectName("headerFrame");
        headerFrame->setFrameShape(QFrame::Shape::StyledPanel);
        horizontalLayout_6 = new QHBoxLayout(headerFrame);
        horizontalLayout_6->setObjectName("horizontalLayout_6");
        horizontalLayout_6->setContentsMargins(18, 6, 18, 6);
        verticalLayout_9 = new QVBoxLayout();
        verticalLayout_9->setObjectName("verticalLayout_9");
        titleLabel = new QLabel(headerFrame);
        titleLabel->setObjectName("titleLabel");
        titleLabel->setStyleSheet(QString::fromUtf8("font-size: 15px; font-weight: 600; color: #12263f;"));

        verticalLayout_9->addWidget(titleLabel);

        subTitleLabel = new QLabel(headerFrame);
        subTitleLabel->setObjectName("subTitleLabel");
        subTitleLabel->setStyleSheet(QString::fromUtf8("font-size: 13px; color: #63768c;"));

        verticalLayout_9->addWidget(subTitleLabel);


        horizontalLayout_6->addLayout(verticalLayout_9);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_6->addItem(horizontalSpacer_3);

        headerStatusLabel = new QLabel(headerFrame);
        headerStatusLabel->setObjectName("headerStatusLabel");
        headerStatusLabel->setStyleSheet(QString::fromUtf8("font-size: 13px; color: #42556d; font-weight: 600;"));

        horizontalLayout_6->addWidget(headerStatusLabel);


        verticalLayout_8->addWidget(headerFrame);

        topModuleTabWidget = new QTabWidget(centralwidget);
        topModuleTabWidget->setObjectName("topModuleTabWidget");
        toolTab = new QWidget();
        toolTab->setObjectName("toolTab");
        verticalLayout = new QVBoxLayout(toolTab);
        verticalLayout->setSpacing(12);
        verticalLayout->setObjectName("verticalLayout");
        toolSelectorFrame = new QFrame(toolTab);
        toolSelectorFrame->setObjectName("toolSelectorFrame");
        toolSelectorFrame->setFrameShape(QFrame::Shape::StyledPanel);
        verticalLayout_18 = new QVBoxLayout(toolSelectorFrame);
        verticalLayout_18->setObjectName("verticalLayout_18");
        verticalLayout_18->setContentsMargins(16, 8, 16, 8);
        toolButtonLayout = new QHBoxLayout();
        toolButtonLayout->setObjectName("toolButtonLayout");
        ntripToolButton = new QPushButton(toolSelectorFrame);
        ntripToolButton->setObjectName("ntripToolButton");
        ntripToolButton->setMinimumSize(QSize(180, 44));
        ntripToolButton->setCheckable(true);
        ntripToolButton->setChecked(true);

        toolButtonLayout->addWidget(ntripToolButton);

        commToolButton = new QPushButton(toolSelectorFrame);
        commToolButton->setObjectName("commToolButton");
        commToolButton->setMinimumSize(QSize(180, 44));
        commToolButton->setCheckable(true);

        toolButtonLayout->addWidget(commToolButton);

        toolBtnSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        toolButtonLayout->addItem(toolBtnSpacer);


        verticalLayout_18->addLayout(toolButtonLayout);


        verticalLayout->addWidget(toolSelectorFrame);

        toolStackedWidget = new QStackedWidget(toolTab);
        toolStackedWidget->setObjectName("toolStackedWidget");
        ntripPage = new QWidget();
        ntripPage->setObjectName("ntripPage");
        verticalLayout_2 = new QVBoxLayout(ntripPage);
        verticalLayout_2->setObjectName("verticalLayout_2");
        ntripFrame = new QFrame(ntripPage);
        ntripFrame->setObjectName("ntripFrame");
        ntripFrame->setFrameShape(QFrame::Shape::StyledPanel);
        verticalLayout_11 = new QVBoxLayout(ntripFrame);
        verticalLayout_11->setSpacing(6);
        verticalLayout_11->setObjectName("verticalLayout_11");
        verticalLayout_11->setContentsMargins(16, 10, 16, 10);
        ntripTitleLabel = new QLabel(ntripFrame);
        ntripTitleLabel->setObjectName("ntripTitleLabel");
        ntripTitleLabel->setStyleSheet(QString::fromUtf8("font-size: 15px; font-weight: 700; color: #12263f;"));

        verticalLayout_11->addWidget(ntripTitleLabel);

        ntripDescLabel = new QLabel(ntripFrame);
        ntripDescLabel->setObjectName("ntripDescLabel");
        ntripDescLabel->setStyleSheet(QString::fromUtf8("font-size: 13px; color: #617487;"));
        ntripDescLabel->setWordWrap(true);

        verticalLayout_11->addWidget(ntripDescLabel);

        horizontalLayout_7 = new QHBoxLayout();
        horizontalLayout_7->setObjectName("horizontalLayout_7");
        ntripConfigButton = new QPushButton(ntripFrame);
        ntripConfigButton->setObjectName("ntripConfigButton");
        ntripConfigButton->setMinimumSize(QSize(80, 42));

        horizontalLayout_7->addWidget(ntripConfigButton);

        ntripStatusCard1 = new QFrame(ntripFrame);
        ntripStatusCard1->setObjectName("ntripStatusCard1");
        ntripStatusCard1->setFrameShape(QFrame::Shape::StyledPanel);
        verticalLayout_13 = new QVBoxLayout(ntripStatusCard1);
        verticalLayout_13->setObjectName("verticalLayout_13");
        label_14 = new QLabel(ntripStatusCard1);
        label_14->setObjectName("label_14");
        label_14->setStyleSheet(QString::fromUtf8("font-size: 12px; color: #70849a;"));

        verticalLayout_13->addWidget(label_14);

        ntripConnectStatusLabel = new QLabel(ntripStatusCard1);
        ntripConnectStatusLabel->setObjectName("ntripConnectStatusLabel");
        ntripConnectStatusLabel->setStyleSheet(QString::fromUtf8("font-size: 14px; font-weight: 700; color: #12263f;"));

        verticalLayout_13->addWidget(ntripConnectStatusLabel);


        horizontalLayout_7->addWidget(ntripStatusCard1);

        ntripStatusCard2 = new QFrame(ntripFrame);
        ntripStatusCard2->setObjectName("ntripStatusCard2");
        ntripStatusCard2->setFrameShape(QFrame::Shape::StyledPanel);
        verticalLayout_14 = new QVBoxLayout(ntripStatusCard2);
        verticalLayout_14->setObjectName("verticalLayout_14");
        label_15 = new QLabel(ntripStatusCard2);
        label_15->setObjectName("label_15");
        label_15->setStyleSheet(QString::fromUtf8("font-size: 12px; color: #70849a;"));

        verticalLayout_14->addWidget(label_15);

        ntripRateLabel = new QLabel(ntripStatusCard2);
        ntripRateLabel->setObjectName("ntripRateLabel");
        ntripRateLabel->setStyleSheet(QString::fromUtf8("font-size: 14px; font-weight: 700; color: #12263f;"));

        verticalLayout_14->addWidget(ntripRateLabel);


        horizontalLayout_7->addWidget(ntripStatusCard2);


        verticalLayout_11->addLayout(horizontalLayout_7);

        ntripPoolGroupBox = new QGroupBox(ntripFrame);
        ntripPoolGroupBox->setObjectName("ntripPoolGroupBox");
        verticalLayout_17 = new QVBoxLayout(ntripPoolGroupBox);
        verticalLayout_17->setObjectName("verticalLayout_17");
        ntripPoolDescLabel = new QLabel(ntripPoolGroupBox);
        ntripPoolDescLabel->setObjectName("ntripPoolDescLabel");
        ntripPoolDescLabel->setStyleSheet(QString::fromUtf8("font-size: 12px; color: #617487;"));
        ntripPoolDescLabel->setWordWrap(true);

        verticalLayout_17->addWidget(ntripPoolDescLabel);

        horizontalLayout_8 = new QHBoxLayout();
        horizontalLayout_8->setObjectName("horizontalLayout_8");
        addTcpTargetButton = new QPushButton(ntripPoolGroupBox);
        addTcpTargetButton->setObjectName("addTcpTargetButton");
        addTcpTargetButton->setMinimumSize(QSize(0, 38));

        horizontalLayout_8->addWidget(addTcpTargetButton);

        addSerialTargetButton = new QPushButton(ntripPoolGroupBox);
        addSerialTargetButton->setObjectName("addSerialTargetButton");
        addSerialTargetButton->setMinimumSize(QSize(0, 38));

        horizontalLayout_8->addWidget(addSerialTargetButton);

        fixGgaCheckBox = new QCheckBox(ntripPoolGroupBox);
        fixGgaCheckBox->setObjectName("fixGgaCheckBox");
        fixGgaCheckBox->setStyleSheet(QString::fromUtf8("color: #70849a; font-size: 13px; font-weight: 600;"));
        fixGgaCheckBox->setChecked(true);

        horizontalLayout_8->addWidget(fixGgaCheckBox);

        horizontalSpacer_4 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_8->addItem(horizontalSpacer_4);


        verticalLayout_17->addLayout(horizontalLayout_8);

        fixGgaFrame = new QFrame(ntripPoolGroupBox);
        fixGgaFrame->setObjectName("fixGgaFrame");
        fixGgaFrame->setEnabled(false);
        fixGgaFrame->setFrameShape(QFrame::Shape::StyledPanel);
        fixGgaLayout = new QHBoxLayout(fixGgaFrame);
        fixGgaLayout->setObjectName("fixGgaLayout");
        fixGgaLayout->setContentsMargins(0, 0, 0, 0);
        fixLonLabel = new QLabel(fixGgaFrame);
        fixLonLabel->setObjectName("fixLonLabel");

        fixGgaLayout->addWidget(fixLonLabel);

        fixLonEdit = new QLineEdit(fixGgaFrame);
        fixLonEdit->setObjectName("fixLonEdit");

        fixGgaLayout->addWidget(fixLonEdit);

        fixLatLabel = new QLabel(fixGgaFrame);
        fixLatLabel->setObjectName("fixLatLabel");

        fixGgaLayout->addWidget(fixLatLabel);

        fixLatEdit = new QLineEdit(fixGgaFrame);
        fixLatEdit->setObjectName("fixLatEdit");

        fixGgaLayout->addWidget(fixLatEdit);

        fixHeightLabel = new QLabel(fixGgaFrame);
        fixHeightLabel->setObjectName("fixHeightLabel");

        fixGgaLayout->addWidget(fixHeightLabel);

        fixHeightEdit = new QLineEdit(fixGgaFrame);
        fixHeightEdit->setObjectName("fixHeightEdit");

        fixGgaLayout->addWidget(fixHeightEdit);


        verticalLayout_17->addWidget(fixGgaFrame);

        ntripForwardTableWidget = new QTableWidget(ntripPoolGroupBox);
        if (ntripForwardTableWidget->columnCount() < 6)
            ntripForwardTableWidget->setColumnCount(6);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        ntripForwardTableWidget->setHorizontalHeaderItem(0, __qtablewidgetitem);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        ntripForwardTableWidget->setHorizontalHeaderItem(1, __qtablewidgetitem1);
        QTableWidgetItem *__qtablewidgetitem2 = new QTableWidgetItem();
        ntripForwardTableWidget->setHorizontalHeaderItem(2, __qtablewidgetitem2);
        QTableWidgetItem *__qtablewidgetitem3 = new QTableWidgetItem();
        ntripForwardTableWidget->setHorizontalHeaderItem(3, __qtablewidgetitem3);
        QTableWidgetItem *__qtablewidgetitem4 = new QTableWidgetItem();
        ntripForwardTableWidget->setHorizontalHeaderItem(4, __qtablewidgetitem4);
        QTableWidgetItem *__qtablewidgetitem5 = new QTableWidgetItem();
        ntripForwardTableWidget->setHorizontalHeaderItem(5, __qtablewidgetitem5);
        ntripForwardTableWidget->setObjectName("ntripForwardTableWidget");
        ntripForwardTableWidget->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);
        ntripForwardTableWidget->setAlternatingRowColors(true);
        ntripForwardTableWidget->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
        ntripForwardTableWidget->setRowCount(0);

        verticalLayout_17->addWidget(ntripForwardTableWidget);


        verticalLayout_11->addWidget(ntripPoolGroupBox);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName("horizontalLayout_3");

        verticalLayout_11->addLayout(horizontalLayout_3);

        ntripLogEdit = new QPlainTextEdit(ntripFrame);
        ntripLogEdit->setObjectName("ntripLogEdit");
        ntripLogEdit->setMinimumSize(QSize(0, 70));

        verticalLayout_11->addWidget(ntripLogEdit);


        verticalLayout_2->addWidget(ntripFrame);

        toolStackedWidget->addWidget(ntripPage);
        commPage = new QWidget();
        commPage->setObjectName("commPage");
        verticalLayout_3 = new QVBoxLayout(commPage);
        verticalLayout_3->setSpacing(8);
        verticalLayout_3->setObjectName("verticalLayout_3");
        verticalLayout_3->setContentsMargins(0, 0, 0, 0);
        serialTcpToolFrame = new QFrame(commPage);
        serialTcpToolFrame->setObjectName("serialTcpToolFrame");
        serialTcpToolFrame->setFrameShape(QFrame::Shape::StyledPanel);
        verticalLayout_4 = new QVBoxLayout(serialTcpToolFrame);
        verticalLayout_4->setObjectName("verticalLayout_4");
        verticalLayout_4->setContentsMargins(16, 12, 16, 12);
        serialNetTitleLabel = new QLabel(serialTcpToolFrame);
        serialNetTitleLabel->setObjectName("serialNetTitleLabel");
        serialNetTitleLabel->setStyleSheet(QString::fromUtf8("font-size: 15px; font-weight: 700; color: #12263f;"));

        verticalLayout_4->addWidget(serialNetTitleLabel);

        commMainHLayout = new QHBoxLayout();
        commMainHLayout->setSpacing(12);
        commMainHLayout->setObjectName("commMainHLayout");
        commConfigFrame = new QFrame(serialTcpToolFrame);
        commConfigFrame->setObjectName("commConfigFrame");
        commConfigFrame->setMinimumSize(QSize(340, 0));
        commConfigFrame->setMaximumSize(QSize(340, 16777215));
        commConfigFrame->setFrameShape(QFrame::Shape::NoFrame);
        commConfigVLayout = new QVBoxLayout(commConfigFrame);
        commConfigVLayout->setSpacing(8);
        commConfigVLayout->setObjectName("commConfigVLayout");
        commConfigVLayout->setContentsMargins(0, 0, 0, 0);
        connectionModeGroupBox = new QGroupBox(commConfigFrame);
        connectionModeGroupBox->setObjectName("connectionModeGroupBox");
        connParamVLayout = new QVBoxLayout(connectionModeGroupBox);
        connParamVLayout->setSpacing(6);
        connParamVLayout->setObjectName("connParamVLayout");
        connModeHLayout = new QHBoxLayout();
        connModeHLayout->setObjectName("connModeHLayout");
        connModeLabel = new QLabel(connectionModeGroupBox);
        connModeLabel->setObjectName("connModeLabel");
        connModeLabel->setMinimumSize(QSize(60, 0));

        connModeHLayout->addWidget(connModeLabel);

        connectionModeComboBox = new QComboBox(connectionModeGroupBox);
        connectionModeComboBox->addItem(QString());
        connectionModeComboBox->addItem(QString());
        connectionModeComboBox->setObjectName("connectionModeComboBox");

        connModeHLayout->addWidget(connectionModeComboBox);


        connParamVLayout->addLayout(connModeHLayout);

        addrHLayout = new QHBoxLayout();
        addrHLayout->setObjectName("addrHLayout");
        addrLabel = new QLabel(connectionModeGroupBox);
        addrLabel->setObjectName("addrLabel");
        addrLabel->setMinimumSize(QSize(60, 0));

        addrHLayout->addWidget(addrLabel);

        toolAddressEdit = new QLineEdit(connectionModeGroupBox);
        toolAddressEdit->setObjectName("toolAddressEdit");

        addrHLayout->addWidget(toolAddressEdit);

        commPortComboBox = new QComboBox(connectionModeGroupBox);
        commPortComboBox->setObjectName("commPortComboBox");
        commPortComboBox->setVisible(false);

        addrHLayout->addWidget(commPortComboBox);


        connParamVLayout->addLayout(addrHLayout);

        portHLayout = new QHBoxLayout();
        portHLayout->setObjectName("portHLayout");
        portLabel = new QLabel(connectionModeGroupBox);
        portLabel->setObjectName("portLabel");
        portLabel->setMinimumSize(QSize(60, 0));

        portHLayout->addWidget(portLabel);

        toolPortEdit = new QLineEdit(connectionModeGroupBox);
        toolPortEdit->setObjectName("toolPortEdit");

        portHLayout->addWidget(toolPortEdit);


        connParamVLayout->addLayout(portHLayout);

        baudHLayout = new QHBoxLayout();
        baudHLayout->setObjectName("baudHLayout");
        baudLabel = new QLabel(connectionModeGroupBox);
        baudLabel->setObjectName("baudLabel");
        baudLabel->setMinimumSize(QSize(60, 0));

        baudHLayout->addWidget(baudLabel);

        baudComboBox = new QComboBox(connectionModeGroupBox);
        baudComboBox->addItem(QString());
        baudComboBox->addItem(QString());
        baudComboBox->addItem(QString());
        baudComboBox->addItem(QString());
        baudComboBox->addItem(QString());
        baudComboBox->addItem(QString());
        baudComboBox->addItem(QString());
        baudComboBox->addItem(QString());
        baudComboBox->setObjectName("baudComboBox");
        baudComboBox->setEditable(true);

        baudHLayout->addWidget(baudComboBox);


        connParamVLayout->addLayout(baudHLayout);

        serialParamsFrame = new QFrame(connectionModeGroupBox);
        serialParamsFrame->setObjectName("serialParamsFrame");
        serialParamsFrame->setFrameShape(QFrame::Shape::NoFrame);
        serialParamsHLayout = new QHBoxLayout(serialParamsFrame);
        serialParamsHLayout->setSpacing(6);
        serialParamsHLayout->setObjectName("serialParamsHLayout");
        serialParamsHLayout->setContentsMargins(0, 0, 0, 0);
        paramLabel = new QLabel(serialParamsFrame);
        paramLabel->setObjectName("paramLabel");
        paramLabel->setMinimumSize(QSize(60, 0));

        serialParamsHLayout->addWidget(paramLabel);

        dataBitsComboBox = new QComboBox(serialParamsFrame);
        dataBitsComboBox->addItem(QString());
        dataBitsComboBox->addItem(QString());
        dataBitsComboBox->addItem(QString());
        dataBitsComboBox->addItem(QString());
        dataBitsComboBox->setObjectName("dataBitsComboBox");
        dataBitsComboBox->setMinimumSize(QSize(52, 0));

        serialParamsHLayout->addWidget(dataBitsComboBox);

        parityComboBox = new QComboBox(serialParamsFrame);
        parityComboBox->addItem(QString());
        parityComboBox->addItem(QString());
        parityComboBox->addItem(QString());
        parityComboBox->setObjectName("parityComboBox");
        parityComboBox->setMinimumSize(QSize(52, 0));

        serialParamsHLayout->addWidget(parityComboBox);

        stopBitsComboBox = new QComboBox(serialParamsFrame);
        stopBitsComboBox->addItem(QString());
        stopBitsComboBox->addItem(QString());
        stopBitsComboBox->addItem(QString());
        stopBitsComboBox->setObjectName("stopBitsComboBox");
        stopBitsComboBox->setMinimumSize(QSize(52, 0));

        serialParamsHLayout->addWidget(stopBitsComboBox);


        connParamVLayout->addWidget(serialParamsFrame);

        toolConnectButton = new QPushButton(connectionModeGroupBox);
        toolConnectButton->setObjectName("toolConnectButton");
        toolConnectButton->setMinimumSize(QSize(0, 42));

        connParamVLayout->addWidget(toolConnectButton);


        commConfigVLayout->addWidget(connectionModeGroupBox);

        extraFuncGroupBox = new QGroupBox(commConfigFrame);
        extraFuncGroupBox->setObjectName("extraFuncGroupBox");
        extraFuncVLayout = new QVBoxLayout(extraFuncGroupBox);
        extraFuncVLayout->setSpacing(0);
        extraFuncVLayout->setObjectName("extraFuncVLayout");
        extraFuncVLayout->setContentsMargins(4, 10, 4, 4);
        extraFuncTabWidget = new QTabWidget(extraFuncGroupBox);
        extraFuncTabWidget->setObjectName("extraFuncTabWidget");
        atCmdTab = new QWidget();
        atCmdTab->setObjectName("atCmdTab");
        atCmdTabVLayout = new QVBoxLayout(atCmdTab);
        atCmdTabVLayout->setSpacing(0);
        atCmdTabVLayout->setObjectName("atCmdTabVLayout");
        atCmdTabVLayout->setContentsMargins(0, 0, 0, 0);
        atCmdPanel = new AtCmdPanel(atCmdTab);
        atCmdPanel->setObjectName("atCmdPanel");

        atCmdTabVLayout->addWidget(atCmdPanel);

        extraFuncTabWidget->addTab(atCmdTab, QString());
        calibTab = new QWidget();
        calibTab->setObjectName("calibTab");
        calibTabVLayout = new QVBoxLayout(calibTab);
        calibTabVLayout->setObjectName("calibTabVLayout");
        calibTabVLayout->setContentsMargins(8, 12, 8, 12);
        calibPlaceholderLabel = new QLabel(calibTab);
        calibPlaceholderLabel->setObjectName("calibPlaceholderLabel");
        calibPlaceholderLabel->setStyleSheet(QString::fromUtf8("font-size: 13px; color: #70849a;"));
        calibPlaceholderLabel->setAlignment(Qt::AlignmentFlag::AlignCenter);

        calibTabVLayout->addWidget(calibPlaceholderLabel);

        calibTabSpacer = new QSpacerItem(20, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        calibTabVLayout->addItem(calibTabSpacer);

        extraFuncTabWidget->addTab(calibTab, QString());
        debugDataTab = new QWidget();
        debugDataTab->setObjectName("debugDataTab");
        debugDataTabVLayout = new QVBoxLayout(debugDataTab);
        debugDataTabVLayout->setObjectName("debugDataTabVLayout");
        debugDataTabVLayout->setContentsMargins(8, 12, 8, 12);
        debugPlaceholderLabel = new QLabel(debugDataTab);
        debugPlaceholderLabel->setObjectName("debugPlaceholderLabel");
        debugPlaceholderLabel->setStyleSheet(QString::fromUtf8("font-size: 13px; color: #70849a;"));
        debugPlaceholderLabel->setAlignment(Qt::AlignmentFlag::AlignCenter);

        debugDataTabVLayout->addWidget(debugPlaceholderLabel);

        debugTabSpacer = new QSpacerItem(20, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        debugDataTabVLayout->addItem(debugTabSpacer);

        extraFuncTabWidget->addTab(debugDataTab, QString());

        extraFuncVLayout->addWidget(extraFuncTabWidget);


        commConfigVLayout->addWidget(extraFuncGroupBox);

        commConfigSpacer = new QSpacerItem(20, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        commConfigVLayout->addItem(commConfigSpacer);


        commMainHLayout->addWidget(commConfigFrame);

        commRecvFrame = new QFrame(serialTcpToolFrame);
        commRecvFrame->setObjectName("commRecvFrame");
        commRecvFrame->setFrameShape(QFrame::Shape::NoFrame);
        commRecvVLayout = new QVBoxLayout(commRecvFrame);
        commRecvVLayout->setSpacing(4);
        commRecvVLayout->setObjectName("commRecvVLayout");
        commRecvVLayout->setContentsMargins(0, 0, 0, 0);
        recvHeaderHLayout = new QHBoxLayout();
        recvHeaderHLayout->setObjectName("recvHeaderHLayout");
        recvLabel = new QLabel(commRecvFrame);
        recvLabel->setObjectName("recvLabel");
        recvLabel->setStyleSheet(QString::fromUtf8("font-size: 14px; font-weight: 600; color: #12263f;"));

        recvHeaderHLayout->addWidget(recvLabel);

        recvCountLabel = new QLabel(commRecvFrame);
        recvCountLabel->setObjectName("recvCountLabel");
        recvCountLabel->setStyleSheet(QString::fromUtf8("font-size: 12px; color: #70849a;"));

        recvHeaderHLayout->addWidget(recvCountLabel);

        recvHeaderSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        recvHeaderHLayout->addItem(recvHeaderSpacer);

        hexDisplayCheckBox = new QCheckBox(commRecvFrame);
        hexDisplayCheckBox->setObjectName("hexDisplayCheckBox");
        hexDisplayCheckBox->setStyleSheet(QString::fromUtf8("color: #70849a; font-size: 12px;"));

        recvHeaderHLayout->addWidget(hexDisplayCheckBox);


        commRecvVLayout->addLayout(recvHeaderHLayout);

        toolConsoleEdit = new QPlainTextEdit(commRecvFrame);
        toolConsoleEdit->setObjectName("toolConsoleEdit");
        QFont font;
        font.setFamilies({QString::fromUtf8("Consolas")});
        font.setPointSize(11);
        toolConsoleEdit->setFont(font);
        toolConsoleEdit->setLineWrapMode(QPlainTextEdit::LineWrapMode::NoWrap);
        toolConsoleEdit->setReadOnly(true);

        commRecvVLayout->addWidget(toolConsoleEdit);


        commMainHLayout->addWidget(commRecvFrame);


        verticalLayout_4->addLayout(commMainHLayout);

        commSendFrame = new QFrame(serialTcpToolFrame);
        commSendFrame->setObjectName("commSendFrame");
        commSendFrame->setFrameShape(QFrame::Shape::StyledPanel);
        commSendVLayout = new QVBoxLayout(commSendFrame);
        commSendVLayout->setSpacing(6);
        commSendVLayout->setObjectName("commSendVLayout");
        commSendVLayout->setContentsMargins(12, 10, 12, 10);
        sendHeaderHLayout = new QHBoxLayout();
        sendHeaderHLayout->setObjectName("sendHeaderHLayout");
        sendLabel = new QLabel(commSendFrame);
        sendLabel->setObjectName("sendLabel");
        sendLabel->setStyleSheet(QString::fromUtf8("font-size: 14px; font-weight: 600; color: #12263f;"));

        sendHeaderHLayout->addWidget(sendLabel);

        sendHeaderSpcacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        sendHeaderHLayout->addItem(sendHeaderSpcacer);

        sendNewlineCheckBox = new QCheckBox(commSendFrame);
        sendNewlineCheckBox->setObjectName("sendNewlineCheckBox");
        sendNewlineCheckBox->setStyleSheet(QString::fromUtf8("color: #70849a; font-size: 12px;"));
        sendNewlineCheckBox->setChecked(true);

        sendHeaderHLayout->addWidget(sendNewlineCheckBox);

        sendHexCheckBox = new QCheckBox(commSendFrame);
        sendHexCheckBox->setObjectName("sendHexCheckBox");
        sendHexCheckBox->setStyleSheet(QString::fromUtf8("color: #70849a; font-size: 12px;"));

        sendHeaderHLayout->addWidget(sendHexCheckBox);


        commSendVLayout->addLayout(sendHeaderHLayout);

        commandEdit = new QPlainTextEdit(commSendFrame);
        commandEdit->setObjectName("commandEdit");
        commandEdit->setMinimumSize(QSize(0, 70));
        commandEdit->setMaximumSize(QSize(16777215, 120));
        commandEdit->setFont(font);

        commSendVLayout->addWidget(commandEdit);

        sendBtnHLayout = new QHBoxLayout();
        sendBtnHLayout->setObjectName("sendBtnHLayout");
        sendCommandButton = new QPushButton(commSendFrame);
        sendCommandButton->setObjectName("sendCommandButton");
        sendCommandButton->setMinimumSize(QSize(100, 42));

        sendBtnHLayout->addWidget(sendCommandButton);

        saveConsoleButton = new QPushButton(commSendFrame);
        saveConsoleButton->setObjectName("saveConsoleButton");
        saveConsoleButton->setMinimumSize(QSize(80, 38));
        saveConsoleButton->setStyleSheet(QString::fromUtf8("QPushButton { background: #27ae60; color: white; border-radius: 6px; }\n"
"QPushButton:hover { background: #2ecc71; }"));

        sendBtnHLayout->addWidget(saveConsoleButton);

        sendBtnSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        sendBtnHLayout->addItem(sendBtnSpacer);

        clearConsoleButton = new QPushButton(commSendFrame);
        clearConsoleButton->setObjectName("clearConsoleButton");
        clearConsoleButton->setMinimumSize(QSize(80, 38));
        clearConsoleButton->setStyleSheet(QString::fromUtf8("QPushButton { background: #7f8c8d; color: white; border-radius: 6px; }\n"
"QPushButton:hover { background: #95a5a6; }"));

        sendBtnHLayout->addWidget(clearConsoleButton);


        commSendVLayout->addLayout(sendBtnHLayout);


        verticalLayout_4->addWidget(commSendFrame);


        verticalLayout_3->addWidget(serialTcpToolFrame);

        toolStackedWidget->addWidget(commPage);

        verticalLayout->addWidget(toolStackedWidget);

        topModuleTabWidget->addTab(toolTab, QString());
        parseTab = new QWidget();
        parseTab->setObjectName("parseTab");
        verticalLayout_5 = new QVBoxLayout(parseTab);
        verticalLayout_5->setObjectName("verticalLayout_5");
        verticalLayout_5->setContentsMargins(0, 0, 0, 0);
        topModuleTabWidget->addTab(parseTab, QString());
        analysisTab = new QWidget();
        analysisTab->setObjectName("analysisTab");
        verticalLayout_7 = new QVBoxLayout(analysisTab);
        verticalLayout_7->setObjectName("verticalLayout_7");
        analysisFrame = new QFrame(analysisTab);
        analysisFrame->setObjectName("analysisFrame");
        analysisFrame->setFrameShape(QFrame::Shape::StyledPanel);
        verticalLayout_10 = new QVBoxLayout(analysisFrame);
        verticalLayout_10->setObjectName("verticalLayout_10");
        verticalLayout_10->setContentsMargins(16, 14, 16, 14);
        analysisTitleLabel = new QLabel(analysisFrame);
        analysisTitleLabel->setObjectName("analysisTitleLabel");
        analysisTitleLabel->setStyleSheet(QString::fromUtf8("font-size:15px;font-weight:700;color:#12263f;"));

        verticalLayout_10->addWidget(analysisTitleLabel);

        analysisInfoLabel = new QLabel(analysisFrame);
        analysisInfoLabel->setObjectName("analysisInfoLabel");
        analysisInfoLabel->setStyleSheet(QString::fromUtf8("font-size:13px;color:#617487;"));
        analysisInfoLabel->setWordWrap(true);

        verticalLayout_10->addWidget(analysisInfoLabel);

        analysisSpacer1 = new QSpacerItem(0, 10, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout_10->addItem(analysisSpacer1);

        singleDeviceGroup = new QGroupBox(analysisFrame);
        singleDeviceGroup->setObjectName("singleDeviceGroup");
        singleDeviceLayout = new QVBoxLayout(singleDeviceGroup);
        singleDeviceLayout->setSpacing(6);
        singleDeviceLayout->setObjectName("singleDeviceLayout");
        singleDeviceDescLabel = new QLabel(singleDeviceGroup);
        singleDeviceDescLabel->setObjectName("singleDeviceDescLabel");
        singleDeviceDescLabel->setStyleSheet(QString::fromUtf8("font-size:12px;color:#617487;"));
        singleDeviceDescLabel->setWordWrap(true);

        singleDeviceLayout->addWidget(singleDeviceDescLabel);

        singleDeviceFeatureLayout = new QHBoxLayout();
        singleDeviceFeatureLayout->setObjectName("singleDeviceFeatureLayout");
        singleCard1 = new QFrame(singleDeviceGroup);
        singleCard1->setObjectName("singleCard1");
        singleCard1->setMinimumSize(QSize(180, 70));
        singleCard1->setFrameShape(QFrame::Shape::StyledPanel);
        verticalLayout_21 = new QVBoxLayout(singleCard1);
        verticalLayout_21->setObjectName("verticalLayout_21");
        singleLabel1 = new QLabel(singleCard1);
        singleLabel1->setObjectName("singleLabel1");
        singleLabel1->setStyleSheet(QString::fromUtf8("font-size:12px;color:#12263f;font-weight:600;"));

        verticalLayout_21->addWidget(singleLabel1);

        singleStatus1 = new QLabel(singleCard1);
        singleStatus1->setObjectName("singleStatus1");
        singleStatus1->setStyleSheet(QString::fromUtf8("font-size:11px;color:#a0a0a0;"));

        verticalLayout_21->addWidget(singleStatus1);


        singleDeviceFeatureLayout->addWidget(singleCard1);

        singleCard2 = new QFrame(singleDeviceGroup);
        singleCard2->setObjectName("singleCard2");
        singleCard2->setMinimumSize(QSize(180, 70));
        singleCard2->setFrameShape(QFrame::Shape::StyledPanel);
        verticalLayout_22 = new QVBoxLayout(singleCard2);
        verticalLayout_22->setObjectName("verticalLayout_22");
        singleLabel2 = new QLabel(singleCard2);
        singleLabel2->setObjectName("singleLabel2");
        singleLabel2->setStyleSheet(QString::fromUtf8("font-size:12px;color:#12263f;font-weight:600;"));

        verticalLayout_22->addWidget(singleLabel2);

        singleStatus2 = new QLabel(singleCard2);
        singleStatus2->setObjectName("singleStatus2");
        singleStatus2->setStyleSheet(QString::fromUtf8("font-size:11px;color:#a0a0a0;"));

        verticalLayout_22->addWidget(singleStatus2);


        singleDeviceFeatureLayout->addWidget(singleCard2);

        singleCard3 = new QFrame(singleDeviceGroup);
        singleCard3->setObjectName("singleCard3");
        singleCard3->setMinimumSize(QSize(180, 70));
        singleCard3->setFrameShape(QFrame::Shape::StyledPanel);
        verticalLayout_23 = new QVBoxLayout(singleCard3);
        verticalLayout_23->setObjectName("verticalLayout_23");
        singleLabel3 = new QLabel(singleCard3);
        singleLabel3->setObjectName("singleLabel3");
        singleLabel3->setStyleSheet(QString::fromUtf8("font-size:12px;color:#12263f;font-weight:600;"));

        verticalLayout_23->addWidget(singleLabel3);

        singleStatus3 = new QLabel(singleCard3);
        singleStatus3->setObjectName("singleStatus3");
        singleStatus3->setStyleSheet(QString::fromUtf8("font-size:11px;color:#a0a0a0;"));

        verticalLayout_23->addWidget(singleStatus3);


        singleDeviceFeatureLayout->addWidget(singleCard3);

        singleCard4 = new QFrame(singleDeviceGroup);
        singleCard4->setObjectName("singleCard4");
        singleCard4->setMinimumSize(QSize(180, 70));
        singleCard4->setFrameShape(QFrame::Shape::StyledPanel);
        verticalLayout_24 = new QVBoxLayout(singleCard4);
        verticalLayout_24->setObjectName("verticalLayout_24");
        singleLabel4 = new QLabel(singleCard4);
        singleLabel4->setObjectName("singleLabel4");
        singleLabel4->setStyleSheet(QString::fromUtf8("font-size:12px;color:#12263f;font-weight:600;"));

        verticalLayout_24->addWidget(singleLabel4);

        singleStatus4 = new QLabel(singleCard4);
        singleStatus4->setObjectName("singleStatus4");
        singleStatus4->setStyleSheet(QString::fromUtf8("font-size:11px;color:#a0a0a0;"));

        verticalLayout_24->addWidget(singleStatus4);


        singleDeviceFeatureLayout->addWidget(singleCard4);

        singleDeviceSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        singleDeviceFeatureLayout->addItem(singleDeviceSpacer);


        singleDeviceLayout->addLayout(singleDeviceFeatureLayout);

        singleDeviceDetailLabel = new QLabel(singleDeviceGroup);
        singleDeviceDetailLabel->setObjectName("singleDeviceDetailLabel");
        singleDeviceDetailLabel->setStyleSheet(QString::fromUtf8("font-size:11px;color:#a0a0a0;padding-left:4px;"));
        singleDeviceDetailLabel->setWordWrap(true);

        singleDeviceLayout->addWidget(singleDeviceDetailLabel);


        verticalLayout_10->addWidget(singleDeviceGroup);

        multiDeviceGroup = new QGroupBox(analysisFrame);
        multiDeviceGroup->setObjectName("multiDeviceGroup");
        multiDeviceLayout = new QVBoxLayout(multiDeviceGroup);
        multiDeviceLayout->setSpacing(6);
        multiDeviceLayout->setObjectName("multiDeviceLayout");
        multiDeviceDescLabel = new QLabel(multiDeviceGroup);
        multiDeviceDescLabel->setObjectName("multiDeviceDescLabel");
        multiDeviceDescLabel->setStyleSheet(QString::fromUtf8("font-size:12px;color:#617487;"));
        multiDeviceDescLabel->setWordWrap(true);

        multiDeviceLayout->addWidget(multiDeviceDescLabel);

        compareTableWidget = new QTableWidget(multiDeviceGroup);
        if (compareTableWidget->columnCount() < 3)
            compareTableWidget->setColumnCount(3);
        QTableWidgetItem *__qtablewidgetitem6 = new QTableWidgetItem();
        compareTableWidget->setHorizontalHeaderItem(0, __qtablewidgetitem6);
        QTableWidgetItem *__qtablewidgetitem7 = new QTableWidgetItem();
        compareTableWidget->setHorizontalHeaderItem(1, __qtablewidgetitem7);
        QTableWidgetItem *__qtablewidgetitem8 = new QTableWidgetItem();
        compareTableWidget->setHorizontalHeaderItem(2, __qtablewidgetitem8);
        if (compareTableWidget->rowCount() < 3)
            compareTableWidget->setRowCount(3);
        QTableWidgetItem *__qtablewidgetitem9 = new QTableWidgetItem();
        compareTableWidget->setVerticalHeaderItem(0, __qtablewidgetitem9);
        QTableWidgetItem *__qtablewidgetitem10 = new QTableWidgetItem();
        compareTableWidget->setVerticalHeaderItem(1, __qtablewidgetitem10);
        QTableWidgetItem *__qtablewidgetitem11 = new QTableWidgetItem();
        compareTableWidget->setVerticalHeaderItem(2, __qtablewidgetitem11);
        QTableWidgetItem *__qtablewidgetitem12 = new QTableWidgetItem();
        compareTableWidget->setItem(0, 0, __qtablewidgetitem12);
        QTableWidgetItem *__qtablewidgetitem13 = new QTableWidgetItem();
        compareTableWidget->setItem(0, 1, __qtablewidgetitem13);
        QTableWidgetItem *__qtablewidgetitem14 = new QTableWidgetItem();
        compareTableWidget->setItem(0, 2, __qtablewidgetitem14);
        QTableWidgetItem *__qtablewidgetitem15 = new QTableWidgetItem();
        compareTableWidget->setItem(1, 0, __qtablewidgetitem15);
        QTableWidgetItem *__qtablewidgetitem16 = new QTableWidgetItem();
        compareTableWidget->setItem(1, 1, __qtablewidgetitem16);
        QTableWidgetItem *__qtablewidgetitem17 = new QTableWidgetItem();
        compareTableWidget->setItem(1, 2, __qtablewidgetitem17);
        QTableWidgetItem *__qtablewidgetitem18 = new QTableWidgetItem();
        compareTableWidget->setItem(2, 0, __qtablewidgetitem18);
        QTableWidgetItem *__qtablewidgetitem19 = new QTableWidgetItem();
        compareTableWidget->setItem(2, 1, __qtablewidgetitem19);
        QTableWidgetItem *__qtablewidgetitem20 = new QTableWidgetItem();
        compareTableWidget->setItem(2, 2, __qtablewidgetitem20);
        compareTableWidget->setObjectName("compareTableWidget");
        compareTableWidget->setMinimumSize(QSize(0, 150));
        compareTableWidget->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);
        compareTableWidget->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
        compareTableWidget->setColumnCount(3);
        compareTableWidget->horizontalHeader()->setStretchLastSection(true);

        multiDeviceLayout->addWidget(compareTableWidget);


        verticalLayout_10->addWidget(multiDeviceGroup);

        analysisBottomSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_10->addItem(analysisBottomSpacer);


        verticalLayout_7->addWidget(analysisFrame);

        topModuleTabWidget->addTab(analysisTab, QString());
        toolsTab = new QWidget();
        toolsTab->setObjectName("toolsTab");
        verticalLayout_20 = new QVBoxLayout(toolsTab);
        verticalLayout_20->setObjectName("verticalLayout_20");
        verticalLayout_20->setContentsMargins(16, 16, 16, 16);
        toolsTitleLabel = new QLabel(toolsTab);
        toolsTitleLabel->setObjectName("toolsTitleLabel");
        toolsTitleLabel->setStyleSheet(QString::fromUtf8("font-size:15px;font-weight:700;color:#12263f;"));

        verticalLayout_20->addWidget(toolsTitleLabel);

        toolsLayout = new QHBoxLayout();
        toolsLayout->setObjectName("toolsLayout");
        converterButton = new QPushButton(toolsTab);
        converterButton->setObjectName("converterButton");
        converterButton->setMinimumSize(QSize(180, 44));

        toolsLayout->addWidget(converterButton);

        converterStatusLabel = new QLabel(toolsTab);
        converterStatusLabel->setObjectName("converterStatusLabel");
        converterStatusLabel->setStyleSheet(QString::fromUtf8("font-size:12px;color:#617487;"));

        toolsLayout->addWidget(converterStatusLabel);

        toolsSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        toolsLayout->addItem(toolsSpacer);


        verticalLayout_20->addLayout(toolsLayout);

        toolsLayout2 = new QHBoxLayout();
        toolsLayout2->setObjectName("toolsLayout2");
        rtkplotButton = new QPushButton(toolsTab);
        rtkplotButton->setObjectName("rtkplotButton");
        rtkplotButton->setMinimumSize(QSize(180, 44));

        toolsLayout2->addWidget(rtkplotButton);

        rtkplotStatusLabel = new QLabel(toolsTab);
        rtkplotStatusLabel->setObjectName("rtkplotStatusLabel");
        rtkplotStatusLabel->setStyleSheet(QString::fromUtf8("font-size:12px;color:#617487;"));

        toolsLayout2->addWidget(rtkplotStatusLabel);

        toolsSpacer2 = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        toolsLayout2->addItem(toolsSpacer2);


        verticalLayout_20->addLayout(toolsLayout2);

        toolsBottomSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_20->addItem(toolsBottomSpacer);

        topModuleTabWidget->addTab(toolsTab, QString());

        verticalLayout_8->addWidget(topModuleTabWidget);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 1440, 18));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        topModuleTabWidget->setCurrentIndex(2);
        toolStackedWidget->setCurrentIndex(0);
        extraFuncTabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "TA Software", nullptr));
        titleLabel->setText(QCoreApplication::translate("MainWindow", "\345\244\232\345\212\237\350\203\275\346\265\213\350\257\225\346\225\260\346\215\256\345\210\206\346\236\220\345\271\263\345\217\260", nullptr));
        subTitleLabel->setText(QCoreApplication::translate("MainWindow", "\346\265\213\350\257\225\345\267\245\345\205\267\343\200\201\346\225\260\346\215\256\350\247\243\346\236\220\343\200\201\346\225\260\346\215\256\345\210\206\346\236\220", nullptr));
        headerStatusLabel->setText(QCoreApplication::translate("MainWindow", "\345\275\223\345\211\215\347\212\266\346\200\201\357\274\232\346\241\206\346\236\266\350\256\276\350\256\241\351\230\266\346\256\265", nullptr));
        ntripToolButton->setText(QCoreApplication::translate("MainWindow", "NTRIP", nullptr));
        commToolButton->setText(QCoreApplication::translate("MainWindow", "Comm Config", nullptr));
        ntripTitleLabel->setText(QCoreApplication::translate("MainWindow", "NTRIP \345\267\245\345\205\267", nullptr));
        ntripDescLabel->setText(QCoreApplication::translate("MainWindow", "\345\237\272\344\272\216 NTRIP HTTP1.0 \350\257\267\346\261\202\345\267\256\345\210\206\346\225\260\346\215\256\357\274\214\346\224\257\346\214\201\345\256\236\346\227\266\346\237\245\347\234\213\346\265\201\351\207\217\343\200\201\347\273\264\346\212\244\346\225\260\346\215\256\346\261\240\357\274\214\345\271\266\350\275\254\345\217\221\345\210\260\345\244\232\344\270\252 TCP \346\210\226\344\270\262\345\217\243\347\233\256\346\240\207\343\200\202", nullptr));
        ntripConfigButton->setText(QCoreApplication::translate("MainWindow", "\351\205\215\347\275\256", nullptr));
        label_14->setText(QCoreApplication::translate("MainWindow", "\350\277\236\346\216\245\347\212\266\346\200\201", nullptr));
        ntripConnectStatusLabel->setText(QCoreApplication::translate("MainWindow", "\346\234\252\350\277\236\346\216\245", nullptr));
        label_15->setText(QCoreApplication::translate("MainWindow", "\345\275\223\345\211\215\351\200\237\347\216\207", nullptr));
        ntripRateLabel->setText(QCoreApplication::translate("MainWindow", "0 B/s", nullptr));
        ntripPoolGroupBox->setTitle(QCoreApplication::translate("MainWindow", "\346\225\260\346\215\256\346\261\240\344\270\216\350\275\254\345\217\221\347\233\256\346\240\207", nullptr));
        ntripPoolDescLabel->setText(QCoreApplication::translate("MainWindow", "\345\267\256\345\210\206\346\225\260\346\215\256\345\205\210\350\277\233\345\205\245\345\206\205\351\203\250\346\225\260\346\215\256\346\261\240\357\274\214\345\206\215\347\224\261\345\244\232\344\270\252 TCP/\344\270\262\345\217\243\347\233\256\346\240\207\345\256\236\346\227\266\350\256\242\351\230\205\350\275\254\345\217\221\343\200\202\346\257\217\344\270\252\347\233\256\346\240\207\345\217\257\344\273\245\345\215\225\347\213\254\345\220\257\345\201\234\357\274\214\345\271\266\344\272\222\346\226\245\351\200\211\346\213\251 GPGGA \346\235\245\346\272\220\347\253\257\345\217\243\343\200\202", nullptr));
        addTcpTargetButton->setText(QCoreApplication::translate("MainWindow", "\346\226\260\345\242\236 TCP \347\233\256\346\240\207", nullptr));
        addSerialTargetButton->setText(QCoreApplication::translate("MainWindow", "\346\226\260\345\242\236\344\270\262\345\217\243\347\233\256\346\240\207", nullptr));
        fixGgaCheckBox->setText(QCoreApplication::translate("MainWindow", "FixGGA", nullptr));
        fixLonLabel->setText(QCoreApplication::translate("MainWindow", "\347\273\217\345\272\246", nullptr));
        fixLonEdit->setText(QCoreApplication::translate("MainWindow", "121.47370", nullptr));
        fixLonEdit->setPlaceholderText(QCoreApplication::translate("MainWindow", "121.123456", nullptr));
        fixLatLabel->setText(QCoreApplication::translate("MainWindow", "\347\272\254\345\272\246", nullptr));
        fixLatEdit->setText(QCoreApplication::translate("MainWindow", "31.23040", nullptr));
        fixLatEdit->setPlaceholderText(QCoreApplication::translate("MainWindow", "31.123456", nullptr));
        fixHeightLabel->setText(QCoreApplication::translate("MainWindow", "\351\253\230", nullptr));
        fixHeightEdit->setText(QCoreApplication::translate("MainWindow", "10.0", nullptr));
        fixHeightEdit->setPlaceholderText(QCoreApplication::translate("MainWindow", "100.0", nullptr));
        QTableWidgetItem *___qtablewidgetitem = ntripForwardTableWidget->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("MainWindow", "\347\261\273\345\236\213", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = ntripForwardTableWidget->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("MainWindow", "\347\233\256\346\240\207\345\234\260\345\235\200", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = ntripForwardTableWidget->horizontalHeaderItem(2);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("MainWindow", "\347\253\257\345\217\243/\346\263\242\347\211\271\347\216\207", nullptr));
        QTableWidgetItem *___qtablewidgetitem3 = ntripForwardTableWidget->horizontalHeaderItem(3);
        ___qtablewidgetitem3->setText(QCoreApplication::translate("MainWindow", "GGA\346\272\220", nullptr));
        QTableWidgetItem *___qtablewidgetitem4 = ntripForwardTableWidget->horizontalHeaderItem(4);
        ___qtablewidgetitem4->setText(QCoreApplication::translate("MainWindow", "\347\212\266\346\200\201", nullptr));
        QTableWidgetItem *___qtablewidgetitem5 = ntripForwardTableWidget->horizontalHeaderItem(5);
        ___qtablewidgetitem5->setText(QCoreApplication::translate("MainWindow", "\346\223\215\344\275\234", nullptr));
        ntripLogEdit->setPlainText(QCoreApplication::translate("MainWindow", "[NTRIP] Ready\n"
"[NTRIP] Waiting for connection...\n"
"[NTRIP] TCP/Serial forwarding UI only.", nullptr));
        serialNetTitleLabel->setText(QCoreApplication::translate("MainWindow", "\351\200\232\350\256\257\345\267\245\345\205\267", nullptr));
        connectionModeGroupBox->setTitle(QCoreApplication::translate("MainWindow", "\350\277\236\346\216\245\345\217\202\346\225\260", nullptr));
        connModeLabel->setText(QCoreApplication::translate("MainWindow", "\350\277\236\346\216\245\346\226\271\345\274\217", nullptr));
        connectionModeComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "\344\270\262\345\217\243", nullptr));
        connectionModeComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "TCP Client", nullptr));

        addrLabel->setText(QCoreApplication::translate("MainWindow", "\345\234\260\345\235\200", nullptr));
        toolAddressEdit->setPlaceholderText(QCoreApplication::translate("MainWindow", "127.0.0.1", nullptr));
#if QT_CONFIG(tooltip)
        commPortComboBox->setToolTip(QCoreApplication::translate("MainWindow", "\351\200\211\346\213\251\344\270\262\345\217\243\345\217\267", nullptr));
#endif // QT_CONFIG(tooltip)
        portLabel->setText(QCoreApplication::translate("MainWindow", "\347\253\257\345\217\243", nullptr));
        toolPortEdit->setPlaceholderText(QCoreApplication::translate("MainWindow", "9000", nullptr));
        baudLabel->setText(QCoreApplication::translate("MainWindow", "\346\263\242\347\211\271\347\216\207", nullptr));
        baudComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "9600", nullptr));
        baudComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "19200", nullptr));
        baudComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "38400", nullptr));
        baudComboBox->setItemText(3, QCoreApplication::translate("MainWindow", "57600", nullptr));
        baudComboBox->setItemText(4, QCoreApplication::translate("MainWindow", "115200", nullptr));
        baudComboBox->setItemText(5, QCoreApplication::translate("MainWindow", "230400", nullptr));
        baudComboBox->setItemText(6, QCoreApplication::translate("MainWindow", "460800", nullptr));
        baudComboBox->setItemText(7, QCoreApplication::translate("MainWindow", "921600", nullptr));

        baudComboBox->setCurrentText(QCoreApplication::translate("MainWindow", "9600", nullptr));
        paramLabel->setText(QCoreApplication::translate("MainWindow", "\345\217\202\346\225\260", nullptr));
        dataBitsComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "8", nullptr));
        dataBitsComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "7", nullptr));
        dataBitsComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "6", nullptr));
        dataBitsComboBox->setItemText(3, QCoreApplication::translate("MainWindow", "5", nullptr));

        dataBitsComboBox->setCurrentText(QCoreApplication::translate("MainWindow", "8", nullptr));
        parityComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "N", nullptr));
        parityComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "E", nullptr));
        parityComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "O", nullptr));

        stopBitsComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "1", nullptr));
        stopBitsComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "1.5", nullptr));
        stopBitsComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "2", nullptr));

        toolConnectButton->setText(QCoreApplication::translate("MainWindow", "\350\277\236\346\216\245", nullptr));
        extraFuncGroupBox->setTitle(QCoreApplication::translate("MainWindow", "\351\231\204\345\212\240\345\212\237\350\203\275", nullptr));
        extraFuncTabWidget->setTabText(extraFuncTabWidget->indexOf(atCmdTab), QCoreApplication::translate("MainWindow", "\345\270\270\347\224\250AT\346\214\207\344\273\244", nullptr));
        calibPlaceholderLabel->setText(QCoreApplication::translate("MainWindow", "\347\273\204\345\220\210\345\257\274\350\210\252\345\217\202\346\225\260\346\240\207\345\256\232\345\212\237\350\203\275\345\274\200\345\217\221\344\270\255...", nullptr));
        extraFuncTabWidget->setTabText(extraFuncTabWidget->indexOf(calibTab), QCoreApplication::translate("MainWindow", "\347\273\204\345\220\210\345\257\274\350\210\252\345\217\202\346\225\260\346\240\207\345\256\232", nullptr));
        debugPlaceholderLabel->setText(QCoreApplication::translate("MainWindow", "DEBUG\346\225\260\346\215\256\346\265\201\350\247\243\346\236\220\345\212\237\350\203\275\345\274\200\345\217\221\344\270\255...", nullptr));
        extraFuncTabWidget->setTabText(extraFuncTabWidget->indexOf(debugDataTab), QCoreApplication::translate("MainWindow", "DEBUG\350\247\243\346\236\220", nullptr));
        recvLabel->setText(QCoreApplication::translate("MainWindow", "\346\216\245\346\224\266\346\266\210\346\201\257", nullptr));
        recvCountLabel->setText(QCoreApplication::translate("MainWindow", "0 Bytes", nullptr));
        hexDisplayCheckBox->setText(QCoreApplication::translate("MainWindow", "HEX\346\230\276\347\244\272", nullptr));
        toolConsoleEdit->setPlainText(QCoreApplication::translate("MainWindow", "[COMM] Ready", nullptr));
        sendLabel->setText(QCoreApplication::translate("MainWindow", "\345\217\221\351\200\201\346\266\210\346\201\257", nullptr));
        sendNewlineCheckBox->setText(QCoreApplication::translate("MainWindow", "\345\217\221\351\200\201\346\226\260\350\241\214", nullptr));
        sendHexCheckBox->setText(QCoreApplication::translate("MainWindow", "HEX \345\217\221\351\200\201", nullptr));
        commandEdit->setPlaceholderText(QCoreApplication::translate("MainWindow", "\350\276\223\345\205\245\350\246\201\345\217\221\351\200\201\347\232\204\346\214\207\344\273\244...", nullptr));
        sendCommandButton->setText(QCoreApplication::translate("MainWindow", "\345\217\221\351\200\201", nullptr));
        saveConsoleButton->setText(QCoreApplication::translate("MainWindow", "\344\277\235\345\255\230\346\226\207\344\273\266", nullptr));
        clearConsoleButton->setText(QCoreApplication::translate("MainWindow", "\346\270\205\347\251\272\346\216\245\346\224\266", nullptr));
        topModuleTabWidget->setTabText(topModuleTabWidget->indexOf(toolTab), QCoreApplication::translate("MainWindow", "\346\265\213\350\257\225\345\267\245\345\205\267", nullptr));
        topModuleTabWidget->setTabText(topModuleTabWidget->indexOf(parseTab), QCoreApplication::translate("MainWindow", "\346\225\260\346\215\256\350\247\243\346\236\220", nullptr));
        analysisTitleLabel->setText(QCoreApplication::translate("MainWindow", "\346\225\260\346\215\256\345\210\206\346\236\220", nullptr));
        analysisInfoLabel->setText(QCoreApplication::translate("MainWindow", "\346\225\260\346\215\256\350\247\243\346\236\220\345\256\214\346\210\220\345\220\216\350\277\233\345\205\245\346\225\260\346\215\256\345\210\206\346\236\220\357\274\214\346\224\257\346\214\201\345\215\225\350\256\276\345\244\207\350\264\250\351\207\217\350\257\204\344\274\260\345\222\214\345\244\232\350\256\276\345\244\207\344\272\244\345\217\211\346\257\224\345\257\271\343\200\202", nullptr));
        singleDeviceGroup->setTitle(QCoreApplication::translate("MainWindow", "\345\215\225\350\256\276\345\244\207\345\210\206\346\236\220", nullptr));
        singleDeviceDescLabel->setText(QCoreApplication::translate("MainWindow", "\351\222\210\345\257\271\345\215\225\345\217\260\350\256\276\345\244\207\347\232\204\346\225\260\346\215\256\350\277\233\350\241\214\350\264\250\351\207\217\350\257\204\344\274\260\357\274\214\346\266\265\347\233\226\345\216\237\345\247\213\350\247\202\346\265\213\345\200\274\345\210\206\346\236\220\343\200\201\344\270\242\345\214\205\346\243\200\346\265\213\343\200\201\346\227\266\345\272\217\347\273\230\345\233\276\345\217\212\345\274\202\345\270\270\350\257\206\345\210\253\343\200\202", nullptr));
        singleLabel1->setText(QCoreApplication::translate("MainWindow", "\345\216\237\345\247\213\350\247\202\346\265\213\345\200\274\345\210\206\346\236\220", nullptr));
        singleStatus1->setText(QCoreApplication::translate("MainWindow", "\345\276\205\345\274\200\345\217\221", nullptr));
        singleLabel2->setText(QCoreApplication::translate("MainWindow", "\344\270\242\345\214\205\346\243\200\346\265\213", nullptr));
        singleStatus2->setText(QCoreApplication::translate("MainWindow", "\345\276\205\345\274\200\345\217\221", nullptr));
        singleLabel3->setText(QCoreApplication::translate("MainWindow", "\346\227\266\345\272\217\347\273\230\345\233\276", nullptr));
        singleStatus3->setText(QCoreApplication::translate("MainWindow", "\345\276\205\345\274\200\345\217\221", nullptr));
        singleLabel4->setText(QCoreApplication::translate("MainWindow", "\345\274\202\345\270\270\350\257\206\345\210\253", nullptr));
        singleStatus4->setText(QCoreApplication::translate("MainWindow", "\345\276\205\345\274\200\345\217\221", nullptr));
        singleDeviceDetailLabel->setText(QCoreApplication::translate("MainWindow", "  \345\216\237\345\247\213\350\247\202\346\265\213\345\200\274\357\274\232\345\215\253\346\230\237\346\225\260\347\273\237\350\256\241\343\200\201\345\244\232\351\242\221/\345\215\225\351\242\221\350\257\206\345\210\253\343\200\201\344\277\241\345\231\252\346\257\224(SNR)\345\210\206\345\270\203\347\255\211  |  \344\270\242\345\214\205\346\243\200\346\265\213\357\274\232\345\270\247\345\272\217\345\217\267\350\277\236\347\273\255\346\200\247\343\200\201RTCM \345\267\256\345\210\206\344\277\256\346\255\243\346\225\260\346\227\266\346\256\265\345\217\257\350\247\206\345\214\226\357\274\210\351\242\221\346\256\265/\346\230\237\345\272\247/\346\266\210\346\201\257\347\261\273\345\236\213\345\210\206\345\270\203\357\274\211", nullptr));
        multiDeviceGroup->setTitle(QCoreApplication::translate("MainWindow", "\345\244\232\350\256\276\345\244\207\346\257\224\345\257\271", nullptr));
        multiDeviceDescLabel->setText(QCoreApplication::translate("MainWindow", "\344\273\245\346\227\266\351\227\264\346\210\263\344\270\272\346\250\252\350\275\264\345\237\272\345\207\206\357\274\214\345\257\271\345\244\232\345\217\260\350\256\276\345\244\207\347\232\204\350\276\223\345\207\272\350\277\233\350\241\214\345\220\214\346\255\245\346\257\224\345\257\271\357\274\214\347\224\250\344\272\216\350\257\204\344\274\260\344\270\200\350\207\264\346\200\247\343\200\202", nullptr));
        QTableWidgetItem *___qtablewidgetitem6 = compareTableWidget->horizontalHeaderItem(0);
        ___qtablewidgetitem6->setText(QCoreApplication::translate("MainWindow", "\346\257\224\345\257\271\351\241\271", nullptr));
        QTableWidgetItem *___qtablewidgetitem7 = compareTableWidget->horizontalHeaderItem(1);
        ___qtablewidgetitem7->setText(QCoreApplication::translate("MainWindow", "\350\257\264\346\230\216", nullptr));
        QTableWidgetItem *___qtablewidgetitem8 = compareTableWidget->horizontalHeaderItem(2);
        ___qtablewidgetitem8->setText(QCoreApplication::translate("MainWindow", "\347\212\266\346\200\201", nullptr));
        QTableWidgetItem *___qtablewidgetitem9 = compareTableWidget->verticalHeaderItem(0);
        ___qtablewidgetitem9->setText(QCoreApplication::translate("MainWindow", "0", nullptr));
        QTableWidgetItem *___qtablewidgetitem10 = compareTableWidget->verticalHeaderItem(1);
        ___qtablewidgetitem10->setText(QCoreApplication::translate("MainWindow", "1", nullptr));
        QTableWidgetItem *___qtablewidgetitem11 = compareTableWidget->verticalHeaderItem(2);
        ___qtablewidgetitem11->setText(QCoreApplication::translate("MainWindow", "2", nullptr));

        const bool __sortingEnabled = compareTableWidget->isSortingEnabled();
        compareTableWidget->setSortingEnabled(false);
        QTableWidgetItem *___qtablewidgetitem12 = compareTableWidget->item(0, 0);
        ___qtablewidgetitem12->setText(QCoreApplication::translate("MainWindow", "\347\273\217\347\272\254\345\272\246/\351\253\230\345\272\246", nullptr));
        QTableWidgetItem *___qtablewidgetitem13 = compareTableWidget->item(0, 1);
        ___qtablewidgetitem13->setText(QCoreApplication::translate("MainWindow", "\345\244\232\350\256\276\345\244\207 lat/lon/alt \346\227\266\351\227\264\345\257\271\351\275\220\346\257\224\345\257\271", nullptr));
        QTableWidgetItem *___qtablewidgetitem14 = compareTableWidget->item(0, 2);
        ___qtablewidgetitem14->setText(QCoreApplication::translate("MainWindow", "\345\276\205\345\215\217\350\256\256\346\225\264\347\220\206", nullptr));
        QTableWidgetItem *___qtablewidgetitem15 = compareTableWidget->item(1, 0);
        ___qtablewidgetitem15->setText(QCoreApplication::translate("MainWindow", "\345\214\227\345\220\221/\344\270\234\345\220\221/\345\234\260\345\220\221\351\200\237\345\272\246", nullptr));
        QTableWidgetItem *___qtablewidgetitem16 = compareTableWidget->item(1, 1);
        ___qtablewidgetitem16->setText(QCoreApplication::translate("MainWindow", "\345\244\232\350\256\276\345\244\207 velN/velE/velD \346\227\266\351\227\264\345\257\271\351\275\220\346\257\224\345\257\271", nullptr));
        QTableWidgetItem *___qtablewidgetitem17 = compareTableWidget->item(1, 2);
        ___qtablewidgetitem17->setText(QCoreApplication::translate("MainWindow", "\345\276\205\345\215\217\350\256\256\346\225\264\347\220\206", nullptr));
        QTableWidgetItem *___qtablewidgetitem18 = compareTableWidget->item(2, 0);
        ___qtablewidgetitem18->setText(QCoreApplication::translate("MainWindow", "\345\247\277\346\200\201\350\247\222 (Roll/Pitch/Yaw)", nullptr));
        QTableWidgetItem *___qtablewidgetitem19 = compareTableWidget->item(2, 1);
        ___qtablewidgetitem19->setText(QCoreApplication::translate("MainWindow", "\345\244\232\350\256\276\345\244\207\345\247\277\346\200\201\350\247\222\346\227\266\351\227\264\345\257\271\351\275\220\346\257\224\345\257\271", nullptr));
        QTableWidgetItem *___qtablewidgetitem20 = compareTableWidget->item(2, 2);
        ___qtablewidgetitem20->setText(QCoreApplication::translate("MainWindow", "\345\276\205\345\215\217\350\256\256\346\225\264\347\220\206", nullptr));
        compareTableWidget->setSortingEnabled(__sortingEnabled);

        topModuleTabWidget->setTabText(topModuleTabWidget->indexOf(analysisTab), QCoreApplication::translate("MainWindow", "\346\225\260\346\215\256\345\210\206\346\236\220", nullptr));
        toolsTitleLabel->setText(QCoreApplication::translate("MainWindow", "\345\270\270\347\224\250\345\267\245\345\205\267", nullptr));
#if QT_CONFIG(tooltip)
        converterButton->setToolTip(QCoreApplication::translate("MainWindow", "\345\220\257\345\212\250 Converter3.0.7.exe (\345\222\214\350\212\257\345\216\237\345\247\213\346\225\260\346\215\256\350\247\243\346\236\220\345\267\245\345\205\267)", nullptr));
#endif // QT_CONFIG(tooltip)
        converterButton->setText(QCoreApplication::translate("MainWindow", "\345\222\214\350\212\257\345\216\237\345\247\213\346\225\260\346\215\256\350\247\243\346\236\220", nullptr));
        converterStatusLabel->setText(QCoreApplication::translate("MainWindow", "\346\234\252\345\220\257\345\212\250", nullptr));
#if QT_CONFIG(tooltip)
        rtkplotButton->setToolTip(QCoreApplication::translate("MainWindow", "\345\220\257\345\212\250 rtkplot.exe (RTKLIB \345\217\257\350\247\206\345\214\226\345\222\214\345\210\206\346\236\220\345\267\245\345\205\267)", nullptr));
#endif // QT_CONFIG(tooltip)
        rtkplotButton->setText(QCoreApplication::translate("MainWindow", "RTKPLOT", nullptr));
        rtkplotStatusLabel->setText(QCoreApplication::translate("MainWindow", "\346\234\252\345\220\257\345\212\250", nullptr));
        topModuleTabWidget->setTabText(topModuleTabWidget->indexOf(toolsTab), QCoreApplication::translate("MainWindow", "\345\270\270\347\224\250\345\267\245\345\205\267", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
