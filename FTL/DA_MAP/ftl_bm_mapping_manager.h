// File: ftl_bm_mapping_manager.h
// Date: 2014. 12. 12.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _BM_MAPPING_MANAGER_H_
#define _BM_MAPPING_MANAGER_H_

extern int32_t* bm_mapping_table;

extern unsigned int flash_index;
extern unsigned int* plane_index;
extern unsigned int* block_index;

void INIT_BM_MAPPING_TABLE(int meta_read);
void TERM_BM_MAPPING_TABLE(void);

int32_t GET_BM_MAPPING_INFO(int32_t lbn);
int GET_NEW_BLOCK(int mode, int mapping_index, int32_t* pbn);

int UPDATE_OLD_BLOCK_MAPPING(int32_t lbn);
int UPDATE_NEW_BLOCK_MAPPING(int32_t lbn, int32_t pbn);

/* Functions for Replacement Block */
int32_t GET_VALID_MAPPING(int32_t lbn, int32_t block_offset, int32_t* real_offset);
int32_t GET_REAL_BLOCK_OFFSET(int32_t pbn, int32_t block_offset);
void INSERT_NEW_RP_BLOCK(int32_t pbn, int32_t new_rp_pbn);

#endif
