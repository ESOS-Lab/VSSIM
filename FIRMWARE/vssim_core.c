// File: vssim_core.c
// Date: 21-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

int vssim_exit;

pthread_t* vssim_thread_id;
pthread_t temp_thread_id;
vssim_core* vs_core;

pthread_cond_t eq_ready = PTHREAD_COND_INITIALIZER;
pthread_cond_t* ssd_io_ready; 
pthread_mutex_t* ssd_io_lock;

void MAKE_TIMEOUT(struct timespec *tsp, long timeout_usec)
{
	struct timeval now;
	long t_usec;

	/* get the current time */
	gettimeofday(&now, NULL);
	tsp->tv_sec = now.tv_sec;
	t_usec = now.tv_usec + timeout_usec;

	if(t_usec > 1000000){
		tsp->tv_sec++;
		t_usec -= 1000000;
	}
	tsp->tv_nsec = t_usec * 1000;
}

void INIT_VSSIM_CORE(void)
{
	int i, j;
	long thread_id = 0;
	int index = 0;

	vssim_exit = 0;

	vssim_thread_id = (pthread_t*)calloc(sizeof(pthread_t), N_CORES);
	if(vssim_thread_id == NULL){
		printf("ERROR[%s] Allocate memory for vssim_thread_id fail!\n",
				__FUNCTION__);
		return;
	}

	/* Create per core request queue */
	vs_core = (vssim_core*)calloc(sizeof(vssim_core), N_IO_CORES);
	if(vs_core == NULL){
		printf("ERROR[%s] Create per core request queue fail!\n", __FUNCTION__);
		return;
	}
	else{
		/* Init the per core request queue */
		for(i=0; i<N_IO_CORES; i++){
			/* Init read queue */
			INIT_PER_CORE_REQUEST_QUEUE(&vs_core[i].read_queue);

			/* Create write queue */
			vs_core[i].write_queue =
				(core_req_queue*)calloc(sizeof(core_req_queue), N_WRITE_BUF);
			if(vs_core[i].write_queue == NULL){
				printf("ERROR[%s] Create write queue fail!\n", 
						__FUNCTION__);
			}

			/* Init write queue */
			for(j=0; j<N_WRITE_BUF; j++){
				INIT_PER_CORE_REQUEST_QUEUE(&vs_core[i].write_queue[j]);
			}

			/* Init discard queue */
			INIT_PER_CORE_REQUEST_QUEUE(&vs_core[i].discard_queue);

			/* Init flash list */
			INIT_FLASH_LIST(i);
			vs_core[i].flash_index = i;

			/* Init GC related info */
			vs_core[i].n_bggc_planes = 0;
			pthread_mutex_init(&vs_core[i].n_bggc_lock, NULL);
			vs_core[i].n_fggc_planes = 0;
			pthread_mutex_init(&vs_core[i].n_fggc_lock, NULL);

			/* Init lock variable for per core GC */
			pthread_mutex_init(&vs_core[i].gc_lock, NULL);

			/* Init flush flag */
			vs_core[i].flush_flag = false;
			vs_core[i].flush_event = NULL;
			pthread_mutex_init(&vs_core[i].flush_lock, NULL);
		}
	}

	/* Create & Init the condition variables and mutex */
	ssd_io_ready = (pthread_cond_t*)calloc(sizeof(pthread_cond_t),
							N_IO_CORES);
	ssd_io_lock = (pthread_mutex_t*)calloc(sizeof(pthread_mutex_t),
							N_IO_CORES);
	if(ssd_io_ready == NULL || ssd_io_lock == NULL){
		printf("ERROR[%s] Create per core condition variables \
				and mutex fail!\n", __FUNCTION__);
		return;
	}
	else{
		for(i=0; i<N_IO_CORES; i++){
			pthread_cond_init(&ssd_io_ready[i], NULL);
			pthread_mutex_init(&ssd_io_lock[i], NULL);
		}
	}


	/* Create the Firmware IO buffer thread */
	pthread_create(&vssim_thread_id[index], NULL, 
				FIRM_IO_BUF_THREAD_MAIN_LOOP, NULL);
	index++;


	/* Create the IO thread */
	for(i=index; i<=N_IO_CORES; i++, index++){
		pthread_create(&vssim_thread_id[i], NULL, 
				SSD_IO_THREAD_MAIN_LOOP, (void*)(thread_id));
		thread_id++;
	}

	/* Create the Background GC thread */
	pthread_create(&vssim_thread_id[index], NULL, 
				BACKGROUND_GC_THREAD_MAIN_LOOP, NULL);
}

