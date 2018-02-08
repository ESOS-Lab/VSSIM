// File: flash_memory.c
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

flash_channel* channel;
flash_memory* flash; 

char flash_version[4] = "0.9";
char flash_date[9] = "18.01.31";

bitmap_t flash_io_bitmap;
pthread_mutex_t flash_io_bitmap_lock;
 
int CREATE_TIMER(timer_t *timerID, int sec, int usec, void func(void))
{  
	struct sigevent         te;  
	struct itimerspec       its;  
	struct sigaction        sa;  
	int                     sigNo = SIGRTMIN;  
   
	/* Set up signal handler. */  
	sa.sa_flags = SA_SIGINFO;  
	sa.sa_sigaction = (void*)func;
	sigemptyset(&sa.sa_mask);  
  
	if(sigaction(sigNo, &sa, NULL) == -1)  
	{  
		printf("ERROR [%s] sigaction error\n", __FUNCTION__);
		return -1;  
	}  
   
	/* Set and enable alarm */  
	te.sigev_notify = SIGEV_SIGNAL;  
	te.sigev_signo = sigNo;  
	te.sigev_value.sival_ptr = timerID;  
	timer_create(CLOCK_REALTIME, &te, timerID);
   
	its.it_interval.tv_sec = sec;
	its.it_interval.tv_nsec = usec * 1000;  

	its.it_value.tv_sec = sec;
	its.it_value.tv_nsec = usec * 1000;

	timer_settime(*timerID, 0, &its, NULL);  
   
	return 0;  
}
 
int INIT_FLASH(void){

	int i, j;

	/* Print SSD version */
	printf("[%s] Flash Version: %s ver. (%s)\n", __FUNCTION__, flash_version, flash_date);

	/* Init Flash Channel */
	channel = (flash_channel*)calloc(sizeof(flash_channel), N_CHANNELS); 
	if(channel == NULL){
		printf("ERROR[%s] alloc channel fail!\n", __FUNCTION__);
		return FAIL;
	}
	else{
		for(i=0; i<N_CHANNELS; i++){
			channel[i].t_next_idle = -1;
			pthread_mutex_init(&channel[i].lock, NULL);
		}
	}

	/* Init Flash memory */
	flash = (flash_memory*)calloc(sizeof(flash_memory), N_FLASH);
	if(flash == NULL){
		printf("ERROR[%s] alloc flash fail!\n", __FUNCTION__);
		return FAIL;
	}
	else{
		for(i=0; i<N_FLASH; i++){
			flash[i].channel_nb = i % N_CHANNELS;

			/* Init Plane */
			flash[i].plane = (plane*)calloc(sizeof(plane), N_PLANES_PER_FLASH);
			if(flash[i].plane == NULL){
				printf("ERROR[%s] alloc plane fail!\n", __FUNCTION__);
				return FAIL;
			}
			else{
				for(j=0; j<N_PLANES_PER_FLASH; j++){

					flash[i].plane[j].cmd = CMD_NOOP;
				}	
			}
		}
	}

	/* Init Register */
	for(i=0; i<N_FLASH; i++){
		for(j=0; j<N_PLANES_PER_FLASH; j++){

			flash[i].plane[j].page_cache.state = REG_NOOP;
			flash[i].plane[j].page_cache.ppn.addr = -1;
			flash[i].plane[j].page_cache.t_end = -1;

			flash[i].plane[j].data_reg.state = REG_NOOP;
			flash[i].plane[j].page_cache.ppn.addr = -1;
			flash[i].plane[j].data_reg.t_end = -1;
		}
	}

	/* Create bitmap for checking io completion */
	flash_io_bitmap = CREATE_BITMAP(N_TOTAL_PAGES);
	pthread_mutex_init(&flash_io_bitmap_lock, NULL);

	/* Set timer wakeup every xx usec */
	timer_t timerID;
//TEMP
	if(PAGE_CACHE_REG_ENABLE){
		CREATE_TIMER(&timerID, 0, 900000, FLASH_STATE_CHECKER_PAGE_CACHE);
	}
	else{
		CREATE_TIMER(&timerID, 0, 900000, FLASH_STATE_CHECKER);
	}

	return 0;
}

int TERM_FLASH(void)
{
	int i;

	for(i=0; i<N_FLASH; i++){
		free(flash[i].plane);
	}
	free(flash);

	/* Destroy bitmap for checking io completion */
	DESTROY_BITMAP(flash_io_bitmap);

	return SUCCESS;
} 

