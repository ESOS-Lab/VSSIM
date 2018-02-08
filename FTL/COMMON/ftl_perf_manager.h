// File: ftl_perf_manager.h
// Date: 2014. 12. 03.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _PERF_MANAGER_H_
#define _PERF_MANAGER_H_

#define LATENCY_WINDOW  	200
#define UPDATE_FREQUENCY	10000

typedef struct nand_io_info
{
	int offset;
	int type;
	int io_page_nb;
	int io_seq_nb;
}nand_io_info;

/* IO Latency */
typedef struct io_request
{
	unsigned int request_nb;
	int	request_type	: 16;
	int	request_size	: 16;
	int	start_count	: 16;
	int	end_count	: 16;
	int64_t* 	start_time;
	int64_t* 	end_time;
	struct io_request* next;
}io_request;

/* IO Latency */
extern unsigned int io_request_nb;
extern unsigned int io_request_seq_nb;

extern struct io_request* io_request_start;
extern struct io_request* io_request_end;

/* GC Latency */
extern unsigned int gc_request_nb;
extern unsigned int gc_request_seq_nb;

extern struct io_request* gc_request_start;
extern struct io_request* gc_request_end;

extern int64_t written_page_nb;

double GET_IO_BANDWIDTH(double delay);

void INIT_PERF_CHECKER(void);
void TERM_PERF_CHECKER(void);

void SEND_TO_PERF_CHECKER(int op_type, int64_t op_delay, int type);

int64_t ALLOC_IO_REQUEST(int32_t sector_nb, unsigned int length, int io_type, int* page_nb);
void FREE_DUMMY_IO_REQUEST(int type);
void FREE_IO_REQUEST(io_request* request);

int64_t UPDATE_IO_REQUEST(int request_nb, int offset, int64_t time, int type);
void INCREASE_IO_REQUEST_SEQ_NB(void);
io_request* LOOKUP_IO_REQUEST(int request_nb, int type);
int64_t CALC_IO_LATENCY(io_request* request);

nand_io_info* CREATE_NAND_IO_INFO(int offset, int type, int io_page_nb, int io_seq_nb);

void PRINT_IO_REQUEST(io_request* request);
void PRINT_ALL_IO_REQUEST(void);

void UPDATE_LOG(int log_type, int arg);
void SEND_LOG_TO_MONITOR(void);
#endif
