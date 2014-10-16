/*
 * firmware.c : Firmware build logic for ia64 platform.
 *
 * Ported from Xen 3.0 Source.
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <zlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cpu.h"

#include "firmware.h"

#include "qemu-common.h"

typedef struct {
    unsigned long signature;
    unsigned int  type;
    unsigned int  length;
} HOB_GENERIC_HEADER;

/*
 * INFO HOB is the first data data in one HOB list
 * it contains the control information of the HOB list
 */
typedef struct {
    HOB_GENERIC_HEADER  header;
    unsigned long       length;    // current length of hob
    unsigned long       cur_pos;   // current poisiton of hob
    unsigned long       buf_size;  // size of hob buffer
} HOB_INFO;

typedef struct{
    unsigned long start;
    unsigned long size;
} hob_mem_t;

typedef enum {
    HOB_TYPE_INFO=0,
    HOB_TYPE_TERMINAL,
    HOB_TYPE_MEM,
    HOB_TYPE_PAL_BUS_GET_FEATURES_DATA,
    HOB_TYPE_PAL_CACHE_SUMMARY,
    HOB_TYPE_PAL_MEM_ATTRIB,
    HOB_TYPE_PAL_CACHE_INFO,
    HOB_TYPE_PAL_CACHE_PROT_INFO,
    HOB_TYPE_PAL_DEBUG_INFO,
    HOB_TYPE_PAL_FIXED_ADDR,
    HOB_TYPE_PAL_FREQ_BASE,
    HOB_TYPE_PAL_FREQ_RATIOS,
    HOB_TYPE_PAL_HALT_INFO,
    HOB_TYPE_PAL_PERF_MON_INFO,
    HOB_TYPE_PAL_PROC_GET_FEATURES,
    HOB_TYPE_PAL_PTCE_INFO,
    HOB_TYPE_PAL_REGISTER_INFO,
    HOB_TYPE_PAL_RSE_INFO,
    HOB_TYPE_PAL_TEST_INFO,
    HOB_TYPE_PAL_VM_SUMMARY,
    HOB_TYPE_PAL_VM_INFO,
    HOB_TYPE_PAL_VM_PAGE_SIZE,
    HOB_TYPE_NR_VCPU,
    HOB_TYPE_NR_NVRAM,
    HOB_TYPE_MAX
} hob_type_t;

static int hob_init(void  *buffer ,unsigned long buf_size);
static int add_pal_hob(void* hob_buf);
static int add_mem_hob(void* hob_buf, unsigned long dom_mem_size);
static int add_vcpus_hob(void* hob_buf, unsigned long nr_vcpu);
static int add_nvram_hob(void *hob_buf, unsigned long nvram_addr);
static int build_hob(void *hob_buf, unsigned long hob_buf_size,
                     unsigned long dom_mem_size, unsigned long vcpus,
                     unsigned long nvram_addr);
static int load_hob(void *hob_buf, unsigned long dom_mem_size);

int
kvm_ia64_build_hob(unsigned long memsize, unsigned long vcpus,
                   unsigned long nvram_addr)
{
    char   *hob_buf;

    hob_buf = malloc(GFW_HOB_SIZE);
    if (hob_buf == NULL) {
        Hob_Output("Hob: Could not allocate hob");
        return -1;
    }

    if (build_hob(hob_buf, GFW_HOB_SIZE, memsize, vcpus, nvram_addr) < 0) {
        free(hob_buf);
        Hob_Output("Could not build hob");
        return -1;
    }

    if (load_hob(hob_buf, memsize) < 0) {
        free(hob_buf);
        Hob_Output("Could not load hob");
        return -1;
    }
    free(hob_buf);

    return 0;
}

static int
hob_init(void *buffer, unsigned long buf_size)
{
    HOB_INFO *phit;
    HOB_GENERIC_HEADER *terminal;

    if (sizeof(HOB_INFO) + sizeof(HOB_GENERIC_HEADER) > buf_size) {
        // buffer too small
        return -1;
    }

    phit = (HOB_INFO*)buffer;
    phit->header.signature = HOB_SIGNATURE;
    phit->header.type = HOB_TYPE_INFO;
    phit->header.length = sizeof(HOB_INFO);
    phit->length = sizeof(HOB_INFO) + sizeof(HOB_GENERIC_HEADER);
    phit->cur_pos = 0;
    phit->buf_size = buf_size;

    terminal = (HOB_GENERIC_HEADER*)(buffer + sizeof(HOB_INFO));
    terminal->signature = HOB_SIGNATURE;
    terminal->type = HOB_TYPE_TERMINAL;
    terminal->length = sizeof(HOB_GENERIC_HEADER);

    return 0;
}

