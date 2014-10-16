/*
 * firmwar.h: Firmware build logic head file
 *
 * Copyright (c) 2007, Intel Corporation.
 * Zhang Xiantao <xiantao.zhang@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 */
#ifndef __FIRM_WARE_H
#define  __FIRM_WARE_
#include "cpu.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <zlib.h>

#define GFW_SIZE                (16UL<<20)
#define GFW_START               ((4UL<<30) - GFW_SIZE)

#define HOB_SIGNATURE           0x3436474953424f48        // "HOBSIG64"
#define GFW_HOB_START           ((4UL<<30) - (14UL<<20))    // 4G - 14M
#define GFW_HOB_SIZE            (1UL<<20)                 // 1M
#define HOB_OFFSET              (GFW_HOB_START-GFW_START)

#define Hob_Output(s)           fprintf(stderr, s)

#define NVRAM_START  (GFW_START + NVRAM_OFFSET)
#define NVRAM_OFFSET (10 * (1UL << 20))
#define NVRAM_SIZE   (64 * (1UL << 10))
#define NVRAM_VALID_SIG  0x4650494e45584948 /* "HIXENIPF" */
#define VALIDATE_NVRAM_FD(x) ((1UL<<(sizeof(x)*8 - 1)) | x)
#define IS_VALID_NVRAM_FD(x) ((uint64_t)x >> (sizeof(x)*8 - 1))
#define READ_FROM_NVRAM 0
#define WRITE_TO_NVRAM 1

struct nvram_save_addr {
    unsigned long addr;
    unsigned long signature;
};

extern const char *nvram;
extern int kvm_ia64_build_hob(unsigned long memsize, unsigned long vcpus,
                              unsigned long nvram_addr);
extern uint8_t *read_image(const char *filename, unsigned long *size);

extern int kvm_ia64_copy_from_GFW_to_nvram(void);
extern int kvm_ia64_nvram_init(unsigned long type);
extern int kvm_ia64_copy_from_nvram_to_GFW(unsigned long nvram_fd);
#endif //__FIRM_WARE_
