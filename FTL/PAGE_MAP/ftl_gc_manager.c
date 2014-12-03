// File: ftl_gc_manager.c
// Data: 2014. 12. 03.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

unsigned int gc_count = 0;

// TEMP
int fail_cnt = 0;
extern double ssd_util;

void GC_CHECK(unsigned int phy_flash_nb, unsigned int phy_block_nb)
{
	int i, ret;
	int plane_nb = phy_block_nb % PLANES_PER_FLASH;
	int mapping_index = plane_nb * FLASH_NB + phy_flash_nb;
	
#ifdef GC_TRIGGER_OVERALL
//	if(total_empty_block_nb < GC_THRESHOLD_BLOCK_NB)
	if(total_empty_block_nb <= FLASH_NB * PLANES_PER_FLASH)
	{
		for(i=0; i<GC_VICTIM_NB; i++){
			ret = GARBAGE_COLLECTION();
			if(ret == FAIL){
				break;
			}
		}
	}
#else
	empty_block_root* curr_root_entry = (empty_block_root*)empty_block_list + mapping_index;

	if(curr_root_entry->empty_block_nb < GC_THRESHOLD_BLOCK_NB_EACH){
		for(i=0; i<GC_VICTIM_NB; i++){
			ret = GARBAGE_COLLECTION();
			if(ret == FAIL){
				break;
			}
		}
	}
#endif
}

int GARBAGE_COLLECTION(void)
{
#ifdef FTL_DEBUG
	printf("[%s] Start\n", __FUNCTION__);
#endif
	int i;
	int ret;
	int32_t lpn;
	int32_t old_ppn;
	int32_t new_ppn;

	unsigned int victim_phy_flash_nb = FLASH_NB;
	unsigned int victim_phy_block_nb = 0;

	char* valid_array;
	int copy_page_nb = 0;

	block_state_entry* b_s_entry;

	ret = SELECT_VICTIM_BLOCK(&victim_phy_flash_nb, &victim_phy_block_nb);
	if(ret == FAIL){
#ifdef FTL_DEBUG
		printf("[%s] There is no available victim block\n", __FUNCTION__);
#endif
		return FAIL;
	}

	int plane_nb = victim_phy_block_nb % PLANES_PER_FLASH;
	int mapping_index = plane_nb * FLASH_NB + victim_phy_flash_nb;

	b_s_entry = GET_BLOCK_STATE_ENTRY(victim_phy_flash_nb, victim_phy_block_nb);
	valid_array = b_s_entry->valid_array;

	for(i=0;i<PAGE_NB;i++){
		if(valid_array[i]=='V'){
#ifdef GC_VICTIM_OVERALL
			ret = GET_NEW_PAGE(VICTIM_OVERALL, EMPTY_TABLE_ENTRY_NB, &new_ppn);
#else
			ret = GET_NEW_PAGE(VICTIM_INCHIP, mapping_index, &new_ppn);
#endif
			if(ret == FAIL){
				printf("ERROR[%s] Get new page fail\n", __FUNCTION__);
				return FAIL;
			}
			SSD_PAGE_READ(victim_phy_flash_nb, victim_phy_block_nb, i, i, GC_READ, -1);
			SSD_PAGE_WRITE(CALC_FLASH(new_ppn), CALC_BLOCK(new_ppn), CALC_PAGE(new_ppn), i, GC_WRITE, -1);

			old_ppn = victim_phy_flash_nb*PAGES_PER_FLASH + victim_phy_block_nb*PAGE_NB + i;

//			lpn = inverse_page_mapping_table[old_ppn];
#ifdef FTL_MAP_CACHE
			lpn = CACHE_GET_LPN(old_ppn);
#else
			lpn = GET_INVERSE_MAPPING_INFO(old_ppn);
#endif
			UPDATE_NEW_PAGE_MAPPING(lpn, new_ppn);

			copy_page_nb++;
		}
	}

	if(copy_page_nb != b_s_entry->valid_page_nb){
		printf("ERROR[%s] The number of valid page is not correct\n", __FUNCTION__);
		return FAIL;
	}

#ifdef FTL_DEBUG
	printf("[%s] [f: %d, b: %d] Copy Page : %d, total victim : %ld, total empty : %ld \n",__FUNCTION__, victim_phy_flash_nb, victim_phy_block_nb,  copy_page_nb, total_victim_block_nb, total_empty_block_nb);
#endif
	SSD_BLOCK_ERASE(victim_phy_flash_nb, victim_phy_block_nb);
	UPDATE_BLOCK_STATE(victim_phy_flash_nb, victim_phy_block_nb, EMPTY_BLOCK);
	INSERT_EMPTY_BLOCK(victim_phy_flash_nb, victim_phy_block_nb);

	gc_count++;

#ifdef MONITOR_ON
	char szTemp[1024];
	sprintf(szTemp, "GC ");
	WRITE_LOG(szTemp);
	sprintf(szTemp, "WB AMP %d", copy_page_nb);
	WRITE_LOG(szTemp);
#endif

#ifdef FTL_DEBUG
	printf("[%s] Complete\n",__FUNCTION__);
#endif
	return SUCCESS;
}

