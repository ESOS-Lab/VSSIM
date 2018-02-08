// File: ftl_bm_mapping_manager.c
// Date: 2014. 12. 11.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

int32_t* bm_mapping_table;

void INIT_BM_MAPPING_TABLE(int meta_read)
{
	/* Allocation Memory for Mapping Table */
	bm_mapping_table = (int32_t*)calloc(DA_BLOCK_MAPPING_ENTRY_NB, sizeof(int32_t));
	if(bm_mapping_table == NULL){
		printf("ERROR[%s] Calloc block mapping table fail\n",__FUNCTION__);
		return;
	}

	/* Initialization Mapping Table */
	if(meta_read == SUCCESS){
		READ_METADATA_FROM_NAND(bm_mapping_table, sizeof(int32_t)*DA_BLOCK_MAPPING_ENTRY_NB);
	}
	else{	
		int i;	
		for(i=0;i<DA_BLOCK_MAPPING_ENTRY_NB;i++){
			bm_mapping_table[i] = -1;
		}
	}
}

void TERM_BM_MAPPING_TABLE(void)
{
	/* Write block mapping table to NAND */
	WRITE_METADATA_TO_NAND(bm_mapping_table, sizeof(int32_t)*DA_BLOCK_MAPPING_ENTRY_NB);

	/* Free memory for mapping table */
	free(bm_mapping_table);
}

/* Get physical block number of the logical block number (lbn) */
int32_t GET_BM_MAPPING_INFO(int32_t lbn)
{
	return bm_mapping_table[lbn];
}

/* Get new empty block */
int GET_NEW_BLOCK(int mode, int mapping_index, int32_t* pbn)
{
	empty_block_entry* curr_e_b_entry;

	/* Get new empty block from empty block pool */
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

	/* Get old physical block number */
	old_pbn = GET_BM_MAPPING_INFO(lbn);

	/* Update the physical block number as invalid*/
	UPDATE_BM_INVERSE_MAPPING(old_pbn, -1);

	return SUCCESS;
}

int UPDATE_NEW_BLOCK_MAPPING(int32_t lbn, int32_t pbn)
{
	/* Update Page Mapping Table */
	bm_mapping_table[lbn] = pbn;

	/* Update Inverse Page Mapping Table */
	UPDATE_BM_INVERSE_MAPPING(pbn, lbn);

	return SUCCESS;
}

/* Get valid physical block number of the logical block number
	with the replacement blocks */
int32_t GET_VALID_MAPPING(int32_t lbn, int32_t block_offset, int32_t* real_offset)
{
#ifdef FTL_DEBUG
	printf("[%s] Start.\n", __FUNCTION__);
#endif

	int page_state = 0;
	int32_t ret_pbn = -1;
	int32_t new_offset = -1;
	block_state_entry* root_b_s_entry;
	block_state_entry* rp_b_s_entry;

	/* Get physical block number of the logical block number */
	int32_t pbn = GET_BM_MAPPING_INFO(lbn);
	if(pbn == -1){
		return ret_pbn;
	}
	else{
		root_b_s_entry = GET_BLOCK_STATE_ENTRY(pbn);
	}

	/* Get real offset of the physical block */
	new_offset = GET_REAL_BLOCK_OFFSET(pbn, block_offset);
	if(new_offset != -1){
		/* Check the validity of the page */
		page_state = GET_BIT(valid_bitmap, pbn * PAGE_NB + new_offset);
	}
	else{
		return -1;
	}

	/* If the root block has valid mapping info,
		return the block number & real offset */
	if(page_state == 1 && new_offset != -1){
		ret_pbn = pbn;
		*real_offset = new_offset;
	}
	else{
		int rp_pbn;
		page_state = 0;

		/* Check the replacement blocks has valid mapping info */
		rp_pbn = root_b_s_entry->rp_block_pbn;
		if(rp_pbn == -1){
			return -1;
		}
		else{
			rp_b_s_entry = GET_BLOCK_STATE_ENTRY(rp_pbn);
		}
		
		/* Get real offset of the physical block */
		new_offset = GET_REAL_BLOCK_OFFSET(rp_pbn, block_offset);
		if(new_offset != -1){
			page_state = GET_BIT(valid_bitmap, rp_pbn * PAGE_NB + new_offset);
		}

		/* If the replacement block has valid mapping info,
			return the block number */
		if(page_state == 1 && new_offset != -1){
			ret_pbn = rp_pbn;
			*real_offset = new_offset;
		}
		else{
			return -1;
		}
	}

#ifdef FTL_DEBUG
	printf("[%s] End. valid pbn: %d, offset: %d\n", __FUNCTION__, ret_pbn, real_offset);
#endif
	/* There is no valid mapping info */
	return ret_pbn;
}

int32_t GET_REAL_BLOCK_OFFSET(int32_t pbn, int32_t block_offset)
{
	int32_t real_offset = -1;

	block_state_entry* b_s_entry = GET_BLOCK_STATE_ENTRY(pbn);
	int32_t start_offset = b_s_entry->start_offset;

	if(start_offset <= block_offset && start_offset != -1){
		real_offset = block_offset - start_offset;
	}
	else if(start_offset == -1){
		printf("ERROR[%s] start offset is not initialized.\n", __FUNCTION__);
	}

	return real_offset;
}

void INSERT_NEW_RP_BLOCK(int32_t pbn, int32_t new_rp_pbn)
{
	/* Get Root Replacement Block Table Entry  */
	block_state_entry* b_s_entry = GET_BLOCK_STATE_ENTRY(pbn);

	/* Update root block state entry */
	b_s_entry->rp_block_pbn = new_rp_pbn;

	/* Update replacement block state entry*/
	b_s_entry = GET_BLOCK_STATE_ENTRY(new_rp_pbn);
	b_s_entry->rp_root_pbn = pbn;
}