void INIT_PER_CORE_REQUEST_QUEUE(core_req_queue* cr_queue)
{
	cr_queue->head = NULL;
	cr_queue->tail = NULL;
	cr_queue->entry_nb = 0;
	pthread_mutex_init(&cr_queue->lock, NULL);
}

void INIT_FLASH_LIST(int core_id)
{
	int i;

	/* Get flash info header for the core */
	flash_info* cur_flash = &flash_i[core_id];
	int cur_flash_index = core_id;
	
	/* Update the flash list */
	vs_core[core_id].flash_i = cur_flash;
	
	while (1){

		/* Update the global metadata */
		for(i=0; i<N_PLANES_PER_FLASH; i++){
			n_total_empty_blocks[core_id] += cur_flash->plane_i[i].empty_list.n_blocks;
			n_total_victim_blocks[core_id] += cur_flash->plane_i[i].victim_list.n_blocks;
		}

		/* Get next flash index for the core */
		cur_flash_index = GET_NEXT_FLASH_LIST_INDEX(core_id, cur_flash_index);

		/* Update flash info */
		flash_i[cur_flash_index].core_id = core_id;

		/* Update the flash list */
		if(cur_flash_index == core_id){
			
			cur_flash->next_flash = &flash_i[cur_flash_index];
			break;
		}
		else{
			cur_flash->next_flash = &flash_i[cur_flash_index];
			cur_flash = cur_flash->next_flash;
		}
	}
}

void TERM_VSSIM_CORE(void)
{
	int i;
	vssim_exit = 1;

	for(i=0; i<N_IO_CORES; i++){
		pthread_cond_destroy(&ssd_io_ready[i]);
		pthread_mutex_destroy(&ssd_io_lock[i]);
	}

	free(vssim_thread_id);
	free(vs_core);
}


void *FIRM_IO_BUF_THREAD_MAIN_LOOP(void *arg)
{
	enum vssim_io_type io_type;

	event_queue_entry* cur_entry = NULL;

	do{
		pthread_mutex_lock(&eq_lock);

		/* Wait new IO event */
		while(e_queue->entry_nb == 0){
			pthread_cond_wait(&eq_ready, &eq_lock);
		}

		/* Get new IO event */
		cur_entry = DEQUEUE_IO();
		
		pthread_mutex_unlock(&eq_lock);

		io_type = cur_entry->io_type;

		if(io_type == READ){
			FIRM_READ_EVENT(cur_entry);
		}
		else if(io_type == WRITE){
			FIRM_WRITE_EVENT(cur_entry, false);
		}
		else if(io_type == DISCARD){
			FIRM_DISCARD_EVENT(cur_entry);	
		}
		else if(io_type == FLUSH){
			FIRM_FLUSH_EVENT(cur_entry);
		}

	}while(vssim_exit != 1);

	return NULL;
}


