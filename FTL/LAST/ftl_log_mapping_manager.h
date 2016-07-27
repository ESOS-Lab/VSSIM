// File: ftl_log_mapping_manager.h
// Data: 2013. 05. 06.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2013
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _LOG_MAPPING_MANAGER_H_
#define _LOG_MAPPING_MANAGER_H_

typedef struct seq_block_mapping_entry
{
	unsigned int phy_flash_nb;
	unsigned int phy_block_nb;
}seq_block_mapping_entry;

typedef struct ran_block_mapping_entry
{
	unsigned int phy_flash_nb;
	unsigned int phy_block_nb;
}ran_block_mapping_entry;

typedef struct seq_log_mapping_entry
{
	unsigned int start_page_nb;
	unsigned int page_length;

	unsigned int data_flash_nb;	// physical flash number
	unsigned int data_block_nb;	// physical block number

	unsigned int log_flash_nb;	// logical flash number
	unsigned int log_block_nb;	// logical block number
}seq_log_mapping_entry;

typedef struct ran_log_mapping_entry
{
	unsigned int data_flash_nb;
	unsigned int data_block_nb;

	unsigned int log_flash_nb;
	unsigned int log_block_nb;
	unsigned int log_page_nb;

	unsigned int valid;
}ran_log_mapping_entry;

typedef struct hot_page_entry
{
	int32_t hot_page_nb;
	struct hot_page_entry* next;
}hot_page_entry;

void INIT_SEQ_BLOCK_MAPPING(int caller);
void INIT_RAN_COLD_BLOCK_MAPPING(int caller);
void INIT_RAN_HOT_BLOCK_MAPPING(int caller);
void INIT_LOG_BLOCK_INDEX(void);
void INIT_HOT_PAGE_LIST(void);

void INIT_SEQ_LOG_MAPPING(int caller);
void INIT_RAN_COLD_LOG_MAPPING(int caller);
void INIT_RAN_HOT_LOG_MAPPING(int caller);

void TERM_SEQ_BLOCK_MAPPING(void);
void TERM_RAN_COLD_BLOCK_MAPPING(void);
void TERM_RAN_HOT_BLOCK_MAPPING(void);
void TERM_LOG_BLOCK_INDEX(void);
void TERM_HOT_PAGE_LIST(void);

void TERM_SEQ_LOG_MAPPING(void);
void TERM_RAN_COLD_LOG_MAPPING(void);
void TERM_RAN_HOT_LOG_MAPPING(void);

int SET_WRITE_TYPE(int32_t sector_nb, unsigned int length);
int FIND_IN_HOT_PAGE_LIST(int32_t sector_nb);
void INSERT_HOT_PAGE(int32_t log_page_nb);

seq_block_mapping_entry* GET_SEQ_BLOCK_MAPPING_ENTRY(int seq_mapping_index);
seq_log_mapping_entry* GET_SEQ_LOG_MAPPING_ENTRY(int seq_mapping_index);

ran_block_mapping_entry* GET_RAN_BLOCK_MAPPING_ENTRY(int ran_mapping_index, int type);
ran_log_mapping_entry* GET_RAN_LOG_MAPPING_ENTRY(int ran_mapping_index, int type);

int WRITE_TO_SEQ_BLOCK(int32_t sector_nb, unsigned int length, int write_type);
int WRITE_TO_RAN_COLD_BLOCK(int32_t sector_nb, unsigned int length);
int WRITE_TO_RAN_HOT_BLOCK(int32_t sector_nb, unsigned int length);

int MERGE_SEQ_BLOCK(void);
int MERGE_RAN_COLD_BLOCK(void);
int MERGE_RAN_HOT_BLOCK(void);

int UPDATE_RAN_LOG_MAPPING_VALID(unsigned int log_flash_nb, unsigned int log_block_nb, unsigned int log_page_nb, int ran_mapping_index, int type);

int FIND_PAGE_IN_SEQ_LOG(int32_t sector_nb, unsigned int* phy_flash_nb, unsigned int* phy_block_nb, unsigned int* phy_page_nb);
int FIND_PAGE_IN_RAN_COLD_LOG(int32_t sector_nb, unsigned int* phy_flash_nb, unsigned int* phy_block_nb, unsigned int* phy_page_nb);
int FIND_PAGE_IN_RAN_HOT_LOG(int32_t sector_nb, unsigned int* phy_flash_nb, unsigned int* phy_block_nb, unsigned int* phy_page_nb);
#endif
