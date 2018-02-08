// Copyright(c)2013 
//
// Hanyang University, Seoul, Korea
// Embedded Software Systems Lab. All right reserved

#ifndef _MAPPING_MANAGER_H_
#define _MAPPING_MANAGER_H_

extern int32_t* mapping_table;
extern void* block_table_start;

extern unsigned int flash_index;
extern unsigned int* plane_index;
extern unsigned int* block_index;

void INIT_MAPPING_TABLE(void);
void TERM_MAPPING_TABLE(void);

int32_t GET_MAPPING_INFO(int32_t lbn);
int GET_NEW_BLOCK(int mode, int mapping_index, int32_t* pbn);

int UPDATE_OLD_BLOCK_MAPPING(int32_t lbn);
int UPDATE_NEW_BLOCK_MAPPING(int32_t lbn, int32_t pbn);

/* Functions for Replacement Block */
int32_t GET_VALID_MAPPING(int32_t lbn, int32_t block_offset);
int32_t GET_AVAILABLE_PBN_FROM_RP_TABLE(int32_t pbn, int32_t block_offset);
void INSERT_NEW_RP_BLOCK(int32_t pbn, int32_t new_rp_pbn);

unsigned int CALC_FLASH(int32_t pbn);
unsigned int CALC_BLOCK(int32_t pbn);
unsigned int CALC_PAGE(int32_t pbn);

#endif