void *SSD_IO_THREAD_MAIN_LOOP(void *arg)
{
	long core_id = (long)arg;
	int w_buf_index = 0;
	int i;

	struct timespec ts_timeout;

	do{
		if(vs_core[core_id].read_queue.entry_nb != 0){

			/* Process each read request */
			DO_PER_CORE_READ(core_id);
		}
		else if(vs_core[core_id].discard_queue.entry_nb != 0){

			/* Discard command */
			DO_PER_CORE_DISCARD(core_id);
		}
		else if(TEST_FLUSH_FLAG(core_id)){

			/* Flush all write buffers */
			for(i=0; i<N_WRITE_BUF; i++){
				pthread_mutex_lock(&vssim_w_buf[i].lock);
				if(vssim_w_buf[i].n_empty_sectors != N_WB_SECTORS)
					vssim_w_buf[i].is_full = 1;
				pthread_mutex_unlock(&vssim_w_buf[i].lock);

				FLUSH_WRITE_BUFFER(core_id, i);
			}
		
			END_PER_CORE_FLUSH_REQUEST(core_id);

			/* Reset the flush flag*/
			RESET_FLUSH_FLAG(core_id);
		}
		else if(GET_WRITE_BUFFER_TO_FLUSH(core_id, &w_buf_index) == SUCCESS){
 
			/* Write data from write buffer to Flash */
			FLUSH_WRITE_BUFFER(core_id, w_buf_index);

			w_buf_index++;
			if(w_buf_index == N_WRITE_BUF){
				w_buf_index = 0;
			}
		}
		else{
			MAKE_TIMEOUT(&ts_timeout, FLUSH_TIMEOUT_USEC);
			pthread_cond_timedwait(&ssd_io_ready[core_id], 
					&ssd_io_lock[core_id],
					&ts_timeout);
		}

	}while(vssim_exit != 1);

	return NULL;
}


void *BACKGROUND_GC_THREAD_MAIN_LOOP(void *arg)
{
	long t_sleep_ms = GC_THREAD_MIN_SLEEP_TIME;
	block_entry* victim_block = NULL;

	do{
		/* Sleep 't_sllep_ms' milliseconds */
		usleep(t_sleep_ms);

next_gc:
		/* Get victim block */
		victim_block = SELECT_VICTIM_BLOCK();

		/* If there is no available victim block,
				increase sleep time and go to sleep */
		if(victim_block == NULL){
			INCREASE_SLEEP_TIME(&t_sleep_ms);
			continue;
		}

		/* Do garbage collection */
		GARBAGE_COLLECTION(victim_block);

		/* Whenever clean a Flash block,
				decrease the sleep time */
		DECREASE_SLEEP_TIME(&t_sleep_ms);

		goto next_gc;
		
	}while(vssim_exit != 1);
	
	return NULL;
}

void INSERT_NEW_PER_CORE_REQUEST(int core_id, event_queue_entry* eq_entry, 
			uint64_t sector_nb, uint32_t length, int w_buf_index)
{
	enum vssim_io_type io_type = eq_entry->io_type;
	bool flush = eq_entry->flush;

//TEMP
//	core_req_entry* last_cr_entry = NULL;
	core_req_entry* new_cr_entry = NULL;
	core_req_queue* cur_cr_queue = NULL;	

	/* Get per-core request queue */
	if(io_type == WRITE){
		cur_cr_queue = &vs_core[core_id].write_queue[w_buf_index];
	}
	else if(io_type == READ){
		cur_cr_queue = &vs_core[core_id].read_queue;
	}
	else if(io_type == DISCARD){
		cur_cr_queue = &vs_core[core_id].discard_queue;
	}
	else{
		printf("ERROR[%s] invalid io type: %d\n",
				__FUNCTION__, io_type);
		return;
	}

	/* Acquire lock for per-core request queue */
	pthread_mutex_lock(&cur_cr_queue->lock);

	/* Get the last entry of the core request queue */
//	last_cr_entry = cur_cr_queue->tail;

	/* Check whether the request can be merged with 
		the last entry */

//TEMP
/*	if(last_cr_entry != NULL
			&& last_cr_entry->parent == eq_entry
			&& last_cr_entry->sector_nb + last_cr_entry->length == sector_nb
			&& last_cr_entry->io_type == io_type
			&& last_cr_entry->flush == flush){

		last_cr_entry->length += length;
	}
	else{
*/
		/* Create core request entry */
		new_cr_entry = CREATE_NEW_CORE_EVENT(eq_entry, core_id, 
						sector_nb, length, flush);

		if(cur_cr_queue->entry_nb == 0){
			cur_cr_queue->head = new_cr_entry;
			cur_cr_queue->tail = new_cr_entry;
		}
		else{
			cur_cr_queue->tail->next = new_cr_entry;
			cur_cr_queue->tail = new_cr_entry;
		}

		cur_cr_queue->entry_nb++;
//	}

	/* Release lock for per-core request queue */
	pthread_mutex_unlock(&cur_cr_queue->lock);
}


