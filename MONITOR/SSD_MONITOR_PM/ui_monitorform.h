/********************************************************************************
** Form generated from reading UI file 'monitorform.ui'
**
** Created by: Qt User Interface Compiler version 5.3.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MONITORFORM_H
#define UI_MONITORFORM_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>

QT_BEGIN_NAMESPACE

class Ui_MonitorForm
{
public:
    QPushButton *btnReset;
    QGroupBox *groupBox;
    QLabel *label;
    QLineEdit *txtWriteCount;
    QLabel *label1;
    QLabel *label2;
    QLineEdit *txtWriteSpeed;
    QLineEdit *txtReadSpeed;
    QLineEdit *txtWriteSectorCount;
    QLineEdit *txtReadSectorCount;
    QLabel *label3;
    QLabel *label4;
    QLineEdit *txtReadCount;
    QGroupBox *groupBox1;
    QLineEdit *txtGCCount;
    QLineEdit *txtGCExchange;
    QLineEdit *txtGCSector;
    QLabel *label5;
    QLabel *label6;
    QLabel *label7;
    QGroupBox *groupBox2;
    QLabel *label8;
    QLineEdit *txtTrimCount;
    QLineEdit *txtTrimEffect;
    QLabel *label9;
    QLabel *label10;
    QLineEdit *txtWriteAmp;
    QLineEdit *txtSSDUtil;
    QLabel *label11;
    QLabel *label12;
    QLineEdit *txtWrittenPage;
    QTextEdit *txtDebug;
    QLabel *label13;
    QLineEdit *txtTime;
    QLabel *label14;
    QPushButton *btnSave;

    void setupUi(QDialog *MonitorForm)
    {
        if (MonitorForm->objectName().isEmpty())
            MonitorForm->setObjectName(QStringLiteral("MonitorForm"));
        MonitorForm->resize(600, 500);
        MonitorForm->setProperty("name", QVariant(QByteArray("Form1")));
        btnReset = new QPushButton(MonitorForm);
        btnReset->setObjectName(QStringLiteral("btnReset"));
        btnReset->setGeometry(QRect(490, 20, 100, 50));
        btnReset->setAutoDefault(true);
        btnReset->setProperty("name", QVariant(QByteArray("btnReset")));
        groupBox = new QGroupBox(MonitorForm);
        groupBox->setObjectName(QStringLiteral("groupBox"));
        groupBox->setGeometry(QRect(21, 20, 461, 91));
        groupBox->setStyleSheet(QLatin1String("#groupBox {\n"
"	border : 1px solid gray;\n"
"	border-radius : 9px;\n"
"}"));
        groupBox->setProperty("name", QVariant(QByteArray("groupBox1")));
        label = new QLabel(groupBox);
        label->setObjectName(QStringLiteral("label"));
        label->setGeometry(QRect(10, 30, 56, 20));
        label->setProperty("name", QVariant(QByteArray("txtLabelWrite")));
        txtWriteCount = new QLineEdit(groupBox);
        txtWriteCount->setObjectName(QStringLiteral("txtWriteCount"));
        txtWriteCount->setGeometry(QRect(110, 30, 89, 22));
        txtWriteCount->setAlignment(Qt::AlignRight);
        txtWriteCount->setProperty("name", QVariant(QByteArray("txtWriteCount")));
        label1 = new QLabel(groupBox);
        label1->setObjectName(QStringLiteral("label1"));
        label1->setGeometry(QRect(130, 10, 60, 20));
        label1->setProperty("name", QVariant(QByteArray("txtLabelCount")));
        label2 = new QLabel(groupBox);
        label2->setObjectName(QStringLiteral("label2"));
        label2->setGeometry(QRect(240, 10, 91, 20));
        label2->setProperty("name", QVariant(QByteArray("txtLabelSpeed")));
        txtWriteSpeed = new QLineEdit(groupBox);
        txtWriteSpeed->setObjectName(QStringLiteral("txtWriteSpeed"));
        txtWriteSpeed->setGeometry(QRect(240, 30, 89, 22));
        txtWriteSpeed->setAlignment(Qt::AlignRight);
        txtWriteSpeed->setProperty("name", QVariant(QByteArray("txtWriteSpeed")));
        txtReadSpeed = new QLineEdit(groupBox);
        txtReadSpeed->setObjectName(QStringLiteral("txtReadSpeed"));
        txtReadSpeed->setGeometry(QRect(240, 60, 89, 22));
        txtReadSpeed->setAlignment(Qt::AlignRight);
        txtReadSpeed->setProperty("name", QVariant(QByteArray("txtReadSpeed")));
        txtWriteSectorCount = new QLineEdit(groupBox);
        txtWriteSectorCount->setObjectName(QStringLiteral("txtWriteSectorCount"));
        txtWriteSectorCount->setGeometry(QRect(360, 30, 89, 22));
        txtWriteSectorCount->setAlignment(Qt::AlignRight);
        txtWriteSectorCount->setProperty("name", QVariant(QByteArray("txtWriteSectorCount")));
        txtReadSectorCount = new QLineEdit(groupBox);
        txtReadSectorCount->setObjectName(QStringLiteral("txtReadSectorCount"));
        txtReadSectorCount->setGeometry(QRect(360, 60, 89, 22));
        txtReadSectorCount->setAlignment(Qt::AlignRight);
        txtReadSectorCount->setProperty("name", QVariant(QByteArray("txtReadSectorCount")));
        label3 = new QLabel(groupBox);
        label3->setObjectName(QStringLiteral("label3"));
        label3->setGeometry(QRect(360, 10, 100, 20));
        label3->setProperty("name", QVariant(QByteArray("txtLabelSectorCount")));
        label4 = new QLabel(groupBox);
        label4->setObjectName(QStringLiteral("label4"));
        label4->setGeometry(QRect(10, 60, 56, 20));
        label4->setProperty("name", QVariant(QByteArray("txtLabelRead")));
        txtReadCount = new QLineEdit(groupBox);
        txtReadCount->setObjectName(QStringLiteral("txtReadCount"));
        txtReadCount->setGeometry(QRect(110, 60, 89, 22));
        txtReadCount->setAlignment(Qt::AlignRight);
        txtReadCount->setProperty("name", QVariant(QByteArray("txtReadCount")));
        groupBox1 = new QGroupBox(MonitorForm);
        groupBox1->setObjectName(QStringLiteral("groupBox1"));
        groupBox1->setGeometry(QRect(20, 120, 461, 80));
        groupBox1->setStyleSheet(QLatin1String("QGroupBox {\n"
"    border: 1px solid gray;\n"
"    border-radius: 9px;\n"
"    margin-top: 0.5em;\n"
"	font-weight : bold;\n"
"}\n"
"\n"
"QGroupBox::title {\n"
"    subcontrol-origin: margin;\n"
"    left: 10px;\n"
"    padding: 0 3px 0 3px;\n"
"}"));
        groupBox1->setProperty("name", QVariant(QByteArray("groupBox2")));
        txtGCCount = new QLineEdit(groupBox1);
        txtGCCount->setObjectName(QStringLiteral("txtGCCount"));
        txtGCCount->setGeometry(QRect(110, 40, 89, 22));
        txtGCCount->setAlignment(Qt::AlignRight);
        txtGCCount->setProperty("name", QVariant(QByteArray("txtGCCount")));
        txtGCExchange = new QLineEdit(groupBox1);
        txtGCExchange->setObjectName(QStringLiteral("txtGCExchange"));
        txtGCExchange->setGeometry(QRect(240, 40, 89, 22));
        txtGCExchange->setAlignment(Qt::AlignRight);
        txtGCExchange->setProperty("name", QVariant(QByteArray("txtGCExchange")));
        txtGCSector = new QLineEdit(groupBox1);
        txtGCSector->setObjectName(QStringLiteral("txtGCSector"));
        txtGCSector->setGeometry(QRect(360, 40, 89, 22));
        txtGCSector->setAlignment(Qt::AlignRight);
        txtGCSector->setProperty("name", QVariant(QByteArray("txtGCSector")));
        label5 = new QLabel(groupBox1);
        label5->setObjectName(QStringLiteral("label5"));
        label5->setGeometry(QRect(130, 20, 66, 20));
        label5->setProperty("name", QVariant(QByteArray("txtLabelGCCount")));
        label6 = new QLabel(groupBox1);
        label6->setObjectName(QStringLiteral("label6"));
        label6->setGeometry(QRect(250, 20, 75, 20));
        label6->setProperty("name", QVariant(QByteArray("txtLabelGCExchange")));
        label7 = new QLabel(groupBox1);
        label7->setObjectName(QStringLiteral("label7"));
        label7->setGeometry(QRect(380, 20, 51, 20));
        label7->setProperty("name", QVariant(QByteArray("txtLabelGCSector")));
        groupBox2 = new QGroupBox(MonitorForm);
        groupBox2->setObjectName(QStringLiteral("groupBox2"));
        groupBox2->setGeometry(QRect(20, 210, 341, 70));
        groupBox2->setStyleSheet(QLatin1String("QGroupBox {\n"
"    border: 1px solid gray;\n"
"    border-radius: 9px;\n"
"    margin-top: 0.5em;\n"
"	font-weight : bold;\n"
"}\n"
"\n"
"QGroupBox::title {\n"
"    subcontrol-origin: margin;\n"
"    left: 10px;\n"
"    padding: 0 3px 0 3px;\n"
"}"));
        groupBox2->setProperty("name", QVariant(QByteArray("groupBox3")));
        label8 = new QLabel(groupBox2);
        label8->setObjectName(QStringLiteral("label8"));
        label8->setGeometry(QRect(130, 20, 56, 20));
        label8->setProperty("name", QVariant(QByteArray("txtLabelTrimCount")));
        txtTrimCount = new QLineEdit(groupBox2);
        txtTrimCount->setObjectName(QStringLiteral("txtTrimCount"));
        txtTrimCount->setGeometry(QRect(110, 40, 89, 22));
        txtTrimCount->setAlignment(Qt::AlignRight);
        txtTrimCount->setProperty("name", QVariant(QByteArray("txtTrimCount")));
        txtTrimEffect = new QLineEdit(groupBox2);
        txtTrimEffect->setObjectName(QStringLiteral("txtTrimEffect"));
        txtTrimEffect->setGeometry(QRect(240, 40, 89, 22));
        txtTrimEffect->setAlignment(Qt::AlignRight);
        txtTrimEffect->setProperty("name", QVariant(QByteArray("txtTrimEffect")));
        label9 = new QLabel(groupBox2);
        label9->setObjectName(QStringLiteral("label9"));
        label9->setGeometry(QRect(260, 20, 56, 20));
        label9->setProperty("name", QVariant(QByteArray("txtLabelTrimEffect")));
        label10 = new QLabel(MonitorForm);
        label10->setObjectName(QStringLiteral("label10"));
        label10->setGeometry(QRect(240, 300, 150, 20));
        label10->setProperty("name", QVariant(QByteArray("txtLabelWriteAmp")));
        txtWriteAmp = new QLineEdit(MonitorForm);
        txtWriteAmp->setObjectName(QStringLiteral("txtWriteAmp"));
        txtWriteAmp->setGeometry(QRect(390, 300, 89, 22));
        txtWriteAmp->setAlignment(Qt::AlignRight);
        txtWriteAmp->setProperty("name", QVariant(QByteArray("txtWriteAmp")));
        txtSSDUtil = new QLineEdit(MonitorForm);
        txtSSDUtil->setObjectName(QStringLiteral("txtSSDUtil"));
        txtSSDUtil->setGeometry(QRect(390, 330, 89, 22));
        txtSSDUtil->setAlignment(Qt::AlignRight);
        txtSSDUtil->setProperty("name", QVariant(QByteArray("txtSSDUtil")));
        label11 = new QLabel(MonitorForm);
        label11->setObjectName(QStringLiteral("label11"));
        label11->setGeometry(QRect(240, 330, 111, 20));
        label11->setProperty("name", QVariant(QByteArray("txtLabelSSDUtil")));
        label12 = new QLabel(MonitorForm);
        label12->setObjectName(QStringLiteral("label12"));
        label12->setGeometry(QRect(20, 300, 100, 20));
        label12->setProperty("name", QVariant(QByteArray("txtLabelWrittenPage")));
        txtWrittenPage = new QLineEdit(MonitorForm);
        txtWrittenPage->setObjectName(QStringLiteral("txtWrittenPage"));
        txtWrittenPage->setGeometry(QRect(130, 300, 89, 22));
        txtWrittenPage->setAlignment(Qt::AlignRight);
        txtWrittenPage->setProperty("name", QVariant(QByteArray("txtWrittenPage")));
        txtDebug = new QTextEdit(MonitorForm);
        txtDebug->setObjectName(QStringLiteral("txtDebug"));
        txtDebug->setGeometry(QRect(20, 400, 460, 80));
        txtDebug->setMouseTracking(false);
        txtDebug->setProperty("name", QVariant(QByteArray("txtDebug")));
        label13 = new QLabel(MonitorForm);
        label13->setObjectName(QStringLiteral("label13"));
        label13->setGeometry(QRect(20, 370, 120, 20));
        QFont font;
        font.setBold(true);
        font.setWeight(75);
        label13->setFont(font);
        label13->setProperty("name", QVariant(QByteArray("txtLabelDebug")));
        txtTime = new QLineEdit(MonitorForm);
        txtTime->setObjectName(QStringLiteral("txtTime"));
        txtTime->setGeometry(QRect(130, 330, 89, 22));
        txtTime->setAlignment(Qt::AlignRight);
        txtTime->setProperty("name", QVariant(QByteArray("txtTime")));
        label14 = new QLabel(MonitorForm);
        label14->setObjectName(QStringLiteral("label14"));
        label14->setGeometry(QRect(20, 330, 50, 20));
        label14->setProperty("name", QVariant(QByteArray("txtLabelTime")));
        btnSave = new QPushButton(MonitorForm);
        btnSave->setObjectName(QStringLiteral("btnSave"));
        btnSave->setGeometry(QRect(490, 90, 100, 50));
        btnSave->setProperty("name", QVariant(QByteArray("btnSave")));

        retranslateUi(MonitorForm);

        QMetaObject::connectSlotsByName(MonitorForm);
    } // setupUi

    void retranslateUi(QDialog *MonitorForm)
    {
        MonitorForm->setProperty("caption", QVariant(QApplication::translate("MonitorForm", "SSD Monitor", 0)));
        btnReset->setText(QApplication::translate("MonitorForm", "Reset", 0));
        groupBox->setTitle(QString());
        label->setText(QApplication::translate("MonitorForm", "Write:", 0));
        txtWriteCount->setText(QApplication::translate("MonitorForm", "0", 0));
        label1->setText(QApplication::translate("MonitorForm", "Count", 0));
        label2->setText(QApplication::translate("MonitorForm", "Speed [MB/s]", 0));
        txtWriteSpeed->setText(QApplication::translate("MonitorForm", "0", 0));
        txtReadSpeed->setText(QApplication::translate("MonitorForm", "0", 0));
        txtWriteSectorCount->setText(QApplication::translate("MonitorForm", "0", 0));
        txtReadSectorCount->setText(QApplication::translate("MonitorForm", "0", 0));
        label3->setText(QApplication::translate("MonitorForm", "Sector Count", 0));
        label4->setText(QApplication::translate("MonitorForm", "Read:", 0));
        txtReadCount->setText(QApplication::translate("MonitorForm", "0", 0));
        groupBox1->setTitle(QApplication::translate("MonitorForm", "Garbage Collection", 0));
        txtGCCount->setText(QApplication::translate("MonitorForm", "0", 0));
        txtGCExchange->setText(QApplication::translate("MonitorForm", "0", 0));
        txtGCSector->setText(QApplication::translate("MonitorForm", "0", 0));
        label5->setText(QApplication::translate("MonitorForm", "Count", 0));
        label6->setText(QApplication::translate("MonitorForm", "Exchange", 0));
        label7->setText(QApplication::translate("MonitorForm", "Sector", 0));
        groupBox2->setTitle(QApplication::translate("MonitorForm", "TRIM", 0));
        label8->setText(QApplication::translate("MonitorForm", "Count", 0));
        txtTrimCount->setText(QApplication::translate("MonitorForm", "0", 0));
        txtTrimEffect->setText(QApplication::translate("MonitorForm", "0", 0));
        label9->setText(QApplication::translate("MonitorForm", "Effect", 0));
        label10->setText(QApplication::translate("MonitorForm", "Write Amplification", 0));
        txtWriteAmp->setText(QApplication::translate("MonitorForm", "0", 0));
        txtSSDUtil->setText(QApplication::translate("MonitorForm", "0", 0));
        label11->setText(QApplication::translate("MonitorForm", "SSD Utilization", 0));
        label12->setText(QApplication::translate("MonitorForm", "Written Page", 0));
        txtWrittenPage->setText(QApplication::translate("MonitorForm", "0", 0));
        label13->setText(QApplication::translate("MonitorForm", "Debug Status", 0));
        txtTime->setText(QApplication::translate("MonitorForm", "0", 0));
        label14->setText(QApplication::translate("MonitorForm", "Time", 0));
        btnSave->setText(QApplication::translate("MonitorForm", "Save to File", 0));
    } // retranslateUi

};

namespace Ui {
    class MonitorForm: public Ui_MonitorForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MONITORFORM_H
