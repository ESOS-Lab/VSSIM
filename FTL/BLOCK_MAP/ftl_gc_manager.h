// Copyright(c)2013 
//
// Hanyang University, Seoul, Korea
// Embedded Software Systems Lab. All right reserved

#ifndef _GC_MANAGER_H_
#define _GC_MANAGER_H_

extern unsigned int gc_count;

void GC_CHECK(void);

int GARBAGE_COLLECTION(void);
int BM_GARBAGE_COLLECTION(int32_t victim_pbn);
int32_t SELECT_VICTIM_BLOCK(void);

int COPY_VALID_PAGES(int32_t old_pbn, int32_t new_pbn);
int MERGE_RP_BLOCKS(int32_t old_pbn, int32_t new_pbn);

#endif
