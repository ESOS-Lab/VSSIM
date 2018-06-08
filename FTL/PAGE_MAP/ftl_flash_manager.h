// File: ftl_inverse_mapping_manager.h
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _INVERSE_MAPPING_MANAGER_H_
#define _INVERSE_MAPPING_MANAGER_H_

#include "vssim_type.h"

extern int64_t* n_total_empty_blocks;
extern int64_t* n_total_victim_blocks;

typedef struct block_state_entry
{
	uint32_t n_valid_pages;
	int type;
	int core_id;		// this field indicates the core
				// who performs gc for this bs entry
	uint32_t erase_count;
	bitmap_t valid_array;
	pthread_mutex_t lock;
}block_state_entry;

typedef struct block_entry
{
	pbn_t pbn;
	uint32_t w_index;
	struct block_entry* prev;
	struct block_entry* next;

}block_entry;

typedef struct block_list
{
	block_entry* head;
	block_entry* tail;
	uint32_t n_blocks;

}block_list;

enum plane_state{
	IDLE,
	NEED_BGGC,
	NEED_FGGC,
	ON_GC,
	IO
};

typedef struct plane_info
{
	int64_t* inverse_mapping_table;
	void* block_state_table;
	enum plane_state p_state;
	pthread_mutex_t state_lock;	
	pthread_mutex_t gc_lock;
	block_list empty_list;
	block_list victim_list;
	struct flash_info* flash_i;
}plane_info;

typedef struct flash_info
{
	int core_id;
	int flash_nb;
	int local_flash_nb;
	plane_info* plane_i;
	struct flash_info* next_flash;
	uint16_t plane_index;
	uint32_t n_empty_blocks;
	pthread_mutex_t gc_lock;
}flash_info;

extern flash_info* flash_i;

int INIT_FLASH_INFO(int init_info);
int INIT_PLANE_INFO(int init_info);
int INIT_INVERSE_MAPPING_TABLE(int init_info);
int INIT_BLOCK_STATE_TABLE(int init_info);
int INIT_EMPTY_BLOCK_LIST(int init_info);
int INIT_VICTIM_BLOCK_LIST(int init_info);
int INIT_VALID_ARRAY(int init_info);

void TERM_FLASH_INFO(void);
void TERM_PLANE_INFO(void);
void TERM_INVERSE_MAPPING_TABLE(void);
void TERM_BLOCK_STATE_TABLE(void);
void TERM_EMPTY_BLOCK_LIST(void);
void TERM_VICTIM_BLOCK_LIST(void);
void TERM_VALID_ARRAY(void);

/* Manipulate empty, victim list */
block_entry* GET_EMPTY_BLOCK(int core_id, pbn_t index, int mode);
int INSERT_EMPTY_BLOCK(int core_id, block_entry* new_empty_block);
int INSERT_VICTIM_BLOCK(int core_id, block_entry* new_victim_block);
int POP_EMPTY_BLOCK(int core_id, block_entry* empty_block);
int POP_VICTIM_BLOCK(int core_id, block_entry* victim_block);

/* Manipulate inverse mapping table */
int64_t GET_INVERSE_MAPPING_INFO(ppn_t ppn);
int UPDATE_INVERSE_MAPPING(ppn_t ppn, int64_t lpn);

/* Get and Update the block state */
block_state_entry* GET_BLOCK_STATE_ENTRY(pbn_t pbn);
int UPDATE_BLOCK_STATE(int core_id, pbn_t pbn, int type);
int UPDATE_BLOCK_STATE_ENTRY(int core_id, pbn_t pbn, int32_t offset, int valid);
int COUNT_BLOCK_STATE_ENTRY(pbn_t pbn);

/* Check, Get, and Update the state of the Flash or the Plane */
int IS_AVAILABLE_FLASH(flash_info* flash_i);
int IS_AVAILABLE_PLANE(plane_info* plane_i);
int GET_FLASH_STATE(flash_info* flash_i);
int GET_PLANE_STATE(plane_info* plane_i);
void UPDATE_FLASH_STATE(int core_id, flash_info* flash_i, int state);
void UPDATE_PLANE_STATE(int core_id, plane_info* plane_i, int state);

#endif
