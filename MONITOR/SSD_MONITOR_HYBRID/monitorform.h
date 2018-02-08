/*
 * File: monitorform.h
 * Date: 2016. 8. 8.
 * Author: Yongseok Jin (mnm102211@gmail.com)
 * Copyright(c)2016
 * Hanyang University, Seoul, Korea
 * Embedded Software Systems Laboratory, All right reserved
 */

#ifndef MONITORFORM_H
#define MONITORFORM_H

#include <QDialog>
#include <QTimer>
#include <QTcpSocket>

namespace Ui {
class MonitorForm;
}

class MonitorForm : public QDialog
{
    Q_OBJECT

public slots:
    void onTimer();
    void onReceive();

public:
    explicit MonitorForm(QWidget *parent = 0);
    ~MonitorForm();
    void init_variables();

private slots:
    void on_btnReset_clicked();
    void on_btnSave_clicked();

private:
    Ui::MonitorForm *ui;
    QTcpSocket *socket;
    QTimer *timer;

    /* variables */
    long long int time;

	int readCount, writeCount;
	long int writeSectorCount, readSectorCount;
    int recCount, eraseCount;

	int exchangeCount;
	int seqMergeCount;
	int randMergeCount;

	int trimCount, trimEffect;

    long int writtenPageCount;
	long int writeAmpCount;
};

#endif // MONITORFORM_H
