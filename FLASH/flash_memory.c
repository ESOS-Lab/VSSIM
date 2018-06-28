// File: flash_memory.c
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

flash_channel* channel;
flash_memory* flash; 
io_proc_info* io_proc_i;

char flash_version[4] = "0.9";
char flash_date[9] = "18.01.31";

int INIT_FLASH(void){

	int i, j, k;

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
			flash[i].flash_nb = i;
			flash[i].channel_nb = i % N_CHANNELS;
			flash[i].next = NULL;

			/* Init Plane */
			flash[i].plane = (plane*)calloc(sizeof(plane), N_PLANES_PER_FLASH);
			if(flash[i].plane == NULL){
				printf("ERROR[%s] alloc plane fail!\n", __FUNCTION__);
				return FAIL;
			}
			else{
				for(j=0; j<N_PLANES_PER_FLASH; j++){

					flash[i].plane[j].cmd = CMD_NOOP;

					/* Init Flash I/O delay */
					flash[i].plane[j].reg_cmd_set_delay = REG_CMD_SET_DELAY;
					flash[i].plane[j].reg_write_delay = REG_WRITE_DELAY;
					flash[i].plane[j].page_program_delay = PAGE_PROGRAM_DELAY;
					flash[i].plane[j].reg_read_delay = REG_READ_DELAY;
					flash[i].plane[j].page_read_delay = PAGE_READ_DELAY;
					flash[i].plane[j].block_erase_delay = BLOCK_ERASE_DELAY;
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
			flash[i].plane[j].page_cache.core_id = -1;

			flash[i].plane[j].data_reg.state = REG_NOOP;
			flash[i].plane[j].data_reg.ppn.addr = -1;
			flash[i].plane[j].data_reg.t_end = -1;
			flash[i].plane[j].data_reg.core_id = -1;

			/* Init ppn list per flash */
			flash[i].plane[j].cmd_list = (uint8_t*)calloc(sizeof(uint8_t), N_PPNS_PER_PLANE);
			flash[i].plane[j].ppn_list = (ppn_t*)calloc(sizeof(ppn_t), N_PPNS_PER_PLANE);
			flash[i].plane[j].copyback_list = (ppn_t*)calloc(sizeof(ppn_t), N_PPNS_PER_PLANE);
			flash[i].plane[j].core_id_list = (int*)calloc(sizeof(int), N_PPNS_PER_PLANE);

			if(flash[i].plane[j].cmd_list == NULL 
					|| flash[i].plane[j].ppn_list == NULL
					|| flash[i].plane[j].copyback_list == NULL
					|| flash[i].plane[j].core_id_list == NULL){
				printf("ERROR[%s] per plane command list alloc fail!\n", __FUNCTION__);
				return FAIL;
			}
			else{
				for(k=0; k<N_PPNS_PER_PLANE; k++){

					flash[i].plane[j].cmd_list[k] = CMD_NOOP;
					flash[i].plane[j].ppn_list[k].addr = -1;
					flash[i].plane[j].copyback_list[k].addr = -1;
					flash[i].plane[j].core_id_list[k] = -1;
				}
				pthread_mutex_init(&flash[i].plane[j].lock, NULL);

				flash[i].plane[j].n_entries = 0;
				flash[i].plane[j].index = 0;
			}
	
		}
	}

	/* Init Flash memory list*/
	for(i=0; i<N_IO_CORES; i++){
		INIT_FLASH_MEMORY_LIST(i);
	}

	/* Init io processing info */
	if(BACKGROUND_GC_ENABLE)
		io_proc_i = (io_proc_info*)calloc(sizeof(io_proc_info), N_IO_CORES+1);
	else
		io_proc_i = (io_proc_info*)calloc(sizeof(io_proc_info), N_IO_CORES);

	if(io_proc_i == NULL){
		printf("ERROR[%s] alloc io proc info fail!\n", __FUNCTION__);
		return FAIL;
	}
	else{
		if(BACKGROUND_GC_ENABLE){
			for(i=0; i<N_IO_CORES + 1; i++){
				io_proc_i[i].n_submitted_io = 0;
				io_proc_i[i].n_completed_io = 0;
			}
		}
		else{
			for(i=0; i<N_IO_CORES; i++){
				io_proc_i[i].n_submitted_io = 0;
				io_proc_i[i].n_completed_io = 0;
			}
		}
	}

	return SUCCESS;
}

void INIT_FLASH_MEMORY_LIST(int core_id)
{
	/* Get flash info header for the core */
	flash_memory* cur_flash = &flash[core_id];
	int cur_flash_index = core_id;
	
	while (1){

		/* Get next flash index for the core */
		cur_flash_index = GET_NEXT_FLASH_LIST_INDEX(core_id, cur_flash_index);

		/* Update the flash list */
		if(cur_flash_index == core_id){
			
			cur_flash->next = &flash[cur_flash_index];
			break;
		}
		else{
			cur_flash->next = &flash[cur_flash_index];
			cur_flash = cur_flash->next;
		}
	}
}

int TERM_FLASH(void)
{
	int i, j;

	for(i=0; i<N_FLASH; i++){

		for(j=0; j<N_PLANES_PER_FLASH; j++){

			free(flash[i].plane[j].cmd_list);
			free(flash[i].plane[j].ppn_list);
		}
		free(flash[i].plane);
	}
	free(flash);
	free(channel);

	return SUCCESS;
} 

