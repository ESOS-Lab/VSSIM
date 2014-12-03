// File: vssim_config_manager.c
// Data: 2014. 12. 03.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

/* SSD Configuration */
int SECTOR_SIZE;
int PAGE_SIZE;

int64_t SECTOR_NB;
int PAGE_NB;
int FLASH_NB;
int BLOCK_NB;
int CHANNEL_NB;
int PLANES_PER_FLASH;

int SECTORS_PER_PAGE;
int PAGES_PER_FLASH;
int64_t PAGES_IN_SSD;

int WAY_NB;
int OVP;

/* Mapping Table */
int DATA_BLOCK_NB;
int64_t BLOCK_MAPPING_ENTRY_NB;		

#ifdef PAGE_MAP
int64_t PAGE_MAPPING_ENTRY_NB;
#endif

#if defined PAGE_MAP || defined BLOCK_MAP
int64_t EACH_EMPTY_TABLE_ENTRY_NB;
int EMPTY_TABLE_ENTRY_NB;
int VICTIM_TABLE_ENTRY_NB;
#endif

#if defined FAST_FTL || defined LAST_FTL
int LOG_RAND_BLOCK_NB;
int LOG_SEQ_BLOCK_NB;

int64_t DATA_MAPPING_ENTRY_NB;
int64_t RAN_MAPPING_ENTRY_NB;
int64_t SEQ_MAPPING_ENTRY_NB;
int64_t RAN_LOG_MAPPING_ENTRY_NB;
int64_t EACH_EMPTY_BLOCK_ENTRY_NB;

int EMPTY_BLOCK_TABLE_NB;
#endif
 
#ifdef DA_MAP
int64_t DA_PAGE_MAPPING_ENTRY_NB;
int64_t DA_BLOCK_MAPPING_ENTRY_NB;
int64_t EACH_EMPTY_TABLE_ENTRY_NB;
int EMPTY_TABLE_ENTRY_NB;
int VICTIM_TABLE_ENTRY_NB;
#endif

/* NAND Flash Delay */
int REG_WRITE_DELAY;
int CELL_PROGRAM_DELAY;
int REG_READ_DELAY;
int CELL_READ_DELAY;
int BLOCK_ERASE_DELAY;
int CHANNEL_SWITCH_DELAY_W;
int CHANNEL_SWITCH_DELAY_R;

int DSM_TRIM_ENABLE;
int IO_PARALLELISM;

/* Garbage Collection */
#if defined PAGE_MAP || defined BLOCK_MAP || defined DA_MAP
double GC_THRESHOLD;			
double GC_THRESHOLD_HARD;	
int GC_THRESHOLD_BLOCK_NB;
int GC_THRESHOLD_BLOCK_NB_HARD;	
int GC_THRESHOLD_BLOCK_NB_EACH;	
int GC_VICTIM_NB;
#endif

/* Write Buffer */
uint32_t WRITE_BUFFER_FRAME_NB;		// 8192 for 4MB with 512B Sector size
uint32_t READ_BUFFER_FRAME_NB;

/* Map Cache */
#if defined FTL_MAP_CACHE || defined Polymorphic_FTL
int CACHE_IDX_SIZE;
#endif

#ifdef FTL_MAP_CACHE
uint32_t MAP_ENTRY_SIZE;
uint32_t MAP_ENTRIES_PER_PAGE;
uint32_t MAP_ENTRY_NB;
#endif

/* HOST Event Queue */
#ifdef HOST_QUEUE
uint32_t HOST_QUEUE_ENTRY_NB;
#endif

/* Rand Log Block Write policy */
#if defined FAST_FTL || defined LAST_FTL
int PARAL_DEGREE;                       // added by js
int PARAL_COUNT;                        // added by js
#endif

/* LAST FTL */
#ifdef LAST_FTL
int HOT_PAGE_NB_THRESHOLD;              // added by js
int SEQ_THRESHOLD;                      // added by js
#endif

/* Polymorphic FTL */
#ifdef Polymorphic_FTL
int PHY_SPARE_SIZE;

int64_t NR_PHY_BLOCKS;
int64_t NR_PHY_PAGES;
int64_t NR_PHY_SECTORS;

int NR_RESERVED_PHY_SUPER_BLOCKS;
int NR_RESERVED_PHY_BLOCKS;
int NR_RESERVED_PHY_PAGES;
#endif

char FILE_NAME_HDA[1024] = {0,};
char FILE_NAME_HDB[1024] = {0,};

