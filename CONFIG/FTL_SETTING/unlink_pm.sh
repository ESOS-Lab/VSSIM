# File: unlink_pm
# Date: 2014. 12. 03.
# Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
# Copyright(c)2014
# Hanyang University, Seoul, Korea
# Embedded Software Systems Laboratory. All right reserved

## VSSIM source code unlink script
#!/bin/bash
# This file used for unlinking : QEMU <-- // --> SSD FTL SOURCE
# For "SSD PAGE MAPPING FTL"
# Usage : Just typing your shell -> " $./unlink_pm.sh "

QEMU_HW_DIR="../../QEMU/hw/block"
EXEC_DIR="../../QEMU"

# Erase Makefile.target for page mapping.
unlink ${QEMU_HW_DIR}/Makefile.objs

# ----- Unlinking -----
# HEADER and SOURCE FILES in COMMON Directory
unlink ${QEMU_HW_DIR}/common.h
unlink ${QEMU_HW_DIR}/common.c
unlink ${QEMU_HW_DIR}/vssim_type.h
unlink ${QEMU_HW_DIR}/bitmap.h
unlink ${QEMU_HW_DIR}/bitmap.c

# HEADER FILE
unlink ${QEMU_HW_DIR}/ftl_type.h
unlink ${QEMU_HW_DIR}/ftl.h
unlink ${QEMU_HW_DIR}/ftl_mapping_manager.h
unlink ${QEMU_HW_DIR}/ftl_flash_manager.h
unlink ${QEMU_HW_DIR}/ftl_gc_manager.h
unlink ${QEMU_HW_DIR}/ftl_cache.h
unlink ${QEMU_HW_DIR}/ftl_perf_manager.h
unlink ${QEMU_HW_DIR}/flash_memory.h
unlink ${QEMU_HW_DIR}/firm_buffer_manager.h
unlink ${QEMU_HW_DIR}/ssd.h
unlink ${QEMU_HW_DIR}/vssim_core.h

# SOURCE FILLE
unlink ${QEMU_HW_DIR}/ftl.c
unlink ${QEMU_HW_DIR}/ftl_mapping_manager.c
unlink ${QEMU_HW_DIR}/ftl_flash_manager.c
unlink ${QEMU_HW_DIR}/ftl_gc_manager.c
unlink ${QEMU_HW_DIR}/ftl_cache.c
unlink ${QEMU_HW_DIR}/ftl_perf_manager.c
unlink ${QEMU_HW_DIR}/flash_memory.c
unlink ${QEMU_HW_DIR}/firm_buffer_manager.c
unlink ${QEMU_HW_DIR}/ssd.c
unlink ${QEMU_HW_DIR}/vssim_core.c

# Remove monitor 
unlink ${EXEC_DIR}/ssd_monitor
unlink ${QEMU_HW_DIR}/ssd_log_manager.h
unlink ${QEMU_HW_DIR}/ssd_log_manager.c

# Delete config file
unlink ${EXEC_DIR}/ssd.conf
unlink ${QEMU_HW_DIR}/vssim.h
unlink ${QEMU_HW_DIR}/vssim_config_manager.h
unlink ${QEMU_HW_DIR}/vssim_config_manager.c
