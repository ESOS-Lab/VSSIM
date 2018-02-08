// File: vssim_type.h
// Date: 18-Dec-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _VSSIM_TYPE_H_
#define _VSSIM_TYPE_H_

#include <stdint.h>

typedef uint32_t* bitmap_t;

typedef union __attribute__ ((__packed__)) physical_block_number{

	int32_t	addr;

	struct{
		int8_t		flash;
		int8_t		plane;
		int16_t		block;
	}path;
}pbn_t;

typedef union __attribute__ ((__packed__)) physical_page_number{

	int64_t			addr;

	struct{
		int8_t		flash;
		int8_t		plane;
		int16_t		block;
		int32_t		page;
	}path;
}ppn_t;

#endif // end of 'ifndef _VSSIM_TYPE_H_'
