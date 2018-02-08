// File: ftl_block_manager.h
// Date: 2014. 12. 11.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _BLOCK_MANAGER_H_
#define _BLOCK_MANAGER_H_

extern void* empty_block_list;
extern void* block_state_table;
extern int64_t total_empty_block_nb;

typedef struct block_state_entry
{
	int32_t lbn;		// inverse address for block mapping (-1 for replacement block)
	int32_t* pm_inv_table;	// inverse address table for page mapping
	int32_t type;		// block type

	int32_t start_offset;
	int32_t write_limit;
	int32_t valid_page_nb;	// the number of valid pages

	int32_t rp_root_pbn;
	int32_t rp_block_pbn;
}block_state_entry;

typedef struct empty_block_root
{
	struct empty_block_entry* head;
	struct empty_block_entry* tail;
	unsigned int empty_block_nb;
}empty_block_root;

typedef struct empty_block_entry
{
	unsigned int phy_block_nb;
	struct empty_block_entry* next;
}empty_block_entry;

void INIT_BLOCK_STATE_TABLE(int meta_read);
void INIT_EMPTY_BLOCK_LIST(int meta_read);
void INIT_PM_INVERSE_TABLE(int meta_read);
void INIT_VALID_ARRAY(int meta_read);

void TERM_BLOCK_STATE_TABLE(void);
void TERM_EMPTY_BLOCK_LIST(void);
void TERM_PM_INVERSE_TABLE(void);
void TERM_VALID_ARRAY(void);

int IS_META_BLOCK(unsigned int pbn);
empty_block_entry* GET_EMPTY_BLOCK(int mode, int mapping_index);
int INSERT_EMPTY_BLOCK(int32_t new_empty_pbn);

block_state_entry* GET_BLOCK_STATE_ENTRY(int32_t pbn);

int UPDATE_BLOCK_STATE(int32_t pbn, int block_type);
int UPDATE_BLOCK_STATE_ENTRY(int32_t pbn, int32_t block_offset, int valid);
void UPDATE_MULTIPLE_BLOCK_STATE_ENTRIES(int32_t sector_nb, unsigned int length, int32_t pbn, int valid);

#endif
