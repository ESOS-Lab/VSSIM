// File: ftl.c
// Date: 2018. 02. 08.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2018
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

#ifndef VSSIM_BENCH
#include "qemu-kvm.h"
#endif

#ifdef DEBUG_MODE9
FILE* fp_dbg9_gc;
#endif

int g_init = 0;
extern double ssd_util;

void FTL_INIT(void)
{
	if(g_init == 0){
        	printf("[%s] start\n", __FUNCTION__);

		INIT_SSD_CONFIG();

		INIT_MAPPING_TABLE();
		INIT_INVERSE_MAPPING_TABLE();

		INIT_BLOCK_STATE_TABLE();
		INIT_VALID_ARRAY();
		INIT_EMPTY_BLOCK_LIST();
		INIT_PERF_CHECKER();

#ifdef FTL_MAP_CACHE
		INIT_CACHE();
#endif
#ifdef FIRM_IO_BUFFER
		INIT_IO_BUFFER();
#endif
#ifdef MONITOR_ON
		INIT_LOG_MANAGER();
#endif
		g_init = 1;

#ifdef VSSIM_BENCH_MULTI_THREAD
		SSD_MT_IO_INIT();
#else
		SSD_IO_INIT();
#endif
		printf("[%s] complete\n", __FUNCTION__);
	}
}

void FTL_TERM(void)
{
	printf("[%s] start\n", __FUNCTION__);

#ifdef FIRM_IO_BUFFER
	TERM_IO_BUFFER();
#endif
	TERM_MAPPING_TABLE();
	TERM_INVERSE_MAPPING_TABLE();
	TERM_VALID_ARRAY();
	TERM_BLOCK_STATE_TABLE();
	TERM_EMPTY_BLOCK_LIST();
	TERM_PERF_CHECKER();
#ifdef MONITOR_ON
	INIT_LOG_MANAGER();
#endif

	printf("[%s] complete\n", __FUNCTION__);
}

void FTL_READ(int32_t sector_nb, unsigned int length)
{
	_FTL_READ(sector_nb, length);
}

void FTL_WRITE(int32_t sector_nb, unsigned int length)
{
	_FTL_WRITE(sector_nb, length);
}

int _FTL_READ(int32_t sector_nb, unsigned int length)
{
#ifdef FTL_DEBUG
	printf("[%s] Start\n", __FUNCTION__);
#endif

	if(sector_nb + length > SECTOR_NB){
		printf("ERROR[%s] Exceed Sector number\n", __FUNCTION__); 
		return FAIL;	
	}

	int32_t lpn;	// Logical Page Number
	int32_t lbn;	// Logical Block Number
	int32_t pbn;	// Physical Block Number
	int32_t lba = sector_nb;
	int32_t block_offset;

	unsigned int remain = length;
	unsigned int left_skip = sector_nb % SECTORS_PER_PAGE;
	unsigned int right_skip;
	unsigned int read_sects;

	unsigned int ret = FAIL;
	int read_page_nb = 0;
	int io_page_nb;

	char page_state; // Valid, Invalid, Empty

#ifdef FIRM_IO_BUFFER
	INCREASE_RB_FTL_POINTER(length);
#endif

	while(remain > 0){

		if(remain > SECTORS_PER_PAGE - left_skip){
			right_skip = 0;
		}
		else{
			right_skip = SECTORS_PER_PAGE - left_skip - remain;
		}
		read_sects = SECTORS_PER_PAGE - left_skip - right_skip;

		/* Calculate address, Get mapping address */
		lpn = lba / (int32_t)SECTORS_PER_PAGE;
		lbn = lpn / PAGE_NB;
		block_offset = lpn % (int32_t)PAGE_NB;
		pbn = GET_VALID_MAPPING(lbn, block_offset);

		if(pbn == -1){
#ifdef FIRM_IO_BUFFER
			INCREASE_RB_LIMIT_POINTER();
#endif
			return FAIL;
		}

		lba += read_sects;
		remain -= read_sects;
		left_skip = 0;
	}

	io_alloc_overhead = ALLOC_IO_REQUEST(sector_nb, length, READ, &io_page_nb);

	remain = length;
	lba = sector_nb;
	left_skip = sector_nb % SECTORS_PER_PAGE;

	while(remain > 0){

		if(remain > SECTORS_PER_PAGE - left_skip){
			right_skip = 0;
		}
		else{
			right_skip = SECTORS_PER_PAGE - left_skip - remain;
		}
		read_sects = SECTORS_PER_PAGE - left_skip - right_skip;

		/* Calculate address, Get mapping address */
		lpn = lba / (int32_t)SECTORS_PER_PAGE;
		lbn = lpn / PAGE_NB;
		block_offset = lpn % (int32_t)PAGE_NB;
		pbn = GET_VALID_MAPPING(lbn, block_offset);

		if(pbn == -1){
			printf("ERROR[%s] No Mapping info\n", __FUNCTION__);
		}

		ret = SSD_PAGE_READ(CALC_FLASH(pbn), CALC_BLOCK(pbn), block_offset, read_page_nb, READ, io_page_nb);

#ifdef FTL_DEBUG
		if(ret == SUCCESS){
			printf("\t read complete [%u]-[%d][%d]\n",lpn, pbn, block_offset);
		}
		else if(ret == FAIL){
			printf("ERROR[%s] [%u] page read fail \n",__FUNCTION__, lpn);
		}
#endif
		read_page_nb++;

		lba += read_sects;
		remain -= read_sects;
		left_skip = 0;
	}

	INCREASE_IO_REQUEST_SEQ_NB();

#ifdef FIRM_IO_BUFFER
	INCREASE_RB_LIMIT_POINTER();
#endif

#ifdef MONITOR_ON
	char szTemp[1024];
	sprintf(szTemp, "READ PAGE %d ", length);
	WRITE_LOG(szTemp);
#endif

#ifdef FTL_DEBUG
	printf("[%s] Complete\n", __FUNCTION__);
#endif

	return ret;
}

