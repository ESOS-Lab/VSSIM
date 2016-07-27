// File: ftl.c
// Data: 2013. 05. 06.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2013
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

#ifdef DEBUG_MODE2
FILE* fp_dbg2_seq;
FILE* fp_dbg2_ran;
#endif
#ifdef DEBUG_MODE3
FILE* fp_dbg3_ran;
FILE* fp_dbg3_1_ran;
#endif
#ifdef DEBUG_MODE4
FILE* fp_dbg4_seq;
FILE* fp_dbg4_ran;
#endif
#ifdef DEBUG_MODE5
FILE* fp_dbg5_seq;
FILE* fp_dbg5_ran;
#endif
#ifdef DEBUG_MODE6
FILE* fp_dbg6_seq;
FILE* fp_dbg6_ran;
#endif
#ifdef DEBUG_MODE7
FILE* fp_dbg7_r;
FILE* fp_dbg7_w;
FILE* fp_dbg7_t;
#endif
#ifdef DEBUG_MODE8
FILE* fp_dbg8_seq;
FILE* fp_dbg8_ran;
#endif

int g_init = 0;
void FTL_INIT(void)
{
	if(g_init == 0){
        	printf("[FTL_INIT] Start\n");

		INIT_SSD_CONFIG();	

		INIT_DATA_BLOCK_MAPPING();
		INIT_INVERSE_BLOCK_MAPPING();
		INIT_VALID_ARRAY();

		INIT_EMPTY_BLOCK_LIST();

		INIT_SEQ_BLOCK_MAPPING(INIT);
		INIT_RAN_BLOCK_MAPPING(INIT);
		INIT_LOG_BLOCK_INDEX();

		INIT_SEQ_LOG_MAPPING(INIT);
		INIT_RAN_LOG_MAPPING(INIT);

		INIT_PERF_CHECKER();

#ifdef SSD_WRITE_BUFFER
		INIT_WRITE_BUFFER();
#endif
#ifdef HOST_QUEUE
		INIT_EVENT_QUEUE(&e_queue);
#endif
#ifdef MONITOR_ON
		INIT_LOG_MANAGER();
#endif

		g_init = 1;

#ifdef DEBUG_MODE2
                fp_dbg2_seq = fopen("./data/f_dbg2_seq.txt","a");
                fp_dbg2_ran = fopen("./data/f_dbg2_ran.txt","a");
#endif
#ifdef DEBUG_MODE3
		fp_dbg3_ran = fopen("./data/f_dbg3_ran.txt","a");
		fp_dbg3_1_ran = fopen("./data/f_dbg3_1_ran.txt","a");
#endif
#ifdef DEBUG_MODE4
		fp_dbg4_seq = fopen("./data/f_dbg4_seq.txt","a");
		fp_dbg4_ran = fopen("./data/f_dbg4_ran.txt","a");
#endif
#ifdef DEBUG_MODE5
		fp_dbg5_seq = fopen("./data/f_dbg5_seq.txt","a");
		fp_dbg5_ran = fopen("./data/f_dbg5_ran.txt","a");
#endif
#ifdef DEBUG_MODE6
		fp_dbg6_seq = fopen("./data/f_dbg6_seq.txt","a");
		fp_dbg6_ran = fopen("./data/f_dbg6_ran.txt","a");
#endif
#ifdef DEBUG_MODE7
		fp_dbg7_r = fopen("./data/f_dbg7_r.txt","a");
		fp_dbg7_w = fopen("./data/f_dbg7_w.txt","a");
		fp_dbg7_t = fopen("./data/f_dbg7_t.txt","a");
#endif
#ifdef DEBUG_MODE8
		fp_dbg8_seq = fopen("./data/f_dbg8_seq.txt","a");
		fp_dbg8_ran = fopen("./data/f_dbg8_ran.txt","a");
#endif
		SSD_IO_INIT();
		printf("[FTL_INIT] Complete\n");
	}
}