/*
 * For the read event, w_buf_index should be -1.
 */
void INSERT_RW_TO_PER_CORE_EVENT_QUEUE(event_queue_entry* eq_entry, int w_buf_index)
{
	int i;
	int core_id;
	uint64_t lpn = 0;
	uint64_t local_lpn = 0;
	uint64_t sector_nb = eq_entry->sector_nb;
	uint64_t local_sector_nb;
	uint32_t remains = eq_entry->length;
	uint32_t sects = 0;
	uint64_t left_skip = sector_nb % SECTORS_PER_PAGE;
	uint64_t right_skip = 0;
	uint32_t n_pages = 0;

	uint64_t per_core_sector_nb[N_IO_CORES];
	uint32_t per_core_length[N_IO_CORES];
	bool per_core_io_flag[N_IO_CORES];

	for(i=0; i<N_IO_CORES; i++){
		per_core_sector_nb[i] = 0;
		per_core_length[i] = 0;
		per_core_io_flag[i] = false;
	}
	
	while(remains > 0){
		
		if(remains > SECTORS_PER_PAGE - left_skip){
			right_skip = 0;
		}
		else{
			right_skip = SECTORS_PER_PAGE - left_skip - remains;
		}
		sects = SECTORS_PER_PAGE - left_skip - right_skip;

		/* Get the core index */
		lpn = sector_nb / SECTORS_PER_PAGE;
		core_id = lpn % N_IO_CORES;
		
		/* Translate the global sector number to the local sector 
			number. For the single IO core, the local sector 
			number has same value with the global sector number */
		local_lpn = lpn / N_IO_CORES;
		local_sector_nb = sector_nb - (local_lpn * SECTORS_PER_PAGE \
				* (N_IO_CORES -1) + core_id * SECTORS_PER_PAGE); 

		if(!per_core_io_flag[core_id]){
			per_core_sector_nb[core_id] = local_sector_nb;
			per_core_length[core_id] = sects;
			per_core_io_flag[core_id] = true;

			eq_entry->n_child++;
		}
		else{
			if(per_core_sector_nb[core_id] + per_core_length[core_id]
					!= local_sector_nb){

				printf("ERROR[%s] per core io merge operation fail!\n",
					__FUNCTION__);
				break;
			}

			per_core_length[core_id] += sects;
		}

		/* Update the local variables */
		n_pages++;
		sector_nb += sects;
		remains -= sects;
		left_skip = 0;
	}

	eq_entry->n_pages = n_pages;

	for(i=0; i<N_IO_CORES; i++){

		if(per_core_io_flag[i]){
			/* Insert new per core request to the per core queue */
			INSERT_NEW_PER_CORE_REQUEST(i, eq_entry,
					per_core_sector_nb[i], per_core_length[i], w_buf_index);
		}
	}
}


