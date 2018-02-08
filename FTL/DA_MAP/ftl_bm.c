// File: ftl_bm.c
// Date: 2014. 12. 11.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

#ifndef VSSIM_BENCH
#include "qemu-kvm.h"
#endif

int FTL_BM_READ(int32_t sector_nb, unsigned int length)
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
	int32_t block_offset;	// Logical Block Offset
	int32_t real_offset;	// Real(Physical) Block Offset

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

	/* Check block mapping info
		whether all logical pages are mapped with a physical page */
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
		lbn = lpn / (int32_t)PAGE_NB;
		block_offset = lpn % (int32_t)PAGE_NB;

		pbn = GET_VALID_MAPPING(lbn, block_offset, &real_offset);

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

		/* Calculate address and offset */
		lpn = lba / (int32_t)SECTORS_PER_PAGE;
		lbn = lpn / (int32_t)PAGE_NB;
		block_offset = lpn % (int32_t)PAGE_NB;

		/* Get physical block number */
		pbn = GET_VALID_MAPPING(lbn, block_offset, &real_offset);
		if(pbn == -1){
			printf("ERROR[%s] No Mapping info\n", __FUNCTION__);
		}

		/* Read data from NAND page */
		n_io_info = CREATE_NAND_IO_INFO(read_page_nb, READ, io_page_nb, io_request_seq_nb);

#ifdef FIRM_IO_THREAD
		ret = ENQUEUE_NAND_IO(READ, CALC_FLASH_FROM_PBN(pbn), CALC_BLOCK_FROM_PBN(pbn), real_offset, n_io_info);
#else
		ret = SSD_PAGE_READ(CALC_FLASH_FROM_PBN(pbn), CALC_BLOCK_FROM_PBN(pbn), real_offset, n_io_info);
#endif

#ifdef FTL_DEBUG
		if(ret == SUCCESS){
			printf("\t read complete [%u]-[%d][%d]\n",lpn, pbn, real_offset);
		}
		else if(ret == FAIL){
			printf("ERROR[%s] [%u] page read fail \n", __FUNCTION__, lpn);
		}
#endif

		lba += read_sects;
		remain -= read_sects;
		left_skip = 0;
		read_page_nb++;
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

