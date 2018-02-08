/*
 * File: monitorform.cpp
 * Date: 2016. 8. 8.
 * Author: Yongseok Jin (mnm102211@gmail.com), Joohyun Kim
 * Copyright(c)2016
 * Hanyang University, Seoul, Korea
 * Embedded Software Systems Laboratory, All right reserved
 */

#include "monitorform.h"
#include "ui_monitorform.h"
#include <QFile>
#include <QFileDialog>

int FLASH_NB, PLANES_PER_FLASH;

MonitorForm::MonitorForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MonitorForm)
{
    printf("INIT SSD_MONITOR!!!\n");

    /* initialize variables. */
    init_variables();

    ui->setupUi(this);

    /* initialize timer. */
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(onTimer()));
    timer->start(1);

    /* initialize socket connected with VSSIM */
    socket = new QTcpSocket(this);
    connect(socket, SIGNAL(readyRead()), this, SLOT(onReceive()));
    socket->connectToHost("127.0.0.1", 9995);
}

MonitorForm::~MonitorForm()
{
    delete ui;
}

/*
 * initialize variables.
 */
void MonitorForm::init_variables()
{
    this->setWindowTitle("SSD Monitor");

    /* initialize count variables. */
	time = 0;
	readCount = writeCount = 0;
	writeSectorCount = readSectorCount = 0;
    recCount = eraseCount = 0;
	exchangeCount = seqMergeCount = randMergeCount = 0;
	trimCount = trimEffect = 0;
    writtenPageCount = writeAmpCount = 0;
}

/*
 * callback method for timer.
 */
void MonitorForm::onTimer()
{
    char sz_timer[20];
    time++;
    sprintf(sz_timer, "%lld", time);
    ui->txtTime->setText(sz_timer);
}

/*
 * callback method for socket.
 */
void MonitorForm::onReceive()
{
    QStringList szCmdList;
    QString szCmd;
    char szTemp[128];

    while(socket->canReadLine())
    {

        szCmd = socket->readLine();
        szCmdList = szCmd.split(" ");

        /* WRITE : write count, write sector count, write speed. */
        if(szCmdList[0] == "WRITE")
        {
            QTextStream stream(&szCmdList[2]);

            /* WRITE - SATA : set write count */
            if(szCmdList[1] == "SATA")
            {
                writeCount++;
                sprintf(szTemp, "%d", writeCount);
                ui->txtWriteCount->setText(szTemp);
            }
            /* WRITE - PAGE : set write sector count, speed */
            else if(szCmdList[1] == "PAGE")
            {
                double _time;
                unsigned int length;
                stream >> _time >> length;

                writeSectorCount += length;

                if(_time != 0)
                {
                    sprintf(szTemp, "%.3lf", _time);
                    ui->txtWriteSpeed->setText(szTemp);
                }
                sprintf(szTemp, "%ld", writeSectorCount);
                ui->txtWriteSectorCount->setText(szTemp);
            }
        }

        /* READ : read count, read sector count, read speed. */
        else if(szCmdList[0] == "READ")
        {
            QTextStream stream(&szCmdList[2]);

            /* READ - SATA : set read count */
            if(szCmdList[1] == "SATA")
			{
                readCount++;
                sprintf(szTemp, "%d", readCount);
                ui->txtReadCount->setText(szTemp);
            }
            /* READ - PAGE : read sector count, speed */
            else if(szCmdList[1] == "PAGE")
			{
                double _time;
                unsigned int length;
                stream >> _time >> length;

                readSectorCount += length;

                if(_time != 0)
                {
                    sprintf(szTemp, "%.3lf", _time);
                    ui->txtReadSpeed->setText(szTemp);
                }
                sprintf(szTemp, "%ld", readSectorCount);
                ui->txtReadSectorCount->setText(szTemp);
            }
        }

        /* ERASE : set erase count */
        else if(szCmdList[0] == "ERASE")
        {
            QTextStream stream(&szCmdList[1]);
            unsigned int erase_cnt;
            stream >> erase_cnt;

            eraseCount += erase_cnt;
            sprintf(szTemp, "%d", eraseCount);
            ui->txtEraseCount->setText(szTemp);
        }

        /* WB : written block count, write amplification. */
        else if(szCmdList[0] == "WB")
        {
            long long int wb = 0;
            QTextStream stream(&szCmdList[2]);
            stream >> wb;

            /* WB - CORRECT : written block count. */
            if(szCmdList[1] == "CORRECT")
            {
                    writtenPageCount += wb;
                    sprintf(szTemp, "%ld", writtenPageCount);
                    ui->txtWrittenPage->setText(szTemp);
            }
            /* WB - others : write amplificaton. */
            else
            {
                    writeAmpCount += wb;
                    sprintf(szTemp, "%ld", writeAmpCount);
                    ui->txtWriteAmp->setText(szTemp);
            }
        }

        /* TRIM : trim count, trim effect. */
        else if(szCmdList[0] == "TRIM")
        {
            /* TRIM - INSERT : trim count. */
            if(szCmdList[1] == "INSERT")
            {
                trimCount++;
                sprintf(szTemp, "%d", trimCount);
                ui->txtTrimCount->setText(szTemp);
            }
            /* TRIM - others : trim effect. */
            else
            {
                int effect = 0;
                QTextStream stream(&szCmdList[2]);
                stream >> effect;
                trimEffect+= effect;
                sprintf(szTemp, "%d", trimEffect);
                ui->txtTrimEffect->setText(szTemp);
            }
        }

		/* MERGE : seq merge, random merge */
		else if(szCmdList[0] == "MERGE")
		{
            QTextStream stream(&szCmdList[2]);
            int cnt;
            stream >> cnt;

            /* MERGE - RAND : random merge count */
            if(szCmdList[1] == "RAND")
            {
                randMergeCount += cnt;
                sprintf(szTemp, "%d", randMergeCount);
                ui->txtFullMerge->setText(szTemp);
            }
            /* MERGE - SEQ : sequential merge count */
            else
            {
                seqMergeCount += cnt;
                sprintf(szTemp, "%d", seqMergeCount);
                ui->txtPartialMerge->setText(szTemp);
            }
		}

		/* EXCHANGE : exchange count */
		else if(szCmdList[0] == "EXCHANGE")
		{
            QTextStream stream(&szCmdList[1]);
            int cnt;
            stream >> cnt;
            exchangeCount += cnt;
            sprintf(szTemp, "%d", exchangeCount);
            ui->txtExchange->setText(szTemp);
		}

        /* UTIL : SSD Util. */
        else if(szCmdList[0] == "UTIL")
        {
            double util = 0;
            QTextStream stream(&szCmdList[1]);
            stream >> util;
            sprintf(szTemp, "%lf", util);
            ui->txtSSDUtil->setText(szTemp);
        }

        /* put socket string to debug status. */
        ui->txtDebug->setText(szCmd);
    }
}


