// File: ftl_perf_manager.c
// Date: 2014. 12. 03.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

/* Average IO Time */
double avg_write_delay;
double total_write_count;
double total_write_delay;

double avg_read_delay;
double total_read_count;
double total_read_delay;

double avg_gc_write_delay;
double total_gc_write_count;
double total_gc_write_delay;

double avg_gc_read_delay;
double total_gc_read_count;
double total_gc_read_delay;

double avg_seq_write_delay;
double total_seq_write_count;
double total_seq_write_delay;

double avg_ran_write_delay;
double total_ran_write_count;
double total_ran_write_delay;

double avg_ran_cold_write_delay;
double total_ran_cold_write_count;
double total_ran_cold_write_delay;

double avg_ran_hot_write_delay;
double total_ran_hot_write_count;
double total_ran_hot_write_delay;

double avg_seq_merge_read_delay;
double total_seq_merge_read_count;
double total_seq_merge_read_delay;

double avg_ran_merge_read_delay;
double total_ran_merge_read_count;
double total_ran_merge_read_delay;

double avg_seq_merge_write_delay;
double total_seq_merge_write_count;
double total_seq_merge_write_delay;

double avg_ran_merge_write_delay;
double total_ran_merge_write_count;
double total_ran_merge_write_delay;

double avg_ran_cold_merge_write_delay;
double total_ran_cold_merge_write_count;
double total_ran_cold_merge_write_delay;

double avg_ran_hot_merge_write_delay;
double total_ran_hot_merge_write_count;
double total_ran_hot_merge_write_delay;

/* IO Latency */
unsigned int io_request_nb;
unsigned int io_request_seq_nb;

struct io_request* io_request_start;
struct io_request* io_request_end;

/* Calculate IO Latency */
double read_latency_count;
double write_latency_count;

double avg_read_latency;
double avg_write_latency;

double total_read_latency;
double total_write_latency;

int64_t* arr_read_latency;
int64_t* arr_write_latency;
int idx_read_latency;
int idx_write_latency;
int arr_read_full;
int arr_write_full;

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
int64_t written_page_nb;

//extern double ssd_util;

#ifdef PERF_DEBUG1
FILE* fp_perf1_w;
#endif
#ifdef PERF_DEBUG2
FILE* fp_perf2_r;
FILE* fp_perf2_w;
#endif
#ifdef PERF_DEBUG3
FILE* fp_perf3_up;
FILE* fp_perf3_al;
#endif