void FLASH_STATE_CHECKER(int core_id)
{
	int i;
	int core_index = 0;
	int channel_nb;
	int64_t t_now;

	flash_memory* cur_flash;
	flash_memory* init_flash;
	plane* cur_plane;

	if(core_id == bggc_core_id){
		cur_flash = &flash[0];
	}
	else{
		cur_flash = &flash[core_id];
	}
	init_flash = cur_flash;

	t_now = get_usec();

	do{
		for(i=0; i<N_PLANES_PER_FLASH; i++){

			channel_nb = cur_flash->channel_nb;
			cur_plane = &cur_flash->plane[i];

#ifdef FLASH_DEBUG_LOOP
			if(cur_plane->data_reg.state != REG_NOOP
					&& cur_plane->data_reg.core_id == core_id){
				printf("[%s] %d core : f %d p %d (cmd %u) data state: %d (%ld), index: %d / %d, core_id: %d (%d / %d)\n", 
					__FUNCTION__, core_id, 
					cur_flash->flash_nb, i, cur_plane->cmd,
					cur_plane->data_reg.state, 
					cur_plane->data_reg.t_end - t_now,
					cur_plane->index,
					cur_plane->n_entries,
					cur_plane->data_reg.core_id,
					io_proc_i[core_id].n_submitted_io,
					io_proc_i[core_id].n_completed_io
				);
			}
#endif

			/* Update data register state of this plane */
			UPDATE_DATA_REGISTER(core_id, cur_plane, channel_nb, t_now, i);

			if(PAGE_CACHE_REG_ENABLE){
				/* Update page cache register state of this plane */
				UPDATE_PAGE_CACHE_REGISTER(core_id, cur_plane, channel_nb, t_now);
			}
		}

		if(core_id == bggc_core_id){
			cur_flash = &flash[++core_index];

			if(core_index == N_FLASH){
				break;
			}
		}
		else{
			cur_flash = cur_flash->next;
		}

	}while(cur_flash != init_flash);
}


