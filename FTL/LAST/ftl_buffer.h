#ifndef _FTL_BUFFER_H_
#define _FTL_BUFFER_H_

int FTL_BUFFER(int type, int64_t sector_nb, unsigned int length);
void FTL_BUFFER_FLUSH(void);
void InsertNode(int type, int64_t sector_nb, unsigned int length);

typedef struct st_ftl_buff{
	int type;
	int64_t sector_nb;
	unsigned int length;
	struct st_ftl_buff* next;
}ftl_buff;

//flt_buff* g_pHead = NULL;
//ftl_buff* g_pTail = NULL;

#endif