void INSERT_DISCARD_TO_PER_CORE_EVENT_QUEUE(event_queue_entry* eq_entry)
{
	int i, j;
	int nr = eq_entry->length; 
	struct nvme_dsm_range range;
	void* src = eq_entry->buf;

	int core_id;
	uint64_t lpn = 0;
	uint64_t local_lpn = 0;
	uint64_t sector_nb = 0;
	uint64_t local_sector_nb;
	uint32_t remains = 0;
	uint32_t sects = 0;
	uint64_t left_skip = sector_nb % SECTORS_PER_PAGE;
	uint64_t right_skip = 0;

	uint64_t per_core_sector_nb[N_IO_CORES];
	uint32_t per_core_length[N_IO_CORES];
	bool per_core_io_flag[N_IO_CORES];

	for(i=0; i<nr; i++){
		
		src = src + i * sizeof(range);
		memcpy(&range, src, sizeof(range));

		sector_nb = range.slba;
		remains = range.nlb;

		for(j=0; j<N_IO_CORES; j++){
			per_core_sector_nb[j] = 0;
			per_core_length[j] = 0;
			per_core_io_flag[j] = false;
		}

		while(remains > 0){
			if(remains > SECTORS_PER_PAGE - left_skip){
				right_skip = 0;
			}
			else{
				right_skip = SECTORS_PER_PAGE - left_skip - remains;
			}
			sects = SECTORS_PER_PAGE - left_skip - right_skip;

			/* Get the core index */
			lpn = sector_nb / SECTORS_PER_PAGE;
			core_id = lpn % N_IO_CORES;
		
			/* Translate the global sector number to the local sector 
				number. For the single IO core, the local sector 
				number has same value with the global sector number */
			local_lpn = lpn / N_IO_CORES;
			local_sector_nb = sector_nb - (local_lpn * SECTORS_PER_PAGE \
					* (N_IO_CORES -1) + core_id * SECTORS_PER_PAGE); 

			if(!per_core_io_flag[core_id]){
				per_core_sector_nb[core_id] = local_sector_nb;
				per_core_length[core_id] = sects;
				per_core_io_flag[core_id] = true;

				eq_entry->n_child++;
			}
			else{
				if(per_core_sector_nb[core_id] + per_core_length[core_id]
						!= local_sector_nb){

					printf("ERROR[%s] per core io merge operation fail!\n",
							__FUNCTION__);
					break;
				}

				per_core_length[core_id] += sects;
			}

			/* Update the local variables */
			sector_nb += sects;
			remains -= sects;
			left_skip = 0;
		}

		for(j=0; j<N_IO_CORES; j++){

			if(per_core_io_flag[j]){
				/* Insert new per core request to the per core queue */
				INSERT_NEW_PER_CORE_REQUEST(j, eq_entry,
						per_core_sector_nb[j], per_core_length[j], -1);
			}
		}
	}

	return;
}


void INSERT_FLUSH_TO_PER_CORE_EVENT_QUEUE(event_queue_entry* eq_entry)
{
	int core_id;
	vssim_core* cur_vs_core;

	for(core_id=0; core_id<N_IO_CORES; core_id++){

		/* Get current vssim core */
		cur_vs_core = &vs_core[core_id];

		/* Acquire flush lock of the core */
		pthread_mutex_lock(&cur_vs_core->flush_lock);

		/* Update flush information of the core */
		cur_vs_core->flush_event = eq_entry;
		cur_vs_core->flush_flag = true;

		/* Release flush lock of the core */
		pthread_mutex_unlock(&cur_vs_core->flush_lock);
	}
}


core_req_entry* CREATE_NEW_CORE_EVENT(event_queue_entry* eq_entry, 
		int core_id, uint64_t sector_nb, uint32_t length, bool flush)
{
	core_req_entry* new_cr_entry = (core_req_entry*)calloc(1, sizeof(core_req_entry));
	if(new_cr_entry == NULL){
		printf("[%s] Allocation new core event fail.\n", __FUNCTION__);
		return NULL;
	}

	new_cr_entry->io_type	= eq_entry->io_type;
	new_cr_entry->sector_nb = sector_nb;
	new_cr_entry->length	= length;
	new_cr_entry->buf	= eq_entry->buf;
	new_cr_entry->parent	= eq_entry;
	new_cr_entry->flush	= flush;
	new_cr_entry->is_trimmed = false;
	new_cr_entry->next	= NULL;

	return new_cr_entry;
}

void WAKEUP_ALL_IO_THREADS(void)
{
	int i;

	for(i=0; i<N_IO_CORES; i++){
		pthread_cond_signal(&ssd_io_ready[i]);
	}	
}

void END_PER_CORE_READ_REQUEST(core_req_entry* cr_entry)
{
	bool is_trimmed = cr_entry->is_trimmed;

	event_queue_entry* parent_event = cr_entry->parent;

	/* If all child completed Flash IO,
		change the event state to COMPLETE */

	pthread_mutex_lock(&parent_event->lock);

	parent_event->n_completed++;

//TEMP
//	printf("[%s] end %lu-th event %d / %d (io_type %d)\n",
//		__FUNCTION__, parent_event->seq_nb, parent_event->n_completed,
//		parent_event->n_child, READ);

	if(is_trimmed){
		parent_event->n_trimmed++;
	}

	if(parent_event->n_child == parent_event->n_completed){

		UPDATE_EVENT_STATE(parent_event, COMPLETED);
	}

	pthread_mutex_unlock(&parent_event->lock);

	/* Release memory for the cr_entry */
	free(cr_entry);
}

