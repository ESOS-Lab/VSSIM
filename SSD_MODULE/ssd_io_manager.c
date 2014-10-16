// ssd_io_manager.c
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

#ifndef VSSIM_BENCH
#include "qemu-kvm.h"
#endif

int* reg_io_cmd;	// READ, WRITE, ERASE
int* reg_io_type;	// SEQ, RAN, MERGE, GC, etc..

int64_t* reg_io_time;
int64_t* cell_io_time;

int** access_nb;
int64_t* io_overhead;

int old_channel_nb;
int old_channel_cmd;
int64_t old_channel_time;

int64_t init_diff_reg=0;

int64_t io_alloc_overhead=0;
int64_t io_update_overhead=0;

char ssd_version[4] = "1.0";
char ssd_date[9] = "13.04.11";


int64_t get_usec(void)
{
	int64_t t = 0;
	struct timeval tv;
	struct timezone tz;

	gettimeofday(&tv, &tz);
	t = tv.tv_sec;
	t *= 1000000;
	t += tv.tv_usec;

	return t;
}

int SSD_IO_INIT(void){

	int i= 0;

	/* Print SSD version */
	printf("[%s] SSD Version: %s ver. (%s)\n", __FUNCTION__, ssd_version, ssd_date);

	/* Init Variable for Channel Switch Delay */
	old_channel_nb = CHANNEL_NB;
	old_channel_cmd = NOOP;
	old_channel_time = 0;

	/* Init Variable for Time-stamp */

	/* Init Command and Command type */
	reg_io_cmd = (int *)malloc(sizeof(int) * FLASH_NB * PLANES_PER_FLASH);
	for(i=0; i< FLASH_NB*PLANES_PER_FLASH; i++){
		*(reg_io_cmd + i) = NOOP;
	}

	reg_io_type = (int *)malloc(sizeof(int) * FLASH_NB * PLANES_PER_FLASH);
	for(i=0; i< FLASH_NB*PLANES_PER_FLASH; i++){
		*(reg_io_type + i) = NOOP;
	}

	/* Init Register and Flash IO Time */
	reg_io_time = (int64_t *)malloc(sizeof(int64_t) * FLASH_NB * PLANES_PER_FLASH);
	for(i=0; i<FLASH_NB*PLANES_PER_FLASH; i++){
		*(reg_io_time +i)= -1;
	}

	cell_io_time = (int64_t *)malloc(sizeof(int64_t) * FLASH_NB * PLANES_PER_FLASH);
	for(i=0; i< FLASH_NB*PLANES_PER_FLASH; i++){
		*(cell_io_time + i) = -1;
	}
  
	/* Init Access sequence_nb */
	access_nb = (int **)malloc(sizeof(int*) * FLASH_NB * PLANES_PER_FLASH);
	for(i=0; i< FLASH_NB*PLANES_PER_FLASH; i++){
		*(access_nb + i) = (int*)malloc(sizeof(int)*2);
		access_nb[i][0] = -1;
		access_nb[i][1] = -1;
	}

	/* Init IO Overhead */
	io_overhead = (int64_t *)malloc(sizeof(int64_t) * FLASH_NB * PLANES_PER_FLASH);
	for(i=0; i< FLASH_NB*PLANES_PER_FLASH; i++){
		*(io_overhead + i) = 0;
	}

	return 0;
}