void FTL_TERM(void)
{
	printf("[FTL_TERM] Start\n");

#ifdef SSD_WRITE_BUFFER
	TERM_WRITE_BUFFER();	
#endif
	TERM_DATA_BLOCK_MAPPING();
	TERM_VALID_ARRAY();
	TERM_INVERSE_BLOCK_MAPPING();

	TERM_EMPTY_BLOCK_LIST();

	TERM_SEQ_BLOCK_MAPPING();
	TERM_RAN_BLOCK_MAPPING();
	TERM_LOG_BLOCK_INDEX();

	TERM_SEQ_LOG_MAPPING();
	TERM_RAN_LOG_MAPPING();

	TERM_PERF_CHECKER();
#ifdef MONITOR_ON
	TERM_LOG_MANAGER();
#endif

#ifdef DEBUG_MODE2
        fclose(fp_dbg2_seq);
        fclose(fp_dbg2_ran);
#endif
#ifdef DEBUG_MODE3
	fclose(fp_dbg3_ran);
	fclose(fp_dbg3_1_ran);
#endif
#ifdef DEBUG_MODE4
	fclose(fp_dbg4_seq);
	fclose(fp_dbg4_ran);
#endif
#ifdef DEBUG_MODE5
	fclose(fp_dbg5_seq);
	fclose(fp_dbg5_ran);
#endif
#ifdef DEBUG_MODE6
	fclose(fp_dbg6_seq);
	fclose(fp_dbg6_ran);
#endif
#ifdef DEBUG_MODE7
	fclose(fp_dbg7_r);
	fclose(fp_dbg7_w);
	fclose(fp_dbg7_t);
#endif
#ifdef DEBUG_MODE8
	fclose(fp_dbg8_seq);
	fclose(fp_dbg8_ran);
#endif

	printf("[FTL_TERM] Complete\n");
}

void FTL_READ(int32_t sector_nb, unsigned int length)
{
	int ret;

#ifdef SSD_WRITE_BUFFER
	CHECK_WRITE_BUFFER_FOR_READ(sector_nb, length);
#else
	ret = _FTL_READ(sector_nb, length);
#endif
}

void FTL_WRITE(int32_t sector_nb, unsigned int length)
{
        int ret;

#ifdef SSD_WRITE_BUFFER
	WRITE_TO_BUFFER(sector_nb, length);
#else
	ret = _FTL_WRITE(sector_nb, length);
#endif
}

int _FTL_READ(int32_t sector_nb, unsigned int length)
{
	if(sector_nb + length > SECTOR_NB){
		printf("Error[FTL_READ] Exceed Sector number\n"); 
		return FAIL;	
	}

#ifdef DEBUG_MODE7
	fprintf(fp_dbg7_r,"R\t%ld\t%d\n", sector_nb, length);
	fprintf(fp_dbg7_t,"T\t%ld\t%d\n", sector_nb, length);
#endif

	unsigned int remain = length;
	unsigned int lba = sector_nb;
	unsigned int left_skip = sector_nb % SECTORS_PER_PAGE;
	unsigned int right_skip;
	unsigned int read_sects;

	unsigned int phy_flash_nb;
	unsigned int phy_block_nb;
	unsigned int phy_page_nb;

	unsigned int ret = SUCCESS;
	unsigned int ret_seq, ret_ran, ret_data;
	int read_page_offset = 0;

	while(remain > 0){
	
		if(remain > SECTORS_PER_PAGE - left_skip){
			right_skip = 0;
		}
		else{
			right_skip = SECTORS_PER_PAGE - left_skip - remain;
		}
		read_sects = SECTORS_PER_PAGE - left_skip - right_skip;

		ret_seq = FIND_PAGE_IN_SEQ_LOG(lba, &phy_flash_nb, &phy_block_nb, &phy_page_nb);

		if(ret_seq != SUCCESS){
			ret_ran = FIND_PAGE_IN_RAN_LOG(lba, &phy_flash_nb, &phy_block_nb, &phy_page_nb);
		}

		if(ret_seq != SUCCESS && ret_ran != SUCCESS){
			ret_data = FIND_PAGE_IN_DATA_BLOCK(lba, &phy_flash_nb, &phy_block_nb, &phy_page_nb);
		}
		

		if(ret_seq != SUCCESS && ret_ran != SUCCESS && ret_data != SUCCESS){
#ifdef FTL_DEBUG
			printf("Error[FTL_READ] No Mapping info %ld %d\n", sector_nb, length);
#endif
			return FAIL;
		}

		lba += read_sects;
		remain -= read_sects;
		left_skip = 0;
	}

	io_alloc_overhead = ALLOC_IO_REQUEST(sector_nb, length, READ);

	remain = length;
	lba = sector_nb;
	left_skip = sector_nb % SECTORS_PER_PAGE;

	while(remain > 0){
	
		if(remain > SECTORS_PER_PAGE - left_skip){
			right_skip = 0;
		}
		else{
			right_skip = SECTORS_PER_PAGE - left_skip - remain;
		}
		read_sects = SECTORS_PER_PAGE - left_skip - right_skip;

		ret_seq = FIND_PAGE_IN_SEQ_LOG(lba, &phy_flash_nb, &phy_block_nb, &phy_page_nb);

		if(ret_seq != SUCCESS){
			ret_ran = FIND_PAGE_IN_RAN_LOG(lba, &phy_flash_nb, &phy_block_nb, &phy_page_nb);
		}

		if(ret_seq != SUCCESS && ret_ran != SUCCESS){
			ret_data = FIND_PAGE_IN_DATA_BLOCK(lba, &phy_flash_nb, &phy_block_nb, &phy_page_nb);
		}
		

		if(ret_seq != SUCCESS && ret_ran != SUCCESS && ret_data != SUCCESS){
			printf("Error[FTL_READ] No Mapping info %d %d\n", sector_nb, length);
		}
		else{
			ret = SSD_PAGE_READ(phy_flash_nb, phy_block_nb, phy_page_nb, read_page_offset, READ);
		}

		if(ret == SUCCESS){
#ifdef FTL_DEBUG
			printf("[FTL_READ] Complete [%d, %d, %d] from ",phy_flash_nb, phy_block_nb, phy_page_nb);

			if(ret_seq == SUCCESS){
				printf("SEQ Block\n");
			}
			else if(ret_ran == SUCCESS){
				printf("RAN Block\n");
			}
			else if(ret_data == SUCCESS){
				printf("DATA Block\n");
			}
			else{
				printf("No Map\n");
			}
#endif
		}
		else if(ret == FAIL){
			printf("Error[FTL_READ] %d page read fail \n", phy_page_nb);
		}
		read_page_offset++;

		lba += read_sects;
		remain -= read_sects;
		left_skip = 0;
	}

	INCREASE_IO_REQUEST_SEQ_NB();

#ifdef MONITOR_ON
	char szTemp[1024];
	sprintf(szTemp, "READ PAGE %d ", length);
	WRITE_LOG(szTemp);
#endif

	return ret;
}