int FTL_BM_WRITE(int32_t sector_nb, unsigned int length)
{
#ifdef FTL_DEBUG
	printf("[%s] Start\n",__FUNCTION__);
#endif
	int ret;
	int io_page_nb;
	int copy_page_nb = 0;
	int write_page_nb = 0;
	int exchange_gc_cnt = 0;

	if(sector_nb + length > SECTOR_NB){
		printf("ERROR[%s] Exceed Sector number\n", __FUNCTION__);
                return FAIL;
        }
	else{
		/* Alloc request struct to measure IO latency */
		io_alloc_overhead = ALLOC_IO_REQUEST(sector_nb, length, WRITE, &io_page_nb);
	}

	int32_t lba = sector_nb;
	int32_t lpn;		// Logical Page Number
	int32_t lbn;		// Logical Block Number
	int32_t new_pbn;	// New Physical Block Number
	int32_t old_pbn;	// Old Physical Block Number

	/* Calculate logical address and block offset */
	lpn = lba / (int32_t)SECTORS_PER_PAGE;
	lbn = lpn / (int32_t)PAGE_NB;
	int32_t block_offset = (int32_t)lpn % PAGE_NB;

	/* Get old physical block number */
	old_pbn = GET_BM_MAPPING_INFO(lbn);

	/* The logical block is written at first time */
	if(old_pbn == -1){
	
		/* Get new empty block to write data */
		ret = GET_NEW_BLOCK(VICTIM_OVERALL, EMPTY_TABLE_ENTRY_NB, &new_pbn);
		if(ret == FAIL){
			printf("ERROR[%s] Get new page fail (1). \n",__FUNCTION__);
			return FAIL;
		}

		/* Update the new empty block state as data block */	
		UPDATE_BLOCK_STATE(new_pbn, DATA_BLOCK);

		/* Write data to NAND pages */
		write_page_nb = _FTL_BM_WRITE(sector_nb, length, new_pbn, io_page_nb);

		/* Update Mapping Information */
		UPDATE_NEW_BLOCK_MAPPING(lbn, new_pbn);
	}
	else{
		int rp_pbn;

		/* Check empty pages of root block */
		ret = CHECK_EMPTY_PAGES_OF_PBN(old_pbn, sector_nb, length);

		block_state_entry* root_b_s_entry = GET_BLOCK_STATE_ENTRY(old_pbn);
		rp_pbn = root_b_s_entry->rp_block_pbn;

		/* Root block is available */
		if(ret == SUCCESS){
			write_page_nb = _FTL_BM_WRITE(sector_nb, length, old_pbn, io_page_nb);

			if(rp_pbn != -1){
				UPDATE_MULTIPLE_BLOCK_STATE_ENTRIES(sector_nb, length, rp_pbn, INVALID);
			}
		}
		else{

			/* Check the root block has replacement block */
			if(rp_pbn == -1){

				/* The root block has no replacement block,
					then We add new replacement block to the root block */
				ret = GET_NEW_BLOCK(VICTIM_OVERALL, EMPTY_TABLE_ENTRY_NB, &rp_pbn);
				if(ret == FAIL){
					printf("ERROR[%s] Get new page fail (2). \n",__FUNCTION__);
					return FAIL;
				}
				UPDATE_BLOCK_STATE(rp_pbn, DATA_BLOCK);

				/* Register the new block as rp block of the root block */
				INSERT_NEW_RP_BLOCK(old_pbn, rp_pbn);

				/* Write data to new replacement block */
				write_page_nb = _FTL_BM_WRITE(sector_nb, length, rp_pbn, io_page_nb);
				UPDATE_MULTIPLE_BLOCK_STATE_ENTRIES(sector_nb, length, old_pbn, INVALID);
			}
			else{
				/* Check if replacement block is available */
				ret = CHECK_EMPTY_PAGES_OF_PBN(rp_pbn, sector_nb, length);

				if(ret == SUCCESS){
					/* Replacement block is available */
					write_page_nb = _FTL_BM_WRITE(sector_nb, length, rp_pbn, io_page_nb);

					UPDATE_MULTIPLE_BLOCK_STATE_ENTRIES(sector_nb, length, old_pbn, INVALID);
				}
				else{
					int32_t new_rp_pbn;

					/* Check the number of valid pages of the root block */
					if(root_b_s_entry->valid_page_nb == 0){

						/* Just Erase the root block */
						copy_page_nb = BM_GARBAGE_COLLECTION(old_pbn);
						exchange_gc_cnt++;

						/* From Now,
							old_pbn: empty_block
							rp_pbn: root_block
							new_rp_pbn: rp_block
						*/

						ret = GET_NEW_BLOCK(VICTIM_OVERALL, EMPTY_TABLE_ENTRY_NB, &new_rp_pbn);
						if(ret == FAIL){
							printf("ERROR[%s] Get new page fail (3). \n",__FUNCTION__);
							return FAIL;
						}

						UPDATE_BLOCK_STATE(new_rp_pbn, DATA_BLOCK);

						/* Register the new block as rp block of the root block */
						INSERT_NEW_RP_BLOCK(rp_pbn, new_rp_pbn);

						/* Write data to new replacement block */
						write_page_nb = _FTL_BM_WRITE(sector_nb, length, new_rp_pbn, io_page_nb);
						UPDATE_MULTIPLE_BLOCK_STATE_ENTRIES(sector_nb, length, rp_pbn, INVALID);
					}
					else{
						/* Start Merge Operation */
						ret = GET_NEW_BLOCK(VICTIM_OVERALL, EMPTY_TABLE_ENTRY_NB, &new_pbn);
						if(ret == FAIL){
							printf("ERROR[%s] Get new page fail (4).\n",__FUNCTION__);
							return FAIL;
						}
						UPDATE_BLOCK_STATE(new_pbn, DATA_BLOCK);

						/* Write to new block with merge operation */
						copy_page_nb = _FTL_BM_MERGE_WRITE(old_pbn, new_pbn, 0, block_offset, block_offset);
						write_page_nb = _FTL_BM_WRITE(sector_nb, length, new_pbn, io_page_nb);
						copy_page_nb += _FTL_BM_MERGE_WRITE(old_pbn, new_pbn, block_offset+io_page_nb, PAGE_NB, block_offset);

						/* Update Mapping Information */
						UPDATE_OLD_BLOCK_MAPPING(lbn);
						UPDATE_NEW_BLOCK_MAPPING(lbn, new_pbn);

						/* Erase root and rp block */
#ifndef ON_BOARD
	#ifdef FIRM_IO_THREAD
						ENQUEUE_NAND_IO(ERASE, \
							CALC_FLASH_FROM_PBN(old_pbn),\
							CALC_BLOCK_FROM_PBN(old_pbn),\
							0, NULL);
						ENQUEUE_NAND_IO(ERASE,
							CALC_FLASH_FROM_PBN(rp_pbn),\
							CALC_BLOCK_FROM_PBN(rp_pbn),\
							0, NULL);
	#else
						SSD_BLOCK_ERASE(CALC_FLASH_FROM_PBN(old_pbn),\
							CALC_BLOCK_FROM_PBN(old_pbn));
						SSD_BLOCK_ERASE(CALC_FLASH_FROM_PBN(rp_pbn),\
							CALC_BLOCK_FROM_PBN(rp_pbn));
	#endif
#endif
	
						/* Update block state */
						UPDATE_BLOCK_STATE(rp_pbn, EMPTY_BLOCK);
						UPDATE_BLOCK_STATE(old_pbn, EMPTY_BLOCK);

						/* Insert the blocks to empty block pool */
						INSERT_EMPTY_BLOCK(rp_pbn);
						INSERT_EMPTY_BLOCK(old_pbn);
					}
				}
			}
		}
	}
	
	INCREASE_IO_REQUEST_SEQ_NB();

#ifdef FIRM_IO_BUFFER
	INCREASE_WB_LIMIT_POINTER();
#endif

#ifdef MONITOR_ON
	char szTemp[1024];
	sprintf(szTemp, "WB CORRECT %d", write_page_nb);
	WRITE_LOG(szTemp);
	sprintf(szTemp,"WB AMP %d", copy_page_nb);
	WRITE_LOG(szTemp);
	if(exchange_gc_cnt > 0){
		sprintf(szTemp,"EXCHANGE %d", exchange_gc_cnt);
		WRITE_LOG(szTemp);
	}
#endif

#ifdef FTL_DEBUG
	printf("[%s] End\n", __FUNCTION__);
#endif
	return ret;
}