void UPDATE_DATA_REGISTER(int core_id, plane* cur_plane, int channel_nb, int64_t t_now, int plane_nb)
{
	reg* data_reg = &cur_plane->data_reg;
	reg* page_cache = &cur_plane->page_cache;

	uint32_t index;
	uint32_t n_entries;

	uint8_t next_cmd;
	uint8_t cmd = cur_plane->cmd;
	ppn_t next_ppn;
	ppn_t copyback_ppn;

	int reg_core_id;

	/* Get plane list lock */
	pthread_mutex_lock(&cur_plane->lock);

	if(data_reg->core_id != -1 && data_reg->core_id != core_id){
		pthread_mutex_unlock(&cur_plane->lock);
		return;
	}

	switch(data_reg->state){

		case REG_NOOP:

			index = cur_plane->index;
			n_entries = cur_plane->n_entries;

			if(!PAGE_CACHE_REG_ENABLE
					&& index != n_entries){

				next_cmd = cur_plane->cmd_list[index];
				next_ppn = cur_plane->ppn_list[index];
				copyback_ppn = cur_plane->copyback_list[index];
				reg_core_id = cur_plane->core_id_list[index];

				/* Init per-plane ppn list */
				cur_plane->index++;
				if(cur_plane->index == cur_plane->n_entries){
					cur_plane->index = 0;
					cur_plane->n_entries = 0;
				}

				/* Get next available channel access time */
				data_reg->t_end = GET_AND_UPDATE_NEXT_AVAILABLE_CH_TIME(channel_nb,
							t_now, next_cmd, cur_plane, SET_CMD1);

				/* Update data reg state */
				data_reg->ppn = next_ppn;
				data_reg->copyback_ppn = copyback_ppn;
				data_reg->core_id = reg_core_id;

				if(data_reg->t_end <= t_now){

					/* Update register  */
					data_reg->state = SET_CMD1;
					data_reg->t_end = t_now + cur_plane->reg_cmd_set_delay;
				}
				else{
					/* Update register  */
					data_reg->state = WAIT_CHANNEL_FOR_CMD1;
				}

				/* Update plane */
				cur_plane->cmd = next_cmd;
			}

			break;

		case SET_CMD1:
			if(data_reg->t_end <= t_now){
				if(cmd == CMD_PAGE_READ){

					/* Update register  */
					data_reg->state = PAGE_READ;
					data_reg->t_end = t_now + cur_plane->page_read_delay;
				}
				else if(cmd == CMD_PAGE_PROGRAM){

					/* Update register  */
					data_reg->state = REG_WRITE;
					data_reg->t_end = t_now + cur_plane->reg_write_delay;
				}
				else if(cmd == CMD_BLOCK_ERASE){

					/* Update register  */
					data_reg->state = BLOCK_ERASE;
					data_reg->t_end = t_now + cur_plane->block_erase_delay;
				}
				else if(cmd == CMD_PAGE_COPYBACK){

					/* Update register  */
					data_reg->state = PAGE_READ;
					data_reg->t_end = t_now + cur_plane->page_read_delay;
				}
				else if(cmd == CMD_PAGE_COPYBACK_PHASE2){

					/* Update register  */
					data_reg->state = PAGE_PROGRAM;
					data_reg->t_end = t_now + cur_plane->page_program_delay;
				}
			}
			break;

		case SET_CMD2:
			if(data_reg->t_end <= t_now){
				if(cmd == CMD_PAGE_READ){

					/* Update register  */
					data_reg->state = REG_READ;
					data_reg->t_end = t_now + cur_plane->reg_read_delay;
				}
				else if(cmd == CMD_PAGE_PROGRAM){

					/* Update register  */
					data_reg->state = PAGE_PROGRAM;
					data_reg->t_end = t_now + cur_plane->page_program_delay;
				}
				else if(cmd == CMD_PAGE_COPYBACK){

					ppn_t dst_ppn = data_reg->copyback_ppn;
					int dst_flash_nb = dst_ppn.path.flash;
					int dst_plane_nb = dst_ppn.path.plane;
					int reg_core_id = data_reg->core_id;

					/* Update register  */
					plane* dst_plane = &flash[dst_flash_nb].plane[dst_plane_nb];

					if(dst_plane == cur_plane){
						data_reg->ppn = dst_ppn;
						data_reg->state = PAGE_PROGRAM;
						data_reg->t_end = t_now + cur_plane->page_program_delay;
					}
					else{
						reg* dst_reg = &dst_plane->data_reg;
						if(dst_reg->state != REG_NOOP){
							data_reg->t_end = dst_reg->t_end;
							data_reg->state = WAIT_REG;
						}
						else{
							/* Update destination reg */
							dst_reg->ppn = dst_ppn;
							dst_reg->state = PAGE_PROGRAM;
							dst_reg->core_id = reg_core_id; 
							dst_reg->t_end = t_now + cur_plane->page_program_delay;

							dst_plane->cmd = CMD_PAGE_COPYBACK;

							/* Init current reg */
							data_reg->ppn.addr = -1;
							data_reg->t_end = -1;
							data_reg->state = REG_NOOP;
							data_reg->core_id = -1;

							cur_plane->cmd = CMD_NOOP;
						}
					}
				}
			}
			break;

		case REG_WRITE:
			if(data_reg->t_end <= t_now
					&& cmd == CMD_PAGE_PROGRAM){

				/* Update register  */
				data_reg->state = SET_CMD2;
				data_reg->t_end = t_now + cur_plane->reg_cmd_set_delay;
			}

			break;

		case REG_READ:
			/* Complete Flash page read */
			if(data_reg->t_end <= t_now){

				int reg_core_id = data_reg->core_id;

				/* Initialize current register */
				data_reg->state = REG_NOOP;
				data_reg->ppn.addr = -1;
				data_reg->t_end = -1;
				data_reg->core_id = -1;

				/* Initialized current plane */	
				if(PAGE_CACHE_REG_ENABLE){
					if(page_cache->state == REG_NOOP)
						cur_plane->cmd = CMD_NOOP;
				}
				else{
					cur_plane->cmd = CMD_NOOP;
				}

				UPDATE_IO_PROC_INFO(reg_core_id);
			}
			break;

		case PAGE_PROGRAM:
			/* Complete Flash page program */
			if (data_reg->t_end <= t_now){

				int reg_core_id = data_reg->core_id;

				/* Initialize current register */
				data_reg->state = REG_NOOP;
				data_reg->ppn.addr = -1;
				data_reg->t_end = -1;
				data_reg->core_id = -1;

				/* Initialized current plane */	
				if(PAGE_CACHE_REG_ENABLE){
					if(page_cache->state == REG_NOOP)
						cur_plane->cmd = CMD_NOOP;
				}
				else{
					cur_plane->cmd = CMD_NOOP;
				}

				UPDATE_IO_PROC_INFO(reg_core_id);
			}
			break;

		case PAGE_READ:
			/* Complete Flash page read */
			if(data_reg->t_end <= t_now){

				if(cmd == CMD_PAGE_READ){
					data_reg->t_end = GET_AND_UPDATE_NEXT_AVAILABLE_CH_TIME(channel_nb, 
								t_now, cmd, cur_plane, PAGE_READ);

					if(data_reg->t_end <= t_now){
					
						/* Update state */
						data_reg->state = SET_CMD2;
						data_reg->t_end = t_now + cur_plane->reg_cmd_set_delay;
					}
					else{
						/* Update state */
						data_reg->state = WAIT_CHANNEL_FOR_CMD2;
					}
				}
				else if(cmd == CMD_PAGE_COPYBACK){

					ppn_t dst_ppn = data_reg->copyback_ppn;
					int dst_flash_nb = dst_ppn.path.flash;
					int dst_plane_nb = dst_ppn.path.plane;
					int reg_core_id = data_reg->core_id;

					/* Update register  */
					plane* dst_plane = &flash[dst_flash_nb].plane[dst_plane_nb];
	
					if(dst_plane == cur_plane){
						/* Start copyback immediately */
						data_reg->ppn = dst_ppn;
						data_reg->state = PAGE_PROGRAM;
						data_reg->t_end = t_now + cur_plane->page_program_delay;		
					}
					else{
						FLASH_PAGE_COPYBACK_PHASE2(reg_core_id, dst_ppn, data_reg->ppn);

						/* Reset the current register */
						data_reg->ppn.addr = -1;
						data_reg->state = REG_NOOP;
						data_reg->t_end = -1;
						data_reg->core_id = -1;

						cur_plane->cmd = CMD_NOOP;
					}
				}
			}

			break;

		case BLOCK_ERASE:

			/* Complete Flash block erase */
			if(data_reg->t_end <= t_now){

				int reg_core_id = data_reg->core_id;

				/* Initialize current register */
				data_reg->state = REG_NOOP;
				data_reg->ppn.addr = -1;
				data_reg->t_end = -1;
				data_reg->core_id = -1;

				/* Initialized current plane */	
				cur_plane->cmd = CMD_NOOP;

				UPDATE_IO_PROC_INFO(reg_core_id);
			}
			break;

		case WAIT_CHANNEL_FOR_CMD1:
			if(data_reg->t_end <= t_now){
				
				/* Update state */
				data_reg->state = SET_CMD1;
				data_reg->t_end = t_now + cur_plane->reg_cmd_set_delay;
			}
			break;

		case WAIT_CHANNEL_FOR_CMD2:

			if(data_reg->t_end <= t_now){
				if(cmd == CMD_PAGE_READ){

					/* Update state */
					data_reg->state = SET_CMD2;
					data_reg->t_end = t_now + cur_plane->reg_cmd_set_delay;
				}
			}
			break;

		default:
			break;
	}

	/* Release plane list lock */
	pthread_mutex_unlock(&cur_plane->lock);
}


