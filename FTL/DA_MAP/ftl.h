// File: ftl.h
// Date: 2014. 12. 10.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _FTL_H_
#define _FTL_H_

#include "common.h"
extern int g_init;
extern int64_t bm_start_sect_nb;

void FTL_INIT(void);
void FTL_TERM(void);

void FTL_READ(int32_t sector_nb, unsigned int length);
void FTL_WRITE(int32_t sector_nb, unsigned int length);

unsigned int CALC_FLASH_FROM_PPN(int32_t ppn);
unsigned int CALC_BLOCK_FROM_PPN(int32_t ppn);
unsigned int CALC_PAGE_FROM_PPN(int32_t ppn);

unsigned int CALC_FLASH_FROM_PBN(int32_t pbn);
unsigned int CALC_BLOCK_FROM_PBN(int32_t pbn);

int CALC_IO_PAGE_NB(int32_t sector_nb, unsigned int length);
#endif