/*
 * callback method for reset button click.
 */
void MonitorForm::on_btnReset_clicked()
{
    init_variables();

    /* set line edit text "0". */
    ui->txtWriteCount->setText("0");
    ui->txtWriteSpeed->setText("0");
    ui->txtWriteSectorCount->setText("0");

    ui->txtReadCount->setText("0");
    ui->txtReadSpeed->setText("0");
    ui->txtReadSectorCount->setText("0");

    ui->txtEraseCount->setText("0");

    ui->txtExchange->setText("0");
    ui->txtPartialMerge->setText("0");
    ui->txtFullMerge->setText("0");

    ui->txtTrimCount->setText("0");
    ui->txtTrimEffect->setText("0");

    ui->txtWrittenPage->setText("0");
    ui->txtSSDUtil->setText("0");
    ui->txtTime->setText("0");
    ui->txtWriteAmp->setText("0");

    ui->txtDebug->setText("");
}


/*
 * callback method for save button click.
 */
void MonitorForm::on_btnSave_clicked()
{
    QString fileName;
    fileName = QFileDialog::getSaveFileName(this, tr("Save file"), tr("./data"), tr("Data files (*.dat"));
    QFile file(fileName);
    file.open(QIODevice::WriteOnly);

    QTextStream out(&file);

    char s_timer[20];
    sprintf(s_timer, "%lld", time);

    out << "==========================================================================\n";
    out << "|                                                                        |\n";
    out << "|           VSSIM : Virtual SSD Simulator                                |\n";
    out << "|                                                                        |\n";
    out << "|                                   Embedded Software Systems Lab.       |\n";
    out << "|                        Dept. of Electronics and Computer Engineering   |\n";
    out << "|                                         Hanynag Univ, Seoul, Korea     |\n";
    out << "|                                                                        |\n";
    out << "==========================================================================\n\n";

    out << "Write Request\t" << writeCount << "\n";
    out << "Write Sector\t" << writeSectorCount << "\n\n";
    out << "Read Request\t" << readCount << "\n";
    out << "Read Sector\t" << readSectorCount << "\n\n";
	out << "Erase Count\t" << eraseCount << "\n\n";

    out << "Merge\n";
    out << "  Exchange\t" << exchangeCount <<"\n";
    out << "  Sequential\t" << seqMergeCount << "\n";
    out << "  Random\t" << randMergeCount << "\n\n";

    out << "TRIM Count\t" << trimCount << "\n";
    out << "TRIM effect\t" << trimEffect << "\n\n";

    out << "Write Amplification\t" << writeAmpCount << "\n";
    out << "Written Page\t" << writtenPageCount << "\n";
    out << "Run-time[ms]\t" << s_timer << "\n";

    file.close();
}
