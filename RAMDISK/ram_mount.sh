# File: ram_mount.sh
# Date: 15-Sep-2017
# Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
# Copyright(c)2017
# Hanyang University, Seoul, Korea
# Embedded Software Systems Laboratory. All right reserved

mkdir mnt
chmod 0755 mnt
sudo mount -t tmpfs -o size=16g tmpfs ./mnt
