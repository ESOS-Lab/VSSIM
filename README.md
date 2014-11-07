VSSIM: Virtual machine based SSD SIMulator
-----
VSSIM is an SSD Simulator performed with full virtualized system based on QEMU. VSSIM operates on top of QEMU/KVM with software based SSD module as an IDE device. VSSIM runs in real-time and allows the user to measure both the host performance and SSD behavior under various design choices.
 By using running virtual machine as a simulation environment, VSSIM can process workload in realtime and preserve the system status after a session of experiment, in other words, VSSIM provides methods to module the actual behavior of the SSD. 

What is the merit?
-----
VSSIM can flexibly model the various hardware components, e.g. the number of channels, the number of ways, block size, page size, the number of planes per chip, program, erase, read latency of NAND cells, channel switch delay, and way switch delay. VSSIM can also facilitate the implementation of the SSD firmware algorithms. 


Architecture
-----
![VSSIM Architecture]( http://dmclab.hanyang.ac.kr/wikidata/img/vssim_architecture.jpg)

User Guide
-----
### Settings

The setting was recorded in a Linux environment as follows.
- Linux OS: Ubuntu 10.04
- Kernel Version: 2.6.32

1. VSSIM Code Download
Download the latest version from github

    $ git clone https://github.com/ESOS-Lab/VSSIM.git

2. Compile /Execution Setting

- QEMU, KVM Installation

    $ sudo apt-get install qemu
    $ sudo apt-get install qemu-kvm

- Qt3 Installation

    $ sudo apt-get install qt3-dev-tools

- Resolving Library Dependency

    $ sudo apt-get install zlib1g-dev libsdl-image1.2-dev libgnutls-dev libvncserver-dev libpci-dev

3. Code Structure

### Folder Composition

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

### Virtual SSD Setting

3.1 ssd.conf File

4. Compile / Execution

4.1 Monitor Compile

4.2 FTL Setting

4.3 OS Image File Preparation

4.4 QEMU Compile

4.5 Ramdisk Formation

4.6 VSSIM Execution

5. Error Settlement

5.1 In case of libqt-mt.so.3 related error

5.2 Failure to connect with SSD Monitor


Publication
-----
* Jinsoo Yoo, Youjip Won, Joongwoo Hwang, Sooyong Kang, Jongmoo Choi, Sungroh Yoon and Jaehyuk Cha, VSSIM: Virtual Machine based SSD Simulator In Proc. of Mass Storage Systems and Technologies (MSST), 2013 IEEE 29th Symposium on, Long Beach, California, USA, May 6-10, 2013

* Joohyun Kim, Haesung Kim, Seongjin Lee, Youjip Won, FTL Design for TRIM Command In Proc. of Software Support for Portable Storage (IWSSPS), 2010 15th International Workshop on, Scottsdale, AZ, USA, October 28, 2010

