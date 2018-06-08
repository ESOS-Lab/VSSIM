// File: ftl_mapping_manager.h
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _MAPPING_MANAGER_H_
#define _MAPPING_MANAGER_H_

extern ppn_t** mapping_table;

int INIT_MAPPING_TABLE(int init_info);
void TERM_MAPPING_TABLE(void);

ppn_t GET_MAPPING_INFO(int core_id, int64_t lpn);
int GET_NEW_PAGE(int core_id, pbn_t index, int mode, ppn_t* ppn, int for_gc);

int UPDATE_OLD_PAGE_MAPPING(int core_id, int owner_core_id, int64_t lpn);
int UPDATE_NEW_PAGE_MAPPING(int core_id, int64_t lpn, ppn_t ppn);

int PARTIAL_UPDATE_PAGE_MAPPING(int core_id, int owner_core_id, int64_t lpn, ppn_t new_ppn,
		ppn_t old_ppn, uint32_t left_skip, uint32_t right_skip);
#endif