void UPDATE_PAGE_CACHE_REGISTER(int core_id, plane* cur_plane, int channel_nb, int64_t t_now)
{
	reg* data_reg = &cur_plane->data_reg;
	reg* page_cache = &cur_plane->page_cache;

	uint32_t index;
	uint32_t n_entries;

	uint8_t next_cmd;
	uint8_t cmd = cur_plane->cmd;
	ppn_t next_ppn;
	ppn_t copyback_ppn;

	int reg_core_id;

	/* Get plane list lock */
	pthread_mutex_lock(&cur_plane->lock);

	if(page_cache->core_id != -1 && page_cache->core_id != core_id){
		pthread_mutex_unlock(&cur_plane->lock);
		return;
	}

	switch(page_cache->state){

		case REG_NOOP:

			index = cur_plane->index;
			n_entries = cur_plane->n_entries;

			if(index != n_entries){

				next_cmd = cur_plane->cmd_list[index];
				next_ppn = cur_plane->ppn_list[index];
				copyback_ppn = cur_plane->copyback_list[index];
				reg_core_id = cur_plane->core_id_list[index];

				if (cur_plane->cmd != CMD_NOOP && cur_plane->cmd != next_cmd){
					break;
				}

				cur_plane->index++;

				/* Init per-plane ppn list */
				if(cur_plane->index == n_entries){
					cur_plane->index = 0;
					cur_plane->n_entries = 0;
				}

				/* Get next available channel access time */
				page_cache->t_end = GET_AND_UPDATE_NEXT_AVAILABLE_CH_TIME(channel_nb,
							t_now, next_cmd, cur_plane, SET_CMD1);

				page_cache->ppn = next_ppn;
				page_cache->copyback_ppn = copyback_ppn;
				page_cache->core_id = reg_core_id;

				if(page_cache->t_end <= t_now){

					/* Update register  */
					page_cache->state = SET_CMD1;
					page_cache->t_end = t_now + cur_plane->reg_cmd_set_delay;

					/* Update plane */
					cur_plane->cmd = next_cmd;
				}
				else{
					/* Update register  */
					page_cache->state = WAIT_CHANNEL_FOR_CMD1;

					/* Update plane */
					cur_plane->cmd = next_cmd;
				}
			}
			break;

		case SET_CMD1:
			if(page_cache->t_end <= t_now){
				if(cmd == CMD_PAGE_READ){

					/* Update register  */
					page_cache->state = PAGE_READ;
					page_cache->t_end = t_now + cur_plane->page_read_delay;
				}
				else if(cmd == CMD_PAGE_PROGRAM){

					/* Update register  */
					page_cache->state = REG_WRITE;
					page_cache->t_end = t_now + cur_plane->reg_write_delay;
				}
				else if(cmd == CMD_BLOCK_ERASE){

					/* Update register  */
					page_cache->state = BLOCK_ERASE;
					page_cache->t_end = t_now + cur_plane->block_erase_delay;
				}
				else if(cmd == CMD_PAGE_COPYBACK){

					/* Update register  */
					page_cache->state = PAGE_READ;
					page_cache->t_end = t_now + cur_plane->page_read_delay;
				}
			}
			break;

		case SET_CMD2:
			if(page_cache->t_end <= t_now){

				if(cmd == CMD_PAGE_PROGRAM){
					if(data_reg->state != REG_NOOP){
						page_cache->t_end = data_reg->t_end;
						page_cache->state = WAIT_REG;
					}
					else{
						/* Update register  */
						COPY_PAGE_CACHE_TO_DATA_REG(data_reg, page_cache);

						/* Set data register */
						cur_plane->cmd = CMD_PAGE_PROGRAM;
						data_reg->t_end = t_now + cur_plane->page_program_delay;
						data_reg->state = PAGE_PROGRAM;

						/* Initialize page cache */
						page_cache->state = REG_NOOP;
						page_cache->ppn.addr = -1;
						page_cache->t_end = -1;
						page_cache->core_id = -1;
					}
				}
				else if(cmd == CMD_PAGE_COPYBACK){
					ppn_t dst_ppn = page_cache->copyback_ppn;
					int dst_flash_nb = dst_ppn.path.flash;
					int dst_plane_nb = dst_ppn.path.plane;
					reg_core_id = page_cache->core_id;

					/* Update register  */
					plane* dst_plane = &flash[dst_flash_nb].plane[dst_plane_nb];

					if(dst_plane == cur_plane){
						/* Update destination reg */
						page_cache->ppn = dst_ppn;
						page_cache->state = PAGE_PROGRAM;
						page_cache->t_end = t_now + cur_plane->page_program_delay;
					}
					else{
						reg* dst_reg = &dst_plane->page_cache;

						if(dst_plane->cmd != CMD_NOOP){
							/* Wait until target plane is idle */
							page_cache->t_end = dst_reg->t_end;
							page_cache->state = WAIT_REG;
						}
						else{
							/* Update destination reg */
							dst_reg->ppn = dst_ppn;
							dst_reg->state = PAGE_PROGRAM; 
							dst_reg->t_end = t_now + cur_plane->page_program_delay;
							dst_reg->core_id = reg_core_id;
							dst_plane->cmd = CMD_PAGE_COPYBACK;

							/* Init current reg */
							page_cache->ppn.addr = -1;
							page_cache->t_end = -1;
							page_cache->state = REG_NOOP;
							page_cache->core_id = -1;
							cur_plane->cmd = CMD_NOOP;
						}
					}
				}
			}
			break;

		case REG_WRITE:
			if(page_cache->t_end <= t_now){
				if(cmd == CMD_PAGE_PROGRAM){

					/* Update register  */
					page_cache->state = SET_CMD2;
					page_cache->t_end = t_now + cur_plane->reg_cmd_set_delay;
				}
			}

			break;

		case PAGE_PROGRAM:
			if(page_cache->t_end <= t_now){

				reg_core_id = page_cache->core_id;

				if(cmd == CMD_PAGE_COPYBACK){

					/* Update destination reg */
					page_cache->ppn.addr = -1;
					page_cache->state = REG_NOOP;
					page_cache->t_end = -1;
					page_cache->core_id = -1;				
	
					cur_plane->cmd = CMD_NOOP;

					UPDATE_IO_PROC_INFO(reg_core_id);
				}
			 	else if(cmd == CMD_PAGE_COPYBACK_PHASE2){
					ppn_t src_ppn = page_cache->copyback_ppn;
					int src_flash_nb = src_ppn.path.flash;
					int src_plane_nb = src_ppn.path.plane;
					plane* src_plane = &flash[src_flash_nb].plane[src_plane_nb];
					reg* src_reg = &src_plane->page_cache;

					/* Init current register */	
					src_reg->ppn.addr = -1;
					src_reg->state = REG_NOOP;
					src_reg->t_end = -1;
					src_reg->core_id = -1;

					src_plane->cmd = CMD_NOOP;

					UPDATE_IO_PROC_INFO(reg_core_id);
				}
			}
			break;

		case PAGE_READ:
			if(page_cache->t_end <= t_now){

				if(cmd == CMD_PAGE_READ){
					if(data_reg->state != REG_NOOP){
						page_cache->t_end = data_reg->t_end;
						page_cache->state = WAIT_REG;
					}
					else{
						COPY_PAGE_CACHE_TO_DATA_REG(data_reg, page_cache);

						/* Set data register */
						data_reg->state = PAGE_READ;
						data_reg->t_end = t_now;
						data_reg->core_id = page_cache->core_id;
						cur_plane->cmd = CMD_PAGE_READ;

						/* Initialize page cache */
						page_cache->state = REG_NOOP;
						page_cache->ppn.addr = -1;
						page_cache->t_end = -1;
						page_cache->core_id = -1;	
					}
				}
				else if(cmd == CMD_PAGE_COPYBACK){

					ppn_t dst_ppn = page_cache->copyback_ppn;
					int dst_flash_nb = dst_ppn.path.flash;
					int dst_plane_nb = dst_ppn.path.plane;
					int reg_core_id = page_cache->core_id;

					/* Update register  */
					plane* dst_plane = &flash[dst_flash_nb].plane[dst_plane_nb];
	
					if(dst_plane == cur_plane){
						page_cache->ppn = dst_ppn;
						page_cache->state = PAGE_PROGRAM;
						page_cache->t_end = t_now + cur_plane->page_program_delay;		
					}
					else{
						FLASH_PAGE_COPYBACK_PHASE2(reg_core_id, 
									dst_ppn, data_reg->ppn);
					}
				}
			}
			break;

		case BLOCK_ERASE:
			/* Complete Flash block erase */
			if(page_cache->t_end <= t_now){

				reg_core_id = page_cache->core_id;

				/* Initialize current register */
				page_cache->state = REG_NOOP;
				page_cache->ppn.addr = -1;
				page_cache->t_end = -1;
				page_cache->core_id = -1;

				/* Initialized current plane */	
				cur_plane->cmd = CMD_NOOP;

				UPDATE_IO_PROC_INFO(reg_core_id);
			}
			break;


		case WAIT_CHANNEL_FOR_CMD1:
			if(page_cache->t_end <= t_now){
				
				/* Update state */
				page_cache->state = SET_CMD1;
				page_cache->t_end = t_now + cur_plane->reg_cmd_set_delay;
			}
			break;

		case WAIT_REG:
			if(page_cache->t_end <= t_now){
				reg_core_id = page_cache->core_id;

				if(cmd == CMD_PAGE_PROGRAM){
					COPY_PAGE_CACHE_TO_DATA_REG(data_reg, page_cache);

					/* Set data register */
					data_reg->t_end = t_now + cur_plane->page_program_delay;
					data_reg->state = PAGE_PROGRAM;
					data_reg->core_id = reg_core_id;
					cur_plane->cmd = CMD_PAGE_PROGRAM;

					/* Initialize page cache */
					page_cache->state = REG_NOOP;
					page_cache->ppn.addr = -1;
					page_cache->t_end = -1;
					page_cache->core_id = -1;
				}
				else if(cmd == CMD_PAGE_READ){
					COPY_PAGE_CACHE_TO_DATA_REG(data_reg, page_cache);

					/* Set data register */
					data_reg->state = PAGE_READ;
					data_reg->t_end = t_now;
					data_reg->core_id = reg_core_id;
					cur_plane->cmd = CMD_PAGE_READ;

					/* Initialize page cache */
					page_cache->state = REG_NOOP;
					page_cache->ppn.addr = -1;
					page_cache->t_end = -1;
					page_cache->core_id = -1;
				}
			}
			break;

		default:
			break;
	}

	/* Release plane list lock */
	pthread_mutex_unlock(&cur_plane->lock);
}