int SSD_PAGE_WRITE(unsigned int flash_nb, unsigned int block_nb, unsigned int page_nb, int offset, int type, int io_page_nb)
{
	int channel, reg;
	int ret = FAIL;
	int delay_ret;

	/* Calculate ch & reg */
	channel = flash_nb % CHANNEL_NB;
	reg = flash_nb*PLANES_PER_FLASH + block_nb%PLANES_PER_FLASH;

	/* Delay Operation */
	SSD_CH_ENABLE(channel);	// channel enable	

	if( IO_PARALLELISM == 0 ){
		delay_ret = SSD_FLASH_ACCESS(flash_nb, reg);
	}
	else{
		delay_ret = SSD_REG_ACCESS(reg);
	}	
	
	/* Check Channel Operation */
	while(ret == FAIL){
		ret = SSD_CH_ACCESS(channel);
	}

	/* Record Time Stamp */
	SSD_CH_RECORD(channel, WRITE, offset, delay_ret);
	SSD_REG_RECORD(reg, WRITE, type, offset, channel);
	SSD_CELL_RECORD(reg, WRITE);

#ifdef O_DIRECT_VSSIM
	if(offset == io_page_nb-1){
		SSD_REMAIN_IO_DELAY(reg);
	}
#endif

//	printf("WRITE reg %d\tch %d\toff %d\n", reg, channel, offset);
//	SSD_PRINT_STAMP();

	return SUCCESS;
}

int SSD_PAGE_PARTIAL_WRITE(unsigned int old_flash_nb, unsigned int old_block_nb, unsigned int old_page_nb, unsigned int new_flash_nb, unsigned int new_block_nb, unsigned int new_page_nb, int offset, int type, int io_page_nb)
{
	int channel, reg;
	int ret = FAIL;
	int delay_ret;

	/* READ Partial Data */

	/* Calculate ch & reg */
	channel = old_flash_nb % CHANNEL_NB;
	reg = old_flash_nb*PLANES_PER_FLASH + old_block_nb%PLANES_PER_FLASH;

	/* Delay Operation */
	SSD_CH_ENABLE(channel);	// channel enable	

	if( IO_PARALLELISM == 0 ){
		delay_ret = SSD_FLASH_ACCESS(old_flash_nb, reg);
	}
	else{
		delay_ret = SSD_REG_ACCESS(reg);
	}	
	
	/* Check Channel Operation */
	while(ret == FAIL){
		ret = SSD_CH_ACCESS(channel);
	}

	/* Record Time Stamp */
	SSD_CH_RECORD(channel, READ, offset, delay_ret);
	SSD_REG_RECORD(reg, READ, type, offset, channel);
	SSD_CELL_RECORD(reg, READ);

	SSD_REMAIN_IO_DELAY(reg);

	/* Write 1 Page */

	/* Calculate ch & reg */
	channel = new_flash_nb % CHANNEL_NB;
	reg = new_flash_nb*PLANES_PER_FLASH + new_block_nb%PLANES_PER_FLASH;

	/* Delay Operation */
	SSD_CH_ENABLE(channel);	// channel enable	

	if( IO_PARALLELISM == 0 ){
		delay_ret = SSD_FLASH_ACCESS(new_flash_nb, reg);
	}
	else{
		delay_ret = SSD_REG_ACCESS(reg);
	}	
	
	/* Check Channel Operation */
	while(ret == FAIL){
		ret = SSD_CH_ACCESS(channel);
	}

	/* Record Time Stamp */
	SSD_CH_RECORD(channel, WRITE, offset, delay_ret);
	SSD_REG_RECORD(reg, WRITE, type, offset, channel);
	SSD_CELL_RECORD(reg, WRITE);

#ifdef O_DIRECT_VSSIM
	if(offset == io_page_nb-1){
		SSD_REMAIN_IO_DELAY(reg);
	}
#endif

	return SUCCESS;	
}

