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
			flash[i].plane[j].data_reg.ppn.addr = -1;
			flash[i].plane[j].data_reg.t_end = -1;

			/* Init ppn list per flash */
			flash[i].plane[j].cmd_list = (uint8_t*)calloc(sizeof(uint8_t), N_PPNS_PER_PLANE);
			flash[i].plane[j].ppn_list = (ppn_t*)calloc(sizeof(ppn_t), N_PPNS_PER_PLANE);
			flash[i].plane[j].copyback_list = (ppn_t*)calloc(sizeof(ppn_t), N_PPNS_PER_PLANE);

			if(flash[i].plane[j].cmd_list == NULL 
					|| flash[i].plane[j].ppn_list == NULL
					|| flash[i].plane[j].copyback_list == NULL){
				printf("ERROR[%s] per plane command list alloc fail!\n", __FUNCTION__);
				return FAIL;
			}
			else{
				for(k=0; k<N_PPNS_PER_PLANE; k++){

					flash[i].plane[j].cmd_list[k] = CMD_NOOP;
					flash[i].plane[j].ppn_list[k].addr = -1;
					flash[i].plane[j].copyback_list[k].addr = -1;
				}
				flash[i].plane[j].n_entries = 0;
				flash[i].plane[j].index = 0;
			}
	
		}
	}

	/* Init Flash memory list*/
	for(i=0; i<N_IO_CORES; i++){
		INIT_FLASH_MEMORY_LIST(i);
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

int FLASH_STATE_CHECKER(int core_id)
{
	int i;
	int channel_nb;
	int64_t t_now;
	int n_completed_pages = 0;

	flash_memory* cur_flash = &flash[core_id];
	flash_memory* init_flash = cur_flash;
	plane* cur_plane;

	t_now = get_usec();

	do{
		for(i=0; i<N_PLANES_PER_FLASH; i++){

			channel_nb = cur_flash->channel_nb;
			cur_plane = &cur_flash->plane[i];

			/* Update data register state of this plane */
			n_completed_pages += UPDATE_DATA_REGISTER(cur_plane, channel_nb, t_now);

			if(PAGE_CACHE_REG_ENABLE){
				/* Update page cache register state of this plane */
				UPDATE_PAGE_CACHE_REGISTER(cur_plane, channel_nb, t_now);
			}

//TEMP
//			if(cur_plane->data_reg.state != REG_NOOP || cur_plane->page_cache.state != REG_NOOP){
//				printf("[%s] %d f: %d plane (cmd %u) data state: %d (%ld), cache state: %d (%ld), index: %d / %d\n", 
//						__FUNCTION__, cur_flash->flash_nb, i, cur_plane->cmd,
//						cur_plane->data_reg.state, 
//						cur_plane->data_reg.t_end - t_now,
//						cur_plane->page_cache.state,
//						cur_plane->page_cache.t_end - t_now,
//						cur_plane->index,
//						cur_plane->n_entries);
//			}
		}
		cur_flash = cur_flash->next;

	}while(cur_flash != init_flash);

	return n_completed_pages;
}


int UPDATE_DATA_REGISTER(plane* cur_plane, int channel_nb, int64_t t_now)
{
	reg* data_reg = &cur_plane->data_reg;
	reg* page_cache = &cur_plane->page_cache;

	uint32_t index = cur_plane->index;
	uint32_t n_entries = cur_plane->n_entries;

	uint8_t next_cmd;
	uint8_t cmd = cur_plane->cmd;
	ppn_t next_ppn;
	ppn_t copyback_ppn;

	int completed_pages = 0;

	switch(data_reg->state){

		case REG_NOOP:
			if(!PAGE_CACHE_REG_ENABLE
					&& index != n_entries){

				next_cmd = cur_plane->cmd_list[index];
				next_ppn = cur_plane->ppn_list[index];
				copyback_ppn = cur_plane->copyback_list[index];
				cur_plane->index++;

				/* Init per-plane ppn list */
				if(cur_plane->index == n_entries){
					cur_plane->index = 0;
					cur_plane->n_entries = 0;
				}

				/* Get next available channel access time */
				data_reg->t_end = GET_AND_UPDATE_NEXT_AVAILABLE_CH_TIME(channel_nb,
							t_now, next_cmd, SET_CMD1);

				if(data_reg->t_end <= t_now){

					/* Update register  */
					data_reg->state = SET_CMD1;
					data_reg->ppn = next_ppn;
					data_reg->copyback_ppn = copyback_ppn;
					data_reg->t_end = t_now + REG_CMD_SET_DELAY;

					/* Update plane */
					cur_plane->cmd = next_cmd;
				}
				else{
					/* Update register  */
					data_reg->state = WAIT_CHANNEL_FOR_CMD1;
					data_reg->ppn = next_ppn;
					data_reg->copyback_ppn = copyback_ppn;

					/* Update plane */
					cur_plane->cmd = next_cmd;
				}
			}

			break;

		case SET_CMD1:
			if(data_reg->t_end <= t_now){
				if(cmd == CMD_PAGE_READ){

					/* Update register  */
					data_reg->state = PAGE_READ;
					data_reg->t_end = t_now + PAGE_READ_DELAY;
				}
				else if(cmd == CMD_PAGE_PROGRAM){

					/* Update register  */
					data_reg->state = REG_WRITE;
					data_reg->t_end = t_now + REG_WRITE_DELAY;
				}
				else if(cmd == CMD_BLOCK_ERASE){

					/* Update register  */
					data_reg->state = BLOCK_ERASE;
					data_reg->t_end = t_now + BLOCK_ERASE_DELAY;
				}
				else if(cmd == CMD_PAGE_COPYBACK){

					/* Update register  */
					data_reg->state = PAGE_READ;
					data_reg->t_end = t_now + PAGE_READ_DELAY;
				}
				else if(cmd == CMD_PAGE_COPYBACK_PHASE2){

					/* Update register  */
					data_reg->state = PAGE_PROGRAM;
					data_reg->t_end = t_now + PAGE_PROGRAM_DELAY;
				}
			}
			break;

		case SET_CMD2:
			if(data_reg->t_end <= t_now){
				if(cmd == CMD_PAGE_READ){

					/* Update register  */
					data_reg->state = REG_READ;
					data_reg->t_end = t_now + REG_READ_DELAY;
				}
				else if(cmd == CMD_PAGE_PROGRAM){

					/* Update register  */
					data_reg->state = PAGE_PROGRAM;
					data_reg->t_end = t_now + PAGE_PROGRAM_DELAY;
				}
				else if(cmd == CMD_PAGE_COPYBACK){

					ppn_t dst_ppn = data_reg->copyback_ppn;
					int dst_flash_nb = dst_ppn.path.flash;
					int dst_plane_nb = dst_ppn.path.plane;

					/* Update register  */
					plane* dst_plane = &flash[dst_flash_nb].plane[dst_plane_nb];

					if(dst_plane == cur_plane){
						data_reg->ppn = dst_ppn;
						data_reg->state = PAGE_PROGRAM;
						data_reg->t_end = t_now + PAGE_PROGRAM_DELAY;
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
							dst_reg->t_end = t_now + PAGE_PROGRAM_DELAY;

							dst_plane->cmd = CMD_PAGE_COPYBACK;

							/* Init current reg */
							data_reg->ppn.addr = -1;
							data_reg->t_end = -1;
							data_reg->state = REG_NOOP;

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
				data_reg->t_end = t_now + REG_CMD_SET_DELAY;
			}

			break;

		case REG_READ:
			/* Complete Flash page read */
			if(data_reg->t_end <= t_now){

				/* Initialize current register */
				data_reg->state = REG_NOOP;
				data_reg->ppn.addr = -1;
				data_reg->t_end = -1;

				/* Initialized current plane */	
				if(PAGE_CACHE_REG_ENABLE){
					if(page_cache->state == REG_NOOP)
						cur_plane->cmd = CMD_NOOP;
				}
				else{
					cur_plane->cmd = CMD_NOOP;
				}

				completed_pages++;
			}
			break;

		case PAGE_PROGRAM:
			/* Complete Flash page program */
			if (data_reg->t_end <= t_now){

				/* Initialize current register */
				data_reg->state = REG_NOOP;
				data_reg->ppn.addr = -1;
				data_reg->t_end = -1;

				if(cmd == CMD_PAGE_COPYBACK_PHASE2){
					ppn_t src_ppn = data_reg->copyback_ppn;
					int src_flash_nb = src_ppn.path.flash;
					int src_plane_nb = src_ppn.path.plane;
					plane* src_plane = &flash[src_flash_nb].plane[src_plane_nb];
					reg* src_reg = &src_plane->data_reg;

					src_reg->ppn.addr = -1;
					src_reg->state = REG_NOOP;
					src_reg->t_end = -1;

					src_plane->cmd = CMD_NOOP;
				}

				/* Initialized current plane */	
				if(PAGE_CACHE_REG_ENABLE){
					if(page_cache->state == REG_NOOP)
						cur_plane->cmd = CMD_NOOP;
				}
				else{
					cur_plane->cmd = CMD_NOOP;
				}

				completed_pages++;
			}
			break;

		case PAGE_READ:
			/* Complete Flash page read */
			if(data_reg->t_end <= t_now){

				if(cmd == CMD_PAGE_READ){
					data_reg->t_end = GET_AND_UPDATE_NEXT_AVAILABLE_CH_TIME(channel_nb, t_now, cmd, PAGE_READ);

					if(data_reg->t_end <= t_now){
					
						/* Update state */
						data_reg->state = SET_CMD2;
						data_reg->t_end = t_now + REG_CMD_SET_DELAY;
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

					/* Update register  */
					plane* dst_plane = &flash[dst_flash_nb].plane[dst_plane_nb];
	
					if(dst_plane == cur_plane){
						data_reg->ppn = dst_ppn;
						data_reg->state = PAGE_PROGRAM;
						data_reg->t_end = t_now + PAGE_PROGRAM_DELAY;		
					}
					else{
						FLASH_PAGE_COPYBACK_PHASE2(dst_ppn, data_reg->ppn);
					}
				}
			}

			break;

		case BLOCK_ERASE:

			/* Complete Flash block erase */
			if(data_reg->t_end <= t_now){

				/* Initialize current register */
				data_reg->state = REG_NOOP;
				data_reg->ppn.addr = -1;
				data_reg->t_end = -1;

				/* Initialized current plane */	
				cur_plane->cmd = CMD_NOOP;
			}
			break;

		case WAIT_CHANNEL_FOR_CMD1:
			if(data_reg->t_end <= t_now){
				
				/* Update state */
				data_reg->state = SET_CMD1;
				data_reg->t_end = t_now + REG_CMD_SET_DELAY;
			}
			break;

		case WAIT_CHANNEL_FOR_CMD2:

			if(data_reg->t_end <= t_now){
				if(cmd == CMD_PAGE_READ){

					/* Update state */
					data_reg->state = SET_CMD2;
					data_reg->t_end = t_now + REG_CMD_SET_DELAY;
				}
			}
			break;

		default:
			break;
	}

	return completed_pages;
}


void UPDATE_PAGE_CACHE_REGISTER(plane* cur_plane, int channel_nb, int64_t t_now)
{
	reg* data_reg = &cur_plane->data_reg;
	reg* page_cache = &cur_plane->page_cache;

	uint32_t index = cur_plane->index;
	uint32_t n_entries = cur_plane->n_entries;

	uint8_t next_cmd;
	uint8_t cmd = cur_plane->cmd;
	ppn_t next_ppn;
	ppn_t copyback_ppn;

	switch(page_cache->state){

		case REG_NOOP:
			if(index != n_entries){

				next_cmd = cur_plane->cmd_list[index];
				next_ppn = cur_plane->ppn_list[index];
				copyback_ppn = cur_plane->copyback_list[index];

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
							t_now, next_cmd, SET_CMD1);

				if(page_cache->t_end <= t_now){

					/* Update register  */
					page_cache->state = SET_CMD1;
					page_cache->ppn = next_ppn;
					page_cache->copyback_ppn = copyback_ppn;
					page_cache->t_end = t_now + REG_CMD_SET_DELAY;

					/* Update plane */
					cur_plane->cmd = next_cmd;
				}
				else{
					/* Update register  */
					page_cache->state = WAIT_CHANNEL_FOR_CMD1;
					page_cache->ppn = next_ppn;
					page_cache->copyback_ppn = copyback_ppn;

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
					page_cache->t_end = t_now + PAGE_READ_DELAY;
				}
				else if(cmd == CMD_PAGE_PROGRAM){

					/* Update register  */
					page_cache->state = REG_WRITE;
					page_cache->t_end = t_now + REG_WRITE_DELAY;
				}
				else if(cmd == CMD_BLOCK_ERASE){

					/* Update register  */
					page_cache->state = BLOCK_ERASE;
					page_cache->t_end = t_now + BLOCK_ERASE_DELAY;
				}
				else if(cmd == CMD_PAGE_COPYBACK){

					/* Update register  */
					page_cache->state = PAGE_READ;
					page_cache->t_end = t_now + PAGE_READ_DELAY;
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
						data_reg->t_end = t_now + PAGE_PROGRAM_DELAY;
						data_reg->state = PAGE_PROGRAM;

						/* Initialize page cache */
						page_cache->state = REG_NOOP;
						page_cache->ppn.addr = -1;
						page_cache->t_end = -1;
					}
				}
				else if(cmd == CMD_PAGE_COPYBACK){
					ppn_t dst_ppn = page_cache->copyback_ppn;
					int dst_flash_nb = dst_ppn.path.flash;
					int dst_plane_nb = dst_ppn.path.plane;

					/* Update register  */
					plane* dst_plane = &flash[dst_flash_nb].plane[dst_plane_nb];

					if(dst_plane == cur_plane){
						/* Update destination reg */
						page_cache->ppn = dst_ppn;
						page_cache->state = PAGE_PROGRAM;
						page_cache->t_end = t_now + PAGE_PROGRAM_DELAY;
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
							dst_reg->t_end = t_now + PAGE_PROGRAM_DELAY;
							dst_plane->cmd = CMD_PAGE_COPYBACK;

							/* Init current reg */
							page_cache->ppn.addr = -1;
							page_cache->t_end = -1;
							page_cache->state = REG_NOOP;
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
					page_cache->t_end = t_now + REG_CMD_SET_DELAY;
				}
			}

			break;

		case PAGE_PROGRAM:
			if(page_cache->t_end <= t_now){
				if(cmd == CMD_PAGE_COPYBACK){
					/* Update destination reg */
					page_cache->ppn.addr = -1;
					page_cache->state = REG_NOOP;
					page_cache->t_end = -1;

					cur_plane->cmd = CMD_NOOP;
				}
			 	else if(cmd == CMD_PAGE_COPYBACK_PHASE2){
					ppn_t src_ppn = page_cache->copyback_ppn;
					int src_flash_nb = src_ppn.path.flash;
					int src_plane_nb = src_ppn.path.plane;
					plane* src_plane = &flash[src_flash_nb].plane[src_plane_nb];
					reg* src_reg = &src_plane->page_cache;
	
					src_reg->ppn.addr = -1;
					src_reg->state = REG_NOOP;
					src_reg->t_end = -1;

					src_plane->cmd = CMD_NOOP;
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
						cur_plane->cmd = CMD_PAGE_READ;

						/* Initialize page cache */
						page_cache->state = REG_NOOP;
						page_cache->ppn.addr = -1;
						page_cache->t_end = -1;	
					}
				}
				else if(cmd == CMD_PAGE_COPYBACK){

					ppn_t dst_ppn = page_cache->copyback_ppn;
					int dst_flash_nb = dst_ppn.path.flash;
					int dst_plane_nb = dst_ppn.path.plane;

					/* Update register  */
					plane* dst_plane = &flash[dst_flash_nb].plane[dst_plane_nb];
	
					if(dst_plane == cur_plane){
						page_cache->ppn = dst_ppn;
						page_cache->state = PAGE_PROGRAM;
						page_cache->t_end = t_now + PAGE_PROGRAM_DELAY;		
					}
					else{
						FLASH_PAGE_COPYBACK_PHASE2(dst_ppn, data_reg->ppn);
					}
				}
			}
			break;

		case BLOCK_ERASE:
			/* Complete Flash block erase */
			if(page_cache->t_end <= t_now){

				/* Initialize current register */
				page_cache->state = REG_NOOP;
				page_cache->ppn.addr = -1;
				page_cache->t_end = -1;

				/* Initialized current plane */	
				cur_plane->cmd = CMD_NOOP;
			}
			break;


		case WAIT_CHANNEL_FOR_CMD1:
			if(page_cache->t_end <= t_now){
				
				/* Update state */
				page_cache->state = SET_CMD1;
				page_cache->t_end = t_now + REG_CMD_SET_DELAY;
			}
			break;

		case WAIT_REG:
			if(page_cache->t_end <= t_now){
				if(cmd == CMD_PAGE_PROGRAM){
					COPY_PAGE_CACHE_TO_DATA_REG(data_reg, page_cache);

					/* Set data register */
					data_reg->t_end = t_now + PAGE_PROGRAM_DELAY;
					data_reg->state = PAGE_PROGRAM;
					cur_plane->cmd = CMD_PAGE_PROGRAM;

					/* Initialize page cache */
					page_cache->state = REG_NOOP;
					page_cache->ppn.addr = -1;
					page_cache->t_end = -1;
				}
				else if(cmd == CMD_PAGE_READ){
					COPY_PAGE_CACHE_TO_DATA_REG(data_reg, page_cache);

					/* Set data register */
					data_reg->state = PAGE_READ;
					data_reg->t_end = t_now;
					cur_plane->cmd = CMD_PAGE_READ;

					/* Initialize page cache */
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


int FLASH_PAGE_WRITE(ppn_t ppn)
{
	uint32_t index;
	int flash_nb = (int)ppn.path.flash;
	int plane_nb = (int)ppn.path.plane;

	plane* cur_plane = &flash[flash_nb].plane[plane_nb];

	/* Get ppn list index */
	index = cur_plane->n_entries;
	if(index >= N_PPNS_PER_PLANE){
		printf("ERROR[%s] Exceed ppn list index: %u\n",
				__FUNCTION__, index);
		return FAIL;
	}

	/* Insert ppn and cmd to the flash ppn list */
	cur_plane->ppn_list[index] = ppn;
	cur_plane->cmd_list[index] = CMD_PAGE_PROGRAM;

	cur_plane->n_entries++;

	return SUCCESS;
}

int FLASH_PAGE_READ(ppn_t ppn)
{
	uint32_t index;
	int flash_nb = (int)ppn.path.flash;
	int plane_nb = (int)ppn.path.plane;

	plane* cur_plane = &flash[flash_nb].plane[plane_nb];

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

	cur_plane->n_entries++;

	return SUCCESS;
}

int FLASH_PAGE_COPYBACK(ppn_t dst_ppn, ppn_t src_ppn)
{
	uint32_t index;
	int flash_nb = (int)src_ppn.path.flash;
	int plane_nb = (int)src_ppn.path.plane;

	plane* cur_plane = &flash[flash_nb].plane[plane_nb];

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

	cur_plane->n_entries++;
	
	return SUCCESS;
}

int FLASH_PAGE_COPYBACK_PHASE2(ppn_t dst_ppn, ppn_t src_ppn)
{
	uint32_t index;
	int flash_nb = (int)dst_ppn.path.flash;
	int plane_nb = (int)dst_ppn.path.plane;

	plane* cur_plane = &flash[flash_nb].plane[plane_nb];

	/* Get ppn list index */
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

	cur_plane->n_entries++;
	
	return SUCCESS;
}

int FLASH_BLOCK_ERASE(pbn_t pbn)
{
	uint32_t index;
	int flash_nb = (int)pbn.path.flash;
	int plane_nb = (int)pbn.path.plane;
	ppn_t ppn = PBN_TO_PPN(pbn, 0);

	plane* cur_plane = &flash[flash_nb].plane[plane_nb];

	/* Get ppn list index */
	index = cur_plane->n_entries;
	if(index >= N_PPNS_PER_PLANE){
		printf("ERROR[%s] Exceed ppn list index: %u\n",
				__FUNCTION__, index);
		return FAIL;
	}

	/* Insert ppn and cmd to the flash ppn list */
	cur_plane->ppn_list[index] = ppn;
	cur_plane->cmd_list[index] = CMD_BLOCK_ERASE;

	cur_plane->n_entries++;

	return SUCCESS;
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
		t_update = REG_CMD_SET_DELAY * 2 + REG_WRITE_DELAY;
	}
	else if(cur_state == SET_CMD1 && cmd == CMD_PAGE_READ){
		t_update = REG_CMD_SET_DELAY;
	}
	else if(cur_state == SET_CMD1 && cmd == CMD_PAGE_COPYBACK){
		t_update = REG_CMD_SET_DELAY;
	}
	else if(cur_state == SET_CMD1 && cmd == CMD_BLOCK_ERASE){
		t_update = REG_CMD_SET_DELAY;
	}
	else if(cur_state == PAGE_READ && cmd == CMD_PAGE_COPYBACK){
		t_update = REG_CMD_SET_DELAY;
	}
	else if(cur_state == PAGE_READ){
		t_update = REG_CMD_SET_DELAY + REG_READ_DELAY;
	}
	else{
		printf("ERROR[%s] Wrong cmd(%d) and state (%d)!\n",
				__FUNCTION__, cmd, cur_state);
		t_update = 0;
	}
	cur_channel->t_next_idle = t_ret + t_update;

	return t_ret;
}


void WAIT_FLASH_IO(int core_id, int io_type, int n_io_pages)
{
	int n_completed_pages = 0;

//TEMP
	FILE* fp_io = fopen("./META/io.dat", "a");
	int64_t start = get_usec();

	/* Wait all inflight Flash IOs */
	while(n_completed_pages != n_io_pages){

		n_completed_pages += FLASH_STATE_CHECKER(core_id);

		usleep(2);
	}

//TEMP
	int64_t end = get_usec();
	if(n_io_pages > 0){
		fprintf(fp_io, "%d\t%d\t%lu\n", io_type, n_io_pages, end-start);
	}
	fclose(fp_io);

	return;
}
