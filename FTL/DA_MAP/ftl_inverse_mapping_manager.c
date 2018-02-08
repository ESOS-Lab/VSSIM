// File: ftl_inverse_mapping_manager.c
// Date: 2014. 12. 11.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

int32_t GET_PM_INVERSE_MAPPING_INFO(int32_t ppn)
{
	/* Get block state entry from the physical page number */
	int32_t pbn = ppn / PAGE_NB;
	block_state_entry* b_s_entry = GET_BLOCK_STATE_ENTRY(pbn);

	/* If the block state entry has no inverse page table, return fail */
	if(b_s_entry->pm_inv_table == NULL){
		printf("ERROR[%s] This Block state entry does not have page inverse table \n", __FUNCTION__);
		return -1;
	}

	/* Return the logical page number according to the physical page number */
	int32_t offset = ppn % PAGE_NB;
	return b_s_entry->pm_inv_table[offset];
}

int UPDATE_PM_INVERSE_MAPPING(int32_t ppn,  int32_t lpn)
{
	int32_t pbn = ppn / PAGE_NB;

	/* Get block state entry */
	block_state_entry* b_s_entry = GET_BLOCK_STATE_ENTRY(pbn);

	/* If the block state entry has no inverse page table, return fail */
	if(b_s_entry->pm_inv_table == NULL && lpn == -1){
		printf("ERROR[%s] There is no inverse table. pbn: %d\n", __FUNCTION__, pbn);
		return FAIL;
	}
	/* This is first update for the inverse mapping table of this physical block */
	else if(b_s_entry->pm_inv_table == NULL){

		/* Allocate inverse mapping table for this physical block */
		b_s_entry->pm_inv_table = (int32_t*)calloc(PAGE_NB, sizeof(int32_t));
		if(b_s_entry->pm_inv_table == NULL){
			printf("ERROR[%s] Calloc pm inv table fail\n", __FUNCTION__);
			return FAIL;
		}
		else{
			/* Initialize the inverse mapping table as 0 */
			memset(b_s_entry->pm_inv_table, 0, PAGE_NB * sizeof(int32_t));
		}
	}

	/* Update the inverse mapping table entry */
	int32_t offset = ppn % PAGE_NB;
	b_s_entry->pm_inv_table[offset] = lpn;

	return SUCCESS;
}

/* Return logical block number according to the physical block number(pbn) */
int32_t GET_BM_INVERSE_MAPPING_INFO(int32_t pbn)
{
	block_state_entry* b_s_entry = GET_BLOCK_STATE_ENTRY(pbn);

	return b_s_entry->lbn;
}

/* Update inverse block mapping pbn-->lbn */
int UPDATE_BM_INVERSE_MAPPING(int32_t pbn, int32_t lbn)
{
	block_state_entry* b_s_entry = GET_BLOCK_STATE_ENTRY(pbn);
	
	b_s_entry->lbn = lbn;

	return SUCCESS;
}