int FLASH_PAGE_WRITE(int core_id, ppn_t ppn)
{
	uint32_t index;
	int flash_nb = (int)ppn.path.flash;
	int plane_nb = (int)ppn.path.plane;

	plane* cur_plane = &flash[flash_nb].plane[plane_nb];

#ifdef FLASH_DEBUG
	printf("[%s] %d-core write to f %d p %d\n",
		__FUNCTION__, core_id, flash_nb, plane_nb);
#endif

	/* Get plane list lock */
	pthread_mutex_lock(&cur_plane->lock);

	/* Get ppn list index */
	index = cur_plane->n_entries;
	if(index >= N_PPNS_PER_PLANE){
		printf("ERROR[%s] %d-core: Exceed ppn list index: %u (f %d p %d)\n",
				__FUNCTION__, core_id, index, flash_nb, plane_nb);
		return FAIL;
	}

	/* Insert ppn and cmd to the flash ppn list */
	cur_plane->ppn_list[index] = ppn;
	cur_plane->cmd_list[index] = CMD_PAGE_PROGRAM;
	cur_plane->core_id_list[index] = core_id;
	cur_plane->n_entries++;
	
#ifdef FLASH_DEBUG
	printf("[%s] %d-core insert w entry to %d index\n",
		__FUNCTION__, core_id, index);
#endif

	/* Release list lock */
	pthread_mutex_unlock(&cur_plane->lock);

	/* Update io proc info */
	io_proc_i[core_id].n_submitted_io++;

	return SUCCESS;
}

