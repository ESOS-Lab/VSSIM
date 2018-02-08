// File: ftl_gc_manager.h
// Date: 2014. 12. 10.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

void GC_CHECK(void)
{
	int i, ret;

	/* If the number of total empty block is smaller than the number of flash memory */
	if(total_empty_block_nb <= FLASH_NB * PLANES_PER_FLASH){

		/* Reclaim empty block */
		for(i=0; i<GC_VICTIM_NB; i++){
#ifdef FIRM_IO_THREAD
			FLUSH_ALL_NAND_IO();
#endif
			ret = GARBAGE_COLLECTION();
			if(ret == FAIL){
				break;
			}
		}
	}
}

int GARBAGE_COLLECTION(void)
{
#ifdef FTL_DEBUG
	printf("[%s] Start\n", __FUNCTION__);
#endif
	/* Select one victim block */
	int32_t victim_pbn = SELECT_VICTIM_BLOCK();
	int copy_page_nb;

	if(victim_pbn == -1){
		/* There is no available victim block */
		return FAIL;
	}

	block_state_entry* victim_b_s_entry = GET_BLOCK_STATE_ENTRY(victim_pbn);

	/* If The Victim Block is in Page mapping Area, */
	if(victim_b_s_entry->pm_inv_table != NULL){
		copy_page_nb = PM_GARBAGE_COLLECTION(victim_pbn);
	}
	/* If The Victim Block is in Block mapping Area, */
	else{
		copy_page_nb = BM_GARBAGE_COLLECTION(victim_pbn);
	}

#ifdef MONITOR_ON
	char szTemp[1024];
	sprintf(szTemp, "GC ");
	WRITE_LOG(szTemp);
	sprintf(szTemp, "WB AMP %d", copy_page_nb);
	WRITE_LOG(szTemp);
#endif

#ifdef FTL_DEBUG
	printf("[%s] Complete %ld\n", __FUNCTION__, total_empty_block_nb);
#endif
	return SUCCESS;
	
}

int PM_GARBAGE_COLLECTION(int32_t victim_pbn)
{
#ifdef FTL_DEBUG
	printf("[%s] Start\n", __FUNCTION__);
#endif

	int i, ret;
	int32_t lpn;
	int32_t old_ppn, new_ppn;
	int copy_page_nb = 0;

	/* Calculate physical flash number and physical block number from the victim block number */
	unsigned int victim_phy_flash_nb = CALC_FLASH_FROM_PBN(victim_pbn);
	unsigned int victim_phy_block_nb = CALC_BLOCK_FROM_PBN(victim_pbn);

	/* Calculate the mapping index of the empty block table */
	int plane_nb = victim_phy_block_nb % PLANES_PER_FLASH;
	int mapping_index = plane_nb * FLASH_NB + victim_phy_flash_nb;

	/* Get the valid page number of the victim block  */
	block_state_entry* victim_b_s_entry = GET_BLOCK_STATE_ENTRY(victim_pbn);

	int32_t victim_start_ppn = victim_pbn * PAGE_NB;

	int valid_page_nb = victim_b_s_entry->valid_page_nb;

	nand_io_info* n_io_info = NULL;

	/* If the victim block has valid pages, then copy the pages to empty block */
	if(valid_page_nb != 0){
		for(i=0;i<PAGE_NB;i++){
			if(GET_BIT(valid_bitmap, victim_start_ppn + i) == 1){
				/* Get new empty page */
#ifdef GC_VICTIM_OVERALL
				ret = GET_NEW_PAGE(VICTIM_OVERALL, EMPTY_TABLE_ENTRY_NB, &new_ppn);
#else
				ret = GET_NEW_PAGE(VICTIM_INCHIP, mapping_index, &new_ppn);
#endif
				if(ret == FAIL){
					printf("ERROR[%s] Copy %dth valid page fail\n", __FUNCTION__, i);
					break;
				}

				/* NAND Read and Write operation */
				n_io_info = CREATE_NAND_IO_INFO(i, GC_READ, -1, io_request_seq_nb);
				SSD_PAGE_READ(victim_phy_flash_nb, victim_phy_block_nb, i, n_io_info);

				n_io_info = CREATE_NAND_IO_INFO(i, GC_WRITE, -1, io_request_seq_nb);
				SSD_PAGE_WRITE(CALC_FLASH_FROM_PPN(new_ppn), CALC_BLOCK_FROM_PPN(new_ppn), CALC_PAGE_FROM_PPN(new_ppn), n_io_info);

				/* Calculate the physical page number*/
				old_ppn = victim_phy_flash_nb*PAGES_PER_FLASH + victim_phy_block_nb*PAGE_NB + i;

				/* Get the logical page number of the old page */
				lpn = GET_PM_INVERSE_MAPPING_INFO(old_ppn);

				/* Update page table mapping*/
				UPDATE_NEW_PAGE_MAPPING(lpn, new_ppn);

				copy_page_nb++;
			}
		}
	}

	if(copy_page_nb != valid_page_nb){
		printf("ERROR[%s] The number of valid page is not correct. %d != %d \n", __FUNCTION__, copy_page_nb, victim_b_s_entry->valid_page_nb);
		return -1;
	}

#ifndef ON_BOARD
	/* Erase the victim block */
	SSD_BLOCK_ERASE(CALC_FLASH_FROM_PBN(victim_pbn), CALC_BLOCK_FROM_PBN(victim_pbn));
#endif

	/* Update the state of the victim block to empty state */
	UPDATE_BLOCK_STATE(victim_pbn, EMPTY_BLOCK);

	/* Insert the victim block to the empty block pool */
	INSERT_EMPTY_BLOCK(victim_pbn);

#ifdef FTL_DEBUG
	printf("[%s] End\n", __FUNCTION__);
#endif

	return copy_page_nb;
}