int SSD_PAGE_READ(unsigned int flash_nb, unsigned int block_nb, unsigned int page_nb, int offset, int type, int io_page_nb)
{
	int channel, reg;
	int delay_ret;

	/* Calculate ch & reg */
	channel = flash_nb % CHANNEL_NB;
	reg = flash_nb*PLANES_PER_FLASH + block_nb%PLANES_PER_FLASH;

	/* Delay Operation */
	SSD_CH_ENABLE(channel);	// channel enable

	/* Access Register */
	if( IO_PARALLELISM == 0 ){
		delay_ret = SSD_FLASH_ACCESS(flash_nb, reg);
	}
	else{
		delay_ret = SSD_REG_ACCESS(reg);
	}

	/* Record Time Stamp */
	SSD_CH_RECORD(channel, READ, offset, delay_ret);
	SSD_CELL_RECORD(reg, READ);
	SSD_REG_RECORD(reg, READ, type, offset, channel);

//	printf("READ reg %d\tch %d\toff %d\n", reg, channel, offset);
//	SSD_PRINT_STAMP();

#ifdef O_DIRECT_VSSIM
	if(offset == io_page_nb - 1){
		SSD_REMAIN_IO_DELAY(reg);
	}
#endif
	return SUCCESS;
}

int SSD_BLOCK_ERASE(unsigned int flash_nb, unsigned int block_nb)
{
	int channel, reg;

	/* Calculate ch & reg */
	channel = flash_nb % CHANNEL_NB;
	reg = flash_nb*PLANES_PER_FLASH + block_nb%PLANES_PER_FLASH;

	/* Delay Operation */
	if( IO_PARALLELISM == 0 ){
		SSD_FLASH_ACCESS(flash_nb, reg);
	}
	else{
		SSD_REG_ACCESS(reg);
	}

       	/* Record Time Stamp */
	SSD_REG_RECORD(reg, ERASE, ERASE, -1, channel);
	SSD_CELL_RECORD(reg, ERASE);

	return SUCCESS;
}

int SSD_FLASH_ACCESS(unsigned int flash_nb, int reg)
{
	int i;
	int r_num = flash_nb * PLANES_PER_FLASH;
	int ret = 0;

	for(i=0;i<PLANES_PER_FLASH;i++){
		if(r_num != reg && access_nb[r_num][0] == io_request_seq_nb){
			/* That's OK */
		}
		else{
			ret = SSD_REG_ACCESS(r_num);
		}
	
		r_num++;
	}	

	return ret;
}

int SSD_REG_ACCESS(int reg)
{
	int reg_cmd = reg_io_cmd[reg];
	int ret = 0;

	if( reg_cmd == NOOP ){
		/* That's OK */
	}
	else if( reg_cmd == READ ){
		ret = SSD_CELL_READ_DELAY(reg);
		ret = SSD_REG_READ_DELAY(reg);
	}
	else if( reg_cmd == WRITE ){
		ret = SSD_REG_WRITE_DELAY(reg);
		ret = SSD_CELL_WRITE_DELAY(reg);
	}
	else if( reg_cmd == ERASE ){
		ret = SSD_BLOCK_ERASE_DELAY(reg);
	}
	else{
		printf("ERROR[%s] Command Error! %d\n", __FUNCTION__, reg_io_cmd[reg]);
	}

	return ret;
}

int SSD_CH_ENABLE(int channel)
{
	int64_t do_delay = 0;

	if(CHANNEL_SWITCH_DELAY_R == 0 && CHANNEL_SWITCH_DELAY_W == 0)
		return SUCCESS;

	if(old_channel_nb != channel){
		do_delay = SSD_CH_SWITCH_DELAY(channel);
	}
	
	return SUCCESS;
}

int SSD_CH_RECORD(int channel, int cmd, int offset, int ret)
{
	old_channel_nb = channel;
	old_channel_cmd = cmd;

	if(cmd == READ && offset != 0 && ret == 0){
		old_channel_time += CHANNEL_SWITCH_DELAY_R;
	}
	else if(cmd == WRITE && offset != 0 && ret == 0){
		old_channel_time += CHANNEL_SWITCH_DELAY_W;
	}
	else{
		old_channel_time = get_usec();
	}

	return SUCCESS;
}

