// File: ftl.c
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

int g_init = 0;
extern double ssd_util;

void FTL_INIT(void)
{
	if(g_init == 0){
        	printf("[%s] start\n", __FUNCTION__);

		INIT_SSD_CONFIG();

		INIT_MAPPING_TABLE();
		INIT_PERF_CHECKER();
		
#ifdef FIRM_IO_BUFFER
		INIT_IO_BUFFER();
#endif
		INIT_FLASH_INFO();
		INIT_VSSIM_CORE();	/* Init Flash -> Init Core */

#ifdef MONITOR_ON
		INIT_LOG_MANAGER();
#endif
		INIT_FLASH();
	
		g_init = 1;
		printf("[%s] complete\n", __FUNCTION__);
	}
}

void FTL_TERM(void)
{
	printf("[%s] start\n", __FUNCTION__);

#ifdef FIRM_IO_BUFFER
	TERM_IO_BUFFER();
#endif

	TERM_VSSIM_CORE();
	TERM_FLASH_INFO();

	TERM_PERF_CHECKER();

	TERM_MAPPING_TABLE();

#ifdef MONITOR_ON
	TERM_LOG_MANAGER();
#endif

	TERM_FLASH();

	printf("[%s] complete\n", __FUNCTION__);
}

int FTL_READ(int core_id, uint64_t sector_nb, uint32_t length)
{
	int ret;

	ret = _FTL_READ(core_id, sector_nb, length);
	if(ret == FAIL)
		printf("ERROR[%s] _FTL_READ function returns FAIL\n", __FUNCTION__);		

	return ret;
}

void FTL_WRITE(int core_id, uint64_t sector_nb, uint32_t length)
{
	int ret;

	ret = _FTL_WRITE(core_id, sector_nb, length);
	if(ret == FAIL)
		printf("ERROR[%s] _FTL_WRITE function returns FAIL\n", __FUNCTION__);		

	/* If needed, perform foreground GC */
	FGGC_CHECK(core_id);
}

void FTL_DISCARD(int core_id, uint64_t sector_nb, uint32_t length)
{
	if(sector_nb + length > N_SECTORS){
		printf("ERROR[%s] Exceed Sector number\n", __FUNCTION__);
                return;
        }

	uint64_t lba = sector_nb;
	int64_t lpn;
	int64_t lpn_4k;
	ppn_t ppn;
	pbn_t pbn;
	block_state_entry* bs_entry = NULL;
	uint32_t bitmap_index;

	uint32_t remain = length;
	uint32_t left_skip = sector_nb % SECTORS_PER_PAGE;

	int ret = FAIL;

	if(left_skip != 0 || (length % SECTORS_PER_4K_PAGE != 0)){
		printf("ERROR[%s] sector_nb: %lu, length: %u\n",
			__FUNCTION__, sector_nb, length);
		return;
	}

	while(remain > 0){

		/* Get the logical page number */
		lpn = lba / (int64_t)SECTORS_PER_PAGE;
		lpn_4k = lba / (int64_t)SECTORS_PER_4K_PAGE;
		
		/* Get the physical page number from the mapping table */
		ppn = GET_MAPPING_INFO(core_id, lpn);

		/* Get the block state entry of the ppn */
		pbn = PPN_TO_PBN(ppn);
		bs_entry = GET_BLOCK_STATE_ENTRY(pbn);	

		/* Update bitmap */
		bitmap_index = (uint32_t)(lpn_4k % BITMAP_SIZE);

		ret = CLEAR_BITMAP(bs_entry->valid_array, bitmap_index);
		if(ret == FAIL){
			return;
		}

		if(!TEST_BITMAP_MASK(bs_entry->valid_array, ppn.path.page)){
			bs_entry->n_valid_pages--;
		}

		lba += SECTORS_PER_4K_PAGE;
		remain -= SECTORS_PER_4K_PAGE;
		left_skip = 0;
	}

	return;
}

