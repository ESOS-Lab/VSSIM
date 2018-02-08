// File: ssd_log_manager.c
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "ssd_log_manager.h"
#include "common.h"

#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <memory.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>

int servSock;
int clientSock;

pthread_t thread_ID;

int g_server_create = 0;
int g_init_log_server = 0;

void INIT_LOG_MANAGER(void)
{
	if(g_init_log_server == 0){
		SSD_MONITOR_SERVER(NULL);

		g_init_log_server = 1;
	}
}

void TERM_LOG_MANAGER(void)
{
	close(servSock);
	close(clientSock);
}

void WRITE_LOG(char* szLog)
{
	int ret1, ret2;
	if(g_server_create == 0){
		printf(" write log is failed\n");
		return;
	}
	ret1 = send(clientSock, szLog, strlen(szLog), 0);
	ret2 = send(clientSock, "\n", 1, MSG_DONTWAIT);

	if(ret1 == -1 || ret2 == -1)
		printf("[%s] send fail! \n", __FUNCTION__);
}

void SSD_MONITOR_SERVER(void* arg)
{
#ifdef MNT_DEBUG
	printf("[%s] SERVER THREAD CREATED!\n", __FUNCTION__);
#endif	
	FILE* fp;
	unsigned int len;
	struct sockaddr_in serverAddr;
	struct sockaddr_in clientAddr;

	if((servSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
#ifdef MNT_DEBUG
		printf("ERROR[%s] Server Socket Creation error!\n", __FUNCTION__);
#endif
	}

	int flags = fcntl(servSock, F_GETFL, 0);

	fcntl(servSock, F_SETFL, flags | O_APPEND);

	int option = 1;
	setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
	memset(&serverAddr, 0x00, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(9997);
	
	if(bind(servSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0){
#ifdef MNT_DEBUG
		printf("ERROR[%s] Server Socket Bind Error!\n", __FUNCTION__);
		return;
#endif
	}

	if(listen(servSock, 100)<0){
#ifdef MNT_DEBUG
		printf("ERROR[%s] Server Socket Listen Error!!!\n", __FUNCTION__);
#endif
	}

	/* Excute the ssd monitor */
	fp = popen("./QEMU/ssd_monitor", "r");
	if(fp == NULL){
		printf("ERROR[%s] ssd monitor open fail!\n", __FUNCTION__);
		return;
	}

#ifdef MNT_DEBUG
	printf("[%s] Wait for client....[%d]\n", __FUNCTION__, servSock);
#endif
	clientSock = accept(servSock, (struct sockaddr*) &clientAddr, &len);
#ifdef MNT_DEBUG
	printf("[%s] Connected![%d]\n", __FUNCTION__, clientSock);
//	printf("[%s] Error No. [%d]\n", errno);
#endif	
	g_server_create = 1;
}

void SSD_MONITOR_CLIENT(void* arg)
{
	int a = *(int*)arg;
#ifdef MNT_DEBUG
	printf("[%s] ClientSock[%d]\n", __FUNCTION__, a);
#endif
	send(a, "test\n", 5, 0);
}
