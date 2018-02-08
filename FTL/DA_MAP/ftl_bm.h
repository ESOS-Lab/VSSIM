// File: ftl_bm.h
// Date: 2014. 12. 12.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _FTL_BM_H_
#define _FTL_BM_H_

#include "common.h"

int FTL_BM_READ(int32_t sector_nb, unsigned int length);
int FTL_BM_WRITE(int32_t sector_nb, unsigned int length);

int _FTL_BM_WRITE(int32_t sector_nb, unsigned int length, int32_t dst_pbn, int io_page_nb);
int _FTL_BM_MERGE_WRITE(int32_t old_pbn, int32_t dst_pbn, int start_offset, int end_offset, int block_offset);

int CHECK_EMPTY_PAGES_OF_PBN(int32_t pbn, int32_t sector_nb, unsigned int length);

#endif