int _FTL_WRITE(int32_t sector_nb, unsigned int length)
{
#ifdef FTL_DEBUG
	int64_t start_dbg, end_dbg;
	start_dbg = get_usec();
#endif
	if(sector_nb + length > SECTOR_NB){
		printf("Error[FTL_WRITE] Exceed Sector number\n");
                return FAIL;
        }

#ifdef DEBUG_MODE7
        fprintf(fp_dbg7_w,"W\t%ld\t%d\n", sector_nb, length);
        fprintf(fp_dbg7_t,"T\t%ld\t%d\n", sector_nb, length);
#endif

	unsigned int ret = FAIL;
	int write_type = 0;

	write_type = SET_WRITE_TYPE(sector_nb, length);

#ifdef FTL_FAST1
	unsigned int left_length = length;
	unsigned int each_length;
	int32_t curr_sector_nb = sector_nb;

	while(left_length != 0)
	{

		if(left_length < SECTORS_PER_PAGE)
		{
			each_length = left_length;
			left_length = 0;
		}
		else
		{
			each_length = SECTORS_PER_PAGE;
			left_length -= each_length;
		}

		write_type = SET_WRITE_TYPE(curr_sector_nb, each_length);

		if(write_type == NEW_SEQ_WRITE || write_type == CONT_SEQ_WRITE){
			/* Write To Seq Log Block */
			ret = WRITE_TO_SEQ_BLOCK(curr_sector_nb, each_length, write_type);
		}
		else if(write_type == NEW_RAN_WRITE){
			/* Write to Ran Log Block */
			ret = WRITE_TO_RAN_BLOCK(curr_sector_nb, each_length);
		}
		else{
			printf("ERROR[FTL_WRITE] Wrong write_type\n");
		}

		curr_sector_nb += each_length;
	}
#else

	if(write_type == NEW_SEQ_WRITE || write_type == CONT_SEQ_WRITE){
		/* Write To Seq Log Block */
		ret = WRITE_TO_SEQ_BLOCK(sector_nb, length, write_type);
	}
	else if(write_type == NEW_RAN_WRITE){
		/* Write to Ran Log Block */
		ret = WRITE_TO_RAN_BLOCK(sector_nb, length);
	}
	else{
		printf("ERROR[FTL_WRITE] Wrong write_type\n");
	}
#endif

#ifdef MONITOR_ON
	char szTemp[1024];
	int write_page_nb = length/SECTORS_PER_PAGE;
	sprintf(szTemp, "WRITE PAGE %d ", length);
	WRITE_LOG(szTemp);
	if(length%SECTORS_PER_PAGE)
		write_page_nb += 1;
	sprintf(szTemp, "WB CORRECT %d ", write_page_nb);
	WRITE_LOG(szTemp);
#endif

#ifdef FTL_DEBUG
	end_dbg = get_usec();

	if(write_type == NEW_SEQ_WRITE){
		printf("[FTL_WRITE] NEW_SEQ_WRITE ");
	}
	else if(write_type == CONT_SEQ_WRITE){
		printf("[FTL_WRITE] CONT_SEQ_WRITE ");
	}
	else if(write_type == NEW_RAN_WRITE){
		printf("[FTL_WRITE] RAN_WRITE ");
	}
	printf("Complete: [%ld]usec\n", end_dbg - start_dbg);
#endif
	return ret;
}
