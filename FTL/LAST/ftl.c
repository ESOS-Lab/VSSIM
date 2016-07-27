// File: ftl.c
// Data: 2013. 05. 06.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2013
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved


#include "common.h"

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
		INIT_RAN_COLD_BLOCK_MAPPING(INIT);
		INIT_RAN_HOT_BLOCK_MAPPING(INIT);
		INIT_LOG_BLOCK_INDEX();

		INIT_HOT_PAGE_LIST();

		INIT_SEQ_LOG_MAPPING(INIT);
		INIT_RAN_COLD_LOG_MAPPING(INIT);
		INIT_RAN_HOT_LOG_MAPPING(INIT);

		INIT_PERF_CHECKER();

#ifdef FIRM_IO_BUFFER
		INIT_IO_BUFFER();
#endif
#ifdef MONITOR_ON
		INIT_LOG_MANAGER();
#endif
		g_init = 1;

#ifdef DEBUG_MODE3
		fp_dbg3_ran = fopen("./data/l_dbg3_ran.txt","a");
		fp_dbg3_1_ran = fopen("./data/l_dbg3_1_ran.txt","a");
#endif
#ifdef DEBUG_MODE4
		fp_dbg4_seq = fopen("./data/l_dbg4_seq.txt","a");
		fp_dbg4_ran = fopen("./data/l_dbg4_ran.txt","a");
#endif
#ifdef DEBUG_MODE5
		fp_dbg5_seq = fopen("./data/l_dbg5_seq.txt","a");
		fp_dbg5_ran = fopen("./data/l_dbg5_ran.txt","a");
#endif
#ifdef DEBUG_MODE6
		fp_dbg6_seq = fopen("./data/l_dbg6_seq.txt","a");
		fp_dbg6_ran = fopen("./data/l_dbg6_ran.txt","a");
#endif
#ifdef DEBUG_MODE8
		fp_dbg8_seq = fopen("./data/l_dbg8_seq.txt","a");
		fp_dbg8_ran = fopen("./data/l_dbg8_ran.txt","a");
#endif
		SSD_IO_INIT();
		printf("[FTL_INIT] Complete\n");
	}
}

void FTL_TERM(void)
{
	printf("[FTL_TERM] Start\n");

#ifdef FIRM_IO_BUFFER
	TERM_IO_BUFFER();
#endif

	TERM_DATA_BLOCK_MAPPING();
	TERM_VALID_ARRAY();
	TERM_INVERSE_BLOCK_MAPPING();

	TERM_EMPTY_BLOCK_LIST();

	TERM_SEQ_BLOCK_MAPPING();
	TERM_RAN_COLD_BLOCK_MAPPING();
	TERM_RAN_HOT_BLOCK_MAPPING();
	TERM_LOG_BLOCK_INDEX();

	TERM_HOT_PAGE_LIST();

	TERM_SEQ_LOG_MAPPING();
	TERM_RAN_COLD_LOG_MAPPING();
	TERM_RAN_HOT_LOG_MAPPING();

	TERM_PERF_CHECKER();
#ifdef MONITOR_ON
	TERM_LOG_MANAGER();
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
#ifdef DEBUG_MODE8
	fclose(fp_dbg8_seq);
	fclose(fp_dbg8_ran);
#endif
	printf("[FTL_TERM] Complete\n");
}

void FTL_READ(int32_t sector_nb, unsigned int length)
{
	int ret;

#ifdef GET_FTL_WORKLOAD
	FILE* fp_workload = fopen("./data/workload_ftl.txt","a");
	struct timeval tv;
	struct tm *lt;
	double curr_time;
	gettimeofday(&tv, 0);
	lt = localtime(&(tv.tv_sec));
	curr_time = lt->tm_hour*3600 + lt->tm_min*60 + lt->tm_sec + (double)tv.tv_usec/(double)1000000;
	//fprintf(fp_workload,"%lf %d %ld %u %x\n",curr_time, 0, sector_nb, length, 1);
	//fprintf(fp_workload,"%lf %d %u %x\n",curr_time, sector_nb, length, 1);
	fprintf(fp_workload,"%lf %d %u %x R\n",curr_time, sector_nb, length, 1);
	fclose(fp_workload);
#endif
	ret = _FTL_READ(sector_nb, length);
}

void FTL_WRITE(int32_t sector_nb, unsigned int length)
{
	int ret;

#ifdef GET_FTL_WORKLOAD
	FILE* fp_workload = fopen("./data/workload_ftl.txt","a");
	struct timeval tv;
	struct tm *lt;
	double curr_time;
	gettimeofday(&tv, 0);
	lt = localtime(&(tv.tv_sec));
	curr_time = lt->tm_hour*3600 + lt->tm_min*60 + lt->tm_sec + (double)tv.tv_usec/(double)1000000;
//	fprintf(fp_workload,"%lf %d %ld %u %x\n",curr_time, 0, sector_nb, length, 0);
	fprintf(fp_workload,"%lf %d %u %x W\n",curr_time, sector_nb, length, 0);
	fclose(fp_workload);
#endif
	ret = _FTL_WRITE(sector_nb, length);
}