int BM_GARBAGE_COLLECTION(int32_t victim_pbn)
{
#ifdef FTL_DEBUG
	printf("[%s] Start\n", __FUNCTION__);
#endif
	int copy_page_nb = 0;
	
	block_state_entry* root_b_s_entry = GET_BLOCK_STATE_ENTRY(victim_pbn);
	block_state_entry* rp_b_s_entry;
	block_state_entry* temp_b_s_entry;

	int32_t root_pbn = root_b_s_entry->rp_root_pbn;
	int32_t rp_pbn;
	int32_t new_pbn;

	if(root_b_s_entry->rp_root_pbn == -1){
		/* This block is root block */
		root_pbn = victim_pbn;
		rp_pbn = root_b_s_entry->rp_block_pbn;
		rp_b_s_entry = GET_BLOCK_STATE_ENTRY(rp_pbn);
	}
	else{
		/* This block is replacement block */
		rp_pbn = victim_pbn;
		rp_b_s_entry = root_b_s_entry;
		root_b_s_entry = GET_BLOCK_STATE_ENTRY(root_pbn);
	}

	int ret;
	int switch_merge_cnt = 0;
	int partial_merge_cnt = 0;
	int full_merge_cnt = 0;

	int32_t lbn = GET_BM_INVERSE_MAPPING_INFO(root_pbn);

	if(lbn == -1){
		printf("ERROR[%s] It is a replacement block[%d] \n", __FUNCTION__, root_pbn);
		return 0;
	}

	if(rp_pbn == -1){
		ret = GET_NEW_BLOCK(VICTIM_OVERALL, EMPTY_TABLE_ENTRY_NB, &new_pbn);
		if(ret == FAIL){
			printf("ERROR[%s] Get new page fail \n",__FUNCTION__);
			return FAIL;
		}
		UPDATE_BLOCK_STATE(new_pbn, DATA_BLOCK);
		copy_page_nb = COPY_VALID_PAGES(root_pbn, new_pbn);

		/* copy the start offset of the root block to new block */
		temp_b_s_entry = GET_BLOCK_STATE_ENTRY(new_pbn);
		temp_b_s_entry->start_offset = root_b_s_entry->start_offset;

		/* Update Mapping Information */
		UPDATE_OLD_BLOCK_MAPPING(lbn);
		UPDATE_NEW_BLOCK_MAPPING(lbn, new_pbn);
#ifndef ON_BOARD
		/* Erase root and rp block */
		SSD_BLOCK_ERASE(CALC_FLASH_FROM_PBN(root_pbn),\
				CALC_BLOCK_FROM_PBN(root_pbn));
#endif
		UPDATE_BLOCK_STATE(root_pbn, EMPTY_BLOCK);

		/* Insert the blocks to empty block pool */
		INSERT_EMPTY_BLOCK(root_pbn);
	
		partial_merge_cnt++;
	}
	/* Switch Merge */
	else if(root_b_s_entry->valid_page_nb == 0){
#ifdef FTL_DEBUG
		printf("\t[%s] Switch Merge.\n", __FUNCTION__);
#endif

		/* Update Mapping Information */
		UPDATE_OLD_BLOCK_MAPPING(lbn);
		UPDATE_NEW_BLOCK_MAPPING(lbn, rp_pbn);

#ifndef ON_BOARD
		/* Erase root and rp block */
		SSD_BLOCK_ERASE(CALC_FLASH_FROM_PBN(root_pbn),\
				CALC_BLOCK_FROM_PBN(root_pbn));
#endif

		rp_b_s_entry->rp_root_pbn = -1;
		UPDATE_BLOCK_STATE(root_pbn, EMPTY_BLOCK);

		/* Insert the blocks to empty block pool */
		INSERT_EMPTY_BLOCK(root_pbn);

		switch_merge_cnt++;
	}
	/* Partial Merge */
	else if(CHECK_PARTIAL_MERGE(root_pbn, rp_pbn)==SUCCESS){
#ifdef FTL_DEBUG
		printf("\t[%s] Partial Merge.\n", __FUNCTION__);
#endif
		/* Need to Modify in 2015.. */
		copy_page_nb = COPY_VALID_PAGES(root_pbn, rp_pbn);

		/* Update Mapping Information */
		UPDATE_OLD_BLOCK_MAPPING(lbn);
		UPDATE_NEW_BLOCK_MAPPING(lbn, rp_pbn);

#ifndef ON_BOARD
		/* Erase root and rp block */
		SSD_BLOCK_ERASE(CALC_FLASH_FROM_PBN(root_pbn),\
				CALC_BLOCK_FROM_PBN(root_pbn));
#endif
		rp_b_s_entry->rp_root_pbn = -1;
				
		UPDATE_BLOCK_STATE(root_pbn, EMPTY_BLOCK);

		/* Insert the blocks to empty block pool */
		INSERT_EMPTY_BLOCK(root_pbn);

		partial_merge_cnt++;
	}
	/* Full Merge */
	else{
#ifdef FTL_DEBUG
		printf("\t[%s] Full Merge.\n", __FUNCTION__);
#endif
		ret = GET_NEW_BLOCK(VICTIM_OVERALL, EMPTY_TABLE_ENTRY_NB, &new_pbn);
		if(ret == FAIL){
			printf("ERROR[%s] Get new page fail \n",__FUNCTION__);
			return FAIL;
		}
		UPDATE_BLOCK_STATE(new_pbn, DATA_BLOCK);
		copy_page_nb = _FTL_BM_MERGE_WRITE(root_pbn, new_pbn, 0, PAGE_NB, 0);

		/* Update Mapping Information */
		UPDATE_OLD_BLOCK_MAPPING(lbn);
		UPDATE_NEW_BLOCK_MAPPING(lbn, new_pbn);

#ifndef ON_BOARD
		/* Erase root and rp block */
		SSD_BLOCK_ERASE(CALC_FLASH_FROM_PBN(root_pbn),\
			CALC_BLOCK_FROM_PBN(root_pbn));
		SSD_BLOCK_ERASE(CALC_FLASH_FROM_PBN(rp_pbn),\
			CALC_BLOCK_FROM_PBN(rp_pbn));
#endif

		/* Update block state */
		UPDATE_BLOCK_STATE(rp_pbn, EMPTY_BLOCK);
		UPDATE_BLOCK_STATE(root_pbn, EMPTY_BLOCK);

		/* Insert the blocks to empty block pool */
		INSERT_EMPTY_BLOCK(rp_pbn);
		INSERT_EMPTY_BLOCK(root_pbn);

		full_merge_cnt++;
	}

#ifdef MONITOR_ON
	char szTemp[1024];
	if(switch_merge_cnt != 0){
		sprintf(szTemp,"EXCHANGE %d", switch_merge_cnt);
		WRITE_LOG(szTemp);
	}
	else if(partial_merge_cnt != 0){
		sprintf(szTemp,"MERGE SEQ %d", partial_merge_cnt);
		WRITE_LOG(szTemp);
	}
	else if(full_merge_cnt != 0){
		sprintf(szTemp,"MERGE RAND %d", partial_merge_cnt);
		WRITE_LOG(szTemp);
	}
	sprintf(szTemp,"WB AMP %d", copy_page_nb);
	WRITE_LOG(szTemp);
#endif

#ifdef FTL_DEBUG
	printf("[%s] End\n", __FUNCTION__);
#endif
	return copy_page_nb;
}