void INIT_PERF_CHECKER(void){

	int i;

	/* Average IO Time */
	avg_write_delay = 0;
	total_write_count = 0;
	total_write_delay = 0;

	avg_read_delay = 0;
	total_read_count = 0;
	total_read_delay = 0;

	avg_gc_write_delay = 0;
	total_gc_write_count = 0;
	total_gc_write_delay = 0;

	avg_gc_read_delay = 0;
	total_gc_read_count = 0;
	total_gc_read_delay = 0;

	avg_seq_write_delay = 0;
	total_seq_write_count = 0;
	total_seq_write_delay = 0;

	avg_ran_write_delay = 0;
	total_ran_write_count = 0;
	total_ran_write_delay = 0;

	avg_ran_cold_write_delay = 0;
	total_ran_cold_write_count = 0;
	total_ran_cold_write_delay = 0;

	avg_ran_hot_write_delay = 0;
	total_ran_hot_write_count = 0;
	total_ran_hot_write_delay = 0;

	avg_seq_merge_read_delay = 0;
	total_seq_merge_read_count = 0;
	total_seq_merge_read_delay = 0;

	avg_ran_merge_read_delay = 0;
	total_ran_merge_read_count = 0;
	total_ran_merge_read_delay = 0;

	avg_seq_merge_write_delay = 0;
	total_seq_merge_write_count = 0;
	total_seq_merge_write_delay = 0;

	avg_ran_merge_write_delay = 0;
	total_ran_merge_write_count = 0;
	total_ran_merge_write_delay = 0;

	avg_ran_cold_merge_write_delay = 0;
	total_ran_cold_merge_write_count = 0;
	total_ran_cold_merge_write_delay = 0;

	avg_ran_hot_merge_write_delay = 0;
	total_ran_hot_merge_write_count = 0;
	total_ran_hot_merge_write_delay = 0;

	/* IO Latency */
	io_request_nb = 0;
	io_request_seq_nb = 0;

	io_request_start = NULL;
	io_request_end = NULL;

	read_latency_count = 0;
	write_latency_count = 0;

	avg_read_latency = 0;
	avg_write_latency = 0;

	total_read_latency = 0;
	total_write_latency = 0;

	ssd_util = 0;

	/* Alloc latency array */
	arr_read_latency = (int64_t*)calloc(LATENCY_WINDOW, sizeof(int64_t));
	arr_write_latency = (int64_t*)calloc(LATENCY_WINDOW, sizeof(int64_t));

	/* Init the latency array */
	for(i=0; i<LATENCY_WINDOW; i++){
		arr_read_latency[i] = 0;
		arr_write_latency[i] = 0;
	}

	/* Init the index for latency array */
	idx_read_latency = 0;
	idx_write_latency = 0;

	arr_read_full = 0;
	arr_write_full = 0;

	/* Init log variables for SSD Monitor  */
	log_read_page_val = 0;
	log_read_request_val = 0;
	log_write_page_val = 0;
	log_write_request_val = 0;
	log_gc_amp_val = 0;
	log_gc_call = 0;
	log_erase_val = 0;

	FILE* fp_perf = fopen("./data/perf_manager.dat", "r");
	if(fp_perf != NULL){
		fread(&written_page_nb, sizeof(int64_t), 1, fp_perf);
		fclose(fp_perf);
	}
	else{
		written_page_nb = 0;
	}

#ifdef PERF_DEBUG1
	fp_perf1_w = fopen("./data/p_perf1_w.txt","a");
#endif
#ifdef PERF_DEBUG2
	fp_perf2_r = fopen("./data/p_perf2_r.txt","a");
	fp_perf2_w = fopen("./data/p_perf2_w.txt","a");
#endif
#ifdef PERF_DEBUG3
	fp_perf3_al = fopen("./data/p_perf3_al.txt","a");
	fp_perf3_up = fopen("./data/p_perf3_up.txt","a");
#endif
}

void TERM_PERF_CHECKER(void){

	printf("Average Read Latency	%.3lf us\n", avg_read_latency);
	printf("Average Write Latency	%.3lf us\n", avg_write_latency);

	free(arr_read_latency);
	free(arr_write_latency);

	FILE* fp_perf_term = fopen("./data/perf_manager.dat","w");
	if(fp_perf_term==NULL){
		printf("ERROR[%s] File open fail\n", __FUNCTION__);
		return;
	}

	fwrite(&written_page_nb, sizeof(int64_t), 1, fp_perf_term);
	fclose(fp_perf_term);

#ifdef PERF_DEBUG1
	fclose(fp_perf1_w);
#endif
#ifdef PERF_DEBUG2
	fclose(fp_perf2_w);
#endif
#ifdef PERF_DEBUG3
	fclose(fp_perf3_al);
	fclose(fp_perf3_up);
#endif
#ifdef PERF_DEBUG4
	printf("Total Seq Write 		%lf\n", total_seq_write_delay);
	printf("Total Ran Write 		%lf\n", total_ran_write_delay);
	printf("Total Ran Cold Write		%lf\n", total_ran_cold_write_delay);
	printf("Total Ran Hot Write 		%lf\n", total_ran_hot_write_delay);
	printf("Total Seq Merge Write 		%lf\n", total_seq_merge_write_delay);
	printf("Total Ran Merge Write 		%lf\n", total_ran_merge_write_delay);
	printf("Total Ran Cold Merge Write	%lf\n", total_ran_cold_merge_write_delay);
	printf("Total Ran Hot Merge Write 	%lf\n", total_ran_hot_merge_write_delay);
#endif
}

