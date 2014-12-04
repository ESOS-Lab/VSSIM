// File: form.ui.h
// Date: 2014. 12. 03.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

/***************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/
#include "form1.h"
#include <qfile.h>
#include <qfiledialog.h>
#include <qdir.h>
#include <stdlib.h>
#include <sys/time.h>
#include<stdio.h>


#define REG_IS_WRITE 706

int FLASH_NB;
int PLANES_PER_FLASH;

long long int get_usec(void){
	long long int t = 0;
	struct timeval tv;
	struct timezone tz;

	gettimeofday(&tv, &tz);
	t = tv.tv_sec;
	t *= 1000000;
	t += tv.tv_usec;

	return t;
}
void Form1::init_var()
{
	int i;
	FILE* pfData;
	pfData = fopen("./ssd.conf","r");
		printf(" Monitor file open success\n");
	if(pfData == NULL){
		printf(" Monitor file open failed\n");
	}
	char* szCommand = NULL;
	szCommand = (char *)malloc(1024);
	memset(szCommand, 0x00, 1024);
	if(pfData != NULL){
		while(fscanf(pfData, "%s", szCommand) != EOF)
		{
			if(strcmp(szCommand, "FLASH_NB") == 0)
			{
				fscanf(pfData, "%d", &FLASH_NB);
			}else if(strcmp(szCommand, "PLANES_PER_FLASH") == 0)
			{
				fscanf(pfData, "%d", &PLANES_PER_FLASH);
			}else if(strcmp(szCommand, "CELL_PROGRAM_DELAY") == 0)
			{
				fscanf(pfData, "%d", &CELL_PROGRAM_DELAY);
			}
			memset(szCommand, 0x00, 1024);
		}
		fclose(pfData);
	}
	WriteSecCount = 0;
	ReadSecCount = 0;
	WriteCount = 0;
	ReadCount = 0;
	PowCount = 0;

	RandMergeCount = 0;
	SeqMergeCount = 0;
	ExchangeCount = 0;
	WriteSectorCount = 0;
	TrimEffect = 0;
	TrimCount = 0;
	WrittenCorrectCount = 0;
	WriteAmpCount = 0;
	GCStarted = 0;
	GC_NB = 0;
//    OverwriteCount = 0;
	TimeProg = 0;
	WriteTime = 0;
	ReadTime = 0;

	access_time_reg_mon = (long long int*)malloc(sizeof(long long int) * FLASH_NB * PLANES_PER_FLASH);
	access_type_reg_mon = (int *)malloc(sizeof(int) * FLASH_NB * PLANES_PER_FLASH);
	
	printf("\n\t initialize\n");
	for(i=0; i<FLASH_NB*PLANES_PER_FLASH; i++){
		access_time_reg_mon[i] = 0;
//	    printf("%lld\n", access_time_reg_mon[i]);
	}
	for(i=0; i<FLASH_NB*PLANES_PER_FLASH; i++){
	access_type_reg_mon[i] = 0;
//	    printf("%d\n", access_type_reg_mon[i]);
	}
}

void Form1::btnConnect_clicked()
{

    
}


void Form1::init()
{
	printf("INIT SSD_MONITOR!!!\n");
	init_var();

	sock = new QSocket(this);
	connect(sock, SIGNAL(readyRead()), this, SLOT(onReceive()));

    	/* socket의 서버 설정. 실험 pc의 서버와 동일하게 한다. */
	sock->connectToHost("127.0.0.1", 9995);

	// timer Setting
	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(onTimer()));
	timer->start(1, false);
	
}

