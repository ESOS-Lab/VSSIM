// File: vssim_config_manager.c
// Date: 2014. 12. 03.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

/* SSD Configuration */
int N_CORES;
int BACKGROUND_GC_ENABLE;
int N_IO_CORES;

int SECTOR_SIZE;
int64_t N_SECTORS;
int LOG_SECTOR_SIZE;
int PAGE_SIZE;
int N_FLASH;

int N_CHANNELS;
int N_WAYS;
int N_PLANES_PER_FLASH;
int N_BLOCKS_PER_PLANE;
int N_BLOCKS_PER_FLASH;
int N_PAGES_PER_BLOCK;
int N_PAGES_PER_PLANE;
int N_PAGES_PER_FLASH;

int SECTORS_PER_PAGE;

int OVP;

/* Valid bitmap */
int N_4K_PAGES;
int BITMAP_SIZE;
int SECTORS_PER_4K_PAGE;

/* Mapping Table */
int64_t N_TOTAL_PAGES;
int64_t N_TOTAL_BLOCKS;

/* NAND Flash Delay */
int REG_WRITE_DELAY;
int PAGE_PROGRAM_DELAY;
int REG_READ_DELAY;
int PAGE_READ_DELAY;
int BLOCK_ERASE_DELAY;
int REG_CMD_SET_DELAY;

int DSM_TRIM_ENABLE;
int PAGE_CACHE_REG_ENABLE;

/* Garbage Collection */
enum vssim_gc_mode gc_mode;

double GC_LOW_WATERMARK_RATIO;
double GC_HIGH_WATERMARK_RATIO;

/* Write Buffer */
uint32_t WRITE_BUFFER_SIZE_KB;		// 8192 for 4MB with 512B Sector size
uint32_t READ_BUFFER_SIZE_KB;
uint32_t N_WB_SECTORS;
uint32_t N_RB_SECTORS;
uint32_t N_DISCARD_BUF_SECTORS;
uint32_t N_NVME_CMD_MAX_SECTORS;

char FILE_NAME_HDA[1024] = {0,};
char FILE_NAME_HDB[1024] = {0,};
char GC_MODE[1024] = {0,};

