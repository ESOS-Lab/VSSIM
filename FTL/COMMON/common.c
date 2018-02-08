// File: common.c
// Date: 18-Dec-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

pbn_t PPN_TO_PBN(ppn_t ppn)
{
	pbn_t pbn;

	pbn.path.flash = ppn.path.flash;
	pbn.path.plane = ppn.path.plane;
	pbn.path.block = ppn.path.block;

	return pbn;
}

ppn_t PBN_TO_PPN(pbn_t pbn, uint32_t page_index)
{
	ppn_t ppn;

	ppn.path.flash = pbn.path.flash;
	ppn.path.plane = pbn.path.plane;
	ppn.path.block = pbn.path.block;
	ppn.path.page = page_index;

	return ppn;
}

int64_t get_usec(void)
{
        int64_t t = 0;
        struct timeval tv;
        struct timezone tz;

        gettimeofday(&tv, &tz);
        t = tv.tv_sec;
        t *= 1000000;
        t += tv.tv_usec;

        return t;
}

uint64_t GET_LINEAR_PPN(ppn_t ppn)
{
	return ppn.path.flash * N_PAGES_PER_FLASH
		+ ppn.path.plane * N_PAGES_PER_PLANE
		+ ppn.path.block * N_PAGES_PER_BLOCK                             
		+ ppn.path.page;
}
