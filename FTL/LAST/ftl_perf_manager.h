// File: ftl_perf_manager.h
// Date: 2018. 02. 08.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2018
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _PERF_MANAGER_H_
#define _PERF_MANAGER_H_

void INIT_PERF_CHECKER(void);
void TERM_PERF_CHECKER(void);

void PERF_CHECKER(int op_type, int64_t op_delay);

#endif