void FLASH_STATE_CHECKER(void)
{
	int i, j;
	int channel_nb;
	int64_t t_now;
	uint8_t cmd;

	reg* data_reg;

	t_now = get_usec();

	for(i=0; i<N_FLASH; i++){
	    for(j=0; j<N_PLANES_PER_FLASH; j++){

	        cmd = flash[i].plane[j].cmd;
	        channel_nb = flash[i].channel_nb;

	        data_reg = &flash[i].plane[j].data_reg;

	        switch(data_reg->state){

	            case REG_NOOP:
	                break;
	            case SET_CMD1:
	                break;

	            case SET_CMD2:
	                if(data_reg->t_end <= t_now
	                        && cmd == CMD_PAGE_READ){

	                    data_reg->state = REG_READ;
	                    data_reg->t_end = t_now + REG_READ_DELAY;
	                }
	                break;

	            case REG_WRITE:
	                break;

	            case REG_READ:
	                /* Complete Flash page read */
	                if(data_reg->t_end <= t_now){

	                    /* Update complete bitmap */
	                    UPDATE_FLASH_IO_BITMAP(data_reg->ppn, false);

	                    /* Initialize current register */
	                    data_reg->state = REG_NOOP;
	                    data_reg->ppn.addr = -1;
	                    data_reg->t_end = -1;

	                    /* Initialized current plane */	
	                    flash[i].plane[j].cmd = CMD_NOOP;
	                }
	                break;

	            case PAGE_PROGRAM:
	                /* Complete Flash page program */
	                if (data_reg->t_end <= t_now){

	                    /* Update complete bitmap */
	                    UPDATE_FLASH_IO_BITMAP(data_reg->ppn, false);

                            /* Initialize current register */
	                    data_reg->state = REG_NOOP;
	                    data_reg->ppn.addr = -1;
	                    data_reg->t_end = -1;

	                    /* Initialized current plane */	
	                    flash[i].plane[j].cmd = CMD_NOOP;
	                }
	                break;

	            case PAGE_READ:
	                /* Complete Flash page read */
	                if(data_reg->t_end <= t_now){

	                    if(cmd == CMD_PAGE_READ){
	                        data_reg->t_end = GET_AND_UPDATE_NEXT_AVAILABLE_CH_TIME(channel_nb, 
	                                                t_now, cmd, PAGE_READ);

	                        if(data_reg->t_end <= t_now){
					
	                            /* Update state */
	                            data_reg->state = SET_CMD2;

	                            /* Update end time of this state */
	                            data_reg->t_end = t_now + REG_CMD_SET_DELAY;
	                        }
	                        else{
	                            /* Update state */
	                            data_reg->state = WAIT_CHANNEL;
	                        }
	                    }
	                    else if(cmd == CMD_PAGE_COPYBACK){
	                        /* Update complete bitmap */
	                        UPDATE_FLASH_IO_BITMAP(data_reg->ppn, false);

                                /* Initialize current register */
	                        data_reg->state = REG_NOOP;
	                        data_reg->ppn.addr = -1;
	                        data_reg->t_end = -1;
	                    }
	                }

	                break;

	            case BLOCK_ERASE:

	                /* Complete Flash block erase */
	                if(data_reg->t_end <= t_now){

	                    /* Update complete bitmap */
	                    UPDATE_FLASH_IO_BITMAP(data_reg->ppn, false);

                            /* Initialize current register */
	                    data_reg->state = REG_NOOP;
	                    data_reg->ppn.addr = -1;
	                    data_reg->t_end = -1;

	                    /* Initialized current plane */	
	                    flash[i].plane[j].cmd = CMD_NOOP;
	                }
	                break;

	            case WAIT_CHANNEL:

	                if(data_reg->t_end <= t_now
	                        && cmd == CMD_PAGE_READ){

	                    /* Update state */
	                    data_reg->state = SET_CMD2;

	                    /* Update end time of this state */
	                    data_reg->t_end = t_now + REG_CMD_SET_DELAY;
	                }
	                break;

                    default:
	                break;
	        }
	    }
	}
}