/*
 *  Add a new HOB to the HOB List.
 *
 *  hob_start  -  start address of hob buffer
 *  type       -  type of the hob to be added
 *  data       -  data of the hob to be added
 *  data_size  -  size of the data
 */
static int
hob_add(void* hob_start, int type, void* data, int data_size)
{
    HOB_INFO *phit;
    HOB_GENERIC_HEADER *newhob, *tail;

    phit = (HOB_INFO*)hob_start;

    if (phit->length + data_size > phit->buf_size) {
        // no space for new hob
        return -1;
    }

    //append new HOB
    newhob = (HOB_GENERIC_HEADER*)(hob_start + phit->length -
                                   sizeof(HOB_GENERIC_HEADER));
    newhob->signature = HOB_SIGNATURE;
    newhob->type = type;
    newhob->length = data_size + sizeof(HOB_GENERIC_HEADER);
    memcpy((void*)newhob + sizeof(HOB_GENERIC_HEADER), data, data_size);

    // append terminal HOB
    tail = (HOB_GENERIC_HEADER*)(hob_start + phit->length + data_size);
    tail->signature = HOB_SIGNATURE;
    tail->type = HOB_TYPE_TERMINAL;
    tail->length = sizeof(HOB_GENERIC_HEADER);

    // adjust HOB list length
    phit->length += sizeof(HOB_GENERIC_HEADER) + data_size;

    return 0;
}

static int
get_hob_size(void* hob_buf)
{
    HOB_INFO *phit = (HOB_INFO*)hob_buf;

    if (phit->header.signature != HOB_SIGNATURE) {
        Hob_Output("xc_get_hob_size:Incorrect signature");
        return -1;
    }
    return phit->length;
}

static  int
add_max_hob_entry(void* hob_buf)
{
    long max_hob = 0;
    return hob_add(hob_buf, HOB_TYPE_MAX, &max_hob, sizeof(long));
}

static int
build_hob(void* hob_buf, unsigned long hob_buf_size,
          unsigned long dom_mem_size, unsigned long vcpus,
          unsigned long nvram_addr)
{
    //Init HOB List
    if (hob_init(hob_buf, hob_buf_size) < 0) {
        Hob_Output("buffer too small");
        goto err_out;
    }

    if (add_mem_hob(hob_buf,dom_mem_size) < 0) {
        Hob_Output("Add memory hob failed, buffer too small");
        goto err_out;
    }

    if (add_vcpus_hob(hob_buf, vcpus) < 0) {
        Hob_Output("Add NR_VCPU hob failed, buffer too small");
        goto err_out;
    }

    if (add_pal_hob(hob_buf) < 0) {
        Hob_Output("Add PAL hob failed, buffer too small");
        goto err_out;
    }

    if (add_nvram_hob(hob_buf, nvram_addr) < 0) {
	    Hob_Output("Add nvram hob failed, buffer too small");
	    goto err_out;
	}

    if (add_max_hob_entry(hob_buf) < 0) {
        Hob_Output("Add max hob entry failed, buffer too small");
        goto err_out;
    }
    return 0;

err_out:
    return -1;
}
static int
load_hob(void *hob_buf, unsigned long dom_mem_size)
{
    int hob_size;

    hob_size = get_hob_size(hob_buf);
    if (hob_size < 0) {
        Hob_Output("Invalid hob data");
        return -1;
    }

    if (hob_size > GFW_HOB_SIZE) {
        Hob_Output("No enough memory for hob data");
        return -1;
    }

    cpu_physical_memory_write(GFW_HOB_START, hob_buf, hob_size);

    return 0;
}

static int
add_mem_hob(void* hob_buf, unsigned long dom_mem_size)
{
    hob_mem_t memhob;

    // less than 3G
    memhob.start = 0;
    memhob.size = MIN(dom_mem_size, 0xC0000000);

    if (hob_add(hob_buf, HOB_TYPE_MEM, &memhob, sizeof(memhob)) < 0)
        return -1;

    if (dom_mem_size > 0xC0000000) {
        // 4G ~ 4G+remain
        memhob.start = 0x100000000; //4G
        memhob.size = dom_mem_size - 0xC0000000;
        if (hob_add(hob_buf, HOB_TYPE_MEM, &memhob, sizeof(memhob)) < 0)
            return -1;
    }
    return 0;
}