void SEND_TO_PERF_CHECKER(int op_type, int64_t op_delay, int type){

	double delay = (double)op_delay;

	if(type == CH_OP){
		switch(op_type){
			case READ:
				total_read_delay += delay;
				total_read_count++;
				avg_read_delay = total_read_delay / total_read_count;
				break;

			case WRITE:
				total_write_delay += delay;
				break;

			case ERASE:
				break;

			case GC_READ:
				total_gc_read_delay += delay;
				break;

			case GC_WRITE:
				total_gc_write_delay += delay;
				break;

			case SEQ_WRITE:
				total_seq_write_delay += delay;
				break;

			case RAN_WRITE:
				total_ran_write_delay += delay;
				break;

			case RAN_COLD_WRITE:
				total_ran_cold_write_delay += delay;
				break;

			case RAN_HOT_WRITE:
				total_ran_hot_write_delay += delay;
				break;

			case SEQ_MERGE_READ:
				total_seq_merge_read_delay += delay;
				break;

			case RAN_MERGE_READ:
				total_ran_merge_read_delay += delay;
				break;

			case SEQ_MERGE_WRITE:
				total_seq_merge_write_delay += delay;
				break;

			case RAN_MERGE_WRITE:
				total_ran_merge_write_delay += delay;
				break;

			case RAN_COLD_MERGE_WRITE:
				total_ran_cold_merge_write_delay += delay;
				break;

			case RAN_HOT_MERGE_WRITE:
				total_ran_hot_merge_write_delay += delay;
				break;
				
			case MAP_READ:
				break;

			case MAP_WRITE:
				break;

			default:
				break;
		}
	}
	else if(type == REG_OP){
		switch (op_type){
			case READ:
				total_read_delay += delay;
				break;

			case WRITE:
				total_write_delay += delay;
				total_write_count++;
				avg_write_delay = total_write_delay / total_write_count;

				/* Calc SSD Util */
				written_page_nb++;
#ifdef PERF_DEBUG1
				fprintf(fp_perf1_w, "%lf\t%lf\n",total_write_count, avg_write_delay);
#endif
				break;

			case ERASE:
				written_page_nb -= PAGE_NB;
				UPDATE_LOG(LOG_ERASE, 1);
				break;

			case GC_READ:
				total_gc_read_delay += delay;
				total_gc_read_count++;
				avg_gc_read_delay = total_gc_read_delay / total_gc_read_count;
				break;

			case GC_WRITE:
				total_gc_write_delay += delay;
				total_gc_write_count++;
				avg_gc_write_delay = total_gc_write_delay / total_gc_write_count;

				/* Calc SSD Util */
				written_page_nb++;
				break;

			case SEQ_WRITE:
				total_seq_write_delay += delay;
				total_seq_write_count++;
				avg_seq_write_delay = total_seq_write_delay / total_seq_write_count;
			
				/* Calc Monitor Write BW */
				total_write_delay += delay;
				total_write_count++;
				avg_write_delay = total_write_delay / total_write_count;

				/* Calc SSD Util */
				written_page_nb++;
				break;

			case RAN_WRITE:
				total_ran_write_delay += delay;
				total_ran_write_count++;
				avg_ran_write_delay = total_ran_write_delay / total_ran_write_count;
			
				/* Calc Monitor Write BW */
				total_write_delay += delay;
				total_write_count++;
				avg_write_delay = total_write_delay / total_write_count;
			
				/* Calc SSD Util */
				written_page_nb++;
				break;

			case RAN_COLD_WRITE:
				total_ran_cold_write_delay += delay;
				total_ran_cold_write_count++;
				avg_ran_cold_write_delay = total_ran_cold_write_delay / total_ran_cold_write_count;
			
				/* Calc Monitor Write BW */
				total_write_delay += delay;
				total_write_count++;
				avg_write_delay = total_write_delay / total_write_count;
			
				/* Calc SSD Util */
				written_page_nb++;
				break;

			case RAN_HOT_WRITE:
				total_ran_hot_write_delay += delay;
				total_ran_hot_write_count++;
				avg_ran_hot_write_delay = total_ran_hot_write_delay / total_ran_hot_write_count;
			
				/* Calc Monitor Write BW */
				total_write_delay += delay;
				total_write_count++;
				avg_write_delay = total_write_delay / total_write_count;

				/* Calc SSD Util */
				written_page_nb++;
				break;

			case SEQ_MERGE_READ:
				total_seq_merge_read_delay += delay;
				total_seq_merge_read_count++;
				avg_seq_merge_read_delay = total_seq_merge_read_delay / total_seq_merge_read_count;
				break;

			case RAN_MERGE_READ:
				total_ran_merge_read_delay += delay;
				total_ran_merge_read_count++;
				avg_ran_merge_read_delay = total_ran_merge_read_delay / total_ran_merge_read_count;
				break;

			case SEQ_MERGE_WRITE:
				total_seq_merge_write_delay += delay;
				total_seq_merge_write_count++;
				avg_seq_merge_write_delay = total_seq_merge_write_delay / total_seq_merge_write_count;
				
				/* Calc SSD Util */
				written_page_nb++;
				break;

			case RAN_MERGE_WRITE:
				total_ran_merge_write_delay += delay;
				total_ran_merge_write_count++;
				avg_ran_merge_write_delay = total_ran_merge_write_delay / total_ran_merge_write_count;
				
				/* Calc SSD Util */
				written_page_nb++;
				break;
			
			case RAN_COLD_MERGE_WRITE:
				total_ran_cold_merge_write_delay += delay;
				total_ran_cold_merge_write_count++;
				avg_ran_cold_merge_write_delay = total_ran_cold_merge_write_delay / total_ran_cold_merge_write_count;
				
				/* Calc SSD Util */
				written_page_nb++;
				break;

			case RAN_HOT_MERGE_WRITE:
				total_ran_hot_merge_write_delay += delay;
				total_ran_hot_merge_write_count++;
				avg_ran_hot_merge_write_delay = total_ran_hot_merge_write_delay / total_ran_hot_merge_write_count;
				
				/* Calc SSD Util */
				written_page_nb++;
				break;

			case MAP_READ:
				break;

			case MAP_WRITE:
				break;

			default:
				break;
		}
#if defined BLOCK_MAP || defined DA_MAP
		ssd_util = (double)(((double)BLOCK_MAPPING_ENTRY_NB - total_empty_block_nb)/BLOCK_MAPPING_ENTRY_NB)*100;
#else
		ssd_util = (double)((double)written_page_nb / PAGES_IN_SSD)*100;
#endif
	}
	else if(type == LATENCY_OP){
		switch (op_type){
			case READ:
#ifdef PERF_DEBUG2
				fprintf(fp_perf2_r, "%lf\t%lf\t%lf\t%lf\n", avg_read_latency, read_latency_count, delay, GET_IO_BANDWIDTH(avg_read_latency));
#endif

				total_read_latency += delay;
				if(arr_read_full == 0){
					avg_read_latency = total_read_latency / idx_read_latency;
				}
				else{
					total_read_latency -= arr_read_latency[idx_read_latency];
					avg_read_latency = total_read_latency / LATENCY_WINDOW;
				}

				break;
			case WRITE:
#ifdef PERF_DEBUG2
				fprintf(fp_perf2_w, "%lf\t%lf\t%lf\t%lf\n", avg_write_latency, write_latency_count, delay, GET_IO_BANDWIDTH(avg_write_latency));
#endif

				total_write_latency += delay;
				if(arr_write_full == 0){
					avg_write_latency = total_write_latency / idx_write_latency;
				}
				else{
					total_write_latency -= arr_write_latency[idx_write_latency];
					avg_write_latency = total_write_latency / LATENCY_WINDOW;
				}

				break;
			default:
				break;
		}
	}
}

