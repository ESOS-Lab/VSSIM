// File: ftl.c
// Date: 2014. 12. 10.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

#ifndef VSSIM_BENCH
#include "qemu-kvm.h"
#endif

int g_init = 0;
int64_t bm_start_sect_nb;

void FTL_INIT(void)
{
	if(g_init == 0){
        	printf("[%s] start\n", __FUNCTION__);

		/* Get SSD specification from ssd.conf file */
		INIT_SSD_CONFIG();
		INIT_METADATA();
		INIT_PERF_CHECKER();
		
#ifdef FIRM_IO_BUFFER
		INIT_FIRM_IO_BUFFER();
#endif
#ifdef FIRM_IO_THREAD
		INIT_FIRM_IO_MANAGER();
#endif
#ifdef MONITOR_ON
		/* Initialize SSD Monitor */
		INIT_LOG_MANAGER();
#endif
		g_init = 1;

		/* Initialize SSD Module */
		SSD_IO_INIT();

		printf("[%s] complete\n", __FUNCTION__);
	}
}

void FTL_TERM(void)
{
	printf("[%s] start\n", __FUNCTION__);

#ifdef FIRM_IO_BUFFER
	TERM_FIRM_IO_BUFFER();
#endif
#ifdef FIRM_IO_THREAD
	TERM_FIRM_IO_MANAGER();
#endif
	TERM_METADATA();
	TERM_PERF_CHECKER();

#ifdef MONITOR_ON
	/* Terminate the SSD Monitor */
	TERM_LOG_MANAGER();
#endif

	printf("[%s] complete\n", __FUNCTION__);
}

void FTL_READ(int32_t sector_nb, unsigned int length)
{
	unsigned int length_pm; // the number of sectors for page mapping
	unsigned int length_bm; // the nubmer of sectors for block mapping

	if(sector_nb + length > SECTOR_NB){
		printf("Error[%s] Exceed Sector number\n",__FUNCTION__); 
		return;	
	}

	/* The sectors are cross the mapping domain */
	if(sector_nb < bm_start_sect_nb && sector_nb + length > bm_start_sect_nb){

		length_pm = bm_start_sect_nb - sector_nb;
		length_bm = length - length_pm;

		/* Read data from the both area */
		FTL_PM_READ(sector_nb, length_pm);
		FTL_BM_READ(0, length_bm);	
	}
	/* The read is requested for page mapping area */
	else if(sector_nb < bm_start_sect_nb){
		FTL_PM_READ(sector_nb, length);
	}
	/* the read is requested for block mapping area */
	else{
		FTL_BM_READ(sector_nb - bm_start_sect_nb, length);
	}

}

void FTL_WRITE(int32_t sector_nb, unsigned int length)
{
	unsigned int length_pm;
	unsigned int length_bm;

	if(sector_nb + length > SECTOR_NB){
		printf("Error[%s] Exceed Sector number\n",__FUNCTION__); 
		return;	
	}

#ifdef GC_ON
	/* Check if the garbage collection has to be operated */
	GC_CHECK();
#endif

	/* The sectors are cross the mapping domain */
	if(sector_nb < bm_start_sect_nb && sector_nb + length > bm_start_sect_nb){
	
		length_pm = bm_start_sect_nb - sector_nb;
		length_bm = length - length_pm;

		/* Write data from the both area */
		FTL_PM_WRITE(sector_nb, length_pm);
		FTL_BM_WRITE(0, length_bm);	
	}
	/* The write is requested for page mapping area */
	else if(sector_nb < bm_start_sect_nb){
		FTL_PM_WRITE(sector_nb, length);
	}
	/* The write is requested for block mapping area */
	else{
		int io_page_nb = 0;
		int32_t lba = sector_nb - bm_start_sect_nb;

		/* Calculate address, Get mapping address */
		int32_t lpn = lba / (int32_t)SECTORS_PER_PAGE;
		int32_t lbn = lpn / PAGE_NB;
		int32_t block_offset = lpn % (int32_t)PAGE_NB;	

		int length_1 = (lbn+1)*(PAGE_NB*SECTORS_PER_PAGE) - lba;
		int length_2 = length - length_1;

		io_page_nb = CALC_IO_PAGE_NB(lba, length);

		if( block_offset + io_page_nb > PAGE_NB){
			FTL_BM_WRITE(lba, length_1);
			FTL_BM_WRITE(lba+length_1, length_2);
		}
		else{
			FTL_BM_WRITE(lba, length);
		}
	}
}

unsigned int CALC_FLASH_FROM_PPN(int32_t ppn)
{
	unsigned int flash_nb = (ppn/PAGE_NB)/BLOCK_NB;

	if(flash_nb >= FLASH_NB){
		printf("ERROR[%s] flash_nb %u\n",__FUNCTION__, flash_nb);
	}
	return flash_nb;
}

unsigned int CALC_BLOCK_FROM_PPN(int32_t ppn)
{
	unsigned int block_nb = (ppn/PAGE_NB)%BLOCK_NB;

	if(block_nb >= BLOCK_NB){
		printf("ERROR[%s] block_nb %u\n",__FUNCTION__, block_nb);
	}
	return block_nb;
}

unsigned int CALC_PAGE_FROM_PPN(int32_t ppn)
{
	unsigned int page_nb = ppn%PAGE_NB;

	return page_nb;
}


unsigned int CALC_FLASH_FROM_PBN(int32_t pbn)
{
	unsigned int flash_nb = pbn/BLOCK_NB;

	if(flash_nb >= FLASH_NB){
		printf("ERROR[%s] flash_nb %u, input pbn: %d\n", __FUNCTION__, flash_nb, pbn);
	}
	return flash_nb;
}

unsigned int CALC_BLOCK_FROM_PBN(int32_t pbn)
{
	unsigned int block_nb = pbn%BLOCK_NB;

	if(block_nb >= BLOCK_NB){
		printf("ERROR[%s] block_nb %u\n", __FUNCTION__, block_nb);
	}
	return block_nb;
}

int CALC_IO_PAGE_NB(int32_t sector_nb, unsigned int length)
{
	int io_page_nb = 0;

	int32_t lba = sector_nb;
	unsigned int remain = length;
	unsigned int left_skip = lba % SECTORS_PER_PAGE;
	unsigned int right_skip;
	unsigned int sects;

	while(remain > 0){
		if(remain > SECTORS_PER_PAGE - left_skip){
			right_skip = 0;
		}
		else{
			right_skip = SECTORS_PER_PAGE - left_skip - remain;
		}
		sects = SECTORS_PER_PAGE - left_skip - right_skip;

		remain -= sects;
		left_skip = 0;
		io_page_nb++;
	}

	return io_page_nb;
}
