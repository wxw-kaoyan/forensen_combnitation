/********************************************************************************
** Form generated from reading UI file 'NTRIP.ui'
**
** Created by: Qt User Interface Compiler version 6.8.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_NTRIP_H
#define UI_NTRIP_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_NTRIPLoginDialog
{
public:
    QVBoxLayout *verticalLayout;
    QFrame *headerFrame;
    QHBoxLayout *horizontalLayout;
    QLabel *titleLabel;
    QSpacerItem *horizontalSpacer;
    QGroupBox *ntripSourceGroupBox;
    QFormLayout *formLayout;
    QLabel *label;
    QLineEdit *ntripHostEdit;
    QLabel *label_2;
    QLineEdit *ntripPortEdit;
    QLabel *label_3;
    QLineEdit *ntripMountEdit;
    QLabel *label_4;
    QLineEdit *ntripUserEdit;
    QLabel *label_5;
    QLineEdit *ntripPasswordEdit;
    QHBoxLayout *buttonLayout;
    QSpacerItem *leftSpacer;
    QPushButton *startButton;
    QSpacerItem *buttonGapSpacer;
    QPushButton *stopButton;
    QSpacerItem *rightSpacer;

    void setupUi(QDialog *NTRIPLoginDialog)
    {
        if (NTRIPLoginDialog->objectName().isEmpty())
            NTRIPLoginDialog->setObjectName("NTRIPLoginDialog");
        NTRIPLoginDialog->resize(520, 360);
        NTRIPLoginDialog->setStyleSheet(QString::fromUtf8("QDialog { background: #eef3f8; }\n"
"QPushButton { background: #17324d; color: white; border-radius: 8px;\n"
" padding: 8px 14px; font-weight: 600; }\n"
"QPushButton:hover { background: #24476b; }\n"
"QPushButton:disabled { background: #7a8fa0; color: #c0c8d0; }\n"
"QLineEdit { background: #fbfdff; border: 1px solid #ccd7e2; border-radius: 8px; padding: 6px; }\n"
"QGroupBox { background: #ffffff; border: 1px solid #d8e2ec; border-radius: 10px;\n"
" font-weight: 700; margin-top: 10px; }\n"
"QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }"));
        verticalLayout = new QVBoxLayout(NTRIPLoginDialog);
        verticalLayout->setSpacing(12);
        verticalLayout->setObjectName("verticalLayout");
        verticalLayout->setContentsMargins(12, 12, 12, 12);
        headerFrame = new QFrame(NTRIPLoginDialog);
        headerFrame->setObjectName("headerFrame");
        headerFrame->setFrameShape(QFrame::StyledPanel);
        headerFrame->setStyleSheet(QString::fromUtf8("QFrame { background: #ffffff; border: 1px solid #d8e2ec; border-radius: 10px; }"));
        horizontalLayout = new QHBoxLayout(headerFrame);
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalLayout->setContentsMargins(18, 14, 18, 14);
        titleLabel = new QLabel(headerFrame);
        titleLabel->setObjectName("titleLabel");
        titleLabel->setStyleSheet(QString::fromUtf8("font-size: 22px; font-weight: 700; color: #12263f;"));

        horizontalLayout->addWidget(titleLabel);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);


        verticalLayout->addWidget(headerFrame);

        ntripSourceGroupBox = new QGroupBox(NTRIPLoginDialog);
        ntripSourceGroupBox->setObjectName("ntripSourceGroupBox");
        formLayout = new QFormLayout(ntripSourceGroupBox);
        formLayout->setObjectName("formLayout");
        formLayout->setHorizontalSpacing(16);
        formLayout->setVerticalSpacing(10);
        label = new QLabel(ntripSourceGroupBox);
        label->setObjectName("label");
        label->setStyleSheet(QString::fromUtf8("font-weight: 600; color: #42556d;"));

        formLayout->setWidget(0, QFormLayout::LabelRole, label);

        ntripHostEdit = new QLineEdit(ntripSourceGroupBox);
        ntripHostEdit->setObjectName("ntripHostEdit");

        formLayout->setWidget(0, QFormLayout::FieldRole, ntripHostEdit);

        label_2 = new QLabel(ntripSourceGroupBox);
        label_2->setObjectName("label_2");
        label_2->setStyleSheet(QString::fromUtf8("font-weight: 600; color: #42556d;"));

        formLayout->setWidget(1, QFormLayout::LabelRole, label_2);

        ntripPortEdit = new QLineEdit(ntripSourceGroupBox);
        ntripPortEdit->setObjectName("ntripPortEdit");

        formLayout->setWidget(1, QFormLayout::FieldRole, ntripPortEdit);

        label_3 = new QLabel(ntripSourceGroupBox);
        label_3->setObjectName("label_3");
        label_3->setStyleSheet(QString::fromUtf8("font-weight: 600; color: #42556d;"));

        formLayout->setWidget(2, QFormLayout::LabelRole, label_3);

        ntripMountEdit = new QLineEdit(ntripSourceGroupBox);
        ntripMountEdit->setObjectName("ntripMountEdit");

        formLayout->setWidget(2, QFormLayout::FieldRole, ntripMountEdit);

        label_4 = new QLabel(ntripSourceGroupBox);
        label_4->setObjectName("label_4");
        label_4->setStyleSheet(QString::fromUtf8("font-weight: 600; color: #42556d;"));

        formLayout->setWidget(3, QFormLayout::LabelRole, label_4);

        ntripUserEdit = new QLineEdit(ntripSourceGroupBox);
        ntripUserEdit->setObjectName("ntripUserEdit");

        formLayout->setWidget(3, QFormLayout::FieldRole, ntripUserEdit);

        label_5 = new QLabel(ntripSourceGroupBox);
        label_5->setObjectName("label_5");
        label_5->setStyleSheet(QString::fromUtf8("font-weight: 600; color: #42556d;"));

        formLayout->setWidget(4, QFormLayout::LabelRole, label_5);

        ntripPasswordEdit = new QLineEdit(ntripSourceGroupBox);
        ntripPasswordEdit->setObjectName("ntripPasswordEdit");
        ntripPasswordEdit->setEchoMode(QLineEdit::Password);

        formLayout->setWidget(4, QFormLayout::FieldRole, ntripPasswordEdit);


        verticalLayout->addWidget(ntripSourceGroupBox);

        buttonLayout = new QHBoxLayout();
        buttonLayout->setObjectName("buttonLayout");
        leftSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        buttonLayout->addItem(leftSpacer);

        startButton = new QPushButton(NTRIPLoginDialog);
        startButton->setObjectName("startButton");
        startButton->setMinimumSize(QSize(100, 38));

        buttonLayout->addWidget(startButton);

        buttonGapSpacer = new QSpacerItem(24, 20, QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Minimum);

        buttonLayout->addItem(buttonGapSpacer);

        stopButton = new QPushButton(NTRIPLoginDialog);
        stopButton->setObjectName("stopButton");
        stopButton->setMinimumSize(QSize(100, 38));

        buttonLayout->addWidget(stopButton);

        rightSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        buttonLayout->addItem(rightSpacer);


        verticalLayout->addLayout(buttonLayout);


        retranslateUi(NTRIPLoginDialog);

        QMetaObject::connectSlotsByName(NTRIPLoginDialog);
    } // setupUi

    void retranslateUi(QDialog *NTRIPLoginDialog)
    {
        NTRIPLoginDialog->setWindowTitle(QCoreApplication::translate("NTRIPLoginDialog", "NTRIP \347\231\273\345\275\225\351\205\215\347\275\256", nullptr));
        titleLabel->setText(QCoreApplication::translate("NTRIPLoginDialog", "NTRIP \347\231\273\345\275\225\351\205\215\347\275\256", nullptr));
        ntripSourceGroupBox->setTitle(QCoreApplication::translate("NTRIPLoginDialog", "\350\257\267\346\261\202\351\205\215\347\275\256", nullptr));
        label->setText(QCoreApplication::translate("NTRIPLoginDialog", "Host", nullptr));
        ntripHostEdit->setPlaceholderText(QCoreApplication::translate("NTRIPLoginDialog", "vrs.example.com", nullptr));
        label_2->setText(QCoreApplication::translate("NTRIPLoginDialog", "Port", nullptr));
        ntripPortEdit->setPlaceholderText(QCoreApplication::translate("NTRIPLoginDialog", "8002", nullptr));
        label_3->setText(QCoreApplication::translate("NTRIPLoginDialog", "Mountpoint", nullptr));
        ntripMountEdit->setPlaceholderText(QCoreApplication::translate("NTRIPLoginDialog", "RTCM32_GRECJ2", nullptr));
        label_4->setText(QCoreApplication::translate("NTRIPLoginDialog", "User", nullptr));
        label_5->setText(QCoreApplication::translate("NTRIPLoginDialog", "Password", nullptr));
        startButton->setText(QCoreApplication::translate("NTRIPLoginDialog", "Start", nullptr));
        stopButton->setText(QCoreApplication::translate("NTRIPLoginDialog", "Stop", nullptr));
    } // retranslateUi

};

namespace Ui {
    class NTRIPLoginDialog: public Ui_NTRIPLoginDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_NTRIP_H
