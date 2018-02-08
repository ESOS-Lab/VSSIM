# File: vssim_rerun.sh
# Date: 15-Sep-2017
# Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
# Copyright(c)2017
# Hanyang University, Seoul, Korea
# Embedded Software Systems Laboratory. All right reserved

#!/bin/bash

MNT="./RAMDISK/mnt"
QEMU_RUN_OPTION="-m 1024 -enable-kvm -vga cirrus -device nvme,drive=nvme1,serial=foo"
QEMU_IMG="ssd_hda.img"
QEMU_DIR="./QEMU/x86_64-softmmu"
OS_DIR="./OS"
OS_IMG="ubuntu-16.04.1-desktop-amd64.iso"

# Run VSSIM
sudo ${QEMU_DIR}/qemu-system-x86_64 -hda ${MNT}/${QEMU_IMG} -drive file=${MNT}/${QEMU_IMG},if=none,id=nvme1 ${QEMU_RUN_OPTION} 