int FLASH_PAGE_READ(int core_id, ppn_t ppn)
{
	uint32_t index;
	int flash_nb = (int)ppn.path.flash;
	int plane_nb = (int)ppn.path.plane;

	plane* cur_plane = &flash[flash_nb].plane[plane_nb];

	/* Get plane list lock */
	pthread_mutex_lock(&cur_plane->lock);

	/* Get ppn list index */
	index = cur_plane->n_entries;
	if(index >= N_PPNS_PER_PLANE){
		printf("ERROR[%s] Exceed ppn list index: %u\n",
				__FUNCTION__, index);
		return FAIL;
	}

	/* Insert ppn and cmd to the flash ppn list */
	cur_plane->ppn_list[index] = ppn;
	cur_plane->cmd_list[index] = CMD_PAGE_READ;
	cur_plane->core_id_list[index] = core_id;
	cur_plane->n_entries++;

	/* Release plane list lock */
	pthread_mutex_unlock(&cur_plane->lock);

	/* Update io proc info */
	io_proc_i[core_id].n_submitted_io++;

	return SUCCESS;
}

int FLASH_PAGE_COPYBACK(int core_id, ppn_t dst_ppn, ppn_t src_ppn)
{
	uint32_t index;
	int flash_nb = (int)src_ppn.path.flash;
	int plane_nb = (int)src_ppn.path.plane;

	plane* cur_plane = &flash[flash_nb].plane[plane_nb];

	/* Get ppn list index */
	pthread_mutex_lock(&cur_plane->lock);

	/* Get ppn list index */
	index = cur_plane->n_entries;

	if(index >= N_PPNS_PER_PLANE){
		printf("ERROR[%s] Exceed ppn list index: %u\n",
				__FUNCTION__, index);
		return FAIL;
	}

	/* Insert ppn and cmd to the flash ppn list */
	cur_plane->ppn_list[index] = src_ppn;
	cur_plane->copyback_list[index] = dst_ppn;
	cur_plane->cmd_list[index] = CMD_PAGE_COPYBACK;
	cur_plane->core_id_list[index] = core_id;

	cur_plane->n_entries++;
	
	pthread_mutex_unlock(&cur_plane->lock);

	/* Update io proc info */
	io_proc_i[core_id].n_submitted_io++;

	return SUCCESS;
}

int FLASH_PAGE_COPYBACK_PHASE2(int core_id, ppn_t dst_ppn, ppn_t src_ppn)
{
	uint32_t index;
	int flash_nb = (int)dst_ppn.path.flash;
	int plane_nb = (int)dst_ppn.path.plane;

	plane* cur_plane = &flash[flash_nb].plane[plane_nb];

	/* Get ppn list index */
	pthread_mutex_lock(&cur_plane->lock);

	index = cur_plane->n_entries;
	if(index >= N_PPNS_PER_PLANE){
		printf("ERROR[%s] Exceed ppn list index: %u\n",
				__FUNCTION__, index);
		return FAIL;
	}

	/* Insert ppn and cmd to the flash ppn list */
	cur_plane->ppn_list[index] = dst_ppn;
	cur_plane->copyback_list[index] = src_ppn;
	cur_plane->cmd_list[index] = CMD_PAGE_COPYBACK_PHASE2;
	cur_plane->core_id_list[index] = core_id;
	cur_plane->n_entries++;
	
	pthread_mutex_unlock(&cur_plane->lock);

	/* Update io proc info */
	io_proc_i[core_id].n_submitted_io++;

	return SUCCESS;
}

