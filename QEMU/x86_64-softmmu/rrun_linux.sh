## VSSIM Linux re-run script
## Copyright(c)2014
## Hanyang University, Seoul, Korea
## Embedded Software Systems Laboratory. All right reserved

./qemu-system-x86_64 -m 2048 -cpu core2duo -hda ../../RAMDISK/rd/ssd_hda.img -cdrom ../../OS/ubuntu-10.04.1-desktop-i386.iso -usbdevice tablet