/* Check whether the valid pages of root block(src_pbn)
	can be moved into the Replacement block(dst_pbn). */
int CHECK_PARTIAL_MERGE(int32_t src_pbn, int32_t dst_pbn)
{
	int i;
	int ret = SUCCESS;
	int page_state;

	/* Get the root block info */
	int src_start_ppn = src_pbn * PAGE_NB;
	block_state_entry* src_b_s_entry = GET_BLOCK_STATE_ENTRY(src_pbn);
	int src_start_offset = src_b_s_entry->start_offset;

	/* Get the replacment block info */
	block_state_entry* dst_b_s_entry = GET_BLOCK_STATE_ENTRY(dst_pbn);
	int dst_start_offset = dst_b_s_entry->start_offset;
	int dst_write_limit = dst_b_s_entry->write_limit;

	/* Calculate the offset range to check */
	int end_offset = dst_start_offset + dst_write_limit;
	int end = end_offset - src_start_offset;
		

	for(i=0; i<=end; i++){
		page_state = GET_BIT(valid_bitmap, src_start_ppn + i);

		if(page_state == 1){
			ret = FAIL;
			break;
		}
	}

	return ret;
}

/* Greedy Garbage Collection Algorithm */
int32_t SELECT_VICTIM_BLOCK(void)
{
	int i;
	int32_t curr_pbn = -1;
	block_state_entry* curr_b_s_entry;
	int curr_valid_page_nb = PAGE_NB;

	for(i=0;i<BLOCK_MAPPING_ENTRY_NB;i++){
		curr_b_s_entry = GET_BLOCK_STATE_ENTRY(i);

		/* If the block is in block mapping and it has no rp block, continue.*/
		if(curr_b_s_entry->pm_inv_table == NULL && curr_b_s_entry->rp_block_pbn == -1){
			continue;
		}

		if(curr_b_s_entry->type == DATA_BLOCK && PM_BLOCK_TABLE_SCAN(i) == SUCCESS){

			/* Select the block which has the lest number of invalid pages */
			if(curr_valid_page_nb > curr_b_s_entry->valid_page_nb){
				curr_valid_page_nb = curr_b_s_entry->valid_page_nb;
				curr_pbn = i;
			}

			/* If the block is dead block, quit the loop */
			if(curr_valid_page_nb == 0){
				break;
			}
		}
	}

	/* There is no available victim block */
	if(curr_valid_page_nb == PAGE_NB){
		return -1;
	}

	return curr_pbn;
}


