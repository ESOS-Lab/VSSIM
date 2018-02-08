// File: ftl_pm.c
// Date: 2014. 12. 11.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"
#ifndef VSSIM_BENCH
#include "qemu-kvm.h"
#endif

int FTL_PM_READ(int32_t sector_nb, unsigned int length)
{
#ifdef FTL_DEBUG
	printf("[%s] Start\n",__FUNCTION__);
#endif

	int32_t lpn;	// logical page number
	int32_t ppn;	// physical page number
	int32_t lba = sector_nb;
	unsigned int remain = length;
	unsigned int left_skip = sector_nb % SECTORS_PER_PAGE;
	unsigned int right_skip;
	unsigned int read_sects;

	unsigned int ret = FAIL;
	int read_page_nb = 0;
	int io_page_nb;

#ifdef FIRM_IO_BUFFER
	INCREASE_RB_FTL_POINTER(length);
#endif

	/* Check page mapping info
		whether all logical pages are mapped with a physical page */
	while(remain > 0){

		if(remain > SECTORS_PER_PAGE - left_skip){
			right_skip = 0;
		}
		else{
			right_skip = SECTORS_PER_PAGE - left_skip - remain;
		}
		read_sects = SECTORS_PER_PAGE - left_skip - right_skip;

		/* Calculate logical page number and Get mapping info */
		lpn = lba / (int32_t)SECTORS_PER_PAGE;
		ppn = GET_PM_MAPPING_INFO(lpn);

		if(ppn == -1){
#ifdef FIRM_IO_BUFFER
			INCREASE_RB_LIMIT_POINTER();
#endif
			return FAIL;
		}

		lba += read_sects;
		remain -= read_sects;
		left_skip = 0;
	}

	/* Alloc request struct to measure IO latency */
	io_alloc_overhead = ALLOC_IO_REQUEST(sector_nb, length, READ, &io_page_nb);

	remain = length;
	lba = sector_nb;
	left_skip = sector_nb % SECTORS_PER_PAGE;

	nand_io_info* n_io_info = NULL;

	while(remain > 0){

		if(remain > SECTORS_PER_PAGE - left_skip){
			right_skip = 0;
		}
		else{
			right_skip = SECTORS_PER_PAGE - left_skip - remain;
		}
		read_sects = SECTORS_PER_PAGE - left_skip - right_skip;

		/* Caculate logical page number */
		lpn = lba / (int32_t)SECTORS_PER_PAGE;

		/* Get physical page number */
		ppn = GET_PM_MAPPING_INFO(lpn);
		if(ppn == -1){
			printf("Error[%s] No Mapping info\n", __FUNCTION__);
		}

		n_io_info = CREATE_NAND_IO_INFO(read_page_nb, READ, io_page_nb, io_request_seq_nb);
#ifdef FIRM_IO_THREAD
		/* Read data from the NAND page */
		ret = ENQUEUE_NAND_IO(READ, CALC_FLASH_FROM_PPN(ppn),
					CALC_BLOCK_FROM_PPN(ppn),
					CALC_PAGE_FROM_PPN(ppn), 
					n_io_info);
#else

		ret = SSD_PAGE_READ(CALC_FLASH_FROM_PPN(ppn),
					CALC_BLOCK_FROM_PPN(ppn),
					CALC_PAGE_FROM_PPN(ppn), 
					n_io_info);
#endif

#ifdef FTL_DEBUG
		if(ret == FAIL){
			printf("ERROR[%s] %u page read fail \n",__FUNCTION__, ppn);
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

#ifdef FTL_DEBUG
	printf("[%s] Complete\n", __FUNCTION__);
#endif

	return ret;
}

int FTL_PM_WRITE(int32_t sector_nb, unsigned int length)
{
#ifdef FTL_DEBUG
	printf("[%s] Start\n", __FUNCTION__);
#endif

	int io_page_nb;

	/* Alloc request struct to measure IO latency */
	io_alloc_overhead = ALLOC_IO_REQUEST(sector_nb, length, WRITE, &io_page_nb);

	int32_t lba = sector_nb;
	int32_t lpn;		// Logical Page Number
	int32_t new_ppn;	// New Physical Page Number
	int32_t old_ppn;	// Old Physical Page Number

	unsigned int remain = length;
	unsigned int left_skip = sector_nb % SECTORS_PER_PAGE;
	unsigned int right_skip;
	unsigned int write_sects;

	unsigned int ret = FAIL;
	int write_page_nb=0;

	nand_io_info* n_io_info = NULL;

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
		/* Get new empty page to write a data */
		ret = GET_NEW_PAGE(VICTIM_OVERALL, EMPTY_TABLE_ENTRY_NB, &new_ppn);
		if(ret == FAIL){
			printf("ERROR[%s] Get new page fail \n", __FUNCTION__);
			return FAIL;
		}

		/* Calculate logical page number and Get mapping info */
		lpn = lba / (int32_t)SECTORS_PER_PAGE;
		old_ppn = GET_PM_MAPPING_INFO(lpn);

		n_io_info = CREATE_NAND_IO_INFO(write_page_nb, WRITE, io_page_nb, io_request_seq_nb);
#ifdef FIRM_IO_THREAD
		ret = ENQUEUE_NAND_IO(WRITE, CALC_FLASH_FROM_PPN(new_ppn),
				CALC_BLOCK_FROM_PPN(new_ppn),
				CALC_PAGE_FROM_PPN(new_ppn),
				n_io_info);
#else
		ret = SSD_PAGE_WRITE(CALC_FLASH_FROM_PPN(new_ppn),
				CALC_BLOCK_FROM_PPN(new_ppn),
				CALC_PAGE_FROM_PPN(new_ppn),
				n_io_info);
#endif
		
		write_page_nb++;

		/* Update page mapping table */
		UPDATE_OLD_PAGE_MAPPING(lpn);
		UPDATE_NEW_PAGE_MAPPING(lpn, new_ppn);

#ifdef FTL_DEBUG
                if(ret == SUCCESS){
                        printf("\twrite complete [%d, %d, %d]\n",CALC_FLASH_FROM_PPN(new_ppn), CALC_BLOCK_FROM_PPN(new_ppn),CALC_PAGE_FROM_PPN(new_ppn));
                }
                else if(ret == FAIL){
                        printf("Error[%s] %d page write fail \n",__FUNCTION__, new_ppn);
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
	sprintf(szTemp, "WB CORRECT %d", write_page_nb);
	WRITE_LOG(szTemp);
#endif

#ifdef FTL_DEBUG
	printf("[%s] End\n", __FUNCTION__);
#endif
	return ret;
}
