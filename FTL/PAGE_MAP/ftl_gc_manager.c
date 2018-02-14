// File: ftl_gc_manager.c
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

unsigned int gc_count = 0;

extern double ssd_util;

int GET_GC_LOCK(plane_info* cur_plane)
{
	int ret = -1;

	flash_info* cur_flash = cur_plane->flash_i;
	int core_id = cur_flash->core_id;	

	if(gc_mode == CORE_GC){
		ret = pthread_mutex_trylock(&vs_core[core_id].gc_lock);
	}
	else if(gc_mode == FLASH_GC){
		ret = pthread_mutex_trylock(&cur_flash->gc_lock);
	}
	else if(gc_mode == PLANE_GC){
		ret = pthread_mutex_trylock(&cur_plane->gc_lock);
	}
	
	if(ret == 0)
		return SUCCESS;
	else
		return FAIL;
}


void RELEASE_GC_LOCK(plane_info* cur_plane)
{
	flash_info* cur_flash = cur_plane->flash_i;
	int core_id = cur_flash->core_id;	

	if(gc_mode == CORE_GC){
		pthread_mutex_unlock(&vs_core[core_id].gc_lock);
	}
	else if(gc_mode == FLASH_GC){
		pthread_mutex_unlock(&cur_flash->gc_lock);
	}
	else if(gc_mode == PLANE_GC){
		pthread_mutex_unlock(&cur_plane->gc_lock);
	}
}


void FGGC_CHECK(int core_id)
{
	int n_fggc_planes = GET_N_FGGC_PLANES(core_id);
	if(n_fggc_planes == 0)
		return;

	int ret;
	int plane_index;
	int plane_state;
	vssim_core* cur_core = &vs_core[core_id];
	plane_info* cur_plane = NULL;
		
	flash_info* init_flash_i = &flash_i[cur_core->flash_index];
	flash_info* cur_flash = &flash_i[cur_core->flash_index];

	/* Check all the flash list of the core */
	do{
		for(plane_index=0; plane_index<N_PLANES_PER_FLASH; plane_index++){

			/* Get the state of the plane */
			cur_plane = &cur_flash->plane_i[plane_index];
			plane_state = GET_PLANE_STATE(cur_plane);

			/* If the plane needs to perform foreground GC, */
			if(plane_state == NEED_FGGC){

				UPDATE_PLANE_STATE(core_id, cur_plane, ON_GC);

				/* Perform foreground GC */
				ret = PLANE_GARBAGE_COLLECTION(cur_plane);

				/* Update the state of the plane */
				if(ret == SUCCESS)
					UPDATE_PLANE_STATE(core_id, cur_plane, IDLE);
			}
		}

		/* Get next flash */
		cur_flash = cur_flash->next_flash;

		/* refresh the number of fggc planes */
		n_fggc_planes = GET_N_FGGC_PLANES(core_id);

		if(n_fggc_planes == 0)
			break;

	}while(cur_flash != init_flash_i);	
}

void CHECK_EMPTY_BLOCKS(int core_id, pbn_t pbn)
{
	int flash_nb = pbn.path.flash;
	int plane_nb = pbn.path.plane;
	int n_empty_blocks = 0;

	/* Get information of the flash and the plane */
	vssim_core* cur_core = &vs_core[core_id];

	flash_info* cur_flash = &flash_i[flash_nb];
	plane_info* cur_plane = &cur_flash->plane_i[plane_nb];

	/* Get the number of empty blocks */
	if(gc_mode == CORE_GC)
		n_empty_blocks = n_total_empty_blocks[core_id];
	else if(gc_mode == FLASH_GC)
		n_empty_blocks = cur_flash->n_empty_blocks;
	else if(gc_mode == PLANE_GC)
		n_empty_blocks = cur_plane->empty_list.n_blocks;
	else{
		printf("ERROR[%s] Wrong gc mode: %d\n",
				__FUNCTION__, gc_mode);
		return;
	}

	/* Check GC threshold of the Flash */
	if(n_empty_blocks <= cur_core->n_gc_high_watermark_blocks){
		UPDATE_PLANE_STATE(core_id, cur_plane, NEED_FGGC);
	}
	else if(n_empty_blocks <= cur_core->n_gc_low_watermark_blocks){
		UPDATE_PLANE_STATE(core_id, cur_plane, NEED_BGGC);
	}
	else{
		UPDATE_PLANE_STATE(core_id, cur_plane, IDLE);
	}
}