void FLASH_STATE_CHECKER_PAGE_CACHE(void)
{
	int i, j;
	int channel_nb;
	int64_t t_now;
	uint8_t cmd;

	reg* data_reg;
	reg* page_cache;

	t_now = get_usec();

	for(i=0; i<N_FLASH; i++){
	    for(j=0; j<N_PLANES_PER_FLASH; j++){

	        cmd = flash[i].plane[j].cmd;
	        channel_nb = flash[i].channel_nb;

	        data_reg = &flash[i].plane[j].data_reg;
	        page_cache = &flash[i].plane[j].page_cache;

	        switch(data_reg->state){

	            case REG_NOOP:
	                break;
	            case SET_CMD1:
	                break;

	            case SET_CMD2:
	                if(data_reg->t_end <= t_now
	                        && cmd == CMD_PAGE_READ){

	                    data_reg->state = REG_READ;
	                    data_reg->t_end = t_now + REG_READ_DELAY;
	                }
	                break;

	            case REG_WRITE:
	                break;

	            case REG_READ:
	                /* Complete Flash page read */
	                if(data_reg->t_end <= t_now){

	                    /* Update complete bitmap */
	                    UPDATE_FLASH_IO_BITMAP(data_reg->ppn, false);

	                    /* Initialize current register */
	                    data_reg->state = REG_NOOP;
	                    data_reg->ppn.addr = -1;
	                    data_reg->t_end = -1;

	                    /* Initialized current plane */	
	                    flash[i].plane[j].cmd = CMD_NOOP;
	                }
	                break;

	            case PAGE_PROGRAM:
	                /* Complete Flash page program */
	                if (data_reg->t_end <= t_now){

	                    /* Update complete bitmap */
	                    UPDATE_FLASH_IO_BITMAP(data_reg->ppn, false);

                            /* Initialize current register */
	                    data_reg->state = REG_NOOP;
	                    data_reg->ppn.addr = -1;
	                    data_reg->t_end = -1;

	                    /* Initialized current plane */	
	                    flash[i].plane[j].cmd = CMD_NOOP;
	                }
	                break;

	            case PAGE_READ:
	                /* Complete Flash page read */
	                if(data_reg->t_end <= t_now){

	                    if(cmd == CMD_PAGE_READ){
	                        data_reg->t_end = GET_AND_UPDATE_NEXT_AVAILABLE_CH_TIME(channel_nb, 
	                                                t_now, cmd, PAGE_READ);

	                        if(data_reg->t_end <= t_now){
					
	                            /* Update state */
	                            data_reg->state = SET_CMD2;

	                            /* Update end time of this state */
	                            data_reg->t_end = t_now + REG_CMD_SET_DELAY;
	                        }
	                        else{
	                            /* Update state */
	                            data_reg->state = WAIT_CHANNEL;
	                        }
	                    }
	                    else if(cmd == CMD_PAGE_COPYBACK){
	                        /* Update complete bitmap */
	                        UPDATE_FLASH_IO_BITMAP(data_reg->ppn, false);

                                /* Initialize current register */
	                        data_reg->state = REG_NOOP;
	                        data_reg->ppn.addr = -1;
	                        data_reg->t_end = -1;
	                    }
	                }

	                break;

	            case BLOCK_ERASE:

	                /* Complete Flash block erase */
	                if(data_reg->t_end <= t_now){

	                    /* Update complete bitmap */
	                    UPDATE_FLASH_IO_BITMAP(data_reg->ppn, false);

                            /* Initialize current register */
	                    data_reg->state = REG_NOOP;
	                    data_reg->ppn.addr = -1;
	                    data_reg->t_end = -1;

	                    /* Initialized current plane */	
	                    flash[i].plane[j].cmd = CMD_NOOP;
	                }
	                break;

	            case WAIT_CHANNEL:

	                if(data_reg->t_end <= t_now
	                        && cmd == CMD_PAGE_READ){

	                    /* Update state */
	                    data_reg->state = SET_CMD2;

	                    /* Update end time of this state */
	                    data_reg->t_end = t_now + REG_CMD_SET_DELAY;
	                }
	                break;

                    default:
	                break;
	        }

	        switch(page_cache->state){
	            case REG_NOOP:
	            case SET_CMD1:
	            case SET_CMD2:
	            case REG_WRITE:
	            case REG_READ:
	            case PAGE_PROGRAM:
	            case PAGE_READ:
	                if(page_cache->t_end <= t_now){
	                    if(data_reg != REG_NOOP){
	                        page_cache->t_end = data_reg->t_end;
	                        page_cache->state = WAIT_REG;
	                    }
	                }
	                break;
	            case BLOCK_ERASE:
	            case WAIT_CHANNEL:
	                break;

	            case WAIT_REG:

	                if(page_cache->t_end <= t_now){

	                    if(cmd == CMD_PAGE_PROGRAM){
	                        COPY_PAGE_CACHE_TO_DATA_REG(data_reg, page_cache);

	                        /* Set data register */
	                        data_reg->state = PAGE_PROGRAM;
	                        data_reg->t_end = t_now + PAGE_PROGRAM_DELAY;
	                        flash[i].plane[j].cmd = CMD_PAGE_PROGRAM;

                                /* Initialize current register */
	                        page_cache->state = REG_NOOP;
	                        page_cache->ppn.addr = -1;
	                        page_cache->t_end = -1;
	                    }
	                    else if(cmd == CMD_PAGE_READ){
	                        COPY_PAGE_CACHE_TO_DATA_REG(data_reg, page_cache);

	                        /* Set data register */
	                        data_reg->state = PAGE_READ;
	                        data_reg->t_end = t_now;
	                        flash[i].plane[j].cmd = CMD_PAGE_READ;

                                /* Initialize current register */
	                        page_cache->state = REG_NOOP;
	                        page_cache->ppn.addr = -1;
	                        page_cache->t_end = -1;
	                    }
	                }
	                break;
	            default:
	                break;
		}
	    }
	}
}


