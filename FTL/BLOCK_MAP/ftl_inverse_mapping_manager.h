// Copyright(c)2013 
//
// Hanyang University, Seoul, Korea
// Embedded Software Systems Lab. All right reserved

#ifndef _INVERSE_MAPPING_MANAGER_H_
#define _INVERSE_MAPPING_MANAGER_H_

extern int32_t* inverse_mapping_table;
extern void* block_state_table;
extern void* empty_block_list;

extern int64_t total_empty_block_nb;
extern unsigned int empty_block_list_index;

typedef struct rp_block_entry
{
	int32_t pbn;
	struct rp_block_entry* next;
}rp_block_entry;

typedef struct block_state_entry
{
	int valid_page_nb;
	int type;
	unsigned int erase_count;
	char* valid_array;

	int rp_count;
	int32_t rp_root_pbn;
	rp_block_entry* rp_head;
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

void INIT_INVERSE_MAPPING_TABLE(void);
void INIT_BLOCK_STATE_TABLE(void);
void INIT_EMPTY_BLOCK_LIST(void);
void INIT_VALID_ARRAY(void);

void TERM_INVERSE_MAPPING_TABLE(void);
void TERM_BLOCK_STATE_TABLE(void);
void TERM_EMPTY_BLOCK_LIST(void);
void TERM_VALID_ARRAY(void);

empty_block_entry* GET_EMPTY_BLOCK(int mode, int mapping_index);
int INSERT_EMPTY_BLOCK(int32_t new_empty_pbn);

char GET_PAGE_STATE(int32_t pbn, int32_t block_offset);
block_state_entry* GET_BLOCK_STATE_ENTRY(int32_t pbn);

int32_t GET_INVERSE_MAPPING_INFO(int32_t lbn);
int UPDATE_INVERSE_MAPPING(int32_t pbn, int32_t lbn);
int UPDATE_BLOCK_STATE(int32_t pbn, int block_type);
int UPDATE_BLOCK_STATE_ENTRY(int32_t pbn, int32_t block_offset, int valid);
void PRINT_VALID_ARRAY(int32_t pbn);

#endif
