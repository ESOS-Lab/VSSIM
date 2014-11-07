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
1. Settings

The setting was recorded in a Linux environment as follows.
- Linux OS: Ubuntu 10.04
- Kernel Version: 2.6.32

1.1. VSSIM Code Download

1.2. Compile /Execution Setting

2. Code Structure

2.1 Folder Composition

3. Virtual SSD Setting

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

