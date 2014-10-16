// ftl_mapping_manager.c
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

int32_t* mapping_table;
void* block_table_start;

void INIT_MAPPING_TABLE(void)
{
	/* Allocation Memory for Mapping Table */
	mapping_table = (int32_t*)calloc(PAGE_MAPPING_ENTRY_NB, sizeof(int32_t));
	if(mapping_table == NULL){
		printf("ERROR[%s] Calloc mapping table fail\n", __FUNCTION__);
		return;
	}

	/* Initialization Mapping Table */
	
	/* If mapping_table.dat file exists */
	FILE* fp = fopen("./data/mapping_table.dat","r");
	if(fp != NULL){
		fread(mapping_table, sizeof(int32_t), PAGE_MAPPING_ENTRY_NB, fp);
	}
	else{	
		int i;	
		for(i=0;i<PAGE_MAPPING_ENTRY_NB;i++){
			mapping_table[i] = -1;
		}
	}
}

void TERM_MAPPING_TABLE(void)
{
	FILE* fp = fopen("./data/mapping_table.dat","w");
	if(fp==NULL){
		printf("ERROR[%s] File open fail\n", __FUNCTION__);
		return;
	}

	/* Write the mapping table to file */
	fwrite(mapping_table, sizeof(int32_t),PAGE_MAPPING_ENTRY_NB,fp);

	/* Free memory for mapping table */
	free(mapping_table);
}

int32_t GET_MAPPING_INFO(int32_t lpn)
{
	int32_t ppn = mapping_table[lpn];

	return ppn;
}

int GET_NEW_PAGE(int mode, int mapping_index, int32_t* ppn)
{
	empty_block_entry* curr_empty_block;

	curr_empty_block = GET_EMPTY_BLOCK(mode, mapping_index);

	if(curr_empty_block == NULL){
		printf("ERROR[%s] fail\n", __FUNCTION__);
		return FAIL;
	}

	*ppn = curr_empty_block->phy_flash_nb*BLOCK_NB*PAGE_NB \
	       + curr_empty_block->phy_block_nb*PAGE_NB \
	       + curr_empty_block->curr_phy_page_nb;

	curr_empty_block->curr_phy_page_nb += 1;

	return SUCCESS;
}

int UPDATE_OLD_PAGE_MAPPING(int32_t lpn)
{
	int32_t old_ppn;

#ifdef FTL_MAP_CACHE
	old_ppn = CACHE_GET_PPN(lpn);
#else
	old_ppn = GET_MAPPING_INFO(lpn);
#endif

	if(old_ppn == -1){
#ifdef FTL_DEBUG
		printf("[%s] New page \n", __FUNCTION__);
#endif
		return SUCCESS;
	}
	else{
		UPDATE_BLOCK_STATE_ENTRY(CALC_FLASH(old_ppn), CALC_BLOCK(old_ppn), CALC_PAGE(old_ppn), INVALID);
		UPDATE_INVERSE_MAPPING(old_ppn, -1);
	}

	return SUCCESS;
}

int UPDATE_NEW_PAGE_MAPPING(int32_t lpn, int32_t ppn)
{
	/* Update Page Mapping Table */
#ifdef FTL_MAP_CACHE
	CACHE_UPDATE_PPN(lpn, ppn);
#else
	mapping_table[lpn] = ppn;
#endif

	/* Update Inverse Page Mapping Table */
	UPDATE_BLOCK_STATE_ENTRY(CALC_FLASH(ppn), CALC_BLOCK(ppn), CALC_PAGE(ppn), VALID);
	UPDATE_BLOCK_STATE(CALC_FLASH(ppn), CALC_BLOCK(ppn), DATA_BLOCK);
	UPDATE_INVERSE_MAPPING(ppn, lpn);

	return SUCCESS;
}

unsigned int CALC_FLASH(int32_t ppn)
{
	unsigned int flash_nb = (ppn/PAGE_NB)/BLOCK_NB;

	if(flash_nb >= FLASH_NB){
		printf("ERROR[%s] flash_nb %u\n", __FUNCTION__,flash_nb);
	}
	return flash_nb;
}

unsigned int CALC_BLOCK(int32_t ppn)
{
	unsigned int block_nb = (ppn/PAGE_NB)%BLOCK_NB;

	if(block_nb >= BLOCK_NB){
		printf("ERROR[%s] block_nb %u\n",__FUNCTION__, block_nb);
	}
	return block_nb;
}

unsigned int CALC_PAGE(int32_t ppn)
{
	unsigned int page_nb = ppn%PAGE_NB;

	return page_nb;
}