double GET_IO_BANDWIDTH(double delay)
{
	double bw;

	if(delay != 0)
		bw = ((double)PAGE_SIZE*1000000)/(delay*1024*1024);
	else
		bw = 0;

	return bw;

}

int64_t ALLOC_IO_REQUEST(int32_t sector_nb, unsigned int length, int io_type, int* page_nb)
{
	int64_t start = get_usec();
	int io_page_nb = 0;
	unsigned int remain = length;
	unsigned int left_skip = sector_nb % SECTORS_PER_PAGE;
	unsigned int right_skip;
	unsigned int sects;

	io_request* curr_io_request = (io_request*)calloc(1, sizeof(io_request));
	if(curr_io_request == NULL){
		printf("ERROR[%s] Calloc io_request fail\n", __FUNCTION__);
		return 0;
	}

	while(remain > 0){
		if(remain > SECTORS_PER_PAGE - left_skip){
			right_skip = 0;
		}
		else{
			right_skip = SECTORS_PER_PAGE - left_skip - remain;
		}
		sects = SECTORS_PER_PAGE - left_skip - right_skip;

		remain -= sects;
		left_skip = 0;
		io_page_nb++;
	}

	*page_nb = io_page_nb;
	int64_t* start_time_arr = (int64_t*)calloc(io_page_nb, sizeof(int64_t));
	int64_t* end_time_arr = (int64_t*)calloc(io_page_nb, sizeof(int64_t));

	if(start_time_arr == NULL || end_time_arr == NULL){
		printf("ERROR[%s] Calloc time array fail\n", __FUNCTION__);
		return 0;
	}
	else{
		memset(start_time_arr, 0, io_page_nb);
		memset(end_time_arr, 0, io_page_nb);
	}

	curr_io_request->request_nb = io_request_seq_nb;
	
	curr_io_request->request_type = io_type;
	curr_io_request->request_size = io_page_nb;
	curr_io_request->start_count = 0;
	curr_io_request->end_count = 0;
	curr_io_request->start_time = start_time_arr;
	curr_io_request->end_time = end_time_arr;
	curr_io_request->next = NULL;

	if(io_request_start == NULL){
		io_request_start = curr_io_request;
		io_request_end = curr_io_request;
	}
	else{
		io_request_end->next = curr_io_request;
		io_request_end = curr_io_request;
	}
	io_request_nb++;

	int64_t end = get_usec();

#ifdef PERF_DEBUG3
	fprintf(fp_perf3_al,"%ld\n", end-start);
#endif
	return (end - start);
}