int _FTL_READ(int core_id, uint64_t sector_nb, uint32_t length)
{
#ifdef FTL_DEBUG
	printf("[%s] Start, sector_nb: %lu, length: %u\n", __FUNCTION__, sector_nb, length);
#endif

	if(sector_nb + length > N_SECTORS){
		printf("Error[%s] Exceed Sector number\n", __FUNCTION__); 
		return FAIL;	
	}

	int64_t lpn;
	ppn_t ppn;
	uint64_t lba = sector_nb;
	uint32_t remain = length;
	uint32_t left_skip = sector_nb % SECTORS_PER_PAGE;
	uint32_t right_skip;
	uint32_t read_sects;
	uint32_t n_trimmed_pages = 0;

	int n_pages = 0;
	int n_read_pages = 0;

	void* ret_buf = NULL;

	while(remain > 0){

		if(remain > SECTORS_PER_PAGE - left_skip){
			right_skip = 0;
		}
		else{
			right_skip = SECTORS_PER_PAGE - left_skip - remain;
		}
		read_sects = SECTORS_PER_PAGE - left_skip - right_skip;

		lpn = lba / (int64_t)SECTORS_PER_PAGE;

		ret_buf = CHECK_WRITE_BUFFER(core_id, lba, read_sects);

		if(ret_buf != NULL){
			/* Hit Write Buffer */	
		}
		else {
			/* Check Mapping Table */
			ppn = GET_MAPPING_INFO(core_id, lpn);

			if(ppn.addr != -1){			
				/* Read data from NAND page */
				FLASH_PAGE_READ(ppn);

				n_read_pages++;
			}
			else{
				/* Trimmed pages  */
				n_trimmed_pages++;
			}
		}

		n_pages++;

		ret_buf = NULL;
		lba += read_sects;
		remain -= read_sects;
		left_skip = 0;
	}

	/* Wait until all flash io are completed */
	WAIT_FLASH_IO(core_id, n_read_pages);

#ifdef FTL_DEBUG
	printf("[%s] Complete\n", __FUNCTION__);
#endif

	/* If thie read request is for trimmed data, mark it to the core req entry */
	if(n_pages == n_trimmed_pages){
		return TRIMMED;
	}

	return SUCCESS;
}

int _FTL_WRITE(int core_id, uint64_t sector_nb, uint32_t length)
{
#ifdef FTL_DEBUG
	printf("[%s] Start\n", __FUNCTION__);
#endif

	if(sector_nb + length > N_SECTORS){
		printf("ERROR[%s] Exceed Sector number\n", __FUNCTION__);
                return FAIL;
        }

	uint64_t lba = sector_nb;
	int64_t lpn;
	ppn_t new_ppn;
	ppn_t old_ppn;
	pbn_t temp_pbn;

	uint32_t remain = length;
	uint32_t left_skip = sector_nb % SECTORS_PER_PAGE;
	uint32_t right_skip;
	uint32_t write_sects;

	int ret = FAIL;
	int n_write_pages = 0;
	temp_pbn.addr = -1;

	while(remain > 0){

		if(remain > SECTORS_PER_PAGE - left_skip){
			right_skip = 0;
		}
		else{
			right_skip = SECTORS_PER_PAGE - left_skip - remain;
		}

		write_sects = SECTORS_PER_PAGE - left_skip - right_skip;

		ret = GET_NEW_PAGE(core_id, temp_pbn, MODE_OVERALL, &new_ppn);

		if(ret == FAIL){
			printf("ERROR[%s] Get new page fail \n", __FUNCTION__);
			return FAIL;
		}

		lpn = lba / (int64_t)SECTORS_PER_PAGE;
		old_ppn = GET_MAPPING_INFO(core_id, lpn);

		if((left_skip || right_skip) && (old_ppn.addr != -1)){
//TEMP
//			FLASH_PAGE_READ(old_ppn);
//			WAIT_FLASH_IO(core_id, 1);

			FLASH_PAGE_WRITE(new_ppn);
			
			PARTIAL_UPDATE_PAGE_MAPPING(core_id, lpn, new_ppn, \
					old_ppn, left_skip, right_skip);
		}
		else{
			ret = FLASH_PAGE_WRITE(new_ppn);

			UPDATE_OLD_PAGE_MAPPING(core_id, lpn);
			UPDATE_NEW_PAGE_MAPPING(core_id, lpn, new_ppn);
		}

		n_write_pages++;
		lba += write_sects;
		remain -= write_sects;
		left_skip = 0;
	}

	/* Wait until all flash io are completed */
	WAIT_FLASH_IO(core_id, n_write_pages);

#ifdef FTL_DEBUG
	printf("[%s] End\n", __FUNCTION__);
#endif
	return ret;
}
