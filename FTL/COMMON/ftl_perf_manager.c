// File: ftl_perf_manager.c
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

/* Calculate IO Latency */
double avg_read_bandwidth;
double avg_write_bandwidth;

uint64_t total_read_latency;
uint64_t total_write_latency;

uint64_t total_read_pages;
uint64_t total_write_pages;

uint64_t* arr_read_latency;
uint64_t* arr_write_latency;

uint64_t* arr_read_pages;
uint64_t* arr_write_pages;

int idx_read_bandwidth;
int idx_write_bandwidth;

/* Send to Monitor  */
int64_t log_read_page_val;
int64_t log_read_request_val;
int64_t log_write_page_val;
int64_t log_write_request_val;
int64_t log_gc_amp_val;
int64_t log_gc_call;
int64_t log_erase_val;

/* SSD Util */
double ssd_util;
int64_t n_total_written_pages;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER; 

int INIT_PERF_CHECKER(void){
	
	int i;
	int ret;

	avg_read_bandwidth = 0;
	avg_write_bandwidth = 0;

	total_read_latency = 0;
	total_write_latency = 0;
	total_read_pages = 0;
	total_write_pages = 0;

	ssd_util = 0;

	/* Alloc bw array */
	arr_read_latency = (uint64_t*)calloc(BANDWIDTH_WINDOW, sizeof(uint64_t));
	arr_write_latency = (uint64_t*)calloc(BANDWIDTH_WINDOW, sizeof(uint64_t));
	arr_read_pages = (uint64_t*)calloc(BANDWIDTH_WINDOW, sizeof(uint64_t));
	arr_write_pages = (uint64_t*)calloc(BANDWIDTH_WINDOW, sizeof(uint64_t));

	/* Init the bw array */
	for(i=0; i<BANDWIDTH_WINDOW; i++){
		arr_read_latency[i] = 0;
		arr_write_latency[i] = 0;
		arr_read_pages[i] = 0;
		arr_write_pages[i] = 0;
	}

	/* Init the index for bw array */
	idx_read_bandwidth = 0;
	idx_write_bandwidth = 0;

	/* Init log variables for SSD Monitor  */
	log_read_page_val = 0;
	log_read_request_val = 0;
	log_write_page_val = 0;
	log_write_request_val = 0;
	log_gc_amp_val = 0;
	log_gc_call = 0;
	log_erase_val = 0;

	FILE* fp_perf = fopen("./META/perf_manager.dat", "r");
	if(fp_perf != NULL){
		ret = fread(&n_total_written_pages, sizeof(int64_t), 1, fp_perf);
		if(ret == -1){
			printf("ERROR[%s] Read the number of written page fail\n",__FUNCTION__);
			return -1;
		}
		fclose(fp_perf);
		return 1;
	}
	else{
		n_total_written_pages = 0;
		return 0;
	}
}

void TERM_PERF_CHECKER(void){

	printf("Average Read Latency	%.3lf us\n", avg_read_bandwidth);
	printf("Average Write Latency	%.3lf us\n", avg_write_bandwidth);

#ifdef GC_DEBUG
	printf("Total GC Latency	%ld us\n", t_total_gc);
#endif

	free(arr_read_latency);
	free(arr_write_latency);

	FILE* fp_perf_term = fopen("./META/perf_manager.dat","w");
	if(fp_perf_term == NULL){
		printf("ERROR[%s] File open fail\n", __FUNCTION__);
		return;
	}

	fwrite(&n_total_written_pages, sizeof(int64_t), 1, fp_perf_term);
	fclose(fp_perf_term);
}


double GET_IO_BANDWIDTH(uint64_t n_pages, uint64_t latency)
{
	double bw;

	if(latency != 0)
		bw = ((double)PAGE_SIZE * n_pages*1000000)/(latency*1024*1024);
	else
		bw = 0;

	return bw;

}

void INSERT_IO_BANDWIDTH_INFO(int io_type, uint32_t n_pages, int64_t latency)
{
	if(io_type == READ){
		arr_read_latency[idx_read_bandwidth] = latency;
		arr_read_pages[idx_read_bandwidth] = n_pages;

		idx_read_bandwidth++;
		if(idx_read_bandwidth == BANDWIDTH_WINDOW){
			idx_read_bandwidth = 0;
		}

		total_read_latency += latency;
		total_read_pages += n_pages;
	
		total_read_latency -= arr_read_latency[idx_read_bandwidth];
		total_read_pages -= arr_read_pages[idx_read_bandwidth];

		avg_read_bandwidth = GET_IO_BANDWIDTH(total_read_pages, total_read_latency);
	}
	
	if(io_type == WRITE){
		arr_write_latency[idx_write_bandwidth] = latency;
		arr_write_pages[idx_write_bandwidth] = n_pages;

		idx_write_bandwidth++;
		if(idx_write_bandwidth == BANDWIDTH_WINDOW){
			idx_write_bandwidth = 0;
		}

		total_write_latency += latency;
		total_write_pages += n_pages;

		total_write_latency -= arr_write_latency[idx_write_bandwidth];
		total_write_pages -= arr_write_pages[idx_write_bandwidth];

		avg_write_bandwidth = GET_IO_BANDWIDTH(total_write_pages, total_write_latency);
	}
}

void UPDATE_BW(int io_type, uint32_t n_pages, int64_t latency)
{
	INSERT_IO_BANDWIDTH_INFO(io_type, n_pages, latency);
}

void UPDATE_LOG(int log_type, int64_t arg)
{
	static int64_t recent_update_time = 0;

	pthread_mutex_lock(&log_lock);

	switch(log_type){
		case LOG_READ_PAGE:
			log_read_page_val += arg;
			log_read_request_val++;
			break;
		case LOG_WRITE_PAGE:
			log_write_page_val += arg;
			log_write_request_val++;

			n_total_written_pages += arg;
			break;
		case LOG_GC_AMP:
			log_gc_amp_val += arg;
			log_gc_call++;

			n_total_written_pages += arg;
			break;
		case LOG_ERASE_BLOCK:
			log_erase_val += arg;
			n_total_written_pages -= N_PAGES_PER_BLOCK;
			break;
		default:
			break;
	}

	/* added to set interval between sending logs to monitor */
	int64_t current_time = get_usec();
	if(current_time - recent_update_time >= UPDATE_FREQUENCY){

		int i;
		int n_empty_blocks = 0;

		for(i=0; i<N_IO_CORES; i++){
			n_empty_blocks += n_total_empty_blocks[i];
		}

//		ssd_util = (double)((double)n_total_written_pages / N_TOTAL_PAGES)*100;
		ssd_util = (double)(((double)N_TOTAL_BLOCKS - n_empty_blocks)/N_TOTAL_BLOCKS)*100;

		recent_update_time = current_time;
		SEND_LOG_TO_MONITOR();
	}

	pthread_mutex_unlock(&log_lock);
}


void SEND_LOG_TO_MONITOR(void)
{
	char szTemp[1024];
	sprintf(szTemp, "READ PAGE %ld ", log_read_page_val);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "READ REQ %ld ", log_read_request_val);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "WRITE PAGE %ld ", log_write_page_val);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "WRITE REQ %ld ", log_write_request_val);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "GC AMP %ld", log_gc_amp_val);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "GC CALL %ld", log_gc_call);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "GC ERASE %ld", log_erase_val);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "UTIL %lf ", ssd_util);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "READ BW %lf ", avg_read_bandwidth);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "WRITE BW %lf ", avg_write_bandwidth);
	WRITE_LOG(szTemp);
} 
