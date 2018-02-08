// File: ssd_log_manager.h
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#ifndef _LOG_MANAGER_H_
#define _LOG_MANAGER_H_

void INIT_LOG_MANAGER(void);
void TERM_LOG_MANAGER(void);
void WRITE_LOG(char* szLog);

void SSD_MONITOR_SERVER(void* arg);
void SSD_MONITOR_CLIENT(void* arg);

#endif
