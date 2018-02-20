// File: firm_buffer_manager.h
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _FIRM_BUFFER_MANAGER_H_
#define _FIRM_BUFFER_MANAGER_H_

#define FLUSH_TIMEOUT_USEC	300000		// 300 msec

extern pthread_cond_t eq_ready;
extern pthread_mutex_t eq_lock;
extern pthread_mutex_t cq_lock;

typedef void CallbackFunc(void *opaque, int ret);

enum vssim_io_type{
	NOOP = 0,
	READ,
	WRITE,
	DISCARD,
	FLUSH,
	ERASE,
	GC_READ,
	GC_WRITE,
	MAP_READ,
	MAP_WRITE,
};

typedef struct vssim_io_buffer
{
	void* addr;
	int n_empty_sectors;
	void* ftl_ptr;
	void* host_ptr;
	void* buffer_end;
	int is_full;		// 1: full, 0: others
	int64_t t_last_flush; 
	pthread_mutex_t lock;
	pthread_cond_t ready;
}vssim_io_buffer;

extern int N_WRITE_BUF;
extern vssim_io_buffer* vssim_w_buf;
extern vssim_io_buffer vssim_r_buf;
extern vssim_io_buffer vssim_discard_buf;
extern int write_buffer_index;

enum event_state{
	WAIT_CHILD,
	COMPLETED,
	DONE_READ_DMA
};

typedef struct event_queue
{
	int entry_nb;
	struct event_queue_entry* head;
	struct event_queue_entry* tail;
}event_queue;

typedef struct event_queue_entry
{
	uint64_t seq_nb;

	enum vssim_io_type io_type;
	int valid;
	uint64_t sector_nb;
	uint32_t length;
	uint32_t ori_length;
	CallbackFunc *cb;
	void* opaque;
	void* buf;
	uint32_t n_child;
	uint32_t n_completed;
	uint32_t n_trimmed;
	enum event_state e_state;
	pthread_mutex_t lock;
	bool flush;

	/* Bandwidth */
	int64_t t_start;
	uint32_t n_pages;

	/* pointers for candidate queue */
	struct event_queue_entry* prev;
	struct event_queue_entry* next;

}event_queue_entry;

void INIT_IO_BUFFER(void);
void TERM_IO_BUFFER(void);

void INIT_VSSIM_WRITE_BUFFER(void);
void INIT_VSSIM_READ_BUFFER(void);
void INIT_VSSIM_DISCARD_BUFFER(void);

void ENQUEUE_IO(event_queue_entry* new_entry);
void INSERT_TO_CANDIDATE_EVENT_QUEUE(event_queue_entry* new_entry);
void REMOVE_FROM_CANDIDATE_EVENT_QUEUE(event_queue_entry* new_entry);

event_queue_entry* DEQUEUE_IO(void);

/* Manipulate event queue entries */
event_queue_entry* CREATE_NEW_EVENT(int io_type, uint64_t slba, 
			uint32_t nlb, void* opaque, CallbackFunc *cb);
void UPDATE_EVENT_STATE(event_queue_entry* eq_entry, enum event_state state); 
int GET_EVENT_STATE(event_queue_entry* eq_entry); 
int GET_N_IO_PAGES(uint64_t sector_nb, uint32_t length);

void FIRM_READ_EVENT(event_queue_entry* r_entry);
void FIRM_WRITE_EVENT(event_queue_entry* w_entry, bool flush);
void FIRM_DISCARD_EVENT(event_queue_entry* dc_entry);
void FIRM_FLUSH_EVENT(event_queue_entry* eq_entry);

void* CHECK_WRITE_BUFFER(int core_id, uint64_t lba, uint32_t read_sects);
void DMA_FROM_HOST_TO_BUFFER(event_queue_entry* w_entry, int w_buf_index);
void DO_DMA_FROM_HOST_TO_BUFFER(event_queue_entry* w_entry, int w_buf_index);
void BUF_FILL_ZEROS(event_queue_entry* eq_entry);
void FLUSH_EVENT_QUEUE_UNTIL(event_queue_entry* e_q_entry);

int GET_WRITE_BUFFER_TO_FLUSH(int core_id, int* w_buf_index);
void FLUSH_WRITE_BUFFER(int core_id, int w_buf_index);
void DO_PER_CORE_READ(int core_id);
void DO_PER_CORE_DISCARD(int core_id);

/* Move Buffer Frame Pointer */
void INCREASE_RB_HOST_POINTER(uint32_t n_sectors);
void INCREASE_WB_HOST_POINTER(uint32_t n_sectors, int w_buf_index);
void INCREASE_RB_FTL_POINTER(uint32_t n_sectors);
void INCREASE_WB_FTL_POINTER(uint32_t n_sectors);
void INCREASE_DISCARD_BUF_HOST_POINTER(uint32_t n_sectors);
void INCREASE_DISCARD_BUF_FTL_POINTER(uint32_t n_sectors);

/* Test IO BUFFER */
int COUNT_READ_EVENT(void);

extern event_queue* e_queue;
extern event_queue* candidate_e_queue;
extern event_queue* complete_e_queue;
#endif