int PLANE_GARBAGE_COLLECTION(plane_info* cur_plane)
{
	int ret;
	block_entry* victim_block;

	/* Get mutex lock according to the GC mode */
	ret = GET_GC_LOCK(cur_plane);
	if(ret == FAIL)
		return FAIL;

	/* Get victim block */
	victim_block = SELECT_VICTIM_BLOCK_FROM_PLANE(cur_plane);
	if(victim_block == NULL)
		return FAIL;

	/* Do garbage Collection */
	ret = GARBAGE_COLLECTION(victim_block);

	/* Release the mutex lock according to the GC mode */
	RELEASE_GC_LOCK(cur_plane);

	if(ret == FAIL)
		return FAIL;
	else
		return SUCCESS;
}

int GARBAGE_COLLECTION(block_entry* victim_entry)
{
	int i, ret;
	int flash_nb = -1;
	int core_id = -1;
	int n_copies = 0;
	int n_valid_pages = 0;

	int64_t lpn;
	pbn_t victim_pbn = victim_entry->pbn;
	ppn_t old_ppn;
	ppn_t new_ppn;
	bitmap_t valid_array;
	block_state_entry* bs_entry;

	flash_nb = (int)victim_pbn.path.flash;
	core_id = flash_i[flash_nb].core_id;

	/* Get block state entry and the valid array of the victim block */
	bs_entry = GET_BLOCK_STATE_ENTRY(victim_pbn);
	valid_array = bs_entry->valid_array;
	n_valid_pages = bs_entry->n_valid_pages;

	/* Move valid pages from the victim block to new empty page */
	for(i=0; i<N_PAGES_PER_BLOCK; i++){
		if(TEST_BITMAP_MASK(valid_array, i)){

			if(gc_mode == FLASH_GC)
				ret = GET_NEW_PAGE(core_id, victim_pbn, MODE_INFLASH, &new_ppn);
			else if(gc_mode == PLANE_GC)
				ret = GET_NEW_PAGE(core_id, victim_pbn, MODE_INPLANE, &new_ppn);
			else if(gc_mode == CORE_GC)
				ret = GET_NEW_PAGE(core_id, victim_pbn, MODE_OVERALL, &new_ppn);

			if(ret == FAIL){
				printf("ERROR[%s] Get new page fail\n", __FUNCTION__);
				return FAIL;
			}

			/* Read a Valid Page from the Victim NAND Block */
			old_ppn = PBN_TO_PPN(victim_pbn, i);

			if(gc_mode == CORE_GC){
				/* read the valid page from the victim block */
				FLASH_PAGE_READ(old_ppn);
				WAIT_FLASH_IO(core_id, READ, 1);

				/* Write the valid page to new free page */
				FLASH_PAGE_WRITE(new_ppn);
				WAIT_FLASH_IO(core_id, WRITE, 1);
			}
			else{
				FLASH_PAGE_COPYBACK(new_ppn, old_ppn);
	
				/* Wait until the page copyback is completed */
				WAIT_FLASH_IO(core_id, WRITE, 1);
			}

			/* Get lpn of the old ppn */
			lpn = GET_INVERSE_MAPPING_INFO(old_ppn);

			/* Update mapping table */
			PARTIAL_UPDATE_PAGE_MAPPING(core_id, lpn, new_ppn, \
					old_ppn, 0, 0);

			n_copies++;
		}
	}

	if(n_copies != n_valid_pages){
		printf("ERROR[%s] The number of valid page is not correct\n", __FUNCTION__);
		return FAIL;
	}

	/* Erase the victim Flash block */
	FLASH_BLOCK_ERASE(victim_pbn);

	/* Wait until block erase completed */
	WAIT_FLASH_IO(core_id, BLOCK_ERASE, 1); 

	/* Move the victim block from victim list to empty list */
	POP_VICTIM_BLOCK(core_id, victim_entry);
	INSERT_EMPTY_BLOCK(core_id, victim_entry);

	/* Set the victim block as EMPTY_BLOCK */
	UPDATE_BLOCK_STATE(victim_pbn, EMPTY_BLOCK);

	gc_count++;

#ifdef MONITOR_ON
	UPDATE_LOG(LOG_GC_AMP, n_copies);
	UPDATE_LOG(LOG_ERASE_BLOCK, 1);
#endif

	return SUCCESS;
}

