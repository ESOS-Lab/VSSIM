// File: ftl_mapping_manager.c
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

ppn_t** mapping_table;

int INIT_MAPPING_TABLE(int init_info)
{
	int i;
	int ret;

	int64_t n_total_pages;

	/* Allocation Memory for Mapping Table */
	mapping_table = (ppn_t**)calloc(sizeof(ppn_t*), N_IO_CORES);
	for(i=0; i<N_IO_CORES; i++){

		n_total_pages = vs_core[i].n_total_pages;

		mapping_table[i] = (ppn_t*)calloc(sizeof(ppn_t),
				n_total_pages);
		if(mapping_table[i] == NULL){
			printf("ERROR[%s] Calloc mapping table fail\n", __FUNCTION__);
			return -1;
		}
	}	

	/* Initialization Mapping Table */
	
	/* If mapping_table.dat file exists */
	if(init_info == 1){
		FILE* fp = fopen("./META/mapping_table.dat","r");
		if(fp != NULL){
			for(i=0; i<N_IO_CORES; i++){

				n_total_pages = vs_core[i].n_total_pages;

				ret = fread(mapping_table[i], sizeof(ppn_t), 
						n_total_pages, fp);
				if(ret == -1){
					printf("ERROR[%s] Read mapping table fail!\n", __FUNCTION__);
					return -1;
				}
			}

			return 1;
		}
		else{
			printf("ERROR[%s] fail to read mapping table from file!\n", __FUNCTION__);
			return -1;
		}
	}
	else{	
		int j;
		for(i=0; i<N_IO_CORES; i++){	
			
			n_total_pages = vs_core[i].n_total_pages;

			for(j=0; j<n_total_pages; j++){
				mapping_table[i][j].addr = -1;
			}
		}

		return 0;
	}
}

void TERM_MAPPING_TABLE(void)
{
	int i;
	int64_t n_total_pages;

	FILE* fp = fopen("./META/mapping_table.dat","w");
	if(fp == NULL){
		printf("ERROR[%s] File open fail\n", __FUNCTION__);
		return;
	}

	/* Write the mapping table to file */
	for(i=0; i<N_IO_CORES; i++){
		n_total_pages = vs_core[i].n_total_pages;
		fwrite(mapping_table[i], sizeof(ppn_t),
			n_total_pages, fp);
	}

	/* Free memory for mapping table */
	for(i=0; i<N_IO_CORES; i++){
		free(mapping_table[i]);
	}
	free(mapping_table);

	fclose(fp);
}

ppn_t GET_MAPPING_INFO(int core_id, int64_t lpn)
{
	ppn_t ppn = mapping_table[core_id][lpn];

	return ppn;
}

int GET_NEW_PAGE(int core_id, pbn_t index, int mode, ppn_t* ppn)
{
	block_entry* empty_block;

	/* Get empty block from the flash list of the core */
	empty_block = GET_EMPTY_BLOCK(core_id, index, mode);
	if(empty_block == NULL){
		printf("ERROR[%s] Get empty block fail\n", __FUNCTION__);
		return FAIL;
	}

	/* Calculate the ppn from the empty block */
	*ppn = PBN_TO_PPN(empty_block->pbn, empty_block->w_index);

	/* Increase current write index of the empty block */
	empty_block->w_index++;

	/* If the empty block is full, insert it to victim block list */
	if(empty_block->w_index == N_PAGES_PER_BLOCK){
		POP_EMPTY_BLOCK(core_id, empty_block);
		INSERT_VICTIM_BLOCK(core_id, empty_block);

		/* Check whether the plane need to GC */
		CHECK_EMPTY_BLOCKS(core_id, empty_block->pbn);
	}

	return SUCCESS;
}

int UPDATE_OLD_PAGE_MAPPING(int core_id, int64_t lpn)
{
	ppn_t old_ppn;
	pbn_t old_pbn;

	old_ppn = GET_MAPPING_INFO(core_id, lpn);

	if(old_ppn.addr == -1){
#ifdef FTL_DEBUG
		printf("[%s] New page \n", __FUNCTION__);
#endif
		return SUCCESS;
	}
	else{
		old_pbn = PPN_TO_PBN(old_ppn);

		UPDATE_BLOCK_STATE_ENTRY(old_pbn, old_ppn.path.page, INVALID);
		UPDATE_INVERSE_MAPPING(old_ppn, -1);
	}

	return SUCCESS;
}

int UPDATE_NEW_PAGE_MAPPING(int core_id, int64_t lpn, ppn_t ppn)
{
	pbn_t pbn;

	/* Update Page Mapping Table */
#ifdef FTL_MAP_CACHE
	CACHE_UPDATE_PPN(lpn, ppn);
#else
	mapping_table[core_id][lpn] = ppn;
#endif

	/* Update Inverse Page Mapping Table */
	pbn = PPN_TO_PBN(ppn);

	UPDATE_BLOCK_STATE_ENTRY(pbn, ppn.path.page, VALID);
	UPDATE_BLOCK_STATE(pbn, DATA_BLOCK);
	UPDATE_INVERSE_MAPPING(ppn, lpn);

	return SUCCESS;
}

int PARTIAL_UPDATE_PAGE_MAPPING(int core_id, int64_t lpn, ppn_t new_ppn,
		ppn_t old_ppn, uint32_t left_skip, uint32_t right_skip)
{
	uint32_t offset = left_skip / SECTORS_PER_4K_PAGE;
	uint32_t length = SECTORS_PER_PAGE - left_skip - right_skip;

	pbn_t dst_pbn = PPN_TO_PBN(new_ppn);
	pbn_t src_pbn = PPN_TO_PBN(old_ppn);

	block_state_entry* dst_bs_entry = GET_BLOCK_STATE_ENTRY(dst_pbn);	
	block_state_entry* src_bs_entry = GET_BLOCK_STATE_ENTRY(src_pbn);

	uint32_t dst_index = new_ppn.path.page;  
	uint32_t src_index = old_ppn.path.page;

	/* Copy bitmap info from src page to dst page */
	COPY_BITMAP_MASK(dst_bs_entry->valid_array, dst_index,
			src_bs_entry->valid_array, src_index);

	/* Validate sectors from left_skip to rigth skip */
	while(length > 0){
		SET_BITMAP(dst_bs_entry->valid_array, dst_index * N_4K_PAGES + offset);

		length -= (SECTORS_PER_4K_PAGE - left_skip);
		left_skip = 0;
		offset++;
	}

	/* Invalidate old ppn */
	UPDATE_OLD_PAGE_MAPPING(core_id, lpn);

	/* Update Mapping Table */
	mapping_table[core_id][lpn] = new_ppn;
	UPDATE_INVERSE_MAPPING(new_ppn, lpn);

	/* Update the number of valid pages */
	dst_bs_entry->n_valid_pages++;

	return SUCCESS;
}
