// File: ftl_pm_mapping_manager.h
// Date: 2014. 12. 12.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _PM_MAPPING_MANAGER_H_
#define _PM_MAPPING_MANAGER_H_

extern int32_t* pm_mapping_table;
extern void* pm_block_table;

typedef struct pm_block_entry
{
	unsigned int phy_block_nb;
	int curr_phy_page_nb;
}pm_block_entry;

void INIT_PM_MAPPING_TABLE(int meta_read);
void TERM_PM_MAPPING_TABLE(void);

int32_t GET_PM_MAPPING_INFO(int32_t lpn);
int GET_NEW_PAGE(int mode, int mapping_index, int32_t* ppn);

int UPDATE_OLD_PAGE_MAPPING(int32_t lpn);
int UPDATE_NEW_PAGE_MAPPING(int32_t lpn, int32_t ppn);

#endif
