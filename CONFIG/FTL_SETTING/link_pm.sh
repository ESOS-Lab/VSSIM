# File: link_pm
# Date: 2017. 09. 16.
# Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
# Copyright(c)2017
# Hanyang University, Seoul, Korea
# Embedded Software Systems Laboratory. All right reserved

## VSSIM source code link script for Page mapping FTL
#!/bin/bash
# This file used for linking : QEMU <-> SSD FTL SOURCE
# For "SSD PAGE MAPPING FTL"
# Usage : Just typing your shell -> " $./link_pm.sh"

# ------------------- Source File location -----------------------      ----- linked file destination ----

FTL_DIR="../../../FTL/PAGE_MAP"
COMMON_DIR="../../../FTL/COMMON"
FLASH_DIR="../../../FLASH"
FIRMWARE_DIR="../../../FIRMWARE"
CONFIG_DIR="../../../CONFIG"
QEMU_HW_DIR="../../QEMU/hw/block"
EXEC_DIR="../../QEMU"
MONITOR_DIR="../MONITOR"

# Link make file configuration
ln -s ../../../CONFIG/QEMU_MAKEFILE/Makefile_vssim.objs		${QEMU_HW_DIR}/Makefile.objs

# HEADER and SOURCE FILES in COMMON directory
ln -s ${COMMON_DIR}/common.h					${QEMU_HW_DIR}/common.h
ln -s ${COMMON_DIR}/common.c					${QEMU_HW_DIR}/common.c
ln -s ${COMMON_DIR}/bitmap.h					${QEMU_HW_DIR}/bitmap.h
ln -s ${COMMON_DIR}/bitmap.c					${QEMU_HW_DIR}/bitmap.c
ln -s ${COMMON_DIR}/vssim_type.h				${QEMU_HW_DIR}/vssim_type.h
ln -s ${COMMON_DIR}/ftl_perf_manager.h				${QEMU_HW_DIR}/ftl_perf_manager.h
ln -s ${COMMON_DIR}/ftl_perf_manager.c				${QEMU_HW_DIR}/ftl_perf_manager.c

# HEADER FILE
ln -s ${FTL_DIR}/ftl_type.h					${QEMU_HW_DIR}/ftl_type.h
ln -s ${FTL_DIR}/ftl.h						${QEMU_HW_DIR}/ftl.h
ln -s ${FTL_DIR}/ftl_mapping_manager.h				${QEMU_HW_DIR}/ftl_mapping_manager.h
ln -s ${FTL_DIR}/ftl_flash_manager.h				${QEMU_HW_DIR}/ftl_flash_manager.h
ln -s ${FTL_DIR}/ftl_gc_manager.h				${QEMU_HW_DIR}/ftl_gc_manager.h
ln -s ${FTL_DIR}/ftl_cache.h					${QEMU_HW_DIR}/ftl_cache.h

ln -s ${FLASH_DIR}/flash_memory.h				${QEMU_HW_DIR}/flash_memory.h

ln -s ${FIRMWARE_DIR}/ssd.h					${QEMU_HW_DIR}/ssd.h
ln -s ${FIRMWARE_DIR}/vssim_core.h				${QEMU_HW_DIR}/vssim_core.h
ln -s ${FIRMWARE_DIR}/firm_buffer_manager.h			${QEMU_HW_DIR}/firm_buffer_manager.h

# SOURCE FILLE
ln -s ${FTL_DIR}/ftl.c						${QEMU_HW_DIR}/ftl.c
ln -s ${FTL_DIR}/ftl_mapping_manager.c				${QEMU_HW_DIR}/ftl_mapping_manager.c
ln -s ${FTL_DIR}/ftl_flash_manager.c				${QEMU_HW_DIR}/ftl_flash_manager.c
ln -s ${FTL_DIR}/ftl_gc_manager.c				${QEMU_HW_DIR}/ftl_gc_manager.c
ln -s ${FTL_DIR}/ftl_cache.c					${QEMU_HW_DIR}/ftl_cache.c

ln -s ${FLASH_DIR}/flash_memory.c				${QEMU_HW_DIR}/flash_memory.c

ln -s ${FIRMWARE_DIR}/ssd.c					${QEMU_HW_DIR}/ssd.c
ln -s ${FIRMWARE_DIR}/vssim_core.c				${QEMU_HW_DIR}/vssim_core.c
ln -s ${FIRMWARE_DIR}/firm_buffer_manager.c			${QEMU_HW_DIR}/firm_buffer_manager.c

# Monitor setting
ln -s ${MONITOR_DIR}/ssd_monitor_p 				${EXEC_DIR}/ssd_monitor
ln -s ../../${MONITOR_DIR}/ssd_log_manager.h			${QEMU_HW_DIR}/ssd_log_manager.h
ln -s ../../${MONITOR_DIR}/ssd_log_manager.c 			${QEMU_HW_DIR}/ssd_log_manager.c

# SSD_configuration setting
ln -s ../CONFIG/ssd.conf					${EXEC_DIR}/ssd.conf
ln -s ${CONFIG_DIR}/vssim.h					${QEMU_HW_DIR}/vssim.h
ln -s ${CONFIG_DIR}/vssim_config_manager.h			${QEMU_HW_DIR}/vssim_config_manager.h
ln -s ${CONFIG_DIR}/vssim_config_manager.c			${QEMU_HW_DIR}/vssim_config_manager.c