int _FTL_READ(int32_t sector_nb, unsigned int length)
{
#ifdef FTL_DEBUG
	printf("[%s] Start\n",__FUNCTION__);
#endif
	if(sector_nb + length > SECTOR_NB){
		printf("Error[FTL_READ] Exceed Sector number\n"); 
		return FAIL;	
	}

	unsigned int remain = length;
	unsigned int lba = sector_nb;
	unsigned int left_skip = sector_nb % SECTORS_PER_PAGE;
	unsigned int right_skip;
	unsigned int read_sects;

	unsigned int phy_flash_nb;
	unsigned int phy_block_nb;
	unsigned int phy_page_nb;

	unsigned int ret = SUCCESS;
	unsigned int ret_seq, ret_ran_hot, ret_ran_cold, ret_data;

	int read_page_offset = 0;
	int io_page_nb;

#ifdef FIRM_IO_BUFFER
	INCREASE_RB_FTL_POINTER(length);
#endif

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
			ret_ran_hot = FIND_PAGE_IN_RAN_HOT_LOG(lba, &phy_flash_nb, &phy_block_nb, &phy_page_nb);
		}

		if(ret_seq != SUCCESS && ret_ran_hot != SUCCESS){
			ret_ran_cold = FIND_PAGE_IN_RAN_COLD_LOG(lba, &phy_flash_nb, &phy_block_nb, &phy_page_nb);
		}

		if(ret_seq != SUCCESS && ret_ran_hot != SUCCESS && ret_ran_cold != SUCCESS){
			ret_data = FIND_PAGE_IN_DATA_BLOCK(lba, &phy_flash_nb, &phy_block_nb, &phy_page_nb);
		}
		
		if(ret_seq != SUCCESS && ret_ran_hot != SUCCESS && ret_ran_cold != SUCCESS && ret_data != SUCCESS){
#ifdef FTL_DEBUG
			printf("Error[FTL_READ] No Mapping info\n");
#endif
#ifdef FIRM_IO_BUFFER
			INCREASE_RB_LIMIT_POINTER();
#endif
			return FAIL;
		}

		lba += read_sects;
		remain -= read_sects;
		left_skip = 0;
	}

	io_alloc_overhead = ALLOC_IO_REQUEST(sector_nb, length, READ, &io_page_nb);

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
			ret_ran_hot = FIND_PAGE_IN_RAN_HOT_LOG(lba, &phy_flash_nb, &phy_block_nb, &phy_page_nb);
		}

		if(ret_seq != SUCCESS && ret_ran_hot != SUCCESS){
			ret_ran_cold = FIND_PAGE_IN_RAN_COLD_LOG(lba, &phy_flash_nb, &phy_block_nb, &phy_page_nb);
		}

		if(ret_seq != SUCCESS && ret_ran_hot != SUCCESS && ret_ran_cold != SUCCESS){
			ret_data = FIND_PAGE_IN_DATA_BLOCK(lba, &phy_flash_nb, &phy_block_nb, &phy_page_nb);
		}
		

		if(ret_seq != SUCCESS && ret_ran_hot != SUCCESS && ret_ran_cold != SUCCESS && ret_data != SUCCESS){
#ifdef FTL_DEBUG
			printf("Error[FTL_READ] No Mapping info\n");
#endif
		}
		else{
			ret = SSD_PAGE_READ(phy_flash_nb, phy_block_nb, phy_page_nb, read_page_offset, READ, io_page_nb);
		}

		if(ret == SUCCESS){
#ifdef FTL_DEBUG
			printf("[FTL_READ] Complete [%d, %d, %d] from ",phy_flash_nb, phy_block_nb, phy_page_nb);

			if(ret_seq == SUCCESS){
				printf("SEQ Block\n");
			}
			else if(ret_ran_hot == SUCCESS){
				printf("RAN HOT Block\n");
			}
			else if(ret_ran_cold == SUCCESS){
				printf("RAN COLD Block\n");
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

#ifdef FIRM_IO_BUFFER
	INCREASE_RB_LIMIT_POINTER();
#endif

#ifdef MONITOR_ON
	char szTemp[1024];
	sprintf(szTemp, "READ PAGE %d ", length);
	WRITE_LOG(szTemp);
#endif

#ifdef FTL_DEBUG
	printf("[%s] End\n",__FUNCTION__);
#endif

	return ret;
}

int _FTL_WRITE(int32_t sector_nb, unsigned int length)
{
#ifdef FTL_DEBUG
	printf("[%s] Start\n",__FUNCTION__);
	int64_t start_dbg, end_dbg;
	start_dbg = get_usec();
#endif
	if(sector_nb + length > SECTOR_NB){
		printf("Error[FTL_WRITE] Exceed Sector number\n");
                return FAIL;
        }

	unsigned int ret = FAIL;
	
	int write_type = 0;

#ifdef FIRM_IO_BUFFER
	INCREASE_WB_FTL_POINTER(length);
#endif

	write_type = SET_WRITE_TYPE(sector_nb, length);

	if(write_type == NEW_SEQ_WRITE || write_type == CONT_SEQ_WRITE){
		/* Write To Seq Log Block */
		ret = WRITE_TO_SEQ_BLOCK(sector_nb, length, write_type);
	}
	else if(write_type == HOT_RAN_WRITE){
		/* Write to Ran Hot Log Block */
		ret = WRITE_TO_RAN_HOT_BLOCK(sector_nb, length);
	}
	else if(write_type == COLD_RAN_WRITE){
		/* Write to Ran Cold Log Block */
		ret = WRITE_TO_RAN_COLD_BLOCK(sector_nb, length);
	}
	else{
		printf("ERROR[FTL_WRITE] Wrong write_type\n");
	}

#ifdef FIRM_IO_BUFFER
	INCREASE_WB_LIMIT_POINTER();
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
	else if(write_type == HOT_RAN_WRITE){
		printf("[FTL_WRITE] HOT_RAN_WRITE ");
	}
	else if(write_type == COLD_RAN_WRITE){
		printf("[FTL_WRITE] COLD_RAN_WRITE ");
	}
	printf("Complete: [%ld]usec\n", end_dbg - start_dbg);
#endif

	return ret;
}
