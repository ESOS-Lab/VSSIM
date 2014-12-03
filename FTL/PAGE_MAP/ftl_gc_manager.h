// File: ftl_gc_manager.h
// Data: 2014. 12. 03.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _GC_MANAGER_H_
#define _GC_MANAGER_H_

extern unsigned int gc_count;

void GC_CHECK(unsigned int phy_flash_nb, unsigned int phy_block_nb);

int GARBAGE_COLLECTION(void);
int SELECT_VICTIM_BLOCK(unsigned int* phy_flash_nb, unsigned int* phy_block_nb);

#endif
