# File: vssim_run.sh
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

# Create QEMU disk
./QEMU/qemu-img create -f qcow2 ${MNT}/${QEMU_IMG} 16G

# Run VSSIM
sudo gdb --args ${QEMU_DIR}/qemu-system-x86_64 -drive file=${MNT}/${QEMU_IMG},if=none,id=nvme1 -cdrom ${OS_DIR}/${OS_IMG} ${QEMU_RUN_OPTION}
