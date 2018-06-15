// File: ftl_gc_manager.c
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

unsigned int gc_count = 0;
int64_t t_total_gc = 0;
int bggc_core_id;

int64_t bggc_n_victim_blocks = 0;
int64_t bggc_n_copy_pages = 0;
int64_t bggc_n_free_pages = 0;	

int64_t fggc_n_victim_blocks = 0;
int64_t fggc_n_copy_pages = 0;
int64_t fggc_n_free_pages = 0;	

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

#ifdef FTL_DEBUG
	printf("[%s] %d core: start\n", __FUNCTION__, core_id);
#endif
#ifdef GC_DEBUG
	int64_t t_gc_start = get_usec();
#endif

	/* Check all the flash list of the core */
	do{

#ifdef FTL_DEBUG
		printf("[%s] %d core: check %d flash, %d plane need FG_GC\n", __FUNCTION__,
				core_id, cur_flash->flash_nb, n_fggc_planes);
#endif
		for(plane_index=0; plane_index<N_PLANES_PER_FLASH; plane_index++){

			/* Get the state of the plane */
			cur_plane = &cur_flash->plane_i[plane_index];
			plane_state = GET_PLANE_STATE(cur_plane);

			/* If the plane needs to perform foreground GC, */
			if(plane_state == NEED_FGGC && plane_state != ON_GC){

#ifdef FTL_DEBUG
				printf("[%s] %d core: f %d, p %d need GC\n",
					__FUNCTION__, core_id, cur_flash->flash_nb,
					plane_index);
#endif

				UPDATE_PLANE_STATE(core_id, cur_plane, ON_GC);

				/* Perform foreground GC */
				ret = PLANE_GARBAGE_COLLECTION(core_id, cur_plane);

				/* Update the state of the plane */
				if(ret == SUCCESS)
					UPDATE_PLANE_STATE(core_id, cur_plane, IDLE);
				else
					UPDATE_PLANE_STATE(core_id, cur_plane, NEED_FGGC);
			}
		}

		/* Get next flash */
		cur_flash = cur_flash->next_flash;

		/* refresh the number of fggc planes */
		n_fggc_planes = GET_N_FGGC_PLANES(core_id);

		if(n_fggc_planes == 0)
			break;

	}while(cur_flash != init_flash_i);	

#ifdef GC_DEBUG
	t_total_gc += get_usec() - t_gc_start;
#endif
#ifdef FTL_DEBUG
	printf("[%s] %d core: check flash complete\n", __FUNCTION__, core_id);
#endif
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

int BACKGROUND_GARBAGE_COLLECTION(block_entry* victim_block)
{
	int ret;
	int core_id;

	/* Check the validity of the address */
	pbn_t victim_pbn = victim_block->pbn;
	int32_t flash = (int32_t)victim_pbn.path.flash;
	int32_t plane = (int32_t)victim_pbn.path.plane;
	int32_t block = (int32_t)victim_pbn.path.block;

	flash_info* cur_flash = &flash_i[flash];
	plane_info* cur_plane = &cur_flash->plane_i[plane];
	core_id = cur_flash->core_id;

	if(flash >= N_FLASH || plane >= N_PLANES_PER_FLASH
			|| block >= N_BLOCKS_PER_PLANE){
		printf("ERROR[%s] Wrong victim block: f %d, p %d, b %d\n", 
				__FUNCTION__, flash, plane, block);
		return FAIL;
	}

	UPDATE_PLANE_STATE(core_id, cur_plane, ON_GC);

	/* Get mutex lock according to the GC mode */
	ret = GET_GC_LOCK(cur_plane);
	if(ret == FAIL){
		UPDATE_PLANE_STATE(core_id, cur_plane, NEED_BGGC);
		return FAIL;
	}
	
	/* Do garbage Collection */
	ret = GARBAGE_COLLECTION(bggc_core_id, victim_block);

	/* Release the mutex lock according to the GC mode */
	RELEASE_GC_LOCK(cur_plane);

	if(ret == FAIL){
		UPDATE_PLANE_STATE(core_id, cur_plane, NEED_BGGC);
		return FAIL;
	}
	else{
		CHECK_EMPTY_BLOCKS(core_id, victim_pbn);
		return SUCCESS;
	}
}

