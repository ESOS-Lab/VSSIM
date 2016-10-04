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
    FILE* pfData;
    char szCommand[1024];

    this->setWindowTitle("SSD Monitor");

    /* initialize count variables. */
    time = 0;
    powCount = 0;

    gcCount = 0;
    gcStarted = 0;
    gcExchange = gcSector = 0;
    randMergeCount = seqMergeCount = 0;
    //overwriteCount = 0;

    writeCount = readCount = 0;
    writeSectorCount = readSectorCount = 0;
    trimEffect = trimCount = 0;
    writeAmpCount = writtenPageCount = 0;

    readTime = writeTime = 0;

    /* open ssd configuration file, set values. */
    pfData = fopen("./ssd.conf","r");

    if(pfData == NULL)
        printf(" Monitor file open failed\n");
    else
    {
        printf(" Monitor file open success\n");

        /* read configuration values from file. */
        while(fscanf(pfData, "%s", szCommand) != EOF)
        {
            if(strcmp(szCommand, "FLASH_NB") == 0)
                fscanf(pfData, "%d", &FLASH_NB);

            else if(strcmp(szCommand, "PLANES_PER_FLASH") == 0)
                fscanf(pfData, "%d", &PLANES_PER_FLASH);

            else if(strcmp(szCommand, "CELL_PROGRAM_DELAY") == 0)
                fscanf(pfData, "%d", &CELL_PROGRAM_DELAY);

            memset(szCommand, 0x00, 1024);
        }

        fclose(pfData);

        access_time_reg_mon = (long long int*)malloc(sizeof(long long int) * FLASH_NB * PLANES_PER_FLASH);
        access_type_reg_mon = (int *)malloc(sizeof(int) * FLASH_NB * PLANES_PER_FLASH);

        printf("\n\t initialize\n");
        memset(access_time_reg_mon, 0x00, sizeof(long long int) * FLASH_NB * PLANES_PER_FLASH);
        memset(access_type_reg_mon, 0x00, sizeof(int) * FLASH_NB * PLANES_PER_FLASH);
    }

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

        szCmd = socket->readLine();
        szCmdList = szCmd.split(" ");

        /* POWER : store inputs to reg_mon's. */
        if(szCmdList[0] == "POWER")
        {
            int regst;
            QTextStream stream1(&szCmdList[1]), stream2(&szCmdList[2]), stream3(&szCmdList[3]);
            stream1 >> regst;
            stream2 >> access_time_reg_mon[regst];
            stream3 >> access_type_reg_mon[regst];
        }

        /* WRITE : write count, write sector count, write speed. */
        else if(szCmdList[0] == "WRITE")
        {
            QTextStream stream(&szCmdList[2]);

            /* WRITE - PAGE : write count, write sector count. */
            if(szCmdList[1] == "PAGE")
            {
                unsigned int length;
                stream >> length;

                // Write Sector Number Count
                writeSectorCount += length;

                sprintf(szTemp, "%ld", writeSectorCount);
                ui->txtWriteSectorCount->setText(szTemp);

                // Write SATA Command Count
                writeCount++;
                sprintf(szTemp, "%ld", writeCount);
                ui->txtWriteCount->setText(szTemp);
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
                unsigned int length;

                /* Read Sector Number Count */
                stream >> length;
                readSectorCount += length;

                sprintf(szTemp, "%ld", readSectorCount);
                ui->txtReadSectorCount->setText(szTemp);

                /* Read SATA Command Count */
                readCount++;
                sprintf(szTemp, "%ld", readCount);
                ui->txtReadCount->setText(szTemp);
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
            gcCount++;
            sprintf(szTemp, "%ld", gcCount);
            ui->txtGCCount->setText(szTemp);
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

    ui->txtGCCount->setText("0");
    ui->txtGCExchange->setText("0");
    ui->txtGCSector->setText("0");

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

    out << "Garbage Collection\n";
    out << "  Count\t" << gcCount <<"\n";
    out << "  Exchange\t" << gcExchange << "\n";
    out << "  Sector Count\t" << gcStarted << "\n\n";

    out << "TRIM Count\t" << trimCount << "\n";
    out << "TRIM effect\t" << trimEffect << "\n\n";

    out << "Write Amplification\t" << writeAmpCount << "\n";
    out << "Written Page\t" << writtenPageCount << "\n";
    out << "Run-time[ms]\t" << s_timer << "\n";

    file.close();
}