void Form1::onReceive()
{

	QStringList szCmdList;
	QString szCmd;
	char szTemp[128];

	while(sock->canReadLine())
	{

		szCmd = sock->readLine();
	 	szCmdList = QStringList::split(" ", szCmd);

		/* POWER인 소켓을 받았을 경우 */
		if(szCmdList[0] == "POWER"){
			int regst;
			sscanf(szCmdList[1], "%d", &regst);
			sscanf(szCmdList[2], "%lld", &access_time_reg_mon[regst]);
			sscanf(szCmdList[3], "%d", &access_type_reg_mon[regst]);
		}

		/* WRITE인 소켓을 받았을 경우 */
		else if(szCmdList[0] == "WRITE")
		{
			if(szCmdList[1] == "PAGE"){        
        			unsigned int length;
				sscanf(szCmdList[2], "%u", &length);
				// Write Sector Number Count
				WriteSecCount += length;

				sprintf(szTemp, "%u", WriteSecCount);
				txtWriteSectorCount->setText(szTemp);

				// Write SATA Command Count
				WriteCount++;  
				sprintf(szTemp, "%d", WriteCount);
				txtWriteCount->setText(szTemp);
			}
			else if(szCmdList[1] == "BW"){
				double time;
				sscanf(szCmdList[2], "%lf", &time);
				if(time != 0){
					sprintf(szTemp, "%0.3lf", time);
					txtWriteSpeed->setText(szTemp);
				}
			}
	  	}	    
		/* READ인 소켓을 받았을 경우 */
		else if(szCmdList[0] == "READ")
		{
			if(szCmdList[1] == "PAGE"){
				unsigned int length;
	  			
				/* Read Sector Number Count */
				sscanf(szCmdList[2], "%u", &length);
	  			ReadSecCount += length;

	  			sprintf(szTemp, "%u", ReadSecCount);
	  			txtReadSectorCount->setText(szTemp);

				/* Read SATA Command Count */
				ReadCount++;
				sprintf(szTemp, "%d", ReadCount);
				txtReadCount->setText(szTemp);
			}
			else if(szCmdList[1] == "BW"){
				double time;
	  			sscanf(szCmdList[2], "%lf", &time);
	  			if(time != 0)
	  			{
	  				sprintf(szTemp, "%0.3lf", time);
	  				txtReadSpeed->setText(szTemp);
				}
			}
		}
		/* READF인 소켓을 받았을 경우 */
		else if(szCmdList[0] == "READF")
		{
			int flash_nb;
			sscanf(szCmdList[1], "%d", &flash_nb);
  		}
		/* GC인 소켓을 받았을 경우 */
		else if(szCmdList[0] == "GC")
  		{
			GC_NB++;
			sprintf(szTemp, "%ld", GC_NB);
			txtGarbageCollectionNB->setText(szTemp);

		}
		/* WB인 소켓을 받았을 경우 */
		else if(szCmdList[0] == "WB")
	    	{
			long long int wb = 0;
			sscanf(szCmdList[2], "%lld", &wb);
			if(szCmdList[1] == "CORRECT")
			{
	      			WrittenCorrectCount += wb;
	      			sprintf(szTemp, "%ld", WrittenCorrectCount);
	      			txtWrittenBlockCount->setText(szTemp);
			}
			else
			{
	      			WriteAmpCount += wb;
	      			sprintf(szTemp, "%ld", WriteAmpCount);
	      			txtWriteAmpCount->setText(szTemp);
			}
		}	
		/* TRIM인 소켓을 받았을 경우 */
		else if(szCmdList[0] == "TRIM")
		{
			if(szCmdList[1] == "INSERT")
			{
				TrimCount++;
				sprintf(szTemp, "%d", TrimCount);
				txtTrimCount->setText(szTemp);	    
			}
			else
			{
				int effect = 0;
				sscanf(szCmdList[2], "%d", &effect);
				TrimEffect+= effect;
				sprintf(szTemp, "%d", TrimEffect);
				txtTrimEffect->setText(szTemp);	
			}
		}
		/* UTIL인 소켓을 받았을 경우 */
		else if(szCmdList[0] == "UTIL")
		{
			double util = 0;
			sscanf(szCmdList[1], "%lf", &util);
			sprintf(szTemp, "%lf", util);
			txtSsdUtil->setText(szTemp);
	  	}

		txtDebug->setText(szCmd);
	} // end while
}


void Form1::btnInit_clicked()
{
    init_var();

    txtWriteCount->setText("0");
    txtWriteSpeed->setText("0");
    txtWriteSectorCount->setText("0");
    txtReadCount->setText("0");
    txtReadSpeed->setText("0");
    txtReadSectorCount->setText("0");
    txtGarbageCollectionNB->setText("0");
    txtGarbageCollection->setText("0");
    txtGCStart->setText("0"); 
    txtTrimCount->setText("0");
    txtTrimEffect->setText("0");
    txtWrittenBlockCount->setText("0");
    txtWriteAmpCount->setText("0");
    txtDebug->setText("");
}


void Form1::btnSave_clicked()
{

    QString fileName;
    fileName = QFileDialog::getSaveFileName("./data" , "Data files (*.dat)", this, "Save file", "Choose a filename to save under");
    QFile file(fileName);
    file.open(IO_WriteOnly);
    
    QTextStream out(&file);
    
    char s_timer[20];
    sprintf(s_timer, "%lld", TimeProg);
    
    /*
    out << "==========================================================================\n";
    out << "|                                                                        |\n";
    out << "|           VSSIM : Virtual SSD Simulator                                |\n";
    out << "|                                                                        |\n";
    out << "|                                   Embedded Software Systems Lab.       |\n";
    out << "|                        Dept. of Electronics and Computer Engineering   |\n";
    out << "|                                         Hanynag Univ, Seoul, Korea     |\n";
    out << "|                                                                        |\n";
    out << "==========================================================================\n\n";
*/
    
    out << "Write Request\t" << WriteCount << "\n";
    out << "Write Sector\t" << WriteSectorCount << "\n\n";
    out << "Read Request\t" << ReadCount << "\n";
    out << "Read Sector\t" << ReadSecCount << "\n\n";

    out << "Garbage Collection\n";
    out << "  Count\t" << GC_NB <<"\n";
    out << "  Exchange\t" << ExchangeCount << "\n";
    out << "  Sector Count\t" << GCStarted << "\n\n";
    
    out << "TRIM Count\t" << TrimCount << "\n";
    out << "TRIM effect\t" << TrimEffect << "\n\n";
    
    out << "Write Amplification\t" << WriteAmpCount << "\n";
    out << "Written Page\t" << WrittenCorrectCount << "\n";
    out << "Run-time[ms]\t" << s_timer << "\n";

    file.close();
}

void Form1::onTimer()
{
    char sz_timer[20];
    TimeProg++;
    sprintf(sz_timer, "%lld", TimeProg);
    txtTimeProg->setText(sz_timer);
}

