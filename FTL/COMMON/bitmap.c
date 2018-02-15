// File: bitmap.c
// Date: 24-Dec-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

uint32_t bitmap_mask;

bitmap_t CREATE_BITMAP(uint32_t size)
{
	uint32_t nbytes = (size / N_BITS) + 1;
	bitmap_t new_bitmap;
   
	new_bitmap = (bitmap_t)calloc(nbytes, sizeof(uint32_t));
	if (new_bitmap == NULL){
		printf("ERROR[%s] new bitmap allocatetion fail!\n",
				__FUNCTION__);
	}
	
	return new_bitmap;
}

void DESTROY_BITMAP(bitmap_t bitmap)
{
	if (bitmap != NULL) {
		free(bitmap);
	}
}

void CREATE_BITMAP_MASK(void)
{
	int i;
	uint32_t temp_mask = 0;

	for(i=0; i<N_4K_PAGES; i++){
		temp_mask = 1;
		temp_mask = temp_mask << i;

		bitmap_mask |= temp_mask;
	}
}

/**
  * return 1: valid page
  * return 0: invaild page
  */
int TEST_BITMAP_MASK(bitmap_t bitmap, uint32_t index)
{
	if (bitmap == NULL){
		printf("ERROR[%s] bitmap is NULL!\n", __FUNCTION__);
		return FAIL;
	}
	else if(index > N_PAGES_PER_BLOCK){
		printf("ERROR[%s] Exceed the bitmap range, %d\n",
				__FUNCTION__, index);
	}
	
	uint32_t bitmap_index = (index * N_4K_PAGES) / N_BITS;
	uint32_t offset = (index * N_4K_PAGES) % N_BITS;
	uint32_t mask = bitmap_mask << offset;

	uint32_t shift = mask & bitmap[bitmap_index];
	
	return (shift != 0);	
}

int SET_BITMAP_MASK(bitmap_t bitmap, uint32_t index)
{
	if (bitmap == NULL){
		printf("ERROR[%s] bitmap is NULL!\n", __FUNCTION__);
		return FAIL;
	}
	else if(index > N_PAGES_PER_BLOCK){
		printf("ERROR[%s] Exceed the bitmap range, %d\n",
				__FUNCTION__, index);
	}
	
	uint32_t bitmap_index = (index * N_4K_PAGES) / N_BITS;
	uint32_t offset = (index * N_4K_PAGES) % N_BITS;
	
	bitmap[bitmap_index] |= (bitmap_mask << offset);
	
	return SUCCESS;
}

int CLEAR_BITMAP_MASK(bitmap_t bitmap, uint32_t index)
{
	if (bitmap == NULL){
		printf("ERROR[%s] bitmap is NULL!\n", __FUNCTION__);
		return FAIL;
	}
	else if(index > N_PAGES_PER_BLOCK){
		printf("ERROR[%s] Exceed the bitmap range, %d\n",
				__FUNCTION__, index);
	}
	
	uint32_t bitmap_index = (index * N_4K_PAGES) / N_BITS;
	uint32_t offset = (index * N_4K_PAGES) % N_BITS;
	
	bitmap[bitmap_index] &= (~(bitmap_mask << offset));
	
	return SUCCESS;
}

int COPY_BITMAP_MASK(bitmap_t dst_bitmap, uint32_t dst_index, 
			bitmap_t src_bitmap, uint32_t src_index)
{
	if (dst_bitmap == NULL || src_bitmap == NULL){
		printf("ERROR[%s] bitmap is NULL!\n", __FUNCTION__);
		return FAIL;
	}
	else if(dst_index >= N_PAGES_PER_BLOCK || src_index >= N_PAGES_PER_BLOCK){
		printf("ERROR[%s] Exceed the bitmap range: dst_index: %d,\
				src_index: %d\n",
				__FUNCTION__, dst_index, src_index);
	}

	uint32_t bitmap_index = (src_index * N_4K_PAGES) / N_BITS;
	uint32_t offset = (src_index * N_4K_PAGES) % N_BITS;
	uint32_t mask = bitmap_mask << offset;

	uint32_t src_bits = mask & src_bitmap[bitmap_index];
	src_bits = src_bits >> offset;

	bitmap_index = (dst_index * N_4K_PAGES) / N_BITS;
	offset = (dst_index * N_4K_PAGES) % N_BITS;
	
	dst_bitmap[bitmap_index] |= (src_bits << offset);

	return 0;
}

int TEST_BITMAP(bitmap_t bitmap, uint32_t index)
{
	if (bitmap == NULL){
		printf("ERROR[%s] bitmap is NULL!\n", __FUNCTION__);
		return FAIL;
	}
	
	uint32_t bitmap_index = index / N_BITS;
	uint32_t offset = index % N_BITS;
	
	uint32_t shift = (0x1 << offset) & bitmap[bitmap_index];
	
	return (shift != 0);
}

int SET_BITMAP(bitmap_t bitmap, uint32_t index)
{
	if (bitmap == NULL){
		printf("ERROR[%s] bitmap is NULL!\n", __FUNCTION__);
		return FAIL;
	}
	
	uint32_t bitmap_index = index / N_BITS;
	uint32_t offset = index % N_BITS;

	bitmap[bitmap_index] |= (0x1 << offset);
	
	return SUCCESS;
}

int CLEAR_BITMAP(bitmap_t bitmap, uint32_t index)
{
	if (bitmap == NULL){
		printf("ERROR[%s] bitmap is NULL!\n", __FUNCTION__);
		return FAIL;
	}
	
	uint32_t bitmap_index =  index / N_BITS;
	uint32_t offset = index % N_BITS;
	
	bitmap[bitmap_index] &= (~(0x1 << offset));
	
	return SUCCESS;
}