/* Greedy Garbage Collection Algorithm */
int SELECT_VICTIM_BLOCK(unsigned int* phy_flash_nb, unsigned int* phy_block_nb)
{
	int i, j;
	int entry_nb = 0;

	victim_block_root* curr_v_b_root;
	victim_block_entry* curr_v_b_entry;
	victim_block_entry* victim_block = NULL;

	block_state_entry* b_s_entry;
	int curr_valid_page_nb;

	if(total_victim_block_nb == 0){
		printf("ERROR[%s] There is no victim block\n", __FUNCTION__);
		return FAIL;
	}

	/* if GC_TRIGGER_OVERALL is defined, then */
#ifdef GC_TRIGGER_OVERALL
	curr_v_b_root = (victim_block_root*)victim_block_list;

	for(i=0;i<VICTIM_TABLE_ENTRY_NB;i++){

		if(curr_v_b_root->victim_block_nb != 0){

			entry_nb = curr_v_b_root->victim_block_nb;
			curr_v_b_entry = curr_v_b_root->head;
			if(victim_block == NULL){
				victim_block = curr_v_b_root->head;
				b_s_entry = GET_BLOCK_STATE_ENTRY(victim_block->phy_flash_nb, victim_block->phy_block_nb);
				curr_valid_page_nb = b_s_entry->valid_page_nb;
			}
		}
		else{
			entry_nb = 0;
		}

		for(j=0;j<entry_nb;j++){
			b_s_entry = GET_BLOCK_STATE_ENTRY(curr_v_b_entry->phy_flash_nb, curr_v_b_entry->phy_block_nb);
	
			if(curr_valid_page_nb > b_s_entry->valid_page_nb){
				victim_block = curr_v_b_entry;
				curr_valid_page_nb = b_s_entry->valid_page_nb;
			}
			curr_v_b_entry = curr_v_b_entry->next;
		}
		curr_v_b_root += 1;
	}
#else
	/* if GC_TREGGER_OVERALL is not defined, then */
	curr_v_b_root = (victim_block_root*)victim_block_list + mapping_index;

	if(curr_v_b_root->victim_block_nb != 0){
		entry_nb = curr_v_b_root->victim_block_nb;
		curr_v_b_entry = curr_v_b_root->head;
		if(victim_block == NULL){
			victim_block = curr_v_b_root->head;
			b_s_entry = GET_BLOCK_STATE_ENTRY(curr_v_b_entry->phy_flash_nb, curr_v_b_entry->phy_block_nb);
			curr_valid_page_nb = b_s_entry->valid_page_nb;
		}
	}
	else{
		printf("ERROR[%s] There is no victim entry\n", __FUNCTION__);
	}

	for(i=0;i<entry_nb;i++){

		b_s_entry = GET_BLOCK_STATE_ENTRY(curr_v_b_entry->phy_flash_nb, curr_v_b_entry->phy_block_nb);

		if(curr_valid_page_nb > b_s_entry->valid_page_nb){
			victim_block = curr_v_b_entry;
			curr_valid_page_nb = b_s_entry->valid_page_nb;
		}
		curr_v_b_entry = curr_v_b_entry->next;
	}
#endif
	if(curr_valid_page_nb == PAGE_NB){
		fail_cnt++;
	//	printf(" Fail Count : %d\n", fail_cnt);
		return FAIL;
	}

	*phy_flash_nb = victim_block->phy_flash_nb;
	*phy_block_nb = victim_block->phy_block_nb;
	EJECT_VICTIM_BLOCK(victim_block);

	return SUCCESS;
}