int FLASH_PAGE_WRITE(ppn_t ppn)
{
	int flash_nb = (int)ppn.path.flash;
	int plane_nb = (int)ppn.path.plane;
	int channel_nb;

	plane* cur_plane = &flash[flash_nb].plane[plane_nb];
	channel_nb = flash[flash_nb].channel_nb; 	

	/* Update complete bitmap */
	UPDATE_FLASH_IO_BITMAP(ppn, true);

	/* Get the channel */ 
	ACQUIRE_CHANNEL(channel_nb, get_usec(), CMD_PAGE_PROGRAM);

	/* Write the command1 to the register */
	SET_CMD1_TO_REG(cur_plane, CMD_PAGE_PROGRAM, ppn); 

	return SUCCESS;
}

int FLASH_PAGE_PARTIAL_WRITE(ppn_t old_ppn, ppn_t new_ppn)
{
	uint64_t index;

	FLASH_PAGE_READ(old_ppn);

	/* Wait read completion*/
	index = GET_LINEAR_PPN(old_ppn);
	while(TEST_BITMAP(flash_io_bitmap, (uint32_t)index)){}

	FLASH_PAGE_WRITE(new_ppn);

	return SUCCESS;	
}

int FLASH_PAGE_READ(ppn_t ppn)
{
	int flash_nb = (int)ppn.path.flash;
	int plane_nb = (int)ppn.path.plane;
	int channel_nb;

	plane* cur_plane = &flash[flash_nb].plane[plane_nb];
	channel_nb = flash[flash_nb].channel_nb; 	

	/* Update complete bitmap */
	UPDATE_FLASH_IO_BITMAP(ppn, true);

	/* Get the channel */
	ACQUIRE_CHANNEL(channel_nb, get_usec(), CMD_PAGE_READ);

	/* Write the command1 to the register */
	SET_CMD1_TO_REG(cur_plane, CMD_PAGE_READ, ppn); 

	return SUCCESS;
}

int FLASH_PAGE_COPYBACK(ppn_t dst_ppn, ppn_t src_ppn)
{
	int flash_nb = (int)src_ppn.path.flash;
	int plane_nb = (int)src_ppn.path.plane;
	int channel_nb;
	uint64_t index;

	plane* cur_plane = &flash[flash_nb].plane[plane_nb];
	channel_nb = flash[flash_nb].channel_nb; 	

	UPDATE_FLASH_IO_BITMAP(dst_ppn, true);

	/* Get the channel */
	ACQUIRE_CHANNEL(channel_nb, get_usec(), CMD_PAGE_COPYBACK);

	/* Write the command1 to the register */
	SET_CMD1_TO_REG(cur_plane, CMD_PAGE_COPYBACK, src_ppn); 

	/* Wait erase operation */
	index = GET_LINEAR_PPN(src_ppn);
	while(TEST_BITMAP(flash_io_bitmap, (uint32_t)index)){}	

	/* Get the plane to write */
	plane_nb = (int)dst_ppn.path.plane;
	cur_plane = &flash[flash_nb].plane[plane_nb];

	/* Write the command2 to the register */
	SET_CMD2_TO_REG(cur_plane, CMD_PAGE_COPYBACK, dst_ppn); 
	
	return SUCCESS;
}