void FREE_DUMMY_IO_REQUEST(int type)
{
	int i;
	int success = 0;
	io_request* prev_request = io_request_start;

	io_request* request = LOOKUP_IO_REQUEST(io_request_seq_nb, type);


	if(io_request_nb == 1){
		io_request_start = NULL;
		io_request_end = NULL;
		success = 1;
	}
	else if(prev_request == request){
		io_request_start = request->next;
		success = 1;
	}
	else{
		for(i=0;i<(io_request_nb-1);i++){
			if(prev_request->next == request && request == io_request_end){
				prev_request->next = NULL;
				io_request_end = prev_request;
				success = 1;
				break;
			}
			else if(prev_request->next == request){
				prev_request->next = request->next;
				success = 1;
				break;
			}
			else{
				prev_request = prev_request->next;
			}
		}
	}


	if(success == 0){
		printf("ERROR[%s] There is no such io request\n", __FUNCTION__);
		return;
	}

	free(request->start_time);
	free(request->end_time);
	free(request);

	io_request_nb--;
}

void FREE_IO_REQUEST(io_request* request)
{
	int i;
	int success = 0;
	io_request* prev_request = io_request_start;

#ifdef FTL_PERF_DEBUG
	printf("\t\t[%s] %d\n",__FUNCTION__, request->request_nb);
#endif	

	if(io_request_nb == 1){
		io_request_start = NULL;
		io_request_end = NULL;
		success = 1;
	}
	else if(prev_request == request){
		io_request_start = request->next;
		success = 1;
	}
	else{
		for(i=0;i<(io_request_nb-1);i++){
			if(prev_request->next == request && request == io_request_end){
				prev_request->next = NULL;
				io_request_end = prev_request;
				success = 1;
				break;
			}
			else if(prev_request->next == request){
				prev_request->next = request->next;
				success = 1;
				break;
			}
			else{
				prev_request = prev_request->next;
			}
		}
	}

	if(success == 0){
		printf("ERROR[%s] There is no such io request\n", __FUNCTION__);
		return;
	}

	free(request->start_time);
	free(request->end_time);
	free(request);

	io_request_nb--;
}