int FLASH_BLOCK_ERASE(int core_id, pbn_t pbn)
{
	uint32_t index;
	int flash_nb = (int)pbn.path.flash;
	int plane_nb = (int)pbn.path.plane;
	ppn_t ppn = PBN_TO_PPN(pbn, 0);

	plane* cur_plane = &flash[flash_nb].plane[plane_nb];

	/* Insert ppn and cmd to the flash ppn list */
	pthread_mutex_lock(&cur_plane->lock);

	/* Get ppn list index */
	index = cur_plane->n_entries;
	if(index >= N_PPNS_PER_PLANE){
		printf("ERROR[%s] Exceed ppn list index: %u\n",
				__FUNCTION__, index);
		return FAIL;
	}

	cur_plane->ppn_list[index] = ppn;
	cur_plane->cmd_list[index] = CMD_BLOCK_ERASE;
	cur_plane->core_id_list[index] = core_id;
	cur_plane->n_entries++;

	pthread_mutex_unlock(&cur_plane->lock);

	/* Update io proc info */
	io_proc_i[core_id].n_submitted_io++;

	return SUCCESS;
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
			uint8_t cmd, plane* cur_plane,  enum reg_state cur_state)
{
	int64_t t_next_idle;
	int64_t t_ret;
	int64_t t_update;

	flash_channel* cur_channel = &channel[channel_nb];

	/* Get next available channel time */
	t_next_idle = cur_channel->t_next_idle;
	if(t_next_idle < t_now){
		t_ret = t_now;
	}
	else{
		t_ret = t_next_idle;
	}

	/* Update next available channel time */
	if(cur_state == SET_CMD1 && cmd == CMD_PAGE_PROGRAM){
		t_update = cur_plane->reg_cmd_set_delay * 2 + cur_plane->reg_write_delay;
	}
	else if(cur_state == SET_CMD1 && cmd == CMD_PAGE_READ){
		t_update = cur_plane->reg_cmd_set_delay;
	}
	else if(cur_state == SET_CMD1 && cmd == CMD_PAGE_COPYBACK){
		t_update = cur_plane->reg_cmd_set_delay;
	}
	else if(cur_state == SET_CMD1 && cmd == CMD_BLOCK_ERASE){
		t_update = cur_plane->reg_cmd_set_delay;
	}
	else if(cur_state == SET_CMD1 && cmd == CMD_PAGE_COPYBACK_PHASE2){
		t_update = cur_plane->reg_cmd_set_delay;
	}
	else if(cur_state == PAGE_READ && cmd == CMD_PAGE_COPYBACK){
		t_update = cur_plane->reg_cmd_set_delay;
	}
	else if(cur_state == PAGE_READ){
		t_update = cur_plane->reg_cmd_set_delay
					+ cur_plane->reg_read_delay;
	}
	else{
		printf("ERROR[%s] Wrong cmd(%d) and state (%d)!\n",
				__FUNCTION__, cmd, cur_state);
		t_update = 0;
	}
	cur_channel->t_next_idle = t_ret + t_update;

	return t_ret;
}


int WAIT_FLASH_IO(int core_id, int io_type, int n_io_pages)
{
	bool ret = false;

#ifdef IO_PERF_DEBUG
	FILE* fp_io = fopen("./META/io.dat", "a");
	int64_t start = get_usec();
#endif
	int remain_pages = 0;

	/* Wait all inflight Flash IOs */
	while(ret == false){

		FLASH_STATE_CHECKER(core_id);
		ret = CHECK_IO_COMPLETION(core_id, n_io_pages, &remain_pages);
	}

#ifdef IO_PERF_DEBUG
	int64_t end = get_usec();
	if(n_io_pages > 0){
		fprintf(fp_io, "%d\t%d\t%d\t%lu\n", core_id, io_type, n_io_pages, end-start);
	}
	fclose(fp_io);
#endif

	return remain_pages;
}

void UPDATE_IO_PROC_INFO(int core_id)
{
	if(core_id == -1){
		printf("[%s] Error: core_id %d\n", __FUNCTION__, core_id);
		return;
	}

	io_proc_i[core_id].n_completed_io++;
}

bool CHECK_IO_COMPLETION(int core_id, int n_io_pages, int* remain_pages)
{
	if(core_id == -1){
		printf("[%s] Error: core_id %d\n", __FUNCTION__, core_id);
		return false ;
	}
	int n_completed_io = io_proc_i[core_id].n_completed_io;

	if(n_completed_io >= n_io_pages){

		if(n_completed_io > n_io_pages)
			*remain_pages = n_completed_io - n_io_pages;
		else
			*remain_pages = 0;

#ifdef FLASH_DEBUG
		printf("[%s] %d-core io completed! %d >= %d, sub: %d\n",
			__FUNCTION__, core_id, n_completed_io, n_io_pages, 
			io_proc_i[core_id].n_submitted_io);
#endif

		io_proc_i[core_id].n_submitted_io -= n_completed_io;
		io_proc_i[core_id].n_completed_io = 0;
		
		return true;
	}

	return false;
}