int FLASH_BLOCK_ERASE(pbn_t pbn)
{
	int flash_nb = (int)pbn.path.flash;
	int plane_nb = (int)pbn.path.plane;
	int channel_nb;
	uint64_t index;

	ppn_t ppn = PBN_TO_PPN(pbn, 0);

	plane* cur_plane = &flash[flash_nb].plane[plane_nb];
	channel_nb = flash[flash_nb].channel_nb; 	

	/* Update complete bitmap */
	UPDATE_FLASH_IO_BITMAP(ppn, true);

	/* Get the channel */
	ACQUIRE_CHANNEL(channel_nb, get_usec(), CMD_BLOCK_ERASE);

	/* Write the command1 to the register */
	SET_CMD1_TO_REG(cur_plane, CMD_BLOCK_ERASE, ppn); 

	/* Wait erase operation */
	index = GET_LINEAR_PPN(ppn);
	while(TEST_BITMAP(flash_io_bitmap, (uint32_t)index)){}	

	return SUCCESS;
}

void ACQUIRE_CHANNEL(int channel_nb, int64_t t_now, uint8_t cmd)
{ 
	int64_t t_next;

	t_next = GET_AND_UPDATE_NEXT_AVAILABLE_CH_TIME(channel_nb,
				t_now, cmd, SET_CMD1);
	
	/* Busy waiting */
	while(t_next < get_usec()){}
}


void SET_CMD1_TO_REG(plane* cur_plane, uint8_t cmd, ppn_t ppn)
{
	int64_t t_now;
	reg* cur_reg = NULL;
	reg* data_reg = NULL;

	if(PAGE_CACHE_REG_ENABLE){
		cur_reg = &cur_plane->page_cache;
		data_reg = &cur_plane->data_reg;

		if(cmd != cur_plane->cmd){

			/* Busy waiting */
			while(cur_plane->cmd != CMD_NOOP){}
		} 
	}
	else{
		cur_reg = &cur_plane->data_reg;
	}

	/* Busy waiting */
	while(cur_reg->state != REG_NOOP){}

	/* Update state */
	cur_reg->state = SET_CMD1;

	/* Update register */
	cur_plane->cmd = cmd;
	cur_reg->ppn = ppn;

	/* Transfer Command1 */
	t_now = BUSY_WAITING_USEC(get_usec() + REG_CMD_SET_DELAY);

	switch(cmd){
	case CMD_PAGE_READ:

		/* Update end time of this state */
		cur_reg->t_end = t_now + PAGE_READ_DELAY;

		/* Update register */
		cur_reg->state = PAGE_READ; 

		break;

	case CMD_PAGE_PROGRAM:

		/* Update state */
		cur_reg->state = REG_WRITE; 
	
		/* Transfer Data */	
		t_now = BUSY_WAITING_USEC(t_now + REG_WRITE_DELAY);

		if(PAGE_CACHE_REG_ENABLE){
			if(data_reg->state != REG_NOOP){

				cur_reg->t_end = data_reg->t_end;
				cur_reg->state = WAIT_REG;
			}
			else{
				COPY_PAGE_CACHE_TO_DATA_REG(data_reg, cur_reg);

	                        data_reg->t_end = t_now + PAGE_PROGRAM_DELAY;
	                        data_reg->state = PAGE_PROGRAM;

				cur_reg->t_end = -1;
				cur_reg->state = REG_NOOP;
	                }
		}
		else{
			/* Update state */
			cur_reg->state = SET_CMD2;

			/* Transfer Command2 */
			t_now = BUSY_WAITING_USEC(t_now + REG_CMD_SET_DELAY);
	
			/* Update end time and state */
			cur_reg->t_end = t_now + PAGE_PROGRAM_DELAY;
			cur_reg->state = PAGE_PROGRAM;
			cur_reg->ppn.addr = -1;
		}
		break;

	case CMD_PAGE_COPYBACK:

		/* Update end time of this state */
		cur_reg->t_end = t_now + PAGE_READ_DELAY;

		/* Update register */
		cur_reg->state = PAGE_READ; 

		break;

	case CMD_BLOCK_ERASE:

		/* Update end time of this state */
		cur_reg->t_end = t_now + BLOCK_ERASE_DELAY;

		/* Update register */
		cur_reg->state = BLOCK_ERASE;

		break;

	default:
		printf("ERROR [%s] Wrong flash command: %d\n",
				__FUNCTION__, cmd);
		break;
	}
}