int64_t UPDATE_IO_REQUEST(int request_nb, int offset, int64_t time, int type)
{
	int64_t start = get_usec();

	int io_type;
	int64_t latency=0;
	int flag = 0;

	if(request_nb == -1)
		return 0;

	io_request* curr_request = LOOKUP_IO_REQUEST(request_nb, type);
	if(curr_request == NULL){
		printf("ERROR[%s] No such io request, nb: %d, type: %d\n",__FUNCTION__, request_nb, type);
		return 0;
	}

	if(type == UPDATE_START_TIME){
		curr_request->start_time[offset] = time;
		curr_request->start_count++;	
	}
	else if(type == UPDATE_END_TIME){
		curr_request->end_time[offset] = time;
		curr_request->end_count++;
	}

	if(curr_request->start_count == curr_request->request_size && curr_request->end_count == curr_request->request_size){
		latency = CALC_IO_LATENCY(curr_request);
		io_type = curr_request->request_type;

		SEND_TO_PERF_CHECKER(io_type, latency, LATENCY_OP);

		FREE_IO_REQUEST(curr_request);
		flag = 1;
	}
	int64_t end = get_usec();

	return (end - start);
}

void INCREASE_IO_REQUEST_SEQ_NB(void)
{
	if(io_request_seq_nb == 0xffffffff){
		io_request_seq_nb = 0;
	}
	else{
		io_request_seq_nb++;
	}
//	printf("%d\n", io_request_nb);
}

io_request* LOOKUP_IO_REQUEST(int request_nb, int type)
{
	int i;
	int total_request=0;
	io_request* curr_request = NULL;

//	PRINT_ALL_IO_REQUEST();

	if(io_request_start != NULL){
		curr_request = io_request_start;
		total_request = io_request_nb;
	}
	else{
		printf("ERROR[%s] There is no request. total_request:%d\n", __FUNCTION__, io_request_nb);
		return NULL;
	}

	for(i=0;i<total_request;i++){
		if(curr_request->request_nb == request_nb){
#ifdef FTL_PERF_DEBUG
			printf("[%s] hit! %d %d\n",__FUNCTION__, curr_request->request_nb, request_nb);
#endif
			return curr_request;
		}

		if(curr_request->next != NULL){
			curr_request = curr_request->next;
		}
		else{
#ifdef FTL_PERF_DEBUG
			printf("[%s] end\n", __FUNCTION__);
#endif

			return NULL;
		}	
	}

	return NULL;
}

int64_t CALC_IO_LATENCY(io_request* request)
{
	int64_t latency;
	int64_t* start_time_arr = request->start_time;
	int64_t* end_time_arr = request->end_time;

	int64_t min_start_time;
	int64_t max_end_time;

	int i;
	int type = request->request_type;
	int size = request->request_size;

	for(i=0; i<size; i++){
		if(end_time_arr[i] == 0){
			if(type == READ){
				end_time_arr[i] = start_time_arr[i] + REG_READ_DELAY + CELL_READ_DELAY;
			}
			else if(type == WRITE){
				end_time_arr[i] = start_time_arr[i] + REG_WRITE_DELAY + CELL_PROGRAM_DELAY;
			}
		}
	}

	min_start_time = start_time_arr[0];
	max_end_time = end_time_arr[0];

	if(size > 1){
		for(i=1; i<size; i++){
			if(min_start_time > start_time_arr[i]){
				min_start_time = start_time_arr[i];
			}
			if(max_end_time < end_time_arr[i]){
				max_end_time = end_time_arr[i];
			}
		}
	}
	
	latency = (max_end_time - min_start_time)/(request->request_size);

	if(type == READ){
		arr_read_latency[idx_read_latency] = latency;

		idx_read_latency++;
		if(idx_read_latency == LATENCY_WINDOW){
			idx_read_latency = 0;

			if(arr_read_full == 0){
				arr_read_full = 1;
			}
		}
	}

	if(type == WRITE){
		arr_write_latency[idx_write_latency] = latency;

		idx_write_latency++;
		if(idx_write_latency == LATENCY_WINDOW){
			idx_write_latency = 0;

			if(arr_write_full == 0){
				arr_write_full = 1;
			}
		}
	}
	
	return latency;
}