int _FTL_BM_WRITE(int32_t sector_nb, unsigned int length, int32_t dst_pbn, int io_page_nb)
{
	unsigned int remain = length;
	unsigned int left_skip = sector_nb % SECTORS_PER_PAGE;
	unsigned int right_skip;
	unsigned int write_sects;

	int32_t lba = sector_nb;
	int32_t lpn = lba / (int32_t)SECTORS_PER_PAGE;	// Logical Page Number
	int32_t lbn = lpn / (int32_t)PAGE_NB;		// Logical Block Number

	int32_t block_offset = lpn % (int32_t)PAGE_NB;
	int32_t start_offset = -1;
	int32_t real_offset = -1;

	block_state_entry* b_s_entry = GET_BLOCK_STATE_ENTRY(dst_pbn);
	if(b_s_entry->start_offset == -1){
		b_s_entry->start_offset = block_offset;
		start_offset = block_offset;
	}
	else{
		start_offset = b_s_entry->start_offset;
	}

	int ret = FAIL;
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
		/* Calculate logical address and block offset */
		lpn = lba / (int32_t)SECTORS_PER_PAGE;
		lbn = lpn / (int32_t)PAGE_NB;
		block_offset = lpn % (int32_t)PAGE_NB;
		real_offset = block_offset - start_offset;

		if(real_offset < 0){
			printf("ERROR[%s] real_offset %d = %d - %d\n", __FUNCTION__, real_offset, block_offset, start_offset);
		}

		n_io_info = CREATE_NAND_IO_INFO(write_page_nb, WRITE, io_page_nb, io_request_seq_nb);

#ifdef FIRM_IO_THREAD
		ret = ENQUEUE_NAND_IO(WRITE, CALC_FLASH_FROM_PBN(dst_pbn), CALC_BLOCK_FROM_PBN(dst_pbn), real_offset, n_io_info);
#else
		ret = SSD_PAGE_WRITE(CALC_FLASH_FROM_PBN(dst_pbn), CALC_BLOCK_FROM_PBN(dst_pbn), real_offset, n_io_info);
#endif

#ifdef FTL_DEBUG
                if(ret == FAIL){
                        printf("ERROR[%s] [%d, %d] page write fail \n",__FUNCTION__, lbn, block_offset);
                }
#endif
		/* Update Mapping Information */
		UPDATE_BLOCK_STATE_ENTRY(dst_pbn, real_offset, VALID);

		write_page_nb++;

		lba += write_sects;
		remain -= write_sects;
		left_skip = 0;
	}

	return write_page_nb;
}

