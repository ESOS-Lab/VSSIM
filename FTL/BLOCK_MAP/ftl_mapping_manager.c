// File: ftl_mapping_manager.c
// Date: 2018. 02. 08.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2018
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

int32_t* mapping_table;
void* block_table_start;

void INIT_MAPPING_TABLE(void)
{
	/* Allocation Memory for Mapping Table */
	mapping_table = (int32_t*)calloc(BLOCK_MAPPING_ENTRY_NB, sizeof(int32_t));
	if(mapping_table == NULL){
		printf("ERROR[%s] Calloc mapping table fail\n",__FUNCTION__);
		return;
	}

	/* Initialization Mapping Table */
	
	/* If mapping_table.dat file exists */
	FILE* fp = fopen("./data/mapping_table.dat","r");
	if(fp != NULL){
		fread(mapping_table, sizeof(int32_t), BLOCK_MAPPING_ENTRY_NB, fp);
	}
	else{	
		int i;	
		for(i=0;i<BLOCK_MAPPING_ENTRY_NB;i++){
			mapping_table[i] = -1;
		}
	}
}

void TERM_MAPPING_TABLE(void)
{
	FILE* fp = fopen("./data/mapping_table.dat","w");
	if(fp==NULL){
		printf("ERROR[%s] File open fail\n",__FUNCTION__);
		return;
	}

	/* Write the mapping table to file */
	fwrite(mapping_table, sizeof(int32_t), BLOCK_MAPPING_ENTRY_NB,fp);

	/* Free memory for mapping table */
	free(mapping_table);
}

int32_t GET_MAPPING_INFO(int32_t lbn)
{
	return mapping_table[lbn];
}

int GET_NEW_BLOCK(int mode, int mapping_index, int32_t* pbn)
{
	empty_block_entry* curr_e_b_entry;

	curr_e_b_entry = GET_EMPTY_BLOCK(mode, mapping_index);

	/* If the flash memory has no empty block,
                Get empty block from the other flash memories */
        if(mode == VICTIM_INCHIP && curr_e_b_entry == NULL){
                /* Try again */
                curr_e_b_entry = GET_EMPTY_BLOCK(VICTIM_OVERALL, mapping_index);
        }

	if(curr_e_b_entry == NULL){
		printf("ERROR[%s] fail\n",__FUNCTION__);
		return FAIL;
	}

	*pbn = curr_e_b_entry->phy_block_nb;

	free(curr_e_b_entry);

	return SUCCESS;
}

int UPDATE_OLD_BLOCK_MAPPING(int32_t lbn)
{
	int32_t old_pbn;

	old_pbn = GET_MAPPING_INFO(lbn);
	UPDATE_INVERSE_MAPPING(old_pbn, -1);

	return SUCCESS;
}

int UPDATE_NEW_BLOCK_MAPPING(int32_t lbn, int32_t pbn)
{
	/* Update Page Mapping Table */
	mapping_table[lbn] = pbn;

	/* Update Inverse Page Mapping Table */
	UPDATE_INVERSE_MAPPING(pbn, lbn);

	return SUCCESS;
}

int32_t GET_VALID_MAPPING(int32_t lbn, int32_t block_offset)
{
	char page_state;
	int pbn = GET_MAPPING_INFO(lbn);
	if(pbn == -1){
		return -1;
	}

	block_state_entry* b_s_entry = GET_BLOCK_STATE_ENTRY(pbn);
	rp_block_entry* rp_b_entry;
	if(b_s_entry->rp_head != NULL){
		rp_b_entry = b_s_entry->rp_head;
	}

	int i;
	int n_rp_blocks = b_s_entry->rp_count;
	int rp_pbn;
	
	page_state = GET_PAGE_STATE(pbn, block_offset);
	if(page_state == 'V')
		return pbn;
	else{
		for(i=0; i< n_rp_blocks; i++){
			rp_pbn = rp_b_entry->pbn;
			page_state = GET_PAGE_STATE(rp_pbn, block_offset);
			
			if(page_state == 'V'){
				return rp_pbn;
			}
			rp_b_entry = rp_b_entry->next;	
		}	
	}
	return -1;
}