int SSD_REG_RECORD(int reg, int cmd, int type, int offset, int channel)
{
	reg_io_cmd[reg] = cmd;
	reg_io_type[reg] = type;

	if(cmd == WRITE){
		reg_io_time[reg] = old_channel_time+CHANNEL_SWITCH_DELAY_W;
		SSD_UPDATE_CH_ACCESS_TIME(channel, reg_io_time[reg]);

		/* Update SATA request Info */
		if(type == WRITE || type == SEQ_WRITE || type == RAN_WRITE || type == RAN_COLD_WRITE || type == RAN_HOT_WRITE){
			access_nb[reg][0] = io_request_seq_nb;
			access_nb[reg][1] = offset;
			io_update_overhead = UPDATE_IO_REQUEST(io_request_seq_nb, offset, old_channel_time, UPDATE_START_TIME);
			SSD_UPDATE_IO_OVERHEAD(reg, io_update_overhead);
		}
		else{
			access_nb[reg][0] = -1;
			access_nb[reg][1] = -1;
		}
	}
	else if(cmd == READ){
		reg_io_time[reg] = SSD_GET_CH_ACCESS_TIME_FOR_READ(channel, reg);

		/* Update SATA request Info */
		if(type == READ){
			access_nb[reg][0] = io_request_seq_nb;
			access_nb[reg][1] = offset;
			io_update_overhead = UPDATE_IO_REQUEST(io_request_seq_nb, offset, old_channel_time, UPDATE_START_TIME);
			SSD_UPDATE_IO_OVERHEAD(reg, io_update_overhead);
		}
		else{
			access_nb[reg][0] = -1;
			access_nb[reg][1] = -1;
		}
	}
	else if(cmd == ERASE){
		/* Update SATA request Info */
		access_nb[reg][0] = -1;
		access_nb[reg][1] = -1;
	}	

	return SUCCESS;
}

int SSD_CELL_RECORD(int reg, int cmd)
{
	if(cmd == WRITE){
		cell_io_time[reg] = reg_io_time[reg] + REG_WRITE_DELAY;
	}
	else if(cmd == READ){
		cell_io_time[reg] = old_channel_time + CHANNEL_SWITCH_DELAY_R;
	}
	else if(cmd == ERASE){
		cell_io_time[reg] = get_usec();
	}

	return SUCCESS;
}

int SSD_CH_ACCESS(int channel)
{
	int i, j;
	int ret = SUCCESS;
	int r_num;

	for(i=0;i<WAY_NB;i++){
		r_num = channel*PLANES_PER_FLASH + i*CHANNEL_NB*PLANES_PER_FLASH; 
		for(j=0;j<PLANES_PER_FLASH;j++){
			if(reg_io_time[r_num] <= get_usec() && reg_io_time[r_num] != -1){
				if(reg_io_cmd[r_num] == READ){
					SSD_CELL_READ_DELAY(r_num);
					SSD_REG_READ_DELAY(r_num);
					ret = FAIL;
				}
				else if(reg_io_cmd[r_num] == WRITE){
					SSD_REG_WRITE_DELAY(r_num);
					ret = FAIL;
				}
			}
			r_num++;	
		}
	}

	return ret;
}

void SSD_UPDATE_IO_OVERHEAD(int reg, int64_t overhead_time)
{
	io_overhead[reg] += overhead_time;
	io_alloc_overhead = 0;
	io_update_overhead = 0;
//	printf("%ld\n",io_overhead[reg]);
}

int64_t SSD_CH_SWITCH_DELAY(int channel)
{
	int64_t start = 0;
       	int64_t	end = 0;
	int64_t diff = 0;

	int64_t switch_delay = 0;

	if(old_channel_cmd == READ){
		switch_delay = CHANNEL_SWITCH_DELAY_R;
	}
	else if(old_channel_cmd == WRITE){
		switch_delay = CHANNEL_SWITCH_DELAY_W;
	}
	else{
		return 0;
	}

	start = get_usec();
	diff = start - old_channel_time;

#ifndef VSSIM_BENCH
  #ifdef DEL_QEMU_OVERHEAD
	if(diff < switch_delay){
		SSD_UPDATE_QEMU_OVERHEAD(switch_delay-diff);
	}
	diff = start - old_channel_time;
  #endif
#endif
	if (diff < switch_delay){
		while( diff < switch_delay ){
			diff = get_usec() - old_channel_time;
		}
	}
	end = get_usec();

	return end-start;
}