/* Copy the valid pages of old_pbn to new_pbn */
int COPY_VALID_PAGES(int32_t old_pbn, int32_t new_pbn)
{
	int i;
	int copy_page_nb = 0;

	/* Get the block state entries */
	block_state_entry* old_b_s_entry = GET_BLOCK_STATE_ENTRY(old_pbn);
	block_state_entry* new_b_s_entry = GET_BLOCK_STATE_ENTRY(new_pbn);

	int old_start_offset = old_b_s_entry->start_offset;
	int new_start_offset = new_b_s_entry->start_offset;

	if(new_start_offset == -1){
		new_start_offset = old_start_offset;
		new_b_s_entry->start_offset = old_start_offset;
	}

	/* Calculate the flash and block number of old_pbn and new_pbn*/
	unsigned int old_flash_nb = CALC_FLASH_FROM_PBN(old_pbn);
	unsigned int old_block_nb = CALC_BLOCK_FROM_PBN(old_pbn);
	unsigned int new_flash_nb = CALC_FLASH_FROM_PBN(new_pbn);
	unsigned int new_block_nb = CALC_BLOCK_FROM_PBN(new_pbn);

	/* Get the state of the old_pbn*/
	int32_t old_start_ppn = old_pbn * PAGE_NB;
	int32_t new_start_ppn = new_pbn * PAGE_NB;
	int real_offset;
	int page_state;

	nand_io_info* n_io_info;

	/* Copy the valid pages of old_pbn to new_pbn */
	for(i=0;i<PAGE_NB;i++){
		if(GET_BIT(valid_bitmap, old_start_ppn + i) == 1){

			/* Calculate real offset of replacement block to write */
			real_offset = i + old_start_offset;
			real_offset = real_offset - new_start_offset;

			/* Check whether the page in real offset is available */
			page_state = GET_BIT(valid_bitmap, new_start_ppn + real_offset);
			if(real_offset < 0 || real_offset >= PAGE_NB || page_state == 1){
				printf("ERROR[%s] The page of %d offset is not available. \n", __FUNCTION__, real_offset);
				break;
			}

			/* SSD Read and Write operation */
			n_io_info = CREATE_NAND_IO_INFO(-1, GC_READ, -1, io_request_seq_nb);
			SSD_PAGE_READ(old_flash_nb, old_block_nb, i, n_io_info);

			n_io_info = CREATE_NAND_IO_INFO(-1, GC_WRITE, -1, io_request_seq_nb);
			SSD_PAGE_WRITE(new_flash_nb, new_block_nb, real_offset, n_io_info);

			/* Update block state of the blocks*/
			UPDATE_BLOCK_STATE_ENTRY(new_pbn, real_offset, VALID);
			UPDATE_BLOCK_STATE_ENTRY(old_pbn, i, INVALID);

			copy_page_nb++;
		}
	}

	return copy_page_nb;
}

int PM_BLOCK_TABLE_SCAN(int32_t victim_pbn){

	int i;

	/* Get the page block table */
	pm_block_entry* p_b_entry = (pm_block_entry*)pm_block_table;

	/* Check if the victim block number is in the page block table */
	for(i=0;i<EMPTY_TABLE_ENTRY_NB;i++){

		/* If the victim block number is in the table, return fail */
		if(p_b_entry->phy_block_nb == victim_pbn){
			return FAIL;
		} 

		p_b_entry += 1;
	}

	return SUCCESS;
}
