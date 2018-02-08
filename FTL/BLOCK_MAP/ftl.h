// File: ftl.h
// Date: 2018. 02. 08.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2018
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _FTL_H_
#define _FTL_H_

#include "common.h"

void FTL_INIT(void);
void FTL_TERM(void);

void FTL_READ(int32_t sector_nb, unsigned int length);
void FTL_WRITE(int32_t sector_nb, unsigned int length);

int _FTL_READ(int32_t sector_nb, unsigned int length);
int _FTL_WRITE(int32_t sector_nb, unsigned int length);

//TEMP
extern int data_block_nb;

#endif