int SSD_REG_WRITE_DELAY(int reg)
{
	int ret = 0;
	int64_t start = 0;
       	int64_t	end = 0;
	int64_t diff = 0;
	int64_t time_stamp = reg_io_time[reg];

	if (time_stamp == -1)
		return 0;

	/* Reg Write Delay */
	start = get_usec();
	diff = start - time_stamp;

#ifndef VSSIM_BENCH
  #ifdef DEL_QEMU_OVERHEAD
	if(diff < REG_WRITE_DELAY){
		SSD_UPDATE_QEMU_OVERHEAD(REG_WRITE_DELAY-diff);
	}
	diff = start - reg_io_time[reg];
  #endif
#endif

	if (diff < REG_WRITE_DELAY){
		while( diff < REG_WRITE_DELAY ){
			diff = get_usec() - time_stamp;
		}
		ret = 1;
	}
	end = get_usec();

	/* Send Delay Info To Perf Checker */
	SEND_TO_PERF_CHECKER(reg_io_type[reg], end-start, CH_OP);

	/* Update Time Stamp Struct */
	reg_io_time[reg] = -1;

//TEMPs
//	FILE* fp_temp = fopen("./data/write.txt","a");
//	fprintf(fp_temp,"%ld\n",end-start);
//	fclose(fp_temp);
//TEMPe

	return ret;
}

int SSD_REG_READ_DELAY(int reg)
{
	int ret = 0;
	int64_t start = 0;
	int64_t end = 0;
	int64_t diff = 0;
	int64_t time_stamp = reg_io_time[reg];

	if (time_stamp == -1)
		return 0;

	/* Reg Read Delay */
	start = get_usec();
	diff = start - time_stamp;

#ifndef VSSIM_BENCH
  #ifdef DEL_QEMU_OVERHEAD
	if(diff < REG_READ_DELAY){
		SSD_UPDATE_QEMU_OVERHEAD(REG_READ_DELAY-diff);
	}
	diff = start - reg_io_time[reg];
  #endif
#endif

	if(diff < REG_READ_DELAY){
		while(diff < REG_READ_DELAY){
			diff = get_usec() - time_stamp;
		}
		ret = 1;
	}
	end = get_usec();


	/* Send Delay Info To Perf Checker */
	SEND_TO_PERF_CHECKER(reg_io_type[reg], end-start, CH_OP);
	SSD_UPDATE_IO_REQUEST(reg);
	
	/* Update Time Stamp Struct */
	reg_io_time[reg] = -1;
	reg_io_cmd[reg] = NOOP;
	reg_io_type[reg] = NOOP;

	return ret;
}

int SSD_CELL_WRITE_DELAY(int reg)
{
	int ret = 0;
	int64_t start = 0;
	int64_t end = 0;
	int64_t diff = 0;
	int64_t time_stamp = cell_io_time[reg];

	if( time_stamp == -1 )
		return 0;

	/* Cell Write Delay */
	start = get_usec();
	diff = start - time_stamp + io_overhead[reg];

#ifndef VSSIM_BENCH
  #ifdef DEL_QEMU_OVERHEAD
	if(diff < CELL_PROGRAM_DELAY){
		SSD_UPDATE_QEMU_OVERHEAD(CELL_PROGRAM_DELAY-diff);
	}
	diff = start - cell_io_time[reg] + io_overhead[reg];
  #endif
#endif


	if( diff < CELL_PROGRAM_DELAY){
		init_diff_reg = diff;
		while(diff < CELL_PROGRAM_DELAY){
			diff = get_usec() - time_stamp + io_overhead[reg];
		}
		ret = 1;
	}
	end = get_usec();

	/* Send Delay Info To Perf Checker */
	SEND_TO_PERF_CHECKER(reg_io_type[reg], end-start, REG_OP);
	SSD_UPDATE_IO_REQUEST(reg);

	/* Update Time Stamp Struct */
	cell_io_time[reg] = -1;
	reg_io_cmd[reg] = NOOP;
	reg_io_type[reg] = NOOP;

	/* Update IO Overhead */
	io_overhead[reg] = 0;

	return ret;
}

