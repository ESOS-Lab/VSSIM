VSSIM: Virtual machine based SSD SIMulator
-----
* Maintainer : Jinsoo Yoo (jedisty@hanyang.ac.kr)
* Contributor : Joongwoo Hwang, Haesung Kim, Joohyun Kim 

VSSIM is an SSD Simulator performed with full virtualized system based on QEMU. VSSIM operates on top of QEMU/KVM with software based SSD module as an IDE device. VSSIM runs in real-time and allows the user to measure both the host performance and SSD behavior under various design choices.
 By using running virtual machine as a simulation environment, VSSIM can process workload in realtime and preserve the system status after a session of experiment, in other words, VSSIM provides methods to module the actual behavior of the SSD. 

What is the merit?
-----
VSSIM can flexibly model the various hardware components, e.g. the number of channels, the number of ways, block size, page size, the number of planes per chip, program, erase, read latency of NAND cells, channel switch delay, and way switch delay. VSSIM can also facilitate the implementation of the SSD firmware algorithms. 

Publication
-----
* Jinsoo Yoo, Youjip Won, Joongwoo Hwang, Sooyong Kang, Jongmoo Choi, Sungroh Yoon and Jaehyuk Cha, VSSIM: Virtual Machine based SSD Simulator In Proc. of Mass Storage Systems and Technologies (MSST), 2013 IEEE 29th Symposium on, Long Beach, California, USA, May 6-10, 2013

* Joohyun Kim, Haesung Kim, Seongjin Lee, Youjip Won, FTL Design for TRIM Command In Proc. of Software Support for Portable Storage (IWSSPS), 2010 15th International Workshop on, Scottsdale, AZ, USA, October 28, 2010

Acknowledgement
-----
* This work is sponsored by IT R&D program MKE/KEIT. [No.10035202, Large Scale hyper-MLC SSD Technology Development].