block_entry* SELECT_VICTIM_BLOCK_FROM_FLASH(flash_info* cur_flash)
{
	int i;
	uint32_t n_valid_pages;

	plane_info* cur_plane;
	block_state_entry* bs_entry;
	block_entry* cur_block_entry = NULL;
	block_entry* ret_block_entry = NULL;

	for(i=0; i<N_PLANES_PER_FLASH; i++){
	
		cur_plane = &cur_flash->plane_i[i];

		/* Get the victim block in the plane */
		cur_block_entry = SELECT_VICTIM_BLOCK_FROM_PLANE(cur_plane);
		if(cur_block_entry == NULL)
			continue;

		/* Get block state of the current block */
		bs_entry = GET_BLOCK_STATE_ENTRY(cur_block_entry->pbn);

		/* Compare the victim block */
		if(ret_block_entry != NULL){

			/* Select victim block: Greedy algorithm */
			if(n_valid_pages > bs_entry->n_valid_pages){
				ret_block_entry = cur_block_entry;
				n_valid_pages = bs_entry->n_valid_pages;
			}
		}
		else{
			/* First selected victim candidate block */
			ret_block_entry = cur_block_entry;
			n_valid_pages = bs_entry->n_valid_pages;
		}

		/* If current block is dead block, return immediately */
		if(n_valid_pages == 0)
			break;
	}

	if(ret_block_entry == NULL || n_valid_pages == N_PAGES_PER_BLOCK){
		ret_block_entry = NULL;
	}

	return ret_block_entry;
}

block_entry* SELECT_VICTIM_BLOCK_FROM_PLANE(plane_info* cur_plane)
{
	int i;
	uint32_t n_valid_pages;
	block_state_entry* bs_entry = NULL;

	block_list* victim_list = &cur_plane->victim_list;
	block_entry* ret_block_entry = NULL;
	block_entry* cur_block_entry = victim_list->head;

	uint32_t n_victim_blocks = cur_plane->victim_list.n_blocks;

	for(i=0; i<n_victim_blocks; i++){

		bs_entry = GET_BLOCK_STATE_ENTRY(cur_block_entry->pbn);

		if(ret_block_entry != NULL){

			/* Select victim block: Greedy algorithm */
			if(n_valid_pages > bs_entry->n_valid_pages){
				ret_block_entry = cur_block_entry;
				n_valid_pages = bs_entry->n_valid_pages;
			}
		}
		else{
			/* First selected victim candidate block */
			ret_block_entry = cur_block_entry;
			n_valid_pages = bs_entry->n_valid_pages;
		}

		/* If current block is dead block, return immediately */
		if(n_valid_pages == 0)
			break;

		cur_block_entry = cur_block_entry->next;
	}	

	if(ret_block_entry == NULL || n_valid_pages == N_PAGES_PER_BLOCK){
		ret_block_entry = NULL;	
	}

	return ret_block_entry;
}


block_entry* SELECT_VICTIM_BLOCK(void)
{
	static int flash_index = 0;
	static int plane_index = 0;
	int plane_state;

	int i;
	int n_bggc_planes = 0;

	/* Check the number of bggc candidate planes */
	for(i=0; i<N_IO_CORES; i++){
		n_bggc_planes += GET_N_BGGC_PLANES(i);
	}

	/* If there is no candidate planes, return */
	if(n_bggc_planes == 0)
		return NULL;

	block_entry* victim_block = NULL;

	/* Get flash and plane information */
	flash_info* cur_flash = &flash_i[flash_index];
	plane_info* cur_plane = &cur_flash->plane_i[plane_index];
	plane_info* init_plane_i = &cur_flash->plane_i[plane_index];

	do{
		/* Get the state of the plane */
		plane_state = GET_PLANE_STATE(cur_plane);

		/* If the plane is bggc candidate,
				select a victim block from the plane */
		if(plane_state == NEED_BGGC){
			victim_block = SELECT_VICTIM_BLOCK_FROM_PLANE(cur_plane);
		}

		/* Increase flash and plane index */
		plane_index++;
		if(plane_index == N_PLANES_PER_FLASH){
			plane_index = 0;
			flash_index++;
			if(flash_index == N_FLASH)
				flash_index = 0;
		}

		/* If the victim block is selected, return */
		if(victim_block != NULL){
			return victim_block;
		}

		/* Get next flash and plane information */
		cur_flash = &flash_i[flash_index];
		cur_plane = &cur_flash->plane_i[plane_index];

	}while(cur_plane != init_plane_i);

	return NULL;
}


void INCREASE_SLEEP_TIME(long* t_sleep_ms)
{
	*t_sleep_ms *= 2;
	if(*t_sleep_ms > GC_THREAD_MAX_SLEEP_TIME){
		*t_sleep_ms = GC_THREAD_MAX_SLEEP_TIME;
	}
}


void DECREASE_SLEEP_TIME(long* t_sleep_ms)
{
	*t_sleep_ms /= 2;
	if(*t_sleep_ms < GC_THREAD_MIN_SLEEP_TIME){
		*t_sleep_ms = GC_THREAD_MIN_SLEEP_TIME;
	}
}
