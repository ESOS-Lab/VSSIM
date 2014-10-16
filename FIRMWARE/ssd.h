// ssd.h
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _SSD_H_
#define _SSD_H_

#include "hw.h"


//FILE *fp;
void SSD_INIT(void);
void SSD_TERM(void);

void SSD_WRITE(unsigned int length, int32_t sector_nb);
void SSD_READ(unsigned int length, int32_t sector_nb);
void SSD_DSM_TRIM(unsigned int length, void* trim_data);
int SSD_IS_SUPPORT_TRIM(void);

#endif