Architecture
-----
![VSSIM Architecture]( http://dmclab.hanyang.ac.kr/wikidata/img/vssim_architecture.jpg)

User Guide
-----
#### Settings

The setting was recorded in a Linux environment as follows.
- Linux OS: Ubuntu 14.04
- Kernel Version: 3.13.0-34-generic

1. VSSIM Code Download
Download the latest version from github

        $ git clone https://github.com/ESOS-Lab/VSSIM.git

2. Compile /Execution Setting

- QEMU, KVM Installation

        $ sudo apt-get install qemu
        $ sudo apt-get install qemu-kvm

- Qt5 Installation

        $ sudo apt-get install qt5-default qttools5-dev-tools

- Resolving Library Dependency

        $ sudo apt-get install zlib1g-dev libsdl-image1.2-dev libgnutls-dev libvncserver-dev libpci-dev

#### Folder Composition

![Folder Composition]( http://dmclab.hanyang.ac.kr/wikidata/img/folder_arch_git.jpg)

1. CONFIG: In CONFIG folder, there is ssd.conf file, which is used to configurate virtual SSD, and a source code that uses this file to design virtual SSD.

2. FIRMWARE: In FIRMWARE folder, there is firmware(IO Buffer) source code.

3. FTL: In FTL folder, subsequent folders of FTL_SOURCE folder are COMMON, PAGE_MAP, PERF_MODULE, and QEMU_MAKEFILE folder.

    * COMMON: There is ‘common.h’ file that includes FTL header file.

    * PAGE_MAP: There is Page Mapping FTL Code and Garbage Collection Code.

    * PERF_MODULE: There is a source code of VSSIM Performance Module, which manages information on VSSIM’s SSD behavior and transfers this to monitor.

    * QEMU_MAKER: There is Makefile, which QEMU uses to compile FTL code.

4. MONITOR: There is a source code of SSD Monitor, which is a graphic user interface.

5. OS: This is a folder where iso files of necessary OS are located when VSSIM installs Guest OS.

6. QEMU: QEMU related source code is located.

7. RAMDISK: There is a Shell script that creates Ramdisk and executes mount.

8. SSD_MODULE: There is SSD IO Manager related source code that emulates SSD’s NAND IO operation, and also SSD Log Manager related code, which is a communication source code that transfers virtual SSD’s operation to SSD Monitor. 

#### Virtual SSD Setting

This section explains about the structure of virtual SSD, in other words, the section will describe how to set the number of flash memories, the number of channels, the number of ways, page size, and etc. Virtual SSD Setting is done by editing ‘ssd.conf file in VSSIM_V/CONFIG. What each parameter refers to in the file is shown as follow.

1. ssd.conf File:

    - FILE_NAME_HDA: the path which virtual ssd image for hda is created
    - FILE_NAME_HDB: the path which virtual ssd image for hdb is created
    - PAGE_SIZE: the size of one page (Byte)
    - SECTOR_SIZE: the size of one sector (Byte)
    - FLASH_NB: the number of flash memories in a whole SSD (unit)
    - BLOCK_NB: the number of blocks per flash memory (unit)
    - PLANES_PER_FLASH: the number of planes per flash memory (unit)
    - REG_WRITE_DELAY: delay in register write (usec)
    - CELL_PROGRAM_DELAY: delay in nand page write (usec)
    - REG_READ_DELAY: delay in register read (usec)
    - CELL_READ_DELAY: delay in nand page read (usec)
    - BLOCK_ERASE_DELAY: delay in nand block erase (usec)
    - CHANNEL_SWITCH_DELAY_R: delay in channel switch during read operation (usec)
    - CHANNEL_SWITCH_DELAY_W: delay in channel switch during write operation (usec)
    - CHANNEL_NB: the number of channels in SSD (usec)
    - WRITE_BUFFER_FRAME_NB: the number of buffer frame for write operation (sector)
    - READ_BUFFER_FRAME_NB: the number of buffer frame for read operation (sector)
    - OVP: Over provisioning percentage (%)


#### Compile / Execution

This section explains the entire process of actually compiling VSSIM and making it operate in QEMU.

1. Monitor Compile

    Location: Inside the VSSIM/MONITOR/SSD_MONITOR_PM folder, there is a GUI(Graphic User Interface) written in Qt5 to show VSSIM operation conditions.

        $ cd VSSIM/MONITOR/
        $ qmake
        $ make

2. FTL Setting

    VSSIM is modularized to make it easy for the user to easily change FTL. One can change FTL using simple link/unlink script.

        $ cd VSSIM/CONFIG/FTL_SETTING/
        $ ./link_pm.sh

3. OS Image File Preparation

    OS image file is needed to execute VSSIM

    Location: Place an OS image file under VSSIM/OS folder. (VSSIM Code distribution version does not include an OS image.)

    Example of an OS Image File:

        MW_WIN7_AIO_FINAL_FF_DVD.iso
        ubuntu-16.04.1-desktop-amd64.iso

4. QEMU Compile

    QEMU Compile should be done in prior to executing VSSIM. Before compiling the QEMU, we configure the QEMU with x86_64-softmmu target option.

        $ cd VSSIM/QEMU/
        $ ./configure --target-list=x86_64-softmmu
        $ make clean
        $ make

5. Ramdisk Formation

    We recommend running on Ramdisk for quick execution of VSSIM. When VSSIM is executed on hard disk, the performance of simulator can fall greatly. Formation of Ramdisk uses Shell script that is included in VSSIM/RAMDISK folder, the contents of which are as follows.

        mkdir mnt
        chmod 0755 mnt
        sudo mount -t tmpfs -o size=20g tmpfs ./mnt


    When Shell script of ram-mount.sh is executed, ‘rd’ folder is created and 8GByte sized Ramdisk is created as tmpfs filesystem. If you wish to alter the size of Ramdisk, you can change the part that is written as ‘size=20g’ in Shell script. (g refers to GByte.)

        $ cd VSSIM/RAMDISK
        $ ./ram_mount.sh

6. VSSIM Execution

        $ cd VSSIM/
        $ ./vssim_run.sh

    Cautions: "vssim_run.sh"file is as follows

        # PATHs
        MNT="./RAMDISK/mnt"
        QEMU_RUN_OPTION="-m 1024 -enable-kvm -vga cirrus -device nvme,drive=nvme1,serial=foo"
        QEMU_IMG="16GB.img"
        QEMU_DIR="./QEMU/x86_64-softmmu"
        OS_DIR="./OS"
        OS_IMG="ubuntu-16.04.1-desktop-amd64.iso"
        # Create QEMU disk
        ./QEMU/qemu-img create -f qcow2 ${MNT}/${QEMU_IMG} 16G
        # Run VSSIM
        sudo ${QEMU_DIR}/qemu-system-x86_64 -drive file=${MNT}/${QEMU_IMG},if=none,id=nvme1 -cdrom ${OS_DIR}/${OS_IMG} ${QEMU_RUN_OPTION}

  
    ubuntu-16.04.1-desktop-amd64.iso File should be located in VSSIM/OS. 


#### Error Settlement

1. Failure to connect with SSD Monitor

    Check port number of SSD Monitor and IP setting

        $ cd VSSIM/MONITOR/SSD_MONITOR_PM/
        $ vi monitorform.cpp
        // Check port number and IP setting in 34th line
        // socket->connetToHost(“127.0.0.1”, 9995)

    Check port number setting of SSD Log Manager

        $ cd /VSSIM/SSD_MODULE/
        $ vi ssd_log_manager.c

    Check port number in 76th line, and see if it it identical to the port number setting of SSD Monitor

        serverAddr.sin_port = htons(9995);

    ※ Port number that was set could be in use in other applications. Change the port number and try executing. (ex, 9990~9998) 


Course Material
-----
VSSIM is designed to be used as an excellent experiment platform to study the various aspects of an SSD. We provide several labs to help the instructor lead the class. VSSIM is open source and you are welcome to share the lab materials which you have developed. 
 
#### Lab1.
 
Configure 8 channel, 2 way SSD. Page size is 4 KByte. NAND Latency is set to 300 usec, 900 usec, and 2.5 msec for read, program and erase. Channel switch delay and way switch delay are 20 msec, and 80 msec, respectively. 

 
    1. Install Linux operating system on SSD. Measure the time to install Linux OS.
    2. Draw the CDF of IO sizes generated during Linux installation.
    3. Determine the number of write and read operations in installing Linux OS.
    4. Configure 2 channel and 8 way SSD and repeat the experiments 1, 2, and 3 again.
    5. Configure 4 channel and 4 way SSD and repeat the experiments 1, 2, and 3 again.
    6. Discuss the effect the channel and way parallelism over SSD performance and its implication on host performance. 