static int
add_vcpus_hob(void* hob_buf, unsigned long vcpus)
{
    return hob_add(hob_buf, HOB_TYPE_NR_VCPU, &vcpus, sizeof(vcpus));
}

static int
add_nvram_hob(void *hob_buf, unsigned long nvram_addr)
{
    return hob_add(hob_buf, HOB_TYPE_NR_NVRAM,
                   &nvram_addr, sizeof(nvram_addr));
}

static const unsigned char config_pal_bus_get_features_data[24] = {
    0, 0, 0, 32, 0, 0, 240, 189, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char config_pal_cache_summary[16] = {
    3, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char config_pal_mem_attrib[8] = {
    241, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char config_pal_cache_info[152] = {
    3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    6, 4, 6, 7, 255, 1, 0, 1, 0, 64, 0, 0, 12, 12,
    49, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 6, 7, 0, 1,
    0, 1, 0, 64, 0, 0, 12, 12, 49, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 6, 8, 7, 7, 255, 7, 0, 11, 0, 0, 16, 0,
    12, 17, 49, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 8, 7,
    7, 7, 5, 9, 11, 0, 0, 4, 0, 12, 15, 49, 0, 254, 255,
    255, 255, 255, 255, 255, 255, 2, 8, 7, 7, 7, 5, 9,
    11, 0, 0, 4, 0, 12, 15, 49, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 3, 12, 7, 7, 7, 14, 1, 3, 0, 0, 192, 0, 12, 20, 49, 0
};

static const unsigned char config_pal_cache_prot_info[200] = {
    3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    45, 0, 16, 8, 0, 76, 12, 64, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    8, 0, 16, 4, 0, 76, 44, 68, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32,
    0, 16, 8, 0, 81, 44, 72, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 0,
    112, 12, 0, 79, 124, 76, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 254, 255, 255, 255, 255, 255, 255, 255,
    32, 0, 112, 12, 0, 79, 124, 76, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 0, 160,
    12, 0, 84, 124, 76, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0
};

static const unsigned char config_pal_debug_info[16] = {
    2, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char config_pal_fixed_addr[8] = {
    0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char config_pal_freq_base[8] = {
    109, 219, 182, 13, 0, 0, 0, 0
};

static const unsigned char config_pal_freq_ratios[24] = {
    11, 1, 0, 0, 77, 7, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 4,
    0, 0, 0, 7, 0, 0, 0
};

static const unsigned char config_pal_halt_info[64] = {
    0, 0, 0, 0, 0, 0, 0, 48, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char config_pal_perf_mon_info[136] = {
    12, 47, 18, 8, 0, 0, 0, 0, 241, 255, 0, 0, 255, 7, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 241, 255, 0, 0, 223, 0, 255, 255,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 240, 255, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 240, 255, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char config_pal_proc_get_features[104] = {
    3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 64, 6, 64, 49, 0, 0, 0, 0, 64, 6, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0,
    231, 0, 0, 0, 0, 0, 0, 0, 228, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 0, 0,
    63, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char config_pal_ptce_info[24] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char config_pal_register_info[64] = {
    255, 0, 47, 127, 17, 17, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0,
    255, 208, 128, 238, 238, 0, 0, 248, 255, 255, 255, 255, 255, 0, 0, 7, 3,
    251, 3, 0, 0, 0, 0, 255, 7, 3, 0, 0, 0, 0, 0, 248, 252, 4,
    252, 255, 255, 255, 255, 2, 248, 252, 255, 255, 255, 255, 255
};

static const unsigned char config_pal_rse_info[16] = {
    96, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char config_pal_test_info[48] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char config_pal_vm_summary[16] = {
    101, 18, 15, 2, 7, 7, 4, 2, 59, 18, 0, 0, 0, 0, 0, 0
};

static const unsigned char config_pal_vm_info[104] = {
    2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    32, 32, 0, 0, 0, 0, 0, 0, 112, 85, 21, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 1, 32, 32, 0, 0, 0, 0, 0, 0, 112, 85,
    21, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 128, 128, 0,
    4, 0, 0, 0, 0, 112, 85, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 128, 128, 0, 4, 0, 0, 0, 0, 112, 85, 0, 0, 0, 0, 0
};

static const unsigned char config_pal_vm_page_size[16] = {
    0, 112, 85, 21, 0, 0, 0, 0, 0, 112, 85, 21, 0, 0, 0, 0
};

typedef struct{
    hob_type_t type;
    void* data;
    unsigned long size;
} hob_batch_t;

static const hob_batch_t hob_batch[]={
    {   HOB_TYPE_PAL_BUS_GET_FEATURES_DATA,
        &config_pal_bus_get_features_data,
        sizeof(config_pal_bus_get_features_data)
    },
    {   HOB_TYPE_PAL_CACHE_SUMMARY,
        &config_pal_cache_summary,
        sizeof(config_pal_cache_summary)
    },
    {   HOB_TYPE_PAL_MEM_ATTRIB,
        &config_pal_mem_attrib,
        sizeof(config_pal_mem_attrib)
    },
    {   HOB_TYPE_PAL_CACHE_INFO,
        &config_pal_cache_info,
        sizeof(config_pal_cache_info)
    },
    {   HOB_TYPE_PAL_CACHE_PROT_INFO,
        &config_pal_cache_prot_info,
        sizeof(config_pal_cache_prot_info)
    },
    {   HOB_TYPE_PAL_DEBUG_INFO,
        &config_pal_debug_info,
        sizeof(config_pal_debug_info)
    },
    {   HOB_TYPE_PAL_FIXED_ADDR,
        &config_pal_fixed_addr,
        sizeof(config_pal_fixed_addr)
    },
    {   HOB_TYPE_PAL_FREQ_BASE,
        &config_pal_freq_base,
        sizeof(config_pal_freq_base)
    },
    {   HOB_TYPE_PAL_FREQ_RATIOS,
        &config_pal_freq_ratios,
        sizeof(config_pal_freq_ratios)
    },
    {   HOB_TYPE_PAL_HALT_INFO,
        &config_pal_halt_info,
        sizeof(config_pal_halt_info)
    },
    {   HOB_TYPE_PAL_PERF_MON_INFO,
        &config_pal_perf_mon_info,
        sizeof(config_pal_perf_mon_info)
    },
    {   HOB_TYPE_PAL_PROC_GET_FEATURES,
        &config_pal_proc_get_features,
        sizeof(config_pal_proc_get_features)
    },
    {   HOB_TYPE_PAL_PTCE_INFO,
        &config_pal_ptce_info,
        sizeof(config_pal_ptce_info)
    },
    {   HOB_TYPE_PAL_REGISTER_INFO,
        &config_pal_register_info,
        sizeof(config_pal_register_info)
    },
    {   HOB_TYPE_PAL_RSE_INFO,
        &config_pal_rse_info,
        sizeof(config_pal_rse_info)
    },
    {   HOB_TYPE_PAL_TEST_INFO,
        &config_pal_test_info,
        sizeof(config_pal_test_info)
    },
    {   HOB_TYPE_PAL_VM_SUMMARY,
        &config_pal_vm_summary,
        sizeof(config_pal_vm_summary)
    },
    {   HOB_TYPE_PAL_VM_INFO,
        &config_pal_vm_info,
        sizeof(config_pal_vm_info)
    },
    {   HOB_TYPE_PAL_VM_PAGE_SIZE,
        &config_pal_vm_page_size,
        sizeof(config_pal_vm_page_size)
    },
};

static int
add_pal_hob(void* hob_buf)
{
    int i;
    for (i = 0; i < sizeof(hob_batch)/sizeof(hob_batch_t); i++) {
        if (hob_add(hob_buf, hob_batch[i].type, hob_batch[i].data,
                    hob_batch[i].size) < 0)
            return -1;
    }
    return 0;
}

uint8_t *read_image(const char *filename, unsigned long *size)
{
    int kernel_fd = -1;
    gzFile kernel_gfd = NULL;
    uint8_t *image = NULL, *tmp;
    unsigned int bytes;

    if ((filename == NULL) || (size == NULL))
        return NULL;

    kernel_fd = open(filename, O_RDONLY);
    if (kernel_fd < 0) {
        Hob_Output("Could not open kernel image\n");
        goto out_1;
    }

    if ((kernel_gfd = gzdopen(kernel_fd, "rb")) == NULL) {
        Hob_Output("Could not allocate decompression state for state file\n");
        goto out_1;
    }

    *size = 0;

#define CHUNK 1*1024*1024
    while(1)
    {
        if ((tmp = realloc(image, *size + CHUNK)) == NULL) {
            Hob_Output("Could not allocate memory for kernel image");
            free(image);
            image = NULL;
            goto out;
        }
        image = tmp;

        bytes = gzread(kernel_gfd, image + *size, CHUNK);
        switch (bytes) {
        case -1:
            Hob_Output("Error reading kernel image");
            free(image);
            image = NULL;
            goto out;
        case 0: /* EOF */
            goto out;
        default:
            *size += bytes;
            break;
        }
    }
#undef CHUNK

out:
    if (*size == 0) {
        Hob_Output("Could not read kernel image");
        free(image);
        image = NULL;
    } else if (image) {
        /* Shrink allocation to fit image. */
        tmp = realloc(image, *size);
        if (tmp)
            image = tmp;
    }

    if (kernel_gfd != NULL)
        gzclose(kernel_gfd);
    else if (kernel_fd >= 0)
        close(kernel_fd);
    return image;

out_1:
    return NULL;
}

int kvm_ia64_nvram_init(unsigned long type)
{
    unsigned long nvram_fd;
    char nvram_path[PATH_MAX];
    unsigned long i;

    if (nvram) {
        if (strlen(nvram) > PATH_MAX) {
            goto out;
        }
        if (type == READ_FROM_NVRAM) {
            if (access(nvram, R_OK | W_OK | X_OK) == -1)
                goto out;
            nvram_fd = open(nvram, O_RDONLY);
            return nvram_fd;
        }
        else { /* write from gfw to nvram file */
            i = access(nvram, R_OK | W_OK | X_OK);
            if ((i == -1) && (errno != ENOENT))
               goto out;
            nvram_fd = open(nvram, O_CREAT|O_RDWR, 0777);
            return nvram_fd;
        }
    }
    else {
        strcpy(nvram_path, "nvram.dat");
        if (type == READ_FROM_NVRAM) {
            if (access(nvram_path, R_OK | W_OK | X_OK) == -1)
                goto out;
            nvram_fd = open(nvram_path, O_RDONLY);
            return nvram_fd;
        }
        else { /* write from gfw to nvram file */
            i = access(nvram_path, R_OK | W_OK | X_OK);
            if ((i == -1) && (errno != ENOENT))
               goto out;
            nvram_fd = open(nvram_path, O_CREAT|O_RDWR, 0777);
            return nvram_fd;
        }
    }
out:
    return -1;
}

int
kvm_ia64_copy_from_nvram_to_GFW(unsigned long nvram_fd)
{
    struct stat file_stat;
    uint8_t *nvram_buf;
    int r = 0;

    nvram_buf = malloc(NVRAM_SIZE);

    if ((fstat(nvram_fd, &file_stat) < 0) ||
        (NVRAM_SIZE  != file_stat.st_size) ||
        (read(nvram_fd, nvram_buf, NVRAM_SIZE) != NVRAM_SIZE)) {
        r = -1;
        goto out;
    }

    cpu_physical_memory_write(NVRAM_START, nvram_buf, NVRAM_SIZE);

 out:
    free(nvram_buf);
    return r;
}

int
kvm_ia64_copy_from_GFW_to_nvram()
{
    struct nvram_save_addr nvram_addr_buf;
    uint8_t *nvram_buf;
    unsigned long nvram_fd;
    unsigned long type = WRITE_TO_NVRAM;
    int ret = -1;

    nvram_buf = malloc(NVRAM_SIZE);
    if (!nvram_buf)
        goto out_free;

    cpu_physical_memory_read(NVRAM_START, (uint8_t *)&nvram_addr_buf,
                             sizeof(struct nvram_save_addr));
    if (nvram_addr_buf.signature != NVRAM_VALID_SIG) {
        goto out_free;
    }

    cpu_physical_memory_read(nvram_addr_buf.addr, nvram_buf, NVRAM_SIZE);

    nvram_fd = kvm_ia64_nvram_init(type);
    if (nvram_fd  == -1)
        goto out;

    lseek(nvram_fd, 0, SEEK_SET);
    if (write(nvram_fd, nvram_buf, NVRAM_SIZE) != NVRAM_SIZE)
        goto out;

    ret = 0;
 out:
    close(nvram_fd);
 out_free:
    free(nvram_buf);
    return ret;
}

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