void INIT_SSD_CONFIG(void)
{
	FILE* pfData;
	pfData = fopen("QEMU/ssd.conf", "r");
	if(pfData == NULL){
		printf("ERROR[%s] ssd.conf open fail!\n", __FUNCTION__);
		return;
	}
		
	int ret;
	char* szCommand = NULL;
		
	szCommand = (char*)malloc(1024);
	memset(szCommand, 0x00, 1024);
	if(pfData!=NULL)
	{
		while(fscanf(pfData, "%s", szCommand)!=EOF)
		{
			if(strcmp(szCommand, "FILE_NAME_HDA") == 0)
			{
				ret = fscanf(pfData, "%s", FILE_NAME_HDA);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			else if(strcmp(szCommand, "FILE_NAME_HDB") == 0)
			{
				ret = fscanf(pfData, "%s", FILE_NAME_HDB);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			else if(strcmp(szCommand, "N_CORES") == 0)
			{
				ret = fscanf(pfData, "%d", &N_CORES);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			else if(strcmp(szCommand, "BACKGROUND_GC_ENABLE") == 0)
			{
				ret = fscanf(pfData, "%d", &BACKGROUND_GC_ENABLE);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			else if(strcmp(szCommand, "PAGE_SIZE") == 0)
			{
				ret = fscanf(pfData, "%d", &PAGE_SIZE);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			else if(strcmp(szCommand, "N_CHANNELS") == 0)
			{
				ret = fscanf(pfData, "%d", &N_CHANNELS);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			else if(strcmp(szCommand, "N_WAYS") == 0)
			{
				ret = fscanf(pfData, "%d", &N_WAYS);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			else if(strcmp(szCommand, "N_PAGES_PER_BLOCK") == 0)
			{
				ret = fscanf(pfData, "%d", &N_PAGES_PER_BLOCK);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			else if(strcmp(szCommand, "N_BLOCKS_PER_PLANE") == 0)
			{
				ret = fscanf(pfData, "%d", &N_BLOCKS_PER_PLANE);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}		
			else if(strcmp(szCommand, "N_PLANES_PER_FLASH") == 0)
			{
				ret = fscanf(pfData, "%d", &N_PLANES_PER_FLASH);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			else if(strcmp(szCommand, "REG_CMD_SET_DELAY") == 0)
			{
				ret = fscanf(pfData, "%d", &REG_CMD_SET_DELAY);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			else if(strcmp(szCommand, "REG_WRITE_DELAY") == 0)
			{
				ret = fscanf(pfData, "%d", &REG_WRITE_DELAY);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}	
			else if(strcmp(szCommand, "PAGE_PROGRAM_DELAY") == 0)
			{
				ret = fscanf(pfData, "%d", &PAGE_PROGRAM_DELAY);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			else if(strcmp(szCommand, "REG_READ_DELAY") == 0)
			{
				ret = fscanf(pfData, "%d", &REG_READ_DELAY);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			else if(strcmp(szCommand, "PAGE_READ_DELAY") == 0)
			{
				ret = fscanf(pfData, "%d", &PAGE_READ_DELAY);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			else if(strcmp(szCommand, "BLOCK_ERASE_DELAY") == 0)
			{
				ret = fscanf(pfData, "%d", &BLOCK_ERASE_DELAY);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			else if(strcmp(szCommand, "DSM_TRIM_ENABLE") == 0)
			{
				ret = fscanf(pfData, "%d", &DSM_TRIM_ENABLE);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			else if(strcmp(szCommand, "PAGE_CACHE_REG_ENABLE") == 0)
			{
				ret = fscanf(pfData, "%d", &PAGE_CACHE_REG_ENABLE);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}	
			else if(strcmp(szCommand, "OVP") == 0)
			{
				ret = fscanf(pfData, "%d", &OVP);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			else if(strcmp(szCommand, "WRITE_BUFFER_SIZE_KB") == 0)
			{
				ret = fscanf(pfData, "%u", &WRITE_BUFFER_SIZE_KB);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			else if(strcmp(szCommand, "READ_BUFFER_SIZE_KB") == 0)
			{
				ret = fscanf(pfData, "%u", &READ_BUFFER_SIZE_KB);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			else if(strcmp(szCommand, "GC_MODE") == 0)
			{
				ret = fscanf(pfData, "%s", GC_MODE);
				if(ret == -1){
					printf("ERROR[%s] Read %s fail\n", __FUNCTION__, szCommand);
				}
			}
			memset(szCommand, 0x00, 1024);
		}	
		fclose(pfData);
	}

	/* Exception Handling */
	if(N_CORES == 1 && BACKGROUND_GC_ENABLE == 1){
		printf("ERROR[%s] Single core cannot support the \
				background GC core. The background gc option \
				is ignored. \n", __FUNCTION__); 
		BACKGROUND_GC_ENABLE = 0;
	}
	if(PAGE_SIZE < 4096 || PAGE_SIZE > 131072){
		printf("ERROR[%s] Flash page size should be 4Kbyte - 128Kbyte. \n", __FUNCTION__);
		return;
	}


	/* SSD core configuration:
	 * In multi core simulation, there are three type of cores;
	 *  1. Host interface core
	 *  2. Background GC core
	 *  3. IO core
	 */
	
	N_IO_CORES = N_CORES - BACKGROUND_GC_ENABLE - 1; 
	if(N_IO_CORES < 0 ){
		printf("ERROR[%s] N_IO_CORES: %d\n", __FUNCTION__,
				N_IO_CORES);
		return;
	}
	else{
		printf("[%s] %d IO CORES Enabled\n", __FUNCTION__,
				N_IO_CORES);
	}

	/* SSD Configuration */
	SECTOR_SIZE = 512;
	LOG_SECTOR_SIZE = 9;
	SECTORS_PER_PAGE = PAGE_SIZE / SECTOR_SIZE;
	N_FLASH = N_CHANNELS * N_WAYS;
	N_BLOCKS_PER_FLASH = N_BLOCKS_PER_PLANE * N_PLANES_PER_FLASH;
	N_PAGES_PER_PLANE = N_PAGES_PER_BLOCK * N_BLOCKS_PER_PLANE;
	N_PAGES_PER_FLASH = N_PAGES_PER_PLANE * N_PLANES_PER_FLASH;
	N_TOTAL_BLOCKS = (int64_t)N_BLOCKS_PER_FLASH * N_FLASH;
	N_TOTAL_PAGES = (int64_t)N_PAGES_PER_FLASH * N_FLASH;
	N_SECTORS = (int64_t)SECTORS_PER_PAGE * N_TOTAL_PAGES;

	/* Valid bitmap */
	N_4K_PAGES = PAGE_SIZE / 4096;
	BITMAP_SIZE = N_PAGES_PER_BLOCK * N_4K_PAGES;
	SECTORS_PER_4K_PAGE = 4096 / SECTOR_SIZE;

	/* Double Write Buffer: WRITE_BUFFER_FRAME_NB is the number of
		512 byte sectors for single write buffer */
	N_NVME_CMD_MAX_SECTORS = 2048;

	if(WRITE_BUFFER_SIZE_KB == 0){
		N_WB_SECTORS = N_CHANNELS * N_WAYS * N_PLANES_PER_FLASH
					* SECTORS_PER_PAGE;
	}
	else{
		N_WB_SECTORS = (WRITE_BUFFER_SIZE_KB * 1024) / SECTOR_SIZE;
	}
	if(N_WB_SECTORS < N_NVME_CMD_MAX_SECTORS)
		N_WB_SECTORS = N_NVME_CMD_MAX_SECTORS;


	if(READ_BUFFER_SIZE_KB == 0){
		N_RB_SECTORS = N_CHANNELS * N_WAYS * N_PLANES_PER_FLASH
					* SECTORS_PER_PAGE;
	}
	else{
		N_RB_SECTORS = (READ_BUFFER_SIZE_KB * 1024) / SECTOR_SIZE;
	}
	if(N_RB_SECTORS < N_NVME_CMD_MAX_SECTORS)
		N_RB_SECTORS = N_NVME_CMD_MAX_SECTORS;

	N_DISCARD_BUF_SECTORS = N_CHANNELS * N_WAYS * N_PLANES_PER_FLASH
                                        * SECTORS_PER_PAGE;

	/* Set Garbage Collection related Global variables */

	/* Garbage Collection Threshold */
	GC_LOW_WATERMARK_RATIO = 0.70; // Ex) 0.7 for 70%, 0.9 for 90%
	if(OVP == 0){
		GC_HIGH_WATERMARK_RATIO = 0.95;
	}
	else{
		GC_HIGH_WATERMARK_RATIO = (100 - OVP) * 0.01;
	}

	/* Set Garbage collection mode */
	if(strcmp(GC_MODE, "CORE_GC") == 0){
		gc_mode = CORE_GC;
	}
	else if(strcmp(GC_MODE, "FLASH_GC") == 0){
		gc_mode = FLASH_GC;
	}
	else if(strcmp(GC_MODE, "PLANE_GC") == 0){
		gc_mode = PLANE_GC;
	}
	else{
		printf("ERROR[%s] Wrong GC mode, FLASH_GC mode is enabled.\n",
				__FUNCTION__);
		gc_mode = FLASH_GC;
	}

	free(szCommand);
}

char* GET_FILE_NAME_HDA(void)
{
	return FILE_NAME_HDA;
}

char* GET_FILE_NAME_HDB(void)
{
	return FILE_NAME_HDB;
}