int PLANE_GARBAGE_COLLECTION(int core_id, plane_info* cur_plane)
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

	/* Check the validity of the address */
	pbn_t victim_pbn = victim_block->pbn;
	int32_t flash = (int32_t)victim_pbn.path.flash;
	int32_t plane = (int32_t)victim_pbn.path.plane;
	int32_t block = (int32_t)victim_pbn.path.block;

	if(flash >= N_FLASH || plane >= N_PLANES_PER_FLASH
			|| block >= N_BLOCKS_PER_PLANE){
		printf("ERROR[%s] Wrong victim block: f %d, p %d, b %d\n", 
				__FUNCTION__, flash, plane, block);
		return FAIL;
	}

	/* Do garbage Collection */
	ret = GARBAGE_COLLECTION(core_id, victim_block);

	/* Release the mutex lock according to the GC mode */
	RELEASE_GC_LOCK(cur_plane);

	if(ret == FAIL)
		return FAIL;
	else
		return SUCCESS;
}

int GARBAGE_COLLECTION(int core_id, block_entry* victim_entry)
{
	int i, ret;
	int n_copies = 0;
	int n_valid_pages = 0;
	int owner_core_id;

	int64_t lpn;
	pbn_t victim_pbn = victim_entry->pbn;
	ppn_t old_ppn;
	ppn_t new_ppn;
	bitmap_t valid_array;
	block_state_entry* bs_entry;

#ifdef GC_DEBUG
	printf("[%s] %d-core start gc\n", __FUNCTION__, core_id);
#endif

	/* Get the core id which manages the victim block */
	owner_core_id = flash_i[victim_pbn.path.flash].core_id;

	/* Get block state entry and the valid array of the victim block */
	bs_entry = GET_BLOCK_STATE_ENTRY(victim_pbn);

	/* Get lock of the block entry */
	pthread_mutex_lock(&bs_entry->lock);
	
	/* Update bs_entry core id */
	if(bs_entry->core_id != -1){
		printf("ERROR [%s] bs_entry has core id %d\n",
				__FUNCTION__, bs_entry->core_id);
	}
	bs_entry->core_id = core_id;

	valid_array = bs_entry->valid_array;
	n_valid_pages = bs_entry->n_valid_pages;

#ifdef GC_DEBUG
	printf("\t %d-core n_valid_pages: %d\n", core_id, n_valid_pages);
#endif

	/* Move valid pages from the victim block to new empty page */
	for(i=0; i<N_PAGES_PER_BLOCK; i++){
		if(TEST_BITMAP_MASK(valid_array, i)){

			if(gc_mode == FLASH_GC)
				ret = GET_NEW_PAGE(owner_core_id, victim_pbn, MODE_INFLASH, &new_ppn, 1);
			else if(gc_mode == PLANE_GC)
				ret = GET_NEW_PAGE(owner_core_id, victim_pbn, MODE_INPLANE, &new_ppn, 1);
			else if(gc_mode == CORE_GC)
				ret = GET_NEW_PAGE(owner_core_id, victim_pbn, MODE_OVERALL, &new_ppn, 1);

			if(ret == FAIL){
				printf("ERROR[%s] Get new page fail\n", __FUNCTION__);
				pthread_mutex_unlock(&bs_entry->lock);

				return FAIL;
			}

			/* Read a Valid Page from the Victim NAND Block */
			old_ppn = PBN_TO_PPN(victim_pbn, i);

			if(gc_mode == CORE_GC){
				/* read the valid page from the victim block */
				FLASH_PAGE_READ(core_id, old_ppn);
				WAIT_FLASH_IO(core_id, READ, 1);

				/* Write the valid page to new free page */
				FLASH_PAGE_WRITE(core_id, new_ppn);
				WAIT_FLASH_IO(core_id, WRITE, 1);
			}
			else{
				FLASH_PAGE_COPYBACK(core_id, new_ppn, old_ppn);
	
				/* Wait until the page copyback is completed */
				WAIT_FLASH_IO(core_id, WRITE, 1);
			}

			/* Get lpn of the old ppn */
			lpn = GET_INVERSE_MAPPING_INFO(old_ppn);

			/* Update mapping table */
			PARTIAL_UPDATE_PAGE_MAPPING(core_id, owner_core_id, lpn, new_ppn, \
					old_ppn, 0, 0);

#ifdef GC_DEBUG
			printf("[%s] %d-core move %d pages from f %d p %d b %d (plane state: %d)\n",
					__FUNCTION__, core_id, n_copies,
					old_ppn.path.flash, old_ppn.path.plane,
					old_ppn.path.block,
					flash_i[old_ppn.path.flash].plane_i[old_ppn.path.plane].p_state				);
#endif

			n_copies++;
		}
	}

	if(n_copies != n_valid_pages){
		printf("ERROR[%s] %d-core: The number of valid page is not correct: %d != %d\n",
			 __FUNCTION__, core_id, n_copies, n_valid_pages);

		return FAIL;
	}

	/* Erase the victim Flash block */
	FLASH_BLOCK_ERASE(core_id, victim_pbn);

	/* Wait until block erase completed */
	WAIT_FLASH_IO(core_id, BLOCK_ERASE, 1); 

	/* Move the victim block from victim list to empty list */
	POP_VICTIM_BLOCK(owner_core_id, victim_entry);

	/* Set the victim block as EMPTY_BLOCK */
	UPDATE_BLOCK_STATE(core_id, victim_pbn, EMPTY_BLOCK);
	INSERT_EMPTY_BLOCK(owner_core_id, victim_entry);

	/* Initialize bs_entry core id */
	bs_entry->core_id = -1;

	/* Release lock of the block entry */
	pthread_mutex_unlock(&bs_entry->lock);

	gc_count++;

#ifdef GET_GC_INFO
	static int64_t bggc_call_count = 0;
	static int64_t fggc_call_count = 0;

	if(core_id == bggc_core_id){
		bggc_call_count++;
		fprintf(fp_gc_info,"BGGC\t%ld\t%d\t%d\n", bggc_call_count, n_copies, 
				N_PAGES_PER_BLOCK - n_copies);
	}
	else{
		fggc_call_count++;
		fprintf(fp_gc_info,"FGGC\t%ld\t%d\t%d\n", fggc_call_count, n_copies, 
				N_PAGES_PER_BLOCK - n_copies);

	}
#endif

#ifdef GC_DEBUG
	printf("[%s] %d-core end gc\n", __FUNCTION__, core_id);
#endif


#ifdef MONITOR_ON
	UPDATE_LOG(LOG_GC_AMP, n_copies);
	UPDATE_LOG(LOG_ERASE_BLOCK, 1);
#endif

	return SUCCESS;
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


block_entry* SELECT_VICTIM_BLOCK_FOR_BGGC(void)
{
	static int flash_index = 0;
	static int plane_index = 0;
	int plane_state;

	int i;
	int n_bggc_planes = 0;
	int n_fggc_planes = 0;

	/* Check the number of bggc candidate planes */
	for(i=0; i<N_IO_CORES; i++){
		n_bggc_planes += GET_N_BGGC_PLANES(i);
		n_fggc_planes += GET_N_FGGC_PLANES(i);
	}

	/* If there is no candidate planes, return */
	if(n_bggc_planes == 0 && n_fggc_planes == 0)
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
		if(plane_state == NEED_BGGC || plane_state == NEED_FGGC){
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
