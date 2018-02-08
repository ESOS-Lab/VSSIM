// File: ftl_pm.h
// Date: 2014. 12. 12.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _FTL_PM_H_
#define _FTL_PM_H_

#include "common.h"

int FTL_PM_READ(int32_t sector_nb, unsigned int length);
int FTL_PM_WRITE(int32_t sector_nb, unsigned int length);
#endif
