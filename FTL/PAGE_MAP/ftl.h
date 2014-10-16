// Copyright(c)2013 
//
// Hanyang University, Seoul, Korea
// Embedded Software Systems Lab. All right reserved

#ifndef _FTL_H_
#define _FTL_H_

#include "common.h"

void FTL_INIT(void);
void FTL_TERM(void);

void FTL_READ(int32_t sector_nb, unsigned int length);
void FTL_WRITE(int32_t sector_nb, unsigned int length);

int _FTL_READ(int32_t sector_nb, unsigned int length);
int _FTL_WRITE(int32_t sector_nb, unsigned int length);
#endif
