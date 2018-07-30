// File: common.h
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include "ftl_type.h"
#include "vssim_type.h"
#include "bitmap.h"

/* FTL */
/* VSSIM Function */
#define MONITOR_ON
//#define FTL_MAP_CACHE		/* FTL MAP Cache for PAGE MAP */

/* HEADER - VSSIM CONFIGURATION */
#include "vssim_config_manager.h"

/* HEADER - FTL MODULE */
#include "ftl.h"
#include "ftl_perf_manager.h"
#include "ftl_flash_manager.h"

/* HEADER - MONITOR */
#include "ssd_log_manager.h"

/* HEADER - FIRMWARE */
#include "vssim_core.h"
#include "firm_buffer_manager.h"

/* HEADER - FLASH MODULE */
#include "flash_memory.h"

/* HEADER - FTL Dependency */
#include "ftl_cache.h"
#include "ftl_gc_manager.h"
#include "ftl_mapping_manager.h"

/*************************
  Define Global Variables 
 *************************/

/* Function Return */
#define SUCCESS		0
#define FAIL		1
#define TRIMMED		2

/* Block Type */
#define EMPTY_BLOCK             30
#define DATA_BLOCK              31
#define EMPTY_DATA_BLOCK        32

/* GC Copy Valid Page Type */
#define MODE_OVERALL		41
#define MODE_INFLASH		42
#define MODE_INPLANE		43
#define MODE_NOPARAL		44

/* Page Type */
#define VALID		50
#define INVALID		51

/* consider QEMU/SW overhead */
#define DEL_FIRM_OVERHEAD

typedef void BlockCompletionFunc(void *opaque, int ret);

ppn_t PBN_TO_PPN(pbn_t pbn, uint32_t page_index);
pbn_t PPN_TO_PBN(ppn_t ppn);
uint64_t GET_LINEAR_PPN(ppn_t ppn);

int64_t get_usec(void);

/* VSSIM Function Debug */

/* SSD Monitor debug */
#define MNT_DEBUG

/* Workload */

/* FTL Debugging */
//#define FTL_DEBUG
//#define GC_DEBUG
//#define BGGC_DEBUG
//#define GET_GC_INFO
//#define GET_CH_UTIL_INFO
//#define GET_W_EVENT_INFO
//#define GET_WB_LAT_INFO
//#define GET_RW_LAT_INFO

/* FLASH Debugging */
//#define FLASH_DEBUG
//#define FLASH_DEBUG_LOOP

/* FIRMWARE Debugging */
//#define THREAD_CORE_BIND
//#define IO_CORE_DEBUG
//#define IO_PERF_DEBUG

#endif // end of 'ifndef _COMMON_H_'
