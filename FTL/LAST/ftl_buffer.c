// File: ftl_buffer.c
// Date: 2018. 02. 08.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2018
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "ftl_buffer.h"
#include "ftl_config_manager.h"
#include "ftl.h"

ftl_buff* g_pHead = NULL;
ftl_buff* g_pTail = NULL;
unsigned int g_ftl_buff_limit = 0;
int buffer_unit_size = sizeof(ftl_buff);

int FTL_BUFFER(int type, int64_t sector_nb, unsigned int length){
	if( g_ftl_buff_limit < FTL_BUFFER_SIZE ){ 
		InsertNode( type, sector_nb, length );
		if( type == READ ){
			g_ftl_buff_limit += buffer_unit_size;
		}else if(type == WRITE){
			g_ftl_buff_limit = g_ftl_buff_limit + buffer_unit_size + length*SECTOR_SIZE;
		}else{
			printf("[FTL_BUFFER][err] type is not valid\n");
		}
	}else{
		FTL_BUFFER_FLUSH();
	}
	return SUCCESS;
}

void FTL_BUFFER_FLUSH(void){
	ftl_buff* pCurrentNode = g_pHead;
	ftl_buff* pDeleteNode =NULL;

#ifdef FTL_BUFFER_DEBUG
	int nCount = 0;
#endif

	while(pCurrentNode != NULL ){
		if( pCurrentNode->type == READ ){
#ifdef FTL_BUFFER_DEBUG
			printf("\t[Flush]FTL_READ is called. [%ld, %u]\n",  pCurrentNode->sector_nb, pCurrentNode->length);
#endif
			FTL_READ( pCurrentNode->sector_nb, pCurrentNode->length );
		}else if(pCurrentNode->type == WRITE){
#ifdef FTL_BUFFER_DEBUG
			FTL_WRITE( pCurrentNode->sector_nb, pCurrentNode->length );
			printf("\t[Flush]FTL_WRITE is called. [%ld, %u]\n",  pCurrentNode->sector_nb, pCurrentNode->length);
#endif
		}else{
			printf("\t[Flush][err] FTL_BUFFER_FLUSH : no matched type\n");
		}
		pDeleteNode = pCurrentNode;
		pCurrentNode = pCurrentNode->next;
		free(pDeleteNode);
#ifdef FTL_BUFFER_DEBUG
		nCount++;
#endif
	}
	g_ftl_buff_limit = 0;
	g_pHead = NULL;
	g_pTail = NULL;
#ifdef FTL_BUFFER_DEBUG
	printf("\t[Flush]buffer is flushed. flushed count is [%d]\n\n",nCount);
#endif
}

void InsertNode(int type, int64_t sector_nb, unsigned int length)
{
	ftl_buff* pNewNode;

	pNewNode = (ftl_buff *)malloc(sizeof(ftl_buff));

	pNewNode->type = type;
	pNewNode->sector_nb = sector_nb;
	pNewNode->length = length;
	pNewNode->next = NULL;

	if( g_pHead == NULL)
	{
		g_pHead = pNewNode;
		g_pTail = pNewNode;
	}else{
		g_pTail->next = pNewNode;
		g_pTail = pNewNode;
	}
	/*
#ifdef FTL_BUFFER_DEBUG
	printf("\t\tInserNode [%d][%ld][%u]\n", pNewNode->type,pNewNode->sector_nb,pNewNode->length);
	printf("\t\tInserNode g_ftl_buff_limit is [%d]\n",g_ftl_buff_limit);
#endif*/
}