int SSD_CELL_READ_DELAY(int reg)
{
	int ret = 0;
	int64_t start = 0;
	int64_t end = 0;
	int64_t diff = 0;
	int64_t time_stamp = cell_io_time[reg];

	int64_t REG_DELAY = CELL_READ_DELAY;

	if( time_stamp == -1)
		return 0;

	/* Cell Read Delay */
	start = get_usec();
	diff = start - time_stamp + io_overhead[reg];

#ifndef VSSIM_BENCH
  #ifdef DEL_QEMU_OVERHEAD
	if( diff < REG_DELAY){
		SSD_UPDATE_QEMU_OVERHEAD(REG_DELAY-diff);
	}
	diff = start - cell_io_time[reg] + io_overhead[reg];
  #endif
#endif

	if( diff < REG_DELAY){
		init_diff_reg = diff;
		while( diff < REG_DELAY ){
			diff = get_usec() - time_stamp + io_overhead[reg];
		}
		ret = 1;

	}
	end = get_usec();

	/* Send Delay Info To Perf Checker */
	SEND_TO_PERF_CHECKER(reg_io_type[reg], end-start, REG_OP);

	/* Update Time Stamp Struct */
	cell_io_time[reg] = -1;

	/* Update IO Overhead */
	io_overhead[reg] = 0;

	return ret;
}

int SSD_BLOCK_ERASE_DELAY(int reg)
{
	int ret = 0;
	int64_t start = 0;
	int64_t end = 0;
	int64_t diff;
	int64_t time_stamp = cell_io_time[reg];

	if( time_stamp == -1)
		return 0;

	/* Block Erase Delay */
	start = get_usec();
	diff = get_usec() - cell_io_time[reg];
	if( diff < BLOCK_ERASE_DELAY){
		while(diff < BLOCK_ERASE_DELAY){
			diff = get_usec() - time_stamp;
	  	}
		ret = 1;
	}
	end = get_usec();

	/* Send Delay Info to Perf Checker */
	SEND_TO_PERF_CHECKER(reg_io_type[reg], end-start, REG_OP);

	/* Update IO Overhead */
	cell_io_time[reg] = -1;
	reg_io_cmd[reg] = NOOP;
	reg_io_type[reg] = NOOP;

	return ret;
}

int64_t SSD_GET_CH_ACCESS_TIME_FOR_READ(int channel, int reg)
{
	int i, j;
	int r_num;
	int64_t latest_time = cell_io_time[reg] + CELL_READ_DELAY;
	int64_t temp_time = 0;

	for(i=0;i<WAY_NB;i++){
		r_num = channel*PLANES_PER_FLASH + i*CHANNEL_NB*PLANES_PER_FLASH; 
		for(j=0;j<PLANES_PER_FLASH;j++){
			temp_time = 0;

			if(reg_io_cmd[r_num] == READ){
				temp_time = reg_io_time[r_num] + REG_READ_DELAY;	
			}
			else if(reg_io_cmd[r_num] == WRITE){
				temp_time = reg_io_time[r_num] + REG_WRITE_DELAY;
			}
	
			if( temp_time > latest_time ){
				latest_time = temp_time;
			}
			r_num++;
		}
	}
//TEMP
//	FILE* fp_temp = fopen("./data/temp_read.txt","a");
//	fprintf(fp_temp,"%ld\n", latest_time - (cell_io_time[reg] + CELL_READ_DELAY));
//	fclose(fp_temp);
	return latest_time;
}

