/*
 * Configuration for Xilinx ZynqMP emulation
 * platforms. See zynqmp-common.h for ZynqMP
 * common configs
 *
 * (C) Copyright 2014 - 2015 Xilinx, Inc.
 * Michal Simek <michal.simek@xilinx.com>
 * Siva Durga Prasad Paladugu <sivadur@xilinx.com>
 *
 * Based on Configuration for Versatile Express
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CONFIG_ZYNQMP_EP_H
#define __CONFIG_ZYNQMP_EP_H

#define CONFIG_ZYNQ_GEM0
#define CONFIG_ZYNQ_GEM_PHY_ADDR0	7

#define CONFIG_ZYNQ_SERIAL_UART0
#define CONFIG_ZYNQ_SDHCI0
#define CONFIG_ZYNQ_I2C0
#define CONFIG_SYS_I2C_ZYNQ
#define CONFIG_ZYNQ_EEPROM
#define CONFIG_AHCI

#include <configs/xilinx_zynqmp.h>

#endif /* __CONFIG_ZYNQMP_EP_H */
