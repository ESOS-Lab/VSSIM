// File: ftl_pm_mapping_manager.c
// Date: 2014. 12. 11.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved


#include "common.h"

int32_t* pm_mapping_table;
void* pm_block_table;
unsigned int pm_block_table_index;

void INIT_PM_MAPPING_TABLE(int meta_read)
{
	int i;
	/* Allocation Memory for Page Mapping Table */
	pm_mapping_table = (int32_t*)calloc(DA_PAGE_MAPPING_ENTRY_NB, sizeof(int32_t));
	if(pm_mapping_table == NULL){
		printf("ERROR[%s] Calloc page mapping table fail\n", __FUNCTION__);
		return;
	}

	/* Init page block table (pm_block_table) */
	pm_block_entry* curr_p_b_entry;
	pm_block_table = (void*)calloc(EMPTY_TABLE_ENTRY_NB, sizeof(pm_block_entry));
	if(pm_block_table == NULL){
		printf("ERROR[%s] Calloc pm block table fail\n", __FUNCTION__);
		return;
	}

	/* Initialization Mapping Table */
	
	if(meta_read == SUCCESS){
		/* Write the page mapping table to NAND */
		READ_METADATA_FROM_NAND(pm_mapping_table, sizeof(int32_t)*DA_PAGE_MAPPING_ENTRY_NB);
		/* Write the pm block table to NAND */
		READ_METADATA_FROM_NAND(pm_block_table, sizeof(pm_block_entry)*EMPTY_TABLE_ENTRY_NB);
	}
	else{
		/* Init Page Mapping Table */
		for(i=0;i<DA_PAGE_MAPPING_ENTRY_NB;i++){
			pm_mapping_table[i] = -1;
		}

		/* Init pm block table */
		curr_p_b_entry = (pm_block_entry*)pm_block_table;
		for(i=0;i<EMPTY_TABLE_ENTRY_NB;i++){
			curr_p_b_entry->phy_block_nb = 0;
			curr_p_b_entry->curr_phy_page_nb = -1;

			curr_p_b_entry += 1;
		}
		pm_block_table_index = 0;
	}
}

void TERM_PM_MAPPING_TABLE(void)
{

	/* Write the page mapping table to NAND */
	WRITE_METADATA_TO_NAND(pm_mapping_table, sizeof(int32_t)*DA_PAGE_MAPPING_ENTRY_NB);

	/* Write the pm block table to NAND */
	WRITE_METADATA_TO_NAND(pm_block_table, sizeof(pm_block_entry)*EMPTY_TABLE_ENTRY_NB);

	/* Free memory for page mapping table */
	free(pm_mapping_table);

	/* Free memory for pm block table */
	free(pm_block_table);
}

/* Get physical page number from the logical page number (lpn) */
int32_t GET_PM_MAPPING_INFO(int32_t lpn)
{
	return pm_mapping_table[lpn];
}

/* Get new page to write a metadata */
int GET_NEW_PAGE(int mode, int mapping_index, int32_t* ppn)
{
	pm_block_entry* curr_p_b_entry = (pm_block_entry*)pm_block_table + pm_block_table_index;
	int temp_mapping_index = 0;

	/* Get mapping index according to the mode  */
	if(mode == VICTIM_OVERALL){
		temp_mapping_index = pm_block_table_index;
	}
	else if(mode == VICTIM_INCHIP){
		temp_mapping_index = mapping_index;
	}
	else{
		printf("ERROR[%s] Wrong Input mode %d \n", __FUNCTION__, mode);
	}

	/* Access the page block table entry from the mapping index */
	curr_p_b_entry = (pm_block_entry*)pm_block_table + temp_mapping_index;
	empty_block_entry* curr_e_b_entry;

	/* If the page block table entry has no empty page, then */
	if(curr_p_b_entry->curr_phy_page_nb == -1 || curr_p_b_entry->curr_phy_page_nb == PAGE_NB){

		/* Get new empty block from empty block pool */
		curr_e_b_entry = GET_EMPTY_BLOCK(mode, temp_mapping_index);

		/* If the flash memory has no empty block, 
			take new empty block from the other flash memories */
		if(mode == VICTIM_INCHIP && curr_e_b_entry == NULL){
			/* Try again */
			curr_e_b_entry = GET_EMPTY_BLOCK(VICTIM_OVERALL, temp_mapping_index);
		}

		if(curr_e_b_entry == NULL){
			printf("ERROR[%s] There is no empty block\n",__FUNCTION__);
			return FAIL;
		}
		else{
			/* Update the state of the new block as data block */
			UPDATE_BLOCK_STATE(curr_e_b_entry->phy_block_nb, DATA_BLOCK);
		}

		/* Update page block table */
		curr_p_b_entry = (pm_block_entry*)pm_block_table + temp_mapping_index;
		curr_p_b_entry->phy_block_nb = curr_e_b_entry->phy_block_nb;
		curr_p_b_entry->curr_phy_page_nb = 0;
		
		/* Remove the block from empty block table */
		free(curr_e_b_entry);
	}

	/* Calculate physical page number to return */
	*ppn = curr_p_b_entry->phy_block_nb * PAGE_NB + curr_p_b_entry->curr_phy_page_nb;
	curr_p_b_entry->curr_phy_page_nb++;

	/* Update the page block table index */
	if(mode == VICTIM_OVERALL){
		pm_block_table_index++;
		if(pm_block_table_index == EMPTY_TABLE_ENTRY_NB){
			pm_block_table_index = 0;
		}
	}

	return SUCCESS;
}

int UPDATE_OLD_PAGE_MAPPING(int32_t lpn)
{
	int32_t old_ppn;

	/* Get physical page number of the logical page number (lpn) */
	old_ppn = GET_PM_MAPPING_INFO(lpn);

	/* If the logical page number is requested for the first time, then*/
	if(old_ppn == -1){
#ifdef FTL_DEBUG
		printf("\t[%s] New page \n", __FUNCTION__);
#endif
		return SUCCESS;
	}
	else{
		/* Update block state entry and Inverse mapping table */
		UPDATE_BLOCK_STATE_ENTRY(old_ppn/PAGE_NB, old_ppn%PAGE_NB, INVALID);
		UPDATE_PM_INVERSE_MAPPING(old_ppn, -1);
	}

	return SUCCESS;
}

int UPDATE_NEW_PAGE_MAPPING(int32_t lpn, int32_t ppn)
{
#ifdef FTL_DEBUG
	printf("  [%s] Start lpn: %d, ppn: %d \n", __FUNCTION__, lpn, ppn);
#endif

	/* Update Page Mapping Table */
	pm_mapping_table[lpn] = ppn;

	/* Update block state entry */
	UPDATE_BLOCK_STATE_ENTRY(ppn/PAGE_NB, ppn%PAGE_NB, VALID);

	/* Update Inverse Page Mapping Table */
	UPDATE_PM_INVERSE_MAPPING(ppn, lpn);

#ifdef FTL_DEBUG
	printf("  [%s] End \n", __FUNCTION__);
#endif
	return SUCCESS;
}
