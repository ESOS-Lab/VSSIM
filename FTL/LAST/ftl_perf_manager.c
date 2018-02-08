// File: ftl_perf_manager.c
// Date: 2018. 02. 08.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2018
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"
#include "ftl_perf_manager.h"
#include "ftl_config_manager.h"
#include "ssd_log_manager.h"

double avg_write_delay;
double avg_read_delay;
double total_write_count;
double total_read_count;

double total_seq_write_count;
double total_ran_write_count;
double total_seq_merge_read_count;
double total_ran_merge_read_count;
double total_seq_merge_write_count;
double total_ran_merge_write_count;

double total_seq_write_delay;
double total_ran_write_delay;
double total_seq_merge_read_delay;
double total_ran_merge_read_delay;
double total_seq_merge_write_delay;
double total_ran_merge_write_delay;

double avg_seq_write_delay;
double avg_ran_write_delay;
double avg_seq_merge_read_delay;
double avg_ran_merge_read_delay;
double avg_seq_merge_write_delay;
double avg_ran_merge_write_delay;

#ifdef PERF_DEBUG1
FILE* fp_perf1_w;
#endif

#ifdef PERF_DEBUG2
FILE* fp_perf2;
#endif

void INIT_PERF_CHECKER(void){

	avg_write_delay = 0;
	avg_read_delay = 0;
	total_write_count = 0;
	total_read_count = 0;


	total_seq_write_count = 0;
	total_ran_write_count = 0;
	total_seq_merge_read_count = 0;
	total_ran_merge_read_count = 0;
	total_seq_merge_write_count = 0;
	total_ran_merge_write_count = 0;

	total_seq_write_delay = 0;
	total_ran_write_delay = 0;
	total_seq_merge_read_delay = 0;
	total_ran_merge_read_delay = 0;
	total_seq_merge_write_delay = 0;
	total_ran_merge_write_delay = 0;

	avg_seq_write_delay = 0;
	avg_ran_write_delay = 0;
	avg_seq_merge_read_delay = 0;
	avg_ran_merge_read_delay = 0;
	avg_seq_merge_write_delay = 0;
	avg_ran_merge_write_delay = 0;

#ifdef PERF_DEBUG1
	fp_perf1_w = fopen("./data/l_perf1_w.txt","a");
#endif
#ifdef PERF_DEBUG2
	fp_perf2 = fopen("./data/l_perf2.txt","a");
#endif
}

void TERM_PERF_CHECKER(void){

//	printf("Average Read Delay	%.3lf us\n", avg_read_delay);
//	printf("Average Write Delay	%.3lf us\n", avg_write_delay);

//	printf("Total GC Delay		%.3lf us\n",total_gc_write_delay);
//	printf("Total GC Count		%lf us\n", total_gc_write_count);
//	printf("Average GC Delay	%.3lf us\n", total_gc_write_delay/total_gc_write_count);

//	printf("GC Count		%d \n", gc_count);
//	printf("GC Overhead		%lf \n", total_gc_write_delay/(double)gc_count);

	avg_seq_write_delay = total_seq_write_delay / total_seq_write_count;
	avg_ran_write_delay = total_ran_write_delay / total_ran_write_count;;
	avg_seq_merge_read_delay = total_seq_merge_read_delay / total_seq_merge_read_count;
	avg_ran_merge_read_delay = total_ran_merge_read_delay / total_ran_merge_read_count;
	avg_seq_merge_write_delay = total_seq_merge_write_delay / total_seq_merge_write_count;
	avg_ran_merge_write_delay = total_ran_merge_write_delay / total_ran_merge_write_count;

	fprintf(fp_perf2,"Average Seq Write Delay 	: %lf [count : %10.0lf]\n", avg_seq_write_delay, total_seq_write_count);
	fprintf(fp_perf2,"Average Ran Write Delay 	: %lf [count : %10.0lf]\n", avg_ran_write_delay, total_ran_write_count);
	fprintf(fp_perf2,"Average Seq Merge Write Delay : %lf [count : %10.0lf]\n", avg_seq_merge_write_delay, total_seq_merge_write_count);
	fprintf(fp_perf2,"Average Ran Merge Write Delay : %lf [count : %10.0lf]\n", avg_ran_merge_write_delay, total_ran_merge_write_count);

	fprintf(fp_perf2,"Total Seq Write Delay		: %lf\n", total_seq_write_delay);
	fprintf(fp_perf2,"Total Ran Write Delay		: %lf\n", total_ran_write_delay);
	fprintf(fp_perf2,"Total Seq Merge Write Delay 	: %lf\n", total_seq_merge_write_delay);
	fprintf(fp_perf2,"Total Ran Merge Write Delay	: %lf\n", total_ran_merge_write_delay);
#ifdef PERF_DEBUG1
	fclose(fp_perf1_w);
#endif
#ifdef PERF_DEBUG2
	fclose(fp_perf2);
#endif
}

void PERF_CHECKER(int op_type, int64_t op_delay){

	char szTemp[1024];
	double delay = (double)op_delay;

	switch (op_type){
		case READ:
			avg_read_delay = (avg_read_delay * total_read_count + delay)/(total_read_count+1);
			total_read_count++;
#ifdef MONITOR_ON
			sprintf(szTemp,"READ %lf 0 0 0", (double)PAGE_SIZE/avg_read_delay);
			WRITE_LOG(szTemp);
#endif
			break;

		case WRITE:
			break;

		case ERASE:
			break;

		case GC_READ:
			break;

		case GC_WRITE:
			break;

		case SEQ_WRITE:
			total_seq_write_delay += delay;
			total_seq_write_count++;
			break;

		case RAN_WRITE:
			total_ran_write_delay += delay;
			total_ran_write_count++;
			break;

		case SEQ_MERGE_READ:
			total_seq_merge_read_delay += delay;
			total_seq_merge_read_count++;
			break;

		case RAN_MERGE_READ:
			total_ran_merge_read_delay += delay;
			total_ran_merge_read_count++;
			break;

		case SEQ_MERGE_WRITE:
			total_seq_merge_write_delay += delay;
			total_seq_merge_write_count++;
			break;

		case RAN_MERGE_WRITE:
			total_ran_merge_write_delay += delay;
			total_ran_merge_write_count++;
			break;

		default:
			break;
	}

}
