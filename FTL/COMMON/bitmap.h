// File: bitmap.h
// Date: 24-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _BITMAP_H_
#define _BITMAP_H_

#define N_BITS		32

extern uint32_t bitmap_mask;

bitmap_t CREATE_BITMAP(uint32_t size);
void DESTROY_BITMAP(bitmap_t bitmap);
void CREATE_BITMAP_MASK(void);

int TEST_BITMAP_MASK(bitmap_t bitmap, uint32_t index);
int SET_BITMAP_MASK(bitmap_t bitmap, uint32_t index);
int CLEAR_BITMAP_MASK(bitmap_t bitmap, uint32_t index);
int COPY_BITMAP_MASK(bitmap_t dst_bitmap, uint32_t dst_index, 
			bitmap_t src_bitmap, uint32_t src_index);

int TEST_BITMAP(bitmap_t bitmap, uint32_t index);
int SET_BITMAP(bitmap_t bitmap, uint32_t index);
int CLEAR_BITMAP(bitmap_t, uint32_t index);

#endif
