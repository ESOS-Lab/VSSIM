// File: vssim_config_manager.h
// Date: 2014. 12. 03.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _CONFIG_MANAGER_H_
#define _CONFIG_MANAGER_H_

#include "common.h"

/* SSD Configuration */
extern int N_CORES;
extern int BACKGROUND_GC_ENABLE;
extern int N_IO_CORES;

extern int SECTOR_SIZE;
extern int64_t N_SECTORS;
extern int LOG_SECTOR_SIZE;
extern int PAGE_SIZE;

extern int N_CHANNELS;
extern int N_FLASH;
extern int N_PLANES_PER_FLASH;
extern int N_BLOCKS_PER_PLANE;
extern int N_BLOCKS_PER_FLASH;
extern int N_PAGES_PER_BLOCK;
extern int N_PAGES_PER_PLANE;
extern int N_PAGES_PER_FLASH;

extern int SECTORS_PER_PAGE;

extern int N_WAYS;
extern int OVP;

/* Valid bitmap */
extern int N_4K_PAGES;
extern int BITMAP_SIZE;
extern int SECTORS_PER_4K_PAGE;

/* Mapping Table */
extern int64_t N_TOTAL_PAGES;
extern int64_t N_TOTAL_BLOCKS;

/* NAND Flash Delay */
extern int REG_CMD_SET_DELAY;
extern int REG_WRITE_DELAY;
extern int PAGE_PROGRAM_DELAY;
extern int REG_READ_DELAY;
extern int PAGE_READ_DELAY;
extern int BLOCK_ERASE_DELAY;

extern int DSM_TRIM_ENABLE;
extern int PAGE_CACHE_REG_ENABLE;

/* Garbage Collection */
enum vssim_gc_mode{
	CORE_GC,
	FLASH_GC,
	PLANE_GC
};
extern enum vssim_gc_mode gc_mode;

extern double GC_LOW_WATERMARK_RATIO;
extern double GC_HIGH_WATERMARK_RATIO;
extern int N_GC_LOW_WATERMARK_BLOCKS;
extern int N_GC_HIGH_WATERMARK_BLOCKS;

/* Read / Write Buffer */
extern uint32_t N_WB_SECTORS;
extern uint32_t N_RB_SECTORS;
extern uint32_t N_DISCARD_BUF_SECTORS;
extern uint32_t N_NVME_CMD_MAX_SECTORS;

void INIT_SSD_CONFIG(void);
char* GET_FILE_NAME_HDA(void);
char* GET_FILE_NAME_HDB(void);

#endif
