// File: ssd.h
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _SSD_H_
#define _SSD_H_

#include "hw/block/nvme.h"

int64_t get_usec_nvme(void);

//extern bool vssim_exit;
typedef void CallbackFunc(void *opaque, int ret);

void SSD_INIT(void);
void SSD_TERM(void);

/* IO functions for the NVMe interface */
event_queue_entry* SSD_NVME_READ(uint64_t slba, uint32_t nlb, NvmeRequest *req, 
			void (*cb)(void *opaque, int ret));
event_queue_entry* SSD_NVME_WRITE(uint64_t slba, uint32_t nlb, NvmeRequest *req, 
			void (*cb)(void *opaque, int ret));
event_queue_entry* SSD_NVME_FLUSH(uint64_t slba, uint32_t nlb, NvmeRequest *req,
		void(*cb)(void *opaque, int ret));

event_queue_entry* SSD_RW(int io_type, uint64_t slba, uint32_t nlb, void* opaque, CallbackFunc *cb);

/* TRIM command support */
void SSD_DSM_DISCARD(NvmeRequest *req, uint32_t nr);
int IS_SSD_TRIM_ENABLED(void);

/* Post processing for the vssim events */
void END_SSD_NVME_RW(event_queue_entry* eq_entry);
void END_SSD_NVME_READ(event_queue_entry* eq_entry);
void END_SSD_NVME_WRITE(event_queue_entry* eq_entry);
void END_SSD_NVME_FLUSH(event_queue_entry* eq_entry);

#endif
