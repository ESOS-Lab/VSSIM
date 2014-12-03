// File: vssim_config_manager.h
// Data: 2014. 12. 03.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _CONFIG_MANAGER_H_
#define _CONFIG_MANAGER_H_

#include "common.h"


/* SSD Configuration */
extern int SECTOR_SIZE;
extern int PAGE_SIZE;

extern int64_t SECTOR_NB;
extern int PAGE_NB;
extern int FLASH_NB;
extern int BLOCK_NB;
extern int CHANNEL_NB;
extern int PLANES_PER_FLASH;

extern int SECTORS_PER_PAGE;
extern int PAGES_PER_FLASH;
extern int64_t PAGES_IN_SSD;

extern int WAY_NB;
extern int OVP;

/* Mapping Table */
extern int DATA_BLOCK_NB;
extern int64_t BLOCK_MAPPING_ENTRY_NB;

#ifdef PAGE_MAP
extern int64_t PAGE_MAPPING_ENTRY_NB;
#endif

#if defined PAGE_MAP || defined BLOCK_MAP
extern int64_t EACH_EMPTY_TABLE_ENTRY_NB;
extern int EMPTY_TABLE_ENTRY_NB;
extern int VICTIM_TABLE_ENTRY_NB;
#endif

#if defined FAST_FTL || defined LAST_FTL
extern int LOG_RAND_BLOCK_NB;
extern int LOG_SEQ_BLOCK_NB;

extern int64_t DATA_MAPPING_ENTRY_NB;          // added by js
extern int64_t RAN_MAPPING_ENTRY_NB;           // added by js
extern int64_t SEQ_MAPPING_ENTRY_NB;           // added by js
extern int64_t RAN_LOG_MAPPING_ENTRY_NB;       // added by js
extern int64_t EACH_EMPTY_BLOCK_ENTRY_NB;      // added by js

extern int EMPTY_BLOCK_TABLE_NB;
#endif

#ifdef DA_MAP
extern int64_t DA_PAGE_MAPPING_ENTRY_NB;
extern int64_t DA_BLOCK_MAPPING_ENTRY_NB;
extern int64_t EACH_EMPTY_TABLE_ENTRY_NB;
extern int EMPTY_TABLE_ENTRY_NB;
extern int VICTIM_TABLE_ENTRY_NB;
#endif

/* NAND Flash Delay */
extern int REG_WRITE_DELAY;
extern int CELL_PROGRAM_DELAY;
extern int REG_READ_DELAY;
extern int CELL_READ_DELAY;
extern int BLOCK_ERASE_DELAY;
extern int CHANNEL_SWITCH_DELAY_R;
extern int CHANNEL_SWITCH_DELAY_W;

extern int DSM_TRIM_ENABLE;
extern int IO_PARALLELISM;

/* Garbage Collection */
#if defined PAGE_MAP || defined BLOCK_MAP || defined DA_MAP
extern double GC_THRESHOLD;
extern int GC_THRESHOLD_BLOCK_NB;
extern int GC_THRESHOLD_BLOCK_NB_HARD;
extern int GC_THRESHOLD_BLOCK_NB_EACH;
extern int GC_VICTIM_NB;
#endif

/* Read / Write Buffer */
extern uint32_t WRITE_BUFFER_FRAME_NB;		// 8192 for 32MB with 4KB Page size
extern uint32_t READ_BUFFER_FRAME_NB;

/* Map Cache */
#if defined FTL_MAP_CACHE || defined Polymorphic_FTL
extern int CACHE_IDX_SIZE;
#endif

#ifdef FTL_MAP_CACHE
extern uint32_t MAP_ENTRY_SIZE;
extern uint32_t MAP_ENTRIES_PER_PAGE;
extern uint32_t MAP_ENTRY_NB;
#endif

/* HOST event queue */
#ifdef HOST_QUEUE
extern uint32_t HOST_QUEUE_ENTRY_NB;
#endif

/* FAST Perf TEST */
#if defined FAST_FTL || defined LAST_FTL
extern int PARAL_DEGREE;                       // added by js
extern int PARAL_COUNT;                        // added by js
#endif

/* LAST FTL */
#ifdef LAST_FTL
extern int HOT_PAGE_NB_THRESHOLD;              // added by js
extern int SEQ_THRESHOLD;                      // added by js
#endif

/* Polymorphic FTL */
#ifdef Polymorphic_FTL
extern int PHY_SPARE_SIZE;

extern int64_t NR_PHY_BLOCKS;
extern int64_t NR_PHY_PAGES;
extern int64_t NR_PHY_SECTORS;

extern int NR_RESERVED_PHY_SUPER_BLOCKS;
extern int NR_RESERVED_PHY_BLOCKS;
extern int NR_RESERVED_PHY_PAGES;
#endif

void INIT_SSD_CONFIG(void);
char* GET_FILE_NAME_HDA(void);
char* GET_FILE_NAME_HDB(void);

#ifdef DA_MAP
int64_t CALC_DA_PM_ENTRY_NB(void);
int64_t CALC_DA_BM_ENTRY_NB(void);
#endif

#endif
