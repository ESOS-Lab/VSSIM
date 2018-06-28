// File: flash_memory.h
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _FLASH_MEMORY_H
#define _FLASH_MEMORY_H

/* Command Definitions */
#define CMD_NOOP			0xFF 
#define CMD_PAGE_READ			0x00
#define CMD_PAGE_PROGRAM		0x80
#define CMD_PAGE_COPYBACK		0x85
#define CMD_PAGE_COPYBACK_PHASE2	0x90
#define CMD_BLOCK_ERASE			0x60

/* The number of ppn list per flash */
#define N_PPNS_PER_PLANE	512

enum reg_state
{
	REG_NOOP = 0,
	SET_CMD1,
	SET_CMD2,
	REG_WRITE,
	REG_READ,
	PAGE_PROGRAM,
	PAGE_READ,
	BLOCK_ERASE,
	WAIT_CHANNEL_FOR_CMD1,
	WAIT_CHANNEL_FOR_CMD2,
	WAIT_REG,
};

typedef struct reg
{
	enum reg_state state;
	ppn_t ppn;
	ppn_t copyback_ppn;
	int core_id;
	int64_t t_end;
}reg;

typedef struct plane
{
	uint8_t cmd;
	struct reg page_cache;
	struct reg data_reg;

	/* Per plane command list */
	uint8_t* cmd_list;
	ppn_t* ppn_list;
	ppn_t* copyback_list;
	int* core_id_list;
	pthread_mutex_t lock;

	uint32_t n_entries;
	uint32_t index;

	/* flash i/o delay */
	int reg_cmd_set_delay;
	int reg_write_delay;
	int page_program_delay; 
	int reg_read_delay;
	int page_read_delay;
	int block_erase_delay;
}plane;

typedef struct flash_channel
{
	int64_t t_next_idle;
}flash_channel;

typedef struct flash_memory
{
	int flash_nb;
	int channel_nb;
	struct plane* plane;
	struct flash_memory* next;
}flash_memory;

typedef struct io_processing_info
{
	int n_submitted_io;
	int n_completed_io;
}io_proc_info;

/* Initialize SSD Module */
int INIT_FLASH(void);
void INIT_FLASH_MEMORY_LIST(int core_id);
int TERM_FLASH(void);

void FLASH_STATE_CHECKER(int core_id);
void UPDATE_DATA_REGISTER(int core_id, plane* cur_plane, int channel_nb, int64_t t_now, int plane_nb);
void UPDATE_PAGE_CACHE_REGISTER(int core_id, plane* cur_plane, int channel_nb, int64_t t_now);

/* GET IO from FTL */
int FLASH_PAGE_READ(int core_id, ppn_t ppn);
int FLASH_PAGE_WRITE(int core_id, ppn_t ppn);
int FLASH_BLOCK_ERASE(int core_id, pbn_t pbn);
int FLASH_PAGE_COPYBACK(int core_id, ppn_t dst_ppn, ppn_t src_ppn);
int FLASH_PAGE_COPYBACK_PHASE2(int core_id, ppn_t dst_ppn, ppn_t src_ppn);

void COPY_PAGE_CACHE_TO_DATA_REG(reg* dst_reg, reg* src_reg);

int64_t BUSY_WAITING_USEC(int64_t t_end);
int64_t GET_AND_UPDATE_NEXT_AVAILABLE_CH_TIME(int channel_nb, 
		int64_t t_now, uint8_t cmd, plane* cur_plane, 
		enum reg_state cur_state);

int WAIT_FLASH_IO(int core_id, int io_type, int n_io_pages);

void UPDATE_IO_PROC_INFO(int core_id);
bool CHECK_IO_COMPLETION(int core_id, int n_io_pages, int* remain_pages);

/* Consiter QEMU, FIRM overhead */
void UPDATE_FLASH_LATENCY(int core_id, int reg_cmd_type, int64_t overhead);
int64_t SET_FIRM_OVERHEAD(int core_id, int io_type, int64_t overhead);
int64_t SET_FIRM_OVERHEAD2(int core_id, int io_type, int64_t overhead);

#endif