int32_t GET_AVAILABLE_PBN_FROM_RP_TABLE(int32_t pbn, int32_t block_offset)
{

#ifdef FTL_DEBUG
	printf("  [%s] Start. \n", __FUNCTION__);
#endif

	int i, ret;
	int n_rp_blocks;
	char page_state;
	int32_t lbn;
	int32_t new_pbn;
	int32_t new_rp_pbn;
	int32_t pbn_temp;
	int copy_page_nb;
//	int plane_nb = CALC_BLOCK(pbn) % PLANES_PER_FLASH;
//	int mapping_index = plane_nb * FLASH_NB + CALC_FLASH(pbn);

	/* If the original block is available, */
	page_state = GET_PAGE_STATE(pbn, block_offset);
	if(page_state == '0'){
		return pbn;
	}

	/* Get the number of replacement blocks */
	block_state_entry* root_b_s_entry = GET_BLOCK_STATE_ENTRY(pbn);
	n_rp_blocks = root_b_s_entry->rp_count;
	rp_block_entry* rp_b_entry = NULL;

	/* If there is no replacement block, */
	if(n_rp_blocks == 0){

		/* Get New Replacement block, */
//		GET_NEW_BLOCK(VICTIM_INCHIP, mapping_index, &new_rp_pbn);
		GET_NEW_BLOCK(VICTIM_OVERALL, EMPTY_TABLE_ENTRY_NB, &new_rp_pbn);
		UPDATE_BLOCK_STATE(new_rp_pbn, DATA_BLOCK);

		/* Insert the new replacement block to rp block table */	
		INSERT_NEW_RP_BLOCK(pbn, new_rp_pbn);

		if(page_state == 'V')
			UPDATE_BLOCK_STATE_ENTRY(pbn, block_offset, INVALID);

		return new_rp_pbn;
	}
	else{
		rp_b_entry = root_b_s_entry->rp_head;
		if(rp_b_entry == NULL){
			printf("ERROR[%s] There is no rp_b_entry\n", __FUNCTION__);
		}
	
		pbn_temp = pbn;
		while(rp_b_entry != NULL){
			page_state = GET_PAGE_STATE(rp_b_entry->pbn, block_offset);
			if(page_state == '0'){
				UPDATE_BLOCK_STATE_ENTRY(pbn_temp, block_offset, INVALID);
				return rp_b_entry->pbn;
			}
			pbn_temp = rp_b_entry->pbn;
			rp_b_entry = rp_b_entry->next;
		}

		/* If there is no available replacement block, then */
		if(n_rp_blocks == 1){
//			GET_NEW_BLOCK(VICTIM_INCHIP, mapping_index, &new_rp_pbn);
			GET_NEW_BLOCK(VICTIM_OVERALL, EMPTY_TABLE_ENTRY_NB, &new_rp_pbn);

			INSERT_NEW_RP_BLOCK(pbn, new_rp_pbn);
			UPDATE_BLOCK_STATE(new_rp_pbn, DATA_BLOCK);

			UPDATE_BLOCK_STATE_ENTRY(pbn, block_offset, INVALID);

			return new_rp_pbn;
		}
		else if(n_rp_blocks == 2){ // Merge & Return New Blocks

			/* Start Merge Operation */
			lbn = GET_INVERSE_MAPPING_INFO(pbn);

			/* Get the last RP block  */
			rp_b_entry = root_b_s_entry->rp_head->next;
			block_state_entry* rp_b_s_entry = GET_BLOCK_STATE_ENTRY(rp_b_entry->pbn);
			block_state_entry* temp_b_s_entry;
			int valid_page_nb = rp_b_s_entry->valid_page_nb;
			int new_pbn;
			int valid_pbn;

			if(valid_page_nb == PAGE_NB){
				new_pbn = rp_b_entry->pbn;

				/* Update Block State Table */
				temp_b_s_entry = GET_BLOCK_STATE_ENTRY(root_b_s_entry->rp_head->next->pbn);
				temp_b_s_entry->rp_root_pbn = -1;
				temp_b_s_entry = GET_BLOCK_STATE_ENTRY(root_b_s_entry->rp_head->pbn);
				temp_b_s_entry->rp_root_pbn = -1;

				/* Get rid of first rp block */
				SSD_BLOCK_ERASE(CALC_FLASH(root_b_s_entry->rp_head->pbn),
					CALC_BLOCK(root_b_s_entry->rp_head->pbn));
				UPDATE_BLOCK_STATE(root_b_s_entry->rp_head->pbn, EMPTY_BLOCK);
				INSERT_EMPTY_BLOCK(root_b_s_entry->rp_head->pbn);

				/* Update rp block table */
				free(root_b_s_entry->rp_head->next);
				free(root_b_s_entry->rp_head);

				/* Get rid of original block */
				root_b_s_entry->rp_count = 0;
				root_b_s_entry->rp_head = NULL;	
				SSD_BLOCK_ERASE(CALC_FLASH(pbn), CALC_BLOCK(pbn));
				UPDATE_INVERSE_MAPPING(pbn, -1);
				UPDATE_BLOCK_STATE(pbn, EMPTY_BLOCK);
				INSERT_EMPTY_BLOCK(pbn);

				/* Update mapping metadata */
				UPDATE_NEW_BLOCK_MAPPING(lbn, new_pbn);

				/* Get new rp block to new pbn */	
				GET_NEW_BLOCK(VICTIM_OVERALL, EMPTY_TABLE_ENTRY_NB, &new_rp_pbn);
				INSERT_NEW_RP_BLOCK(new_pbn, new_rp_pbn);
				UPDATE_BLOCK_STATE(new_rp_pbn, DATA_BLOCK);
				UPDATE_BLOCK_STATE_ENTRY(new_pbn, block_offset, INVALID);

				return new_rp_pbn;
			}
			else{
				valid_pbn = GET_VALID_MAPPING(lbn, block_offset);
				UPDATE_BLOCK_STATE_ENTRY(valid_pbn, block_offset, INVALID);

	//			GET_NEW_BLOCK(VICTIM_INCHIP, mapping_index, &new_rp_pbn);
				GET_NEW_BLOCK(VICTIM_OVERALL, EMPTY_TABLE_ENTRY_NB, &new_pbn);
				copy_page_nb = MERGE_RP_BLOCKS(pbn, new_pbn);

				UPDATE_OLD_BLOCK_MAPPING(lbn);
				UPDATE_NEW_BLOCK_MAPPING(lbn, new_pbn);
				UPDATE_BLOCK_STATE(new_pbn, DATA_BLOCK);

#ifdef MONITOR_ON
				char szTemp[1024];
				sprintf(szTemp, "GC ");
				WRITE_LOG(szTemp);
				sprintf(szTemp, "WB AMP %d", copy_page_nb);
				WRITE_LOG(szTemp);
#endif

				return new_pbn;
			}
		}
		else{
			printf("ERROR[%s] n_rp_blocks can't be %d\n", __FUNCTION__, n_rp_blocks);
			return -1;
		}
	}
}