int64_t SET_FIRM_OVERHEAD(int core_id, int io_type, int64_t overhead)
{
	int i;
	int step = 0;
	int64_t remain_overhead = 0;
	flash_memory* cur_flash = &flash[core_id];
	flash_memory* init_flash = cur_flash;
	plane* cur_plane;

	remain_overhead = overhead;
	
	if(overhead == 0){
		do{
			for(i=0; i<N_PLANES_PER_FLASH; i++){

				cur_plane = &cur_flash->plane[i];

				/* Reset the delay to the init value  */
				cur_plane->reg_cmd_set_delay = REG_CMD_SET_DELAY;
				cur_plane->reg_write_delay = REG_WRITE_DELAY;
				cur_plane->page_program_delay = PAGE_PROGRAM_DELAY;
				cur_plane->reg_read_delay = REG_READ_DELAY;
				cur_plane->page_read_delay = PAGE_READ_DELAY;
				cur_plane->block_erase_delay = BLOCK_ERASE_DELAY;
			}
			cur_flash = cur_flash->next;

		}while(cur_flash != init_flash);
	}
	else{

		while(remain_overhead > 0 && step < 3){

			if(step == 0){

				if(remain_overhead > REG_CMD_SET_DELAY * 2){
					UPDATE_FLASH_LATENCY(core_id, SET_CMD1, REG_CMD_SET_DELAY);
					remain_overhead -= REG_CMD_SET_DELAY * 2;
				}
				else{
					UPDATE_FLASH_LATENCY(core_id, SET_CMD1, remain_overhead/2);
					remain_overhead = 0;
				}
				step++;
			}
			else if(step == 1){

				if(remain_overhead > REG_WRITE_DELAY){
					UPDATE_FLASH_LATENCY(core_id, REG_WRITE, REG_WRITE_DELAY);
					remain_overhead -= REG_WRITE_DELAY;
				}
				else{
					UPDATE_FLASH_LATENCY(core_id, REG_WRITE, REG_WRITE_DELAY);
					remain_overhead = 0;
				}
				step++;
			}
			else if(step == 2){
	
				if(remain_overhead > PAGE_PROGRAM_DELAY){
					UPDATE_FLASH_LATENCY(core_id, PAGE_PROGRAM, PAGE_PROGRAM_DELAY);
					remain_overhead -= PAGE_PROGRAM_DELAY;
				}
				else{
					UPDATE_FLASH_LATENCY(core_id, PAGE_PROGRAM, remain_overhead);
					remain_overhead = 0;
				}
				step++;			
			}
		}
	}

	return remain_overhead;
}

void UPDATE_FLASH_LATENCY(int core_id, int reg_cmd_type, int64_t overhead)
{
	int i;
	flash_memory* cur_flash = &flash[core_id];
	flash_memory* init_flash = cur_flash;
	plane* cur_plane;

	do{
		for(i=0; i<N_PLANES_PER_FLASH; i++){

			cur_plane = &cur_flash->plane[i];

			/* Reduce Flash program delay */
			if(reg_cmd_type == SET_CMD1){

				cur_plane->reg_cmd_set_delay -= overhead;
			}
			else if(reg_cmd_type == REG_WRITE){
				cur_plane->reg_write_delay -= overhead;
			}
			else if(reg_cmd_type == PAGE_PROGRAM){
				cur_plane->page_program_delay -= overhead;
			}
		}
		cur_flash = cur_flash->next;

	}while(cur_flash != init_flash);
}

int64_t SET_FIRM_OVERHEAD2(int core_id, int io_type, int64_t overhead)
{
	int i;
	int64_t remain_overhead = 0;
	flash_memory* cur_flash = &flash[core_id];
	flash_memory* init_flash = cur_flash;
	plane* cur_plane;

	do{
		for(i=0; i<N_PLANES_PER_FLASH; i++){

			remain_overhead = overhead;
			cur_plane = &cur_flash->plane[i];

			if(overhead == 0){

				/* Reset the delay to the init value  */
				cur_plane->reg_cmd_set_delay = REG_CMD_SET_DELAY;
				cur_plane->reg_write_delay = REG_WRITE_DELAY;
				cur_plane->page_program_delay = PAGE_PROGRAM_DELAY;
				cur_plane->reg_read_delay = REG_READ_DELAY;
				cur_plane->page_read_delay = PAGE_READ_DELAY;
				cur_plane->block_erase_delay = BLOCK_ERASE_DELAY;
			}
			else{

				/* Reduce Flash I/O delay as amount as overhead */
				if(remain_overhead > REG_CMD_SET_DELAY){
					cur_plane->reg_cmd_set_delay = 0;
					remain_overhead -= REG_CMD_SET_DELAY;
				}
				else{
					cur_plane->reg_cmd_set_delay -= remain_overhead;
					remain_overhead = 0;
				}

				/* Reduce Flash read delay */
				if(io_type == READ){

					if(remain_overhead > PAGE_READ_DELAY){
						cur_plane->page_read_delay = 0;
						remain_overhead -= PAGE_READ_DELAY;
					}
					else{
						cur_plane->page_read_delay -= remain_overhead;
						remain_overhead = 0;
					}

					if(remain_overhead > REG_READ_DELAY){
						cur_plane->reg_read_delay = 0;
						remain_overhead -= REG_READ_DELAY;
					}
					else{
						cur_plane->reg_read_delay -= remain_overhead;
						remain_overhead = 0;
					}
				}

				/* Reduce Flash program delay */
				else if(io_type == WRITE){

					if(remain_overhead > REG_WRITE_DELAY){
						cur_plane->reg_write_delay = 0;
						remain_overhead -= REG_WRITE_DELAY;
					}
					else{
						cur_plane->reg_write_delay -= remain_overhead;
						remain_overhead = 0;
					}

					if(remain_overhead > PAGE_PROGRAM_DELAY){
						cur_plane->page_program_delay = 0;
						remain_overhead -= PAGE_PROGRAM_DELAY;
					}
					else{
						cur_plane->page_program_delay -= remain_overhead;
						remain_overhead = 0;
					}
				}

				/* Reduce Flash erase delay */
				else if(io_type == ERASE){
	
					if(remain_overhead > BLOCK_ERASE_DELAY){
						cur_plane->block_erase_delay = 0;
						remain_overhead -= BLOCK_ERASE_DELAY;
					}
					else{
						cur_plane->block_erase_delay -= remain_overhead;
						remain_overhead = 0;
					}				
				}
			}
		}
		cur_flash = cur_flash->next;

	}while(cur_flash != init_flash);

	return remain_overhead;
}
