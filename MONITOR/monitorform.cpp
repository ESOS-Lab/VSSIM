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

MonitorForm::MonitorForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MonitorForm)
{
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
    socket->connectToHost("127.0.0.1", 9997);
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

    gcCount = 0;
    gcStarted = 0;
    blockEraseCount = 0;
    randMergeCount = seqMergeCount = 0;
    //overwriteCount = 0;

    writeCmdCount = readCmdCount = 0;
    writePageCount = readPageCount = 0;
    trimEffect = trimCount = 0;
    writeAmpCount = writtenPageCount = 0;

    readTime = writeTime = 0;

    fflush(stdout);
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
        int64_t arg;

        szCmd = socket->readLine();
        szCmdList = szCmd.split(" ");

        /* WRITE : write count, write sector count, write speed. */
        if(szCmdList[0] == "WRITE")
        {
            QTextStream stream(&szCmdList[2]);

            /* WRITE - PAGE : write count, write sector count. */
            if(szCmdList[1] == "PAGE")
            {
                // Write Page Count
                stream >> arg;
                writePageCount = arg;

                sprintf(szTemp, "%ld", writePageCount);
                ui->txtWritePageCount->setText(szTemp);
	    }
	    else if(szCmdList[1] == "REQ")
	    {
                // Write SATA Command Count
                stream >> arg;
                writeCmdCount = arg;

                sprintf(szTemp, "%ld", writeCmdCount);
                ui->txtWriteCmdCount->setText(szTemp);
            }
            /* WRITE - BW : write speed. */
            else if(szCmdList[1] == "BW")
            {
                double t;
                stream >> t;

                if(t != 0){
                    sprintf(szTemp, "%0.3lf", t);
                    ui->txtWriteSpeed->setText(szTemp);
                }
            }
        }

        /* READ : read count, read sector count, read speed. */
        else if(szCmdList[0] == "READ")
        {
            QTextStream stream(&szCmdList[2]);

            /* READ - PAGE : read count, read sector count. */
            if(szCmdList[1] == "PAGE"){

                /* Read Page Number Count */
                stream >> arg;
                readPageCount = arg;

                sprintf(szTemp, "%ld", readPageCount);
                ui->txtReadPageCount->setText(szTemp);
	    }
	    else if(szCmdList[1] == "REQ"){
	
                /* Read SATA Command Count */
	        stream >> arg;
                readCmdCount = arg ;

                sprintf(szTemp, "%ld", readCmdCount);
                ui->txtReadCmdCount->setText(szTemp);
            }
            /* READ - BW : read speed. */
            else if(szCmdList[1] == "BW"){
                double t;
                stream >> t;

                if(t != 0)
                {
                    sprintf(szTemp, "%0.3lf", t);
                    ui->txtReadSpeed->setText(szTemp);
                }
            }
        }

        /* GC : gc has been occured. */
        else if(szCmdList[0] == "GC")
        {
            QTextStream stream(&szCmdList[2]);

            /* READ - PAGE : read count, read sector count. */
            if(szCmdList[1] == "CALL"){

	        /* GC call count */
		stream >> arg;
	        gcCount = arg;

	        sprintf(szTemp, "%ld", gcCount);
	        ui->txtGCCount->setText(szTemp);
	    }
	    else if(szCmdList[1] == "AMP"){

	        /* GC Amplification count */
		stream >> arg;
	        writeAmpCount = arg;

	        sprintf(szTemp, "%ld", writeAmpCount);
                ui->txtWriteAmp->setText(szTemp);
	    }
	    else if(szCmdList[1] == "ERASE"){
	        
		/* GC Amplification count */
		stream >> arg;
	        blockEraseCount = arg;

	        sprintf(szTemp, "%ld", blockEraseCount);
                ui->txtGCErase->setText(szTemp);
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
    ui->txtWriteCmdCount->setText("0");
    ui->txtWriteSpeed->setText("0");
    ui->txtWritePageCount->setText("0");

    ui->txtReadCmdCount->setText("0");
    ui->txtReadSpeed->setText("0");
    ui->txtReadPageCount->setText("0");

    ui->txtGCCount->setText("0");
    ui->txtGCErase->setText("0");

    ui->txtTrimCount->setText("0");
    ui->txtTrimEffect->setText("0");

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

    out << "Write Request\t" << writeCmdCount << "\n";
    out << "Write Page\t" << writePageCount << "\n\n";
    out << "Read Request\t" << readCmdCount << "\n";
    out << "Read Page\t" << readPageCount << "\n\n";

    out << "Garbage Collection\n";
    out << "  Count\t" << gcCount <<"\n";
    out << "  Block Erase\t" << blockEraseCount << "\n\n";

    out << "TRIM Count\t" << trimCount << "\n";
    out << "TRIM effect\t" << trimEffect << "\n\n";

    out << "Write Amplification\t" << writeAmpCount << "\n";
    out << "Written Page\t" << writtenPageCount << "\n";
    out << "Run-time[ms]\t" << s_timer << "\n";

    file.close();
}