void INIT_SSD_CONFIG(void)
{
	FILE* pfData;
	pfData = fopen("./data/ssd.conf", "r");
	
	char* szCommand = NULL;
	
	szCommand = (char*)malloc(1024);
	memset(szCommand, 0x00, 1024);
	if(pfData!=NULL)
	{
		while(fscanf(pfData, "%s", szCommand)!=EOF)
		{
			if(strcmp(szCommand, "FILE_NAME_HDA") == 0)
			{
				fscanf(pfData, "%s", FILE_NAME_HDA);
			}
			else if(strcmp(szCommand, "FILE_NAME_HDB") == 0)
			{
				fscanf(pfData, "%s", FILE_NAME_HDB);
			}
			else if(strcmp(szCommand, "PAGE_SIZE") == 0)
			{
				fscanf(pfData, "%d", &PAGE_SIZE);
			}
			else if(strcmp(szCommand, "PAGE_NB") == 0)
			{
				fscanf(pfData, "%d", &PAGE_NB);
			}
			else if(strcmp(szCommand, "SECTOR_SIZE") == 0)
			{
				fscanf(pfData, "%d", &SECTOR_SIZE);
			}	
			else if(strcmp(szCommand, "FLASH_NB") == 0)
			{
				fscanf(pfData, "%d", &FLASH_NB);
			}	
			else if(strcmp(szCommand, "BLOCK_NB") == 0)
			{
				fscanf(pfData, "%d", &BLOCK_NB);
			}					
			else if(strcmp(szCommand, "PLANES_PER_FLASH") == 0)
			{
				fscanf(pfData, "%d", &PLANES_PER_FLASH);
			}
			else if(strcmp(szCommand, "REG_WRITE_DELAY") == 0)
			{
				fscanf(pfData, "%d", &REG_WRITE_DELAY);
			}	
			else if(strcmp(szCommand, "CELL_PROGRAM_DELAY") == 0)
			{
				fscanf(pfData, "%d", &CELL_PROGRAM_DELAY);
			}
			else if(strcmp(szCommand, "REG_READ_DELAY") == 0)
			{
				fscanf(pfData, "%d", &REG_READ_DELAY);
			}
			else if(strcmp(szCommand, "CELL_READ_DELAY") == 0)
			{
				fscanf(pfData, "%d", &CELL_READ_DELAY);
			}
			else if(strcmp(szCommand, "BLOCK_ERASE_DELAY") == 0)
			{
				fscanf(pfData, "%d", &BLOCK_ERASE_DELAY);
			}
			else if(strcmp(szCommand, "CHANNEL_SWITCH_DELAY_R") == 0)
			{
				fscanf(pfData, "%d", &CHANNEL_SWITCH_DELAY_R);
			}
			else if(strcmp(szCommand, "CHANNEL_SWITCH_DELAY_W") == 0)
			{
				fscanf(pfData, "%d", &CHANNEL_SWITCH_DELAY_W);
			}
			else if(strcmp(szCommand, "DSM_TRIM_ENABLE") == 0)
			{
				fscanf(pfData, "%d", &DSM_TRIM_ENABLE);
			}
			else if(strcmp(szCommand, "IO_PARALLELISM") == 0)
			{
				fscanf(pfData, "%d", &IO_PARALLELISM);
			}
			else if(strcmp(szCommand, "CHANNEL_NB") == 0)
			{
				fscanf(pfData, "%d", &CHANNEL_NB);
			}
			else if(strcmp(szCommand, "OVP") == 0)
			{
				fscanf(pfData, "%d", &OVP);
			}
#if defined FTL_MAP_CACHE || defined Polymorphic_FTL
			else if(strcmp(szCommand, "CACHE_IDX_SIZE") == 0)
			{
				fscanf(pfData, "%d", &CACHE_IDX_SIZE);
			}
#endif
#ifdef FIRM_IO_BUFFER
			else if(strcmp(szCommand, "WRITE_BUFFER_FRAME_NB") == 0)
			{
				fscanf(pfData, "%u", &WRITE_BUFFER_FRAME_NB);
			}
			else if(strcmp(szCommand, "READ_BUFFER_FRAME_NB") == 0)
			{
				fscanf(pfData, "%u", &READ_BUFFER_FRAME_NB);
			}
#endif
#ifdef HOST_QUEUE
			else if(strcmp(szCommand, "HOST_QUEUE_ENTRY_NB") == 0)
			{
				fscanf(pfData, "%u", &HOST_QUEUE_ENTRY_NB);
			}
#endif
#if defined FAST_FTL || defined LAST_FTL
			else if(strcmp(szCommand, "LOG_RAND_BLOCK_NB") == 0)
			{
				fscanf(pfData, "%d", &LOG_RAND_BLOCK_NB);
			}	
			else if(strcmp(szCommand, "LOG_SEQ_BLOCK_NB") == 0)
			{
				fscanf(pfData, "%d", &LOG_SEQ_BLOCK_NB);
			}	
#endif
			memset(szCommand, 0x00, 1024);
		}	
		fclose(pfData);

	}

	/* Exception Handler */
	if(FLASH_NB < CHANNEL_NB){
		printf("ERROR[%s] Wrong CHANNEL_NB %d\n",__FUNCTION__, CHANNEL_NB);
		return;
	}
	if(PLANES_PER_FLASH != 1 && PLANES_PER_FLASH % 2 != 0){
		printf("ERROR[%s] Wrong PLANAES_PER_FLASH %d\n", __FUNCTION__, PLANES_PER_FLASH);
		return;
	}
#ifdef FIRM_IO_BUFFER
	if(WRITE_BUFFER_FRAME_NB == 0 || READ_BUFFER_FRAME_NB == 0){
		printf("ERROR[%s] Wrong parameter for SSD_IO_BUFFER",__FUNCTION__);
		return;
	}
#endif

	/* SSD Configuration */
	SECTORS_PER_PAGE = PAGE_SIZE / SECTOR_SIZE;
	PAGES_PER_FLASH = PAGE_NB * BLOCK_NB;
	SECTOR_NB = (int64_t)SECTORS_PER_PAGE * (int64_t)PAGE_NB * (int64_t)BLOCK_NB * (int64_t)FLASH_NB;
#ifndef Polymorphic_FTL
	WAY_NB = FLASH_NB / CHANNEL_NB;
#endif

	/* Mapping Table */
	BLOCK_MAPPING_ENTRY_NB = (int64_t)BLOCK_NB * (int64_t)FLASH_NB;
	PAGES_IN_SSD = (int64_t)PAGE_NB * (int64_t)BLOCK_NB * (int64_t)FLASH_NB;

#ifdef PAGE_MAP
	PAGE_MAPPING_ENTRY_NB = (int64_t)PAGE_NB * (int64_t)BLOCK_NB * (int64_t)FLASH_NB;
#endif

#if defined PAGE_MAP || defined BLOCK_MAP
	EACH_EMPTY_TABLE_ENTRY_NB = (int64_t)BLOCK_NB / (int64_t)PLANES_PER_FLASH;
	EMPTY_TABLE_ENTRY_NB = FLASH_NB * PLANES_PER_FLASH;
	VICTIM_TABLE_ENTRY_NB = FLASH_NB * PLANES_PER_FLASH;

	DATA_BLOCK_NB = BLOCK_NB;
#endif

#if defined FAST_FTL || defined LAST_FTL
	DATA_BLOCK_NB = BLOCK_NB - LOG_RAND_BLOCK_NB - LOG_SEQ_BLOCK_NB;
	DATA_MAPPING_ENTRY_NB = (int64_t)FLASH_NB * (int64_t)DATA_BLOCK_NB;

	EACH_EMPTY_BLOCK_ENTRY_NB = (int64_t)BLOCK_NB / (int64_t)PLANES_PER_FLASH;
	EMPTY_BLOCK_TABLE_NB = FLASH_NB * PLANES_PER_FLASH;
#endif

#ifdef DA_MAP
	if(BM_START_SECTOR_NB < 0 || BM_START_SECTOR_NB >= SECTOR_NB){
		printf("ERROR[%s] BM_START_SECTOR_NB %d \n", __FUNCTION__, BM_START_SECTOR_NB);
	}

	DA_PAGE_MAPPING_ENTRY_NB = CALC_DA_PM_ENTRY_NB(); 
	DA_BLOCK_MAPPING_ENTRY_NB = CALC_DA_BM_ENTRY_NB();
	EACH_EMPTY_TABLE_ENTRY_NB = (int64_t)BLOCK_NB / (int64_t)PLANES_PER_FLASH;
	EMPTY_TABLE_ENTRY_NB = FLASH_NB * PLANES_PER_FLASH;
	VICTIM_TABLE_ENTRY_NB = FLASH_NB * PLANES_PER_FLASH;
#endif

	/* FAST Performance Test */
#ifdef FAST_FTL
	SEQ_MAPPING_ENTRY_NB = (int64_t)FLASH_NB * (int64_t)LOG_SEQ_BLOCK_NB;
	RAN_MAPPING_ENTRY_NB = (int64_t)FLASH_NB * (int64_t)LOG_RAND_BLOCK_NB;
	RAN_LOG_MAPPING_ENTRY_NB = (int64_t)FLASH_NB * (int64_t)LOG_RAND_BLOCK_NB * (int64_t)PAGE_NB;

	// PARAL_DEGREE = 4;
	PARAL_DEGREE = RAN_MAPPING_ENTRY_NB;
	if(PARAL_DEGREE > RAN_MAPPING_ENTRY_NB){
		printf("[INIT_SSD_CONFIG] ERROR PARAL_DEGREE \n");
		return;
	}
	PARAL_COUNT = PARAL_DEGREE * PAGE_NB;
#endif

#ifdef LAST_FTL
	SEQ_MAPPING_ENTRY_NB = (int64_t)FLASH_NB * (int64_t)LOG_SEQ_BLOCK_NB;
	RAN_MAPPING_ENTRY_NB = (int64_t)FLASH_NB * ((int64_t)LOG_RAND_BLOCK_NB/2);
	RAN_LOG_MAPPING_ENTRY_NB = (int64_t)FLASH_NB * ((int64_t)LOG_RAND_BLOCK_NB/2) * (int64_t)PAGE_NB;

	SEQ_THRESHOLD = SECTORS_PER_PAGE * 4;
	HOT_PAGE_NB_THRESHOLD = PAGE_NB;

	PARAL_DEGREE = RAN_MAPPING_ENTRY_NB;
	if(PARAL_DEGREE > RAN_MAPPING_ENTRY_NB){
		printf("[INIT_SSD_CONFIG] ERROR PARAL_DEGREE \n");
		return;
	}
	PARAL_COUNT = PARAL_DEGREE * PAGE_NB;
#endif

	/* Garbage Collection */
#if defined PAGE_MAP || defined BLOCK_MAP || defined DA_MAP
	GC_THRESHOLD = 0.95; // 0.7 for 70%, 0.9 for 90%
	GC_THRESHOLD_HARD = 0.98;
	GC_THRESHOLD_BLOCK_NB = (int)((1-GC_THRESHOLD) * (double)BLOCK_MAPPING_ENTRY_NB);
	GC_THRESHOLD_BLOCK_NB_HARD = (int)((1-GC_THRESHOLD_HARD) * (double)BLOCK_MAPPING_ENTRY_NB);
	GC_THRESHOLD_BLOCK_NB_EACH = (int)((1-GC_THRESHOLD) * (double)EACH_EMPTY_TABLE_ENTRY_NB);
	if(OVP != 0){
		GC_VICTIM_NB = FLASH_NB * BLOCK_NB * OVP / 100 / 2;
	}
	else{
		GC_VICTIM_NB = 20;
	}
#endif

	/* Map Cache */
#ifdef FTL_MAP_CACHE
	MAP_ENTRY_SIZE = sizeof(int32_t);
	MAP_ENTRIES_PER_PAGE = PAGE_SIZE / MAP_ENTRY_SIZE;
	MAP_ENTRY_NB = PAGE_MAPPING_ENTRY_NB / MAP_ENTRIES_PER_PAGE;	
#endif

	/* Polymorphic FTL */
#ifdef Polymorphic_FTL
	WAY_NB = 1;

	PHY_SPARE_SIZE = 436;
	NR_PHY_BLOCKS = (int64_t)FLASH_NB * (int64_t)WAY_NB * (int64_t)BLOCK_NB;
	NR_PHY_PAGES = NR_PHY_BLOCKS * (int64_t)PAGE_NB;
	NR_PHY_SECTORS = NR_PHY_PAGES * (int64_t)SECTORS_PER_PAGE;

	NR_RESERVED_PHY_SUPER_BLOCKS = 4;
	NR_RESERVED_PHY_BLOCKS = FLASH_NB * WAY_NB * NR_RESERVED_PHY_SUPER_BLOCKS;
	NR_RESERVED_PHY_PAGES = NR_RESERVED_PHY_BLOCKS * PAGE_NB;
#endif
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

#ifdef DA_MAP
int64_t CALC_DA_PM_ENTRY_NB(void)
{
	int64_t ret_page_nb = (int64_t)BM_START_SECTOR_NB / SECTORS_PER_PAGE;
	if((BM_START_SECTOR_NB % SECTORS_PER_PAGE) != 0)
		ret_page_nb += 1;

	int64_t block_nb = ret_page_nb / PAGE_NB;
	if((ret_page_nb % PAGE_NB) != 0)
		block_nb += 1;

	ret_page_nb = block_nb * PAGE_NB;
	bm_start_sect_nb = ret_page_nb * SECTORS_PER_PAGE;

	return ret_page_nb;
}

int64_t CALC_DA_BM_ENTRY_NB(void)
{
	int64_t total_page_nb = (int64_t)PAGE_NB * BLOCK_NB * FLASH_NB;
	int64_t ret_block_nb = ((int64_t)total_page_nb - DA_PAGE_MAPPING_ENTRY_NB)/(int64_t)PAGE_NB;

	int64_t temp_total_page_nb = ret_block_nb * PAGE_NB + DA_PAGE_MAPPING_ENTRY_NB;
	if(temp_total_page_nb != total_page_nb){
		printf("ERROR[%s] %ld != %ld\n", __FUNCTION__, total_page_nb, temp_total_page_nb);
	}

	return ret_block_nb;
}
#endif