void INSERT_NEW_RP_BLOCK(int32_t pbn, int32_t new_rp_pbn)
{
	/* Get Root Replacement Block Table Entry  */
	block_state_entry* root_b_s_entry = GET_BLOCK_STATE_ENTRY(pbn);
	int n_rp_blocks = root_b_s_entry->rp_count;

	/* If Replacement Block Entry is Full, */
	if(n_rp_blocks >= 2){
		printf("ERROR[%s] Cannot insert a new rp blocks, %d\n", __FUNCTION__, n_rp_blocks);
		return;
	}

	/* Create New Entry */
	rp_block_entry* new_rp_b_entry = (rp_block_entry*)calloc(1, sizeof(rp_block_entry));
	new_rp_b_entry->pbn = new_rp_pbn;
	new_rp_b_entry->next = NULL;


	/* Add New entry to the list */
	if(n_rp_blocks == 0){
		root_b_s_entry->rp_head = new_rp_b_entry;		
	}
	else{
		root_b_s_entry->rp_head->next = new_rp_b_entry;
	}

	/* Update Replacement Block Root */
	root_b_s_entry->rp_count++;	// Update Replacement Block Count

	block_state_entry* rp_b_s_entry = GET_BLOCK_STATE_ENTRY(new_rp_pbn);
	rp_b_s_entry->rp_root_pbn = pbn;
	
}

unsigned int CALC_FLASH(int32_t pbn)
{
	unsigned int flash_nb = pbn/BLOCK_NB;

	if(flash_nb >= FLASH_NB){
		printf("ERROR[%s] flash_nb %u\n", __FUNCTION__, flash_nb);
	}
	return flash_nb;
}

unsigned int CALC_BLOCK(int32_t pbn)
{
	unsigned int block_nb = pbn%BLOCK_NB;

	if(block_nb >= BLOCK_NB){
		printf("ERROR[%s] block_nb %u\n", __FUNCTION__, block_nb);
	}
	return block_nb;
}
