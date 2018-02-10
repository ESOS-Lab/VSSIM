// File: ftl_perf_manager.h
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _PERF_MANAGER_H_
#define _PERF_MANAGER_H_

#define BANDWIDTH_WINDOW	200
#define UPDATE_FREQUENCY	10000

/* FTL PERFORMANCE MODULE */
enum vssim_perf_type{ 
	UPDATE_START_TIME,
	UPDATE_END_TIME,
	UPDATE_GC_START_TIME,
	UPDATE_GC_END_TIME,
	LOG_READ_PAGE,
	LOG_WRITE_PAGE,
	LOG_GC_AMP,
	LOG_ERASE_BLOCK,
};

double GET_IO_BANDWIDTH(uint64_t n_pages, uint64_t latency);

int INIT_PERF_CHECKER(void);
void TERM_PERF_CHECKER(void);

void INSERT_IO_BANDWIDTH_INFO(int io_type, uint32_t n_pages, int64_t latency);

void UPDATE_LOG(int log_type, int64_t arg);
void UPDATE_BW(int io_type, uint32_t n_pages, int64_t latency);
void SEND_LOG_TO_MONITOR(void);

#endif