int _FTL_WRITE(int32_t sector_nb, unsigned int length)
{
#ifdef FTL_DEBUG
	printf("[%s] Start\n",__FUNCTION__);
#endif
#ifdef GC_ON
	GC_CHECK();
#endif

	int io_page_nb;

	if(sector_nb + length > SECTOR_NB){
		printf("ERROR[%s] Exceed Sector number\n", __FUNCTION__);
                return FAIL;
        }
	else{
		io_alloc_overhead = ALLOC_IO_REQUEST(sector_nb, length, WRITE, &io_page_nb);
	}

	int32_t lba = sector_nb;
	int32_t lpn;	// Logical Page Number
	int32_t lbn;	// Logical Block Number
	int32_t new_pbn;	// New Physical Block Number
	int32_t old_pbn;	// Old Physical Block Number
	int32_t block_offset;

	unsigned int remain = length;
	unsigned int left_skip = sector_nb % SECTORS_PER_PAGE;
	unsigned int right_skip;
	unsigned int write_sects;

	unsigned int ret = FAIL;
	int write_page_nb=0;
	char page_state;

	while(remain > 0){

		if(remain > SECTORS_PER_PAGE - left_skip){
			right_skip = 0;
		}
		else{
			right_skip = SECTORS_PER_PAGE - left_skip - remain;
		}

		write_sects = SECTORS_PER_PAGE - left_skip - right_skip;

#ifdef FIRM_IO_BUFFER
		INCREASE_WB_FTL_POINTER(write_sects);
#endif
		/* Calculate logical address and block offset */
		lpn = lba / (int32_t)SECTORS_PER_PAGE;
		lbn = lpn / PAGE_NB;
		block_offset = lpn % (int32_t)PAGE_NB;

		/* Get old physical block number */
		old_pbn = GET_MAPPING_INFO(lbn);

		if(old_pbn == -1){
			/* Write date to new page in new block */
			ret = GET_NEW_BLOCK(VICTIM_OVERALL, EMPTY_TABLE_ENTRY_NB, &new_pbn);
			if(ret == FAIL){
				printf("ERROR[%s] Get new page fail \n",__FUNCTION__);
				return FAIL;
			}
			else{
				UPDATE_BLOCK_STATE(new_pbn, DATA_BLOCK);
			}

			/* SSD PAGE WRITE */	
			ret = SSD_PAGE_WRITE(CALC_FLASH(new_pbn), CALC_BLOCK(new_pbn), block_offset, write_page_nb, WRITE, io_page_nb);

			/* Update Mapping Information */
			UPDATE_NEW_BLOCK_MAPPING(lbn, new_pbn);
			UPDATE_BLOCK_STATE_ENTRY(new_pbn, block_offset, VALID);
		}
		else{
			/* Get Newest Replacement block */
			old_pbn = GET_AVAILABLE_PBN_FROM_RP_TABLE(old_pbn, block_offset);

			/* Write data to new page in old block */
			ret = SSD_PAGE_WRITE(CALC_FLASH(old_pbn), CALC_BLOCK(old_pbn), block_offset, write_page_nb, WRITE, io_page_nb);

			/* Update Mapping Information */
			UPDATE_BLOCK_STATE_ENTRY(old_pbn, block_offset, VALID);
		}
		
		write_page_nb++;

#ifdef FTL_DEBUG
                if(ret == SUCCESS){
                        printf("\twrite complete [%d, %d, %d]\n",CALC_FLASH(new_pbn), CALC_BLOCK(new_pbn), block_offset);
                }
                else if(ret == FAIL){
                        printf("ERROR[%s] [%d, %d] page write fail \n",__FUNCTION__, lbn, block_offset);
                }
#endif
		lba += write_sects;
		remain -= write_sects;
		left_skip = 0;
	}

	INCREASE_IO_REQUEST_SEQ_NB();

#ifdef FIRM_IO_BUFFER
	INCREASE_WB_LIMIT_POINTER();
#endif

#ifdef MONITOR_ON
	char szTemp[1024];
	sprintf(szTemp, "WRITE PAGE %d ", length);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "WB CORRECT %d", write_page_nb);
	WRITE_LOG(szTemp);
#endif

#ifdef FTL_DEBUG
	printf("[%s] End\n", __FUNCTION__);
#endif
	return ret;
}