/* Caution: the old pbn block must have a replacement block */
int _FTL_BM_MERGE_WRITE(int32_t old_pbn, int32_t dst_pbn, int start_offset, int end_offset, int block_offset)
{
#ifdef FTL_DEBUG
	printf("[%s] Start, start_offset:%d\n", __FUNCTION__, start_offset);
#endif

	int i;
	int32_t src_pbn;
	int write_page_nb=0;

	block_state_entry* root_b_s_entry = GET_BLOCK_STATE_ENTRY(old_pbn);
	int32_t rp_pbn = root_b_s_entry->rp_block_pbn;

	if(rp_pbn == -1){
		printf("ERROR[%s] The block has no replacement block.\n", __FUNCTION__);
		return -1;
	}
	block_state_entry* rp_b_s_entry = GET_BLOCK_STATE_ENTRY(rp_pbn);
	block_state_entry* dst_b_s_entry = GET_BLOCK_STATE_ENTRY(dst_pbn);

	int root_start_offset = root_b_s_entry->start_offset;
	int rp_start_offset = rp_b_s_entry->start_offset;
	int dst_start_offset = dst_b_s_entry->start_offset;

	int root_write_limit = root_b_s_entry->write_limit;
	int rp_write_limit = rp_b_s_entry->write_limit;

	/* Intialized the start offset of the destination block */
	if(dst_start_offset == -1){
		dst_start_offset = block_offset;
		if(dst_start_offset > root_start_offset){
			dst_start_offset = root_start_offset;
		}
		if(dst_start_offset > rp_start_offset){
			dst_start_offset = rp_start_offset;
		}
		dst_b_s_entry->start_offset = dst_start_offset;
	}

	/* page state from valid bitmap */
	int old_page_state = -1;
	int rp_page_state = -1;

	int root_real_offset = -1;
	int rp_real_offset = -1;
	int src_real_offset = -1;
	int dst_real_offset = -1;

	nand_io_info* n_io_info;

	/* Pre-writing */
	for(i=start_offset; i<end_offset; i++){

		if(root_start_offset <= i){
			root_real_offset = i - root_start_offset;
			if(root_real_offset <= root_write_limit){ 
				/* Check if the root block or rp block has valid page */
				old_page_state = GET_BIT(valid_bitmap, old_pbn * PAGE_NB + root_real_offset);
			}
			else{
				old_page_state = -1;
			}
		}
		if(rp_start_offset <= i){
			rp_real_offset = i - rp_start_offset;
			if(rp_real_offset <= rp_write_limit){
				/* Check if the root block or rp block has valid page */
				rp_page_state = GET_BIT(valid_bitmap, rp_pbn * PAGE_NB + rp_real_offset);
			}
			else{
				rp_page_state = -1;
			}
		}

		if(old_page_state == 1 && rp_page_state == 1){
			printf("ERROR[%s] Both pages have valid state.\n", __FUNCTION__);
			return -1;
		}

		if(old_page_state == 1){
			src_pbn = old_pbn;
			src_real_offset = root_real_offset;
		}
		else if(rp_page_state == 1){
			src_pbn = rp_pbn;
			src_real_offset = rp_real_offset;
		}
		dst_real_offset = i - dst_start_offset;

		if(old_page_state == 1 || rp_page_state == 1){
			/* SSD Read and Write operation */

#ifdef FIRM_IO_THREAD
			n_io_info = CREATE_NAND_IO_INFO(-1, GC_READ, -1, io_request_seq_nb);
			ENQUEUE_NAND_IO(READ,CALC_FLASH_FROM_PBN(src_pbn), \
					CALC_BLOCK_FROM_PBN(src_pbn), \
					src_real_offset, n_io_info);

			n_io_info = CREATE_NAND_IO_INFO(-1, GC_WRITE, -1, io_request_seq_nb);
			ENQUEUE_NAND_IO(WRITE,CALC_FLASH_FROM_PBN(dst_pbn), \
					CALC_BLOCK_FROM_PBN(dst_pbn), \
					dst_real_offset, n_io_info);

#else
			n_io_info = CREATE_NAND_IO_INFO(-1, GC_READ, -1, io_request_seq_nb);
			SSD_PAGE_READ(CALC_FLASH_FROM_PBN(src_pbn), \
					CALC_BLOCK_FROM_PBN(src_pbn), \
					src_real_offset, n_io_info);
			n_io_info = CREATE_NAND_IO_INFO(-1, GC_WRITE, -1, io_request_seq_nb);
			SSD_PAGE_WRITE(CALC_FLASH_FROM_PBN(dst_pbn), \
					CALC_BLOCK_FROM_PBN(dst_pbn), \
					dst_real_offset, n_io_info);
#endif

			/* Update Mapping Information */
			UPDATE_BLOCK_STATE_ENTRY(src_pbn, src_real_offset, INVALID);
			UPDATE_BLOCK_STATE_ENTRY(dst_pbn, dst_real_offset, VALID);

			write_page_nb++;
		}
	}

#ifdef FTL_DEBUG
	printf("[%s] End. Last Offset:%d\n", __FUNCTION__, dst_real_offset);
#endif

	return write_page_nb;
}

int CHECK_EMPTY_PAGES_OF_PBN(int32_t pbn, int32_t sector_nb, unsigned int length)
{
	int ret = FAIL;

	block_state_entry* b_s_entry = GET_BLOCK_STATE_ENTRY(pbn);
	if(b_s_entry->start_offset == -1){
		printf("ERROR[%s] start offset of %d is not initialized. \n", __FUNCTION__, pbn);
		return ret;
	}

	/* Calculate logical address and block offset */
	int32_t lba = sector_nb;
	int32_t lpn = lba / (int32_t)SECTORS_PER_PAGE;
	int32_t block_offset = lpn % (int32_t)PAGE_NB;
	int32_t real_offset = block_offset - b_s_entry->start_offset; 

	/* Check write limit */
	if(real_offset ==  b_s_entry->write_limit + 1){
		ret = SUCCESS;
	}

	return ret;
}