void END_PER_CORE_WRITE_REQUEST(core_req_entry* cr_entry, int w_buf_index)
{
	if(cr_entry->flush == true){
		/* Update parent event entry */
		event_queue_entry* parent_event = cr_entry->parent;


		/* If all child completed Flash IO,
			change the event state to COMPLETE */
		pthread_mutex_lock(&parent_event->lock);

		parent_event->n_completed++;

//TEMP
//		printf("[%s] end %lu-th event %d / %d (io_type %d)\n",
//		__FUNCTION__, parent_event->seq_nb, parent_event->n_completed,
//		parent_event->n_child, WRITE);
	
		if(parent_event->n_child == parent_event->n_completed){

			UPDATE_EVENT_STATE(parent_event, COMPLETED);
		}

		pthread_mutex_unlock(&parent_event->lock);
	}

	/* Get the write buffer lock */
	pthread_mutex_lock(&vssim_w_buf[w_buf_index].lock);

	/* Update the number of empty sectors */
	vssim_w_buf[w_buf_index].n_empty_sectors += cr_entry->length;

	/* If the write buffer is empty, update the write buffer info. */
	if(vssim_w_buf[w_buf_index].n_empty_sectors
			== N_WB_SECTORS){

		vssim_w_buf[w_buf_index].is_full = 0;
		vssim_w_buf[w_buf_index].host_ptr
				= vssim_w_buf[w_buf_index].addr;

		/* Update last flush time */
		vssim_w_buf[w_buf_index].t_last_flush = get_usec();

		pthread_cond_signal(&vssim_w_buf[w_buf_index].ready);
	}

	/* Release the write buffer lock */
	pthread_mutex_unlock(&vssim_w_buf[w_buf_index].lock);

	/* Release memory for the cr_entry */
	free(cr_entry);
}


void END_PER_CORE_DISCARD_REQUEST(core_req_entry* cr_entry)
{
	enum event_state e_state;

	/* Update parent event entry */
	event_queue_entry* parent_event = cr_entry->parent;
	event_queue_entry* cur_eq_entry = NULL;
	event_queue_entry* temp_eq_entry = NULL;

	while(parent_event != NULL){
	    /* If all child completed discard operation,
  		deallocate the parent event  */
	    pthread_mutex_lock(&parent_event->lock);

	    parent_event->n_completed++;
	    if(parent_event->n_child == parent_event->n_completed){

	        UPDATE_EVENT_STATE(parent_event, COMPLETED);

	        pthread_mutex_unlock(&parent_event->lock);

	        if(parent_event->buf == vssim_discard_buf.ftl_ptr){

	            INCREASE_DISCARD_BUF_FTL_POINTER(parent_event->length);

	            pthread_mutex_lock(&cq_lock);
			
	            cur_eq_entry = parent_event->next;

	            REMOVE_FROM_CANDIDATE_EVENT_QUEUE(parent_event);

	            while(cur_eq_entry != NULL){

	                e_state = GET_EVENT_STATE(cur_eq_entry);

	                if(e_state == COMPLETED){
	                    /* Update the host pointer of the read buffer */
	                    INCREASE_DISCARD_BUF_FTL_POINTER(cur_eq_entry->length);

	                    temp_eq_entry = cur_eq_entry;
	                    cur_eq_entry = cur_eq_entry->next;

	                    /* Remove from the candidate event queue */
	                    REMOVE_FROM_CANDIDATE_EVENT_QUEUE(temp_eq_entry);
	                }
	                else{
	                    break;
	                }
	            }
	        }
	    }
	    else{
	        pthread_mutex_unlock(&parent_event->lock);
	    }
	}

	/* Release memory for the cr_entry */
	free(cr_entry);
}

