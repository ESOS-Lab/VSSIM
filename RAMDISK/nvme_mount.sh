# File: nvme_mount.sh
# Date: 15-Sep-2017
# Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
# Copyright(c)2017
# Hanyang University, Seoul, Korea
# Embedded Software Systems Laboratory. All right reserved

#!/bin/bash

DEV="/dev/nvme0n1"
MNT="./mnt"

mkdir ${MNT}
sudo chmod 0755 ${MNT}
#sudo mkfs -t ext4 ${DEV}
sudo mount -t ext4 ${DEV} ${MNT}
