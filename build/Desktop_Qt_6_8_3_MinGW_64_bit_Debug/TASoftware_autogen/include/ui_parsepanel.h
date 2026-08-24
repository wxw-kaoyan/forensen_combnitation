/********************************************************************************
** Form generated from reading UI file 'parsepanel.ui'
**
** Created by: Qt User Interface Compiler version 6.8.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_PARSEPANEL_H
#define UI_PARSEPANEL_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ParsePanel
{
public:
    QVBoxLayout *rootLayout;
    QHBoxLayout *titleLayout;
    QLabel *titleLabel;
    QSpacerItem *spacerItem;
    QFrame *loadFrame;
    QHBoxLayout *loadLayout;
    QPushButton *loadFileButton;
    QCheckBox *crcCheckBox;
    QLabel *filePathLabel;
    QSpacerItem *spacerItem1;
    QLabel *fileInfoLabel;
    QFrame *filterFrame;
    QHBoxLayout *filterLayout;
    QLabel *protoLabel;
    QSpacerItem *spacerItem2;
    QTextEdit *outputTextEdit;
    QHBoxLayout *actionLayout;
    QPushButton *exportButton;
    QPushButton *newCodeButton;
    QPushButton *downsampleButton;
    QSpacerItem *spacerItem3;
    QPushButton *clearButton;

    void setupUi(QWidget *ParsePanel)
    {
        if (ParsePanel->objectName().isEmpty())
            ParsePanel->setObjectName("ParsePanel");
        ParsePanel->resize(800, 600);
        rootLayout = new QVBoxLayout(ParsePanel);
        rootLayout->setSpacing(10);
        rootLayout->setObjectName("rootLayout");
        rootLayout->setContentsMargins(12, 12, 12, 12);
        titleLayout = new QHBoxLayout();
        titleLayout->setObjectName("titleLayout");
        titleLabel = new QLabel(ParsePanel);
        titleLabel->setObjectName("titleLabel");

        titleLayout->addWidget(titleLabel);

        spacerItem = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        titleLayout->addItem(spacerItem);


        rootLayout->addLayout(titleLayout);

        loadFrame = new QFrame(ParsePanel);
        loadFrame->setObjectName("loadFrame");
        loadFrame->setFrameShape(QFrame::StyledPanel);
        loadLayout = new QHBoxLayout(loadFrame);
        loadLayout->setObjectName("loadLayout");
        loadLayout->setContentsMargins(12, 10, 12, 10);
        loadFileButton = new QPushButton(loadFrame);
        loadFileButton->setObjectName("loadFileButton");
        loadFileButton->setMinimumSize(QSize(100, 36));

        loadLayout->addWidget(loadFileButton);

        crcCheckBox = new QCheckBox(loadFrame);
        crcCheckBox->setObjectName("crcCheckBox");

        loadLayout->addWidget(crcCheckBox);

        filePathLabel = new QLabel(loadFrame);
        filePathLabel->setObjectName("filePathLabel");

        loadLayout->addWidget(filePathLabel);

        spacerItem1 = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        loadLayout->addItem(spacerItem1);

        fileInfoLabel = new QLabel(loadFrame);
        fileInfoLabel->setObjectName("fileInfoLabel");

        loadLayout->addWidget(fileInfoLabel);


        rootLayout->addWidget(loadFrame);

        filterFrame = new QFrame(ParsePanel);
        filterFrame->setObjectName("filterFrame");
        filterFrame->setFrameShape(QFrame::StyledPanel);
        filterLayout = new QHBoxLayout(filterFrame);
        filterLayout->setObjectName("filterLayout");
        filterLayout->setContentsMargins(12, 10, 12, 10);
        protoLabel = new QLabel(filterFrame);
        protoLabel->setObjectName("protoLabel");

        filterLayout->addWidget(protoLabel);

        spacerItem2 = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        filterLayout->addItem(spacerItem2);


        rootLayout->addWidget(filterFrame);

        outputTextEdit = new QTextEdit(ParsePanel);
        outputTextEdit->setObjectName("outputTextEdit");
        outputTextEdit->setReadOnly(true);
        QFont font;
        font.setFamilies({QString::fromUtf8("Consolas")});
        font.setPointSize(11);
        outputTextEdit->setFont(font);

        rootLayout->addWidget(outputTextEdit);

        actionLayout = new QHBoxLayout();
        actionLayout->setObjectName("actionLayout");
        exportButton = new QPushButton(ParsePanel);
        exportButton->setObjectName("exportButton");
        exportButton->setMinimumSize(QSize(100, 36));

        actionLayout->addWidget(exportButton);

        newCodeButton = new QPushButton(ParsePanel);
        newCodeButton->setObjectName("newCodeButton");
        newCodeButton->setMinimumSize(QSize(100, 36));

        actionLayout->addWidget(newCodeButton);

        downsampleButton = new QPushButton(ParsePanel);
        downsampleButton->setObjectName("downsampleButton");
        downsampleButton->setMinimumSize(QSize(100, 36));

        actionLayout->addWidget(downsampleButton);

        spacerItem3 = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        actionLayout->addItem(spacerItem3);

        clearButton = new QPushButton(ParsePanel);
        clearButton->setObjectName("clearButton");
        clearButton->setMinimumSize(QSize(80, 36));

        actionLayout->addWidget(clearButton);


        rootLayout->addLayout(actionLayout);


        retranslateUi(ParsePanel);

        QMetaObject::connectSlotsByName(ParsePanel);
    } // setupUi

    void retranslateUi(QWidget *ParsePanel)
    {
        titleLabel->setStyleSheet(QCoreApplication::translate("ParsePanel", "font-size:16px;font-weight:700;color:#12263f;", nullptr));
        titleLabel->setText(QCoreApplication::translate("ParsePanel", "\346\225\260\346\215\256\350\247\243\346\236\220", nullptr));
        loadFileButton->setText(QCoreApplication::translate("ParsePanel", "\345\212\240\350\275\275\346\226\207\344\273\266", nullptr));
        loadFileButton->setStyleSheet(QCoreApplication::translate("ParsePanel", "QPushButton{background:#17324d;color:white;border-radius:4px;font-size:13px;font-weight:600;padding:0 16px;}QPushButton:hover{background:#1e426b;}", nullptr));
        crcCheckBox->setText(QCoreApplication::translate("ParsePanel", "CRC\346\240\241\351\252\214", nullptr));
        crcCheckBox->setStyleSheet(QCoreApplication::translate("ParsePanel", "font-size:13px;font-weight:600;color:#42556d;", nullptr));
        filePathLabel->setStyleSheet(QCoreApplication::translate("ParsePanel", "font-size:13px;color:#617487;", nullptr));
        filePathLabel->setText(QCoreApplication::translate("ParsePanel", "\346\234\252\345\212\240\350\275\275\346\226\207\344\273\266", nullptr));
        fileInfoLabel->setStyleSheet(QCoreApplication::translate("ParsePanel", "font-size:13px;color:#12263f;", nullptr));
        fileInfoLabel->setText(QString());
        protoLabel->setStyleSheet(QCoreApplication::translate("ParsePanel", "font-size:13px;font-weight:600;color:#12263f;", nullptr));
        protoLabel->setText(QCoreApplication::translate("ParsePanel", "\345\215\217\350\256\256:", nullptr));
        outputTextEdit->setStyleSheet(QCoreApplication::translate("ParsePanel", "QTextEdit{background:#f8f9fa;border:1px solid #d0d5dd;border-radius:4px;padding:8px;color:#12263f;}", nullptr));
        exportButton->setText(QCoreApplication::translate("ParsePanel", "\345\257\274\345\207\272CSV", nullptr));
        exportButton->setStyleSheet(QCoreApplication::translate("ParsePanel", "QPushButton{background:#17324d;color:white;border-radius:4px;font-size:13px;font-weight:600;padding:0 16px;}QPushButton:hover{background:#1e426b;}", nullptr));
        newCodeButton->setText(QCoreApplication::translate("ParsePanel", "New Code", nullptr));
        newCodeButton->setStyleSheet(QCoreApplication::translate("ParsePanel", "QPushButton{background:#2a5a8c;color:white;border-radius:4px;font-size:13px;font-weight:600;padding:0 16px;}QPushButton:hover{background:#3169a3;}", nullptr));
        downsampleButton->setText(QCoreApplication::translate("ParsePanel", "\351\231\215\351\207\207\346\240\267", nullptr));
        downsampleButton->setStyleSheet(QCoreApplication::translate("ParsePanel", "QPushButton{background:#2a5a8c;color:white;border-radius:4px;font-size:13px;font-weight:600;padding:0 16px;}QPushButton:hover{background:#3169a3;}", nullptr));
        clearButton->setText(QCoreApplication::translate("ParsePanel", "\346\270\205\347\251\272", nullptr));
        clearButton->setStyleSheet(QCoreApplication::translate("ParsePanel", "QPushButton{background:#e0e0e0;color:#333;border-radius:4px;font-size:13px;padding:0 16px;}QPushButton:hover{background:#ccc;}", nullptr));
        (void)ParsePanel;
    } // retranslateUi

};

namespace Ui {
    class ParsePanel: public Ui_ParsePanel {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PARSEPANEL_H