void END_PER_CORE_FLUSH_REQUEST(int core_id)
{
	event_queue_entry* flush_event = vs_core[core_id].flush_event;

	/* If all child completed Flash IO,
		change the event state to COMPLETE */
	pthread_mutex_lock(&flush_event->lock);

	flush_event->n_completed++;
	if(flush_event->n_child == flush_event->n_completed){
		UPDATE_EVENT_STATE(flush_event, COMPLETED);
	}

	pthread_mutex_unlock(&flush_event->lock);
}


void SET_FLUSH_FLAG(int core_id)
{
	vssim_core* cur_vs_core = &vs_core[core_id];

	pthread_mutex_lock(&cur_vs_core->flush_lock);
	cur_vs_core->flush_flag = true;
	pthread_mutex_unlock(&cur_vs_core->flush_lock);
}


void RESET_FLUSH_FLAG(int core_id)
{
	vssim_core* cur_vs_core = &vs_core[core_id];

	pthread_mutex_lock(&cur_vs_core->flush_lock);
	cur_vs_core->flush_flag = false;
	pthread_mutex_unlock(&cur_vs_core->flush_lock);
}


bool TEST_FLUSH_FLAG(int core_id)
{
	bool ret;
	vssim_core* cur_vs_core = &vs_core[core_id];

	pthread_mutex_lock(&cur_vs_core->flush_lock);
	ret = cur_vs_core->flush_flag;
	pthread_mutex_unlock(&cur_vs_core->flush_lock);

	return ret;
}


int GET_NEXT_FLASH_LIST_INDEX(int core_id, int cur_flash_index)
{
	int cur_flash = cur_flash_index;
	int cur_channel = cur_flash % N_CHANNELS;
	int cur_way = cur_flash / N_CHANNELS;
	int ret_index = 0;

	/* First: channels */
        if(cur_channel + N_IO_CORES < N_CHANNELS){
                ret_index = cur_flash + N_IO_CORES;
        }
        else{
                cur_flash = cur_way * N_CHANNELS + core_id;
                cur_channel = cur_flash % N_CHANNELS;

                /* Second: ways */
                if(cur_flash + N_CHANNELS < N_FLASH){
                        ret_index = cur_flash + N_CHANNELS;
                }
                else{
			/* Third: go to first flash */
                        ret_index = core_id;
                }
        }

	return ret_index;
}

uint32_t GET_N_BGGC_PLANES(int core_id)
{
	uint32_t n_planes;
	vssim_core* cur_core = &vs_core[core_id];

	pthread_mutex_lock(&cur_core->n_bggc_lock);
	n_planes = cur_core->n_bggc_planes;
	pthread_mutex_unlock(&cur_core->n_bggc_lock);

	return n_planes;
}

uint32_t GET_N_FGGC_PLANES(int core_id)
{
	uint32_t n_planes;
	vssim_core* cur_core = &vs_core[core_id];

	pthread_mutex_lock(&cur_core->n_fggc_lock);
	n_planes = cur_core->n_fggc_planes;
	pthread_mutex_unlock(&cur_core->n_fggc_lock);

	return n_planes;
}

void INCREASE_N_BGGC_PLANES(int core_id)
{
	vssim_core* cur_core = &vs_core[core_id];

	pthread_mutex_lock(&cur_core->n_bggc_lock);
	cur_core->n_bggc_planes++;
	pthread_mutex_unlock(&cur_core->n_bggc_lock);
}

void DECREASE_N_BGGC_PLANES(int core_id)
{
	vssim_core* cur_core = &vs_core[core_id];

	pthread_mutex_lock(&cur_core->n_bggc_lock);
	cur_core->n_bggc_planes--;
	pthread_mutex_unlock(&cur_core->n_bggc_lock);
}

void INCREASE_N_FGGC_PLANES(int core_id)
{
	vssim_core* cur_core = &vs_core[core_id];

	pthread_mutex_lock(&cur_core->n_fggc_lock);
	cur_core->n_fggc_planes++;
	pthread_mutex_unlock(&cur_core->n_fggc_lock);
}

void DECREASE_N_FGGC_PLANES(int core_id)
{
	vssim_core* cur_core = &vs_core[core_id];

	pthread_mutex_lock(&cur_core->n_fggc_lock);
	cur_core->n_fggc_planes--;
	pthread_mutex_unlock(&cur_core->n_fggc_lock);
}

