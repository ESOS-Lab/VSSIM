// File: ftl_inverse_mapping_manager.h
// Date: 2014. 12. 11.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _INVERSE_MAPPING_MANAGER_H_
#define _INVERSE_MAPPING_MANAGER_H_

int32_t GET_PM_INVERSE_MAPPING_INFO(int32_t ppn);
int UPDATE_PM_INVERSE_MAPPING(int32_t ppn, int32_t lpn);

int32_t GET_BM_INVERSE_MAPPING_INFO(int32_t pbn);
int UPDATE_BM_INVERSE_MAPPING(int32_t pbn, int32_t lbn);

#endif
