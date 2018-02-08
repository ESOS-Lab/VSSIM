// Copyright(c)2014
//
// Hanyang University, Seoul, Korea
// Embedded Software Systems Lab. All right reserved

#ifndef _BM_GC_MANAGER_H_
#define _BM_GC_MANAGER_H_

extern unsigned int gc_count;

void GC_CHECK(void);

int GARBAGE_COLLECTION(void);
int PM_GARBAGE_COLLECTION(int32_t victim_pbn);
int BM_GARBAGE_COLLECTION(int32_t victim_pbn);

int CHECK_PARTIAL_MERGE(int32_t src_pbn, int32_t dst_pbn);

int32_t SELECT_VICTIM_BLOCK(void);
int COPY_VALID_PAGES(int32_t old_pbn, int32_t new_pbn);

int PM_BLOCK_TABLE_SCAN(int32_t victim_pbn);

#endif