nand_io_info* CREATE_NAND_IO_INFO(int offset, int type, int io_page_nb, int io_seq_nb)
{
	nand_io_info* n_io_info = (nand_io_info*)calloc(1, sizeof(nand_io_info));
	if(n_io_info == NULL){
		printf("ERROR[%s] Alloc memory fail. \n", __FUNCTION__);
		return NULL;
	}

	n_io_info->offset = offset;
	n_io_info->type = type;
	n_io_info->io_page_nb = io_page_nb;
	n_io_info->io_seq_nb = io_seq_nb;

	return n_io_info;
}

void PRINT_IO_REQUEST(io_request* request)
{
	int i;

	printf("\t----- io request -----\n");
	printf("\trequeset_nb 	: %d\n", request->request_nb);
	printf("\trequest_type 	: ");
	if(request->request_type == READ)
		printf("READ\n");
	else if(request->request_type == WRITE)
		printf("WRITE\n");
	printf("\trequest_size	: %d\n", request->request_size);
	printf("\tstart_count	: %d\n", request->start_count);
	printf("\tend_count	: %d\n", request->end_count);
	printf("\tstart_time	: \n");
	for(i=0; i<request->request_size; i++){
		printf("\t[%d][%ld]\n", i, request->start_time[i]);
	}
	printf("\tend_time	: \n");
	for(i=0; i<request->request_size; i++){
		printf("\t[%d][%ld]\n", i, request->end_time[i]);
	}

	return;
}

void PRINT_ALL_IO_REQUEST(void)
{
	int i;

	io_request* curr_io_request = io_request_start;
	for(i=0; i<io_request_nb; i++)
	{
		PRINT_IO_REQUEST(curr_io_request);
		curr_io_request = curr_io_request->next;
	}

	return;
}

void UPDATE_LOG(int log_type, int arg)
{
	static int64_t recent_update_time = 0;

	switch(log_type){
		case LOG_READ_PAGE:
			log_read_page_val += arg;
			log_read_request_val++;
			break;
		case LOG_WRITE_PAGE:
			log_write_page_val += arg;
			log_write_request_val++;
			break;
		case LOG_GC_AMP:
			log_gc_amp_val += arg;
			log_gc_call++;
			break;
		case LOG_ERASE:
			log_erase_val += arg;
			break;
		default:
			break;
	}

	/* added to set interval between sending logs to monitor */
	int64_t current_time = get_usec();
	if(current_time - recent_update_time >= UPDATE_FREQUENCY){
		recent_update_time = current_time;
		SEND_LOG_TO_MONITOR();
	}
}

void SEND_LOG_TO_MONITOR(void)
{
	char szTemp[1024];
	sprintf(szTemp, "READ PAGE %d ", log_read_page_val);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "READ REQ %d ", log_read_request_val);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "WRITE PAGE %d ", log_write_page_val);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "WRITE REQ %d ", log_write_request_val);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "GC AMP %d", log_gc_amp_val);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "GC CALL %d", log_gc_call);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "ERASE %d", log_erase_val);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "UTIL %lf ", ssd_util);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "READ BW %lf ", GET_IO_BANDWIDTH(avg_read_latency));
	WRITE_LOG(szTemp);
	sprintf(szTemp, "WRITE BW %lf ", GET_IO_BANDWIDTH(avg_write_latency));
	WRITE_LOG(szTemp);
} 