void SET_CMD2_TO_REG(plane* cur_plane, uint8_t cmd, ppn_t ppn)
{
	int64_t t_now;
	reg* cur_reg = NULL;

	cur_reg = &cur_plane->data_reg;

	/* Busy waiting */
	while(cur_reg->state != REG_NOOP){}

	/* Update state */
	cur_reg->state = SET_CMD2;

	/* Update register */
	cur_plane->cmd = cmd;
	cur_reg->ppn = ppn;

	/* Transfer Command1 */
	t_now = BUSY_WAITING_USEC(get_usec() + REG_CMD_SET_DELAY);

	switch(cmd){

	case CMD_PAGE_COPYBACK:

		/* Update end time of this state */
		cur_reg->t_end = t_now + PAGE_PROGRAM;

		/* Update register */
		cur_reg->state = PAGE_PROGRAM; 

		break;

	default:
		printf("ERROR [%s] Wrong flash command: %d\n",
				__FUNCTION__, cmd);
		break;
	}
}


void COPY_PAGE_CACHE_TO_DATA_REG(reg* dst_reg, reg* src_reg)
{
	dst_reg->ppn = src_reg->ppn;
}

int64_t BUSY_WAITING_USEC(int64_t t_end)
{
	int64_t t_now = get_usec();

	while(t_now < t_end){
		t_now = get_usec();
	}

	return t_now;
}

int64_t GET_AND_UPDATE_NEXT_AVAILABLE_CH_TIME(int channel_nb, int64_t t_now, 
			uint8_t cmd, enum reg_state cur_state)
{
	int64_t t_next_idle;
	int64_t ret_time;

	flash_channel* cur_channel = &channel[channel_nb];

	/* Acquire channel lock */
	pthread_mutex_lock(&cur_channel->lock);

	/* Get next available channel time */
	t_next_idle = cur_channel->t_next_idle;
	if(t_next_idle < t_now){
		ret_time = t_now;
	}
	else{
		ret_time = t_next_idle;
	}

	/* Update next available channel time */
	if(cur_state == SET_CMD1 && cmd == CMD_PAGE_PROGRAM){
		cur_channel->t_next_idle = ret_time 
				+ REG_CMD_SET_DELAY * 2 + REG_WRITE_DELAY;
	}
	else if(cur_state == SET_CMD1 && cmd == CMD_PAGE_READ){
		cur_channel->t_next_idle = ret_time + REG_CMD_SET_DELAY;
	}
	else if(cur_state == SET_CMD1 && cmd == CMD_PAGE_COPYBACK){
		cur_channel->t_next_idle = ret_time + REG_CMD_SET_DELAY;
	}
	else if(cur_state == SET_CMD1 && cmd == CMD_BLOCK_ERASE){
		cur_channel->t_next_idle = ret_time + REG_CMD_SET_DELAY;
	}
	else if(cur_state == PAGE_READ){
		cur_channel->t_next_idle = ret_time
				+ REG_CMD_SET_DELAY + REG_READ_DELAY;
	}
	else{
		printf("ERROR[%s] Wrong cmd(%d) and state (%d)!\n",
				__FUNCTION__, cmd, cur_state);
	} 

	/* Release channel lock */
	pthread_mutex_unlock(&cur_channel->lock);

	return ret_time;
}

void UPDATE_FLASH_IO_BITMAP(ppn_t ppn, bool set)
{
	uint64_t index = GET_LINEAR_PPN(ppn);

	pthread_mutex_lock(&flash_io_bitmap_lock);

	if(set){
		SET_BITMAP(flash_io_bitmap, (uint32_t)index);
	}
	else{
		CLEAR_BITMAP(flash_io_bitmap, (uint32_t)index);
	}

	pthread_mutex_unlock(&flash_io_bitmap_lock);
}

void WAIT_FLASH_IO(ppn_t* ppn_array, int n_pages)
{
	int i;
	uint32_t index; 

	for(i=0; i<n_pages; i++){
		index = GET_LINEAR_PPN(ppn_array[i]);

		while(TEST_BITMAP(flash_io_bitmap, (uint32_t)index)){}
	}

	free(ppn_array);
}
