## VSSIM Windows run script
## Copyright(c)2014
## Hanyang University, Seoul, Korea
## Embedded Software Systems Laboratory. All right reserved

#!/bin/bash

rm -rf data/*.dat
qemu-img create -f qcow ../../RAMDISK/rd/ssd_hda.img 20G
./qemu-system-x86_64 -m 2048 -cpu core2duo -hda ../../RAMDISK/rd/ssd_hda.img -cdrom ../../OS/MW_WIN7_AIO_FINAL_FF_DVD.iso -usbdevice tablet