void SSD_UPDATE_CH_ACCESS_TIME(int channel, int64_t current_time)
{
	int i, j;
	int r_num;

	for(i=0;i<WAY_NB;i++){
		r_num = channel*PLANES_PER_FLASH + i*CHANNEL_NB*PLANES_PER_FLASH; 
		for(j=0;j<PLANES_PER_FLASH;j++){
			if(reg_io_cmd[r_num] == READ && reg_io_time[r_num] > current_time ){
				reg_io_time[r_num] += REG_WRITE_DELAY;
			}
			r_num++;	
		}
	}
}

void SSD_UPDATE_IO_REQUEST(int reg)
{
	int64_t curr_time = get_usec();
	if(init_diff_reg != 0){
		io_update_overhead = UPDATE_IO_REQUEST(access_nb[reg][0], access_nb[reg][1], curr_time, UPDATE_END_TIME);
		SSD_UPDATE_IO_OVERHEAD(reg, io_update_overhead);
		access_nb[reg][0] = -1;
	}
	else{
		io_update_overhead = UPDATE_IO_REQUEST(access_nb[reg][0], access_nb[reg][1], 0, UPDATE_END_TIME);
		SSD_UPDATE_IO_OVERHEAD(reg, io_update_overhead);
		access_nb[reg][0] = -1;
	}
}

void SSD_REMAIN_IO_DELAY(int reg)
{
	SSD_REG_ACCESS(reg);
}

#ifndef VSSIM_BENCH
void SSD_UPDATE_QEMU_OVERHEAD(int64_t delay)
{
	int i;
	int p_num = FLASH_NB * PLANES_PER_FLASH;
	int64_t diff = delay;

	if(qemu_overhead == 0){
		return;
	}
	else{
		if(diff > qemu_overhead){
			diff = qemu_overhead;
		}
	}

	old_channel_time -= diff;
	for(i=0;i<p_num;i++){
		cell_io_time[i] -= diff;
		reg_io_time[i] -= diff;
	}
	qemu_overhead -= diff;
}
#endif

void SSD_PRINT_STAMP(void)
{
	int i, j, k;
	int op;
	int r_num;

//	FILE* fp_temp = fopen("./data/stamp.txt","a");

	r_num = 0;
	for(i=0;i<CHANNEL_NB;i++){
		for(j=0;j<WAY_NB;j++){
			r_num = i*PLANES_PER_FLASH + j*CHANNEL_NB*PLANES_PER_FLASH; 
			for(k=0;k<PLANES_PER_FLASH;k++){

				op = reg_io_type[r_num];
				if(op == NOOP)
					printf("[      ]");
				else if(op == READ)
					printf("[READ%2d]",access_nb[r_num][1]);
				else if(op == WRITE)
					printf("[PROG%2d]",access_nb[r_num][1]);
				else if(op == SEQ_WRITE)
					printf("[SEQW%2d]",access_nb[r_num][1]);
				else if(op == RAN_WRITE)
					printf("[RNDW%2d]",access_nb[r_num][1]);
				else if(op == RAN_COLD_WRITE)
					printf("[RNCW%2d]",access_nb[r_num][1]);
				else if(op == RAN_HOT_WRITE)
					printf("[RNHW%2d]",access_nb[r_num][1]);
				else if(op == SEQ_MERGE_WRITE)
					printf("[SMGW%2d]",access_nb[r_num][1]);
				else if(op == RAN_MERGE_WRITE)
					printf("[RMGW%2d]",access_nb[r_num][1]);
				else if(op == SEQ_MERGE_READ)
					printf("[SMGR%2d]",access_nb[r_num][1]);
				else if(op == RAN_MERGE_READ)
					printf("[RMGR%2d]",access_nb[r_num][1]);
				else
					printf("[ %d ]",op);
				
				r_num ++;
			}
		}
		printf("\t");
	}
	printf("\n\n");

//	fclose(fp_temp);
}
