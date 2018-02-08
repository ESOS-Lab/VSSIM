// File: ftl_cache.h
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _VSSIM_CACHE_H_
#define _VSSIM_CACHE_H_

#define CACHE_MAP_SIZE	(PASE_SIZE * CACHE_IDX_SIZE)

#define MAP	0
#define INV_MAP	1

extern struct map_data* cache_map_data_start;
extern struct map_data* cache_map_data_end;
extern uint32_t cache_map_data_nb; 

typedef struct map_data
{
	uint32_t ppn;
	void* data;
	struct map_data* prev;
	struct map_data* next;
}map_data;

typedef struct cache_idx_entry
{
	uint32_t map_num	:29;
	uint32_t clock_bit	:1;
	uint32_t map_type	:1;	// map (0), inv_map(1)
	uint32_t update_bit	:1;
	void* data;
}cache_idx_entry;

typedef struct map_state_entry
{
	int32_t ppn;
	uint32_t is_cached; // cached (1) not cached(0)
	cache_idx_entry* cache_entry;
}map_state_entry;

void INIT_CACHE(void);

int32_t CACHE_GET_PPN(int32_t lpn);
int32_t CACHE_GET_LPN(int32_t ppn);
int CACHE_UPDATE_PPN(int32_t lpn, int32_t ppn);
int CACHE_UPDATE_LPN(int32_t lpn, int32_t ppn);

cache_idx_entry* CACHE_GET_MAP(uint32_t map_index, uint32_t map_type, int32_t* map_data);
cache_idx_entry* LOOKUP_CACHE(uint32_t map_index, uint32_t map_type);
cache_idx_entry* CACHE_INSERT_MAP(uint32_t map_index, uint32_t map_type, uint32_t victim_index);
uint32_t CACHE_EVICT_MAP(void); 
uint32_t CACHE_SELECT_VICTIM(void);

int WRITE_MAP(uint32_t page_nb, void* buf);
void* READ_MAP(uint32_t page_nb);
map_data* LOOKUP_MAP_DATA_ENTRY(uint32_t page_nb);
int REARRANGE_MAP_DATA_ENTRY(struct map_data* new_entry);
#endif
