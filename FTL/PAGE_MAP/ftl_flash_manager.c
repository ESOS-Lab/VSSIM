// File: ftl_flash_manager.c
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

flash_info* flash_i;

int64_t* n_total_empty_blocks;
int64_t* n_total_victim_blocks;

int INIT_FLASH_INFO(int init_info)
{
	int i;
	int ret;

	/* Allocate memory for flash information */
	flash_i = (flash_info*)calloc(sizeof(flash_info), N_FLASH);
	if(flash_i == NULL){
		printf("ERROR[%s] Allocate memory for flash memory info fail!\n",
				__FUNCTION__);
		return -1;	
	}

	/* Initialize flash information */
	for(i=0; i<N_FLASH; i++){
		flash_i[i].core_id = -1;
		flash_i[i].flash_nb = i;
		flash_i[i].plane_i = NULL;
		flash_i[i].next_flash = NULL;
		flash_i[i].plane_index = 0;
		flash_i[i].n_empty_blocks = 0;
		pthread_mutex_init(&flash_i[i].gc_lock, NULL);
	}

	/* Initialize plane information */
	ret = INIT_PLANE_INFO(init_info);

	return ret;
}

int INIT_PLANE_INFO(int init_info)
{
	int i, j;
	int ret;
	plane_info* cur_plane = NULL;

	for(i=0; i<N_FLASH; i++){
		/* Allocate memory for plane information */
		cur_plane = (plane_info*)calloc(sizeof(plane_info),
							N_PLANES_PER_FLASH);
		if(cur_plane == NULL){
			printf("ERROR[%s] Allocate memory for plane info fail\n",
					__FUNCTION__);
			return -1;
		}

		/* Set variables to NULL */
		for(j=0; j<N_PLANES_PER_FLASH; j++){
			cur_plane[j].inverse_mapping_table 	= NULL;
			cur_plane[j].block_state_table		= NULL;
			cur_plane[j].p_state			= IDLE;
			pthread_mutex_init(&cur_plane[j].state_lock, NULL);
			pthread_mutex_init(&cur_plane[j].gc_lock, NULL);

			/* Init empty, victim block list */
			cur_plane[j].empty_list.head		= NULL;
			cur_plane[j].empty_list.tail		= NULL;
			cur_plane[j].empty_list.n_blocks 	= 0;
			cur_plane[j].victim_list.head		= NULL;
			cur_plane[j].victim_list.tail		= NULL;
			cur_plane[j].victim_list.n_blocks 	= 0;

			/* Init parent pointer */
			cur_plane[j].flash_i = &flash_i[i];
		}

		flash_i[i].plane_i = cur_plane;
	}

	/* Initialize the variables in the current plane structure*/
	ret = INIT_INVERSE_MAPPING_TABLE(init_info);
	if(ret == -1) goto fail;

	ret = INIT_BLOCK_STATE_TABLE(init_info);
	if(ret == -1) goto fail;

	ret = INIT_EMPTY_BLOCK_LIST(init_info);
	if(ret == -1) goto fail;

	ret = INIT_VICTIM_BLOCK_LIST(init_info);
	if(ret == -1) goto fail;

	return ret;
fail:
	printf("[%s] init fail\n", __FUNCTION__);
	return -1;
}

int INIT_INVERSE_MAPPING_TABLE(int init_info)
{
	int i, j, k;
	int ret;

	for(i=0; i<N_FLASH; i++){
		for(j=0; j<N_PLANES_PER_FLASH; j++){
			/* Allocation Memory for Inverse Page Mapping Table */
			flash_i[i].plane_i[j].inverse_mapping_table = 
					(void*)calloc(N_PAGES_PER_PLANE, sizeof(int64_t));

			if(flash_i[i].plane_i[j].inverse_mapping_table == NULL){
				printf("ERROR[%s] Allocate mapping table fail!\n", __FUNCTION__);
				return -1;
			}
		}
	}


	/* Initialize Inverse Mapping Table */
	if(init_info == 1){
		FILE* fp = fopen("./META/inverse_mapping.dat","r");
		if(fp != NULL){
			/* Restore inverse mapping table*/
			for(i=0; i<N_FLASH; i++){
				for(j=0; j<N_PLANES_PER_FLASH; j++){
					ret = fread(flash_i[i].plane_i[j].inverse_mapping_table, 
							N_PAGES_PER_PLANE, sizeof(int64_t), fp);
					if(ret == -1){
						printf("ERROR[%s] Read inverse mapping table fail!\n", 
							__FUNCTION__);
						return -1;
					}
				}
			}
			fclose(fp);
			return 1;
		}
		else{
			printf("ERROR[%s] Fail to read file!\n", __FUNCTION__);
			return -1;
		}
	}
	else{
		/* First boot */
		for(i=0; i<N_FLASH; i++){
			for(j=0; j<N_PLANES_PER_FLASH; j++){
				for(k=0; k<N_PAGES_PER_PLANE; k++){
					flash_i[i].plane_i[j].inverse_mapping_table[k] = -1;
				}
			}
		}
		return 0;
	}
}

int INIT_BLOCK_STATE_TABLE(int init_info)
{
	int i, j, k;
	int ret;

	/* Allocation Memory for Inverse Block Mapping Table */
	for(i=0; i<N_FLASH; i++){
		for(j=0; j<N_PLANES_PER_FLASH; j++){
			flash_i[i].plane_i[j].block_state_table = 
					(void*)calloc(N_BLOCKS_PER_PLANE, sizeof(block_state_entry));

			if(flash_i[i].plane_i[j].block_state_table == NULL){
				printf("ERROR[%s] Allocate mapping table fail!\n", __FUNCTION__);
				return -1;
			}
		}
	}

	/* Initialize block status table */
	if(init_info == 1){
		FILE* fp = fopen("./META/block_state_table.dat","r");
		if(fp != NULL){

			/* Restore block status table from the file */
			for(i=0; i<N_FLASH; i++){
				for(j=0; j<N_PLANES_PER_FLASH; j++){
					ret = fread(flash_i[i].plane_i[j].block_state_table, 
						sizeof(block_state_entry), N_BLOCKS_PER_PLANE, fp);

					if(ret == -1){
						printf("ERROR[%s] Read block state table fail!\n", 
							__FUNCTION__);
						return -1;
					}
					
					block_state_entry* cur_bs_entry = 
						(block_state_entry*)flash_i[i].plane_i[j].block_state_table;
					for(k=0; k<N_BLOCKS_PER_PLANE; k++){
						pthread_mutex_init(&cur_bs_entry->lock, NULL);
						cur_bs_entry += 1;
					}
				}
			}

			fclose(fp);
		}
		else{
			printf("ERROR[%s] Fail to read file!\n", __FUNCTION__);
			return -1;
		}
	}
	else{
		/* First boot */
		for(i=0; i<N_FLASH; i++){
			for(j=0; j<N_PLANES_PER_FLASH; j++){
				block_state_entry* cur_bs_entry = 
						(block_state_entry*)flash_i[i].plane_i[j].block_state_table;

				for(k=0; k<N_BLOCKS_PER_PLANE; k++){
					cur_bs_entry->n_valid_pages	= 0;
					cur_bs_entry->type		= EMPTY_BLOCK;
					cur_bs_entry->core_id		= -1;
					cur_bs_entry->erase_count	= 0;
					cur_bs_entry->valid_array	= NULL;
					pthread_mutex_init(&cur_bs_entry->lock, NULL);
					cur_bs_entry += 1;
				}
			}
		}
	}

	/* Init the valid array of each block status entry */
	ret = INIT_VALID_ARRAY(init_info);
	if(ret == -1) return -1;

	CREATE_BITMAP_MASK();

	return ret;
}

int INIT_VALID_ARRAY(int init_info)
{
	int i, j, k;
	int ret;
	uint32_t nbytes = (BITMAP_SIZE / N_BITS) + 1;
	bitmap_t valid_array = NULL;
	FILE* fp = NULL;
	block_state_entry* cur_bs_entry = NULL;

	for(i=0; i<N_FLASH; i++){
		for(j=0; j<N_PLANES_PER_FLASH; j++){
			cur_bs_entry = (block_state_entry*)flash_i[i].plane_i[j].block_state_table;

			for(k=0; k<N_BLOCKS_PER_PLANE; k++){
				valid_array = CREATE_BITMAP(BITMAP_SIZE);
				if(valid_array == NULL){
					printf("ERROR[%s] Allocate memory for valid array fail!\n", 
							__FUNCTION__);
					return -1;
				}

				cur_bs_entry->valid_array = valid_array;
				cur_bs_entry += 1;
			}
		}
	}

	if(init_info == 1){
		fp = fopen("./META/valid_array.dat","r");

		if(fp != NULL){
			for(i=0; i<N_FLASH; i++){
				for(j=0; j<N_PLANES_PER_FLASH; j++){
					cur_bs_entry = (block_state_entry*)flash_i[i].plane_i[j].block_state_table;

					for(k=0; k<N_BLOCKS_PER_PLANE; k++){
						ret = fread(cur_bs_entry->valid_array, 
							sizeof(uint32_t), nbytes, fp);
						if(ret == -1)
							printf("ERROR[%s] Read valid array fail!\n", 
									__FUNCTION__);

						cur_bs_entry += 1;
					}
				}
			}
			fclose(fp);
			return 1;
		}
		else{
			printf("ERROR[%s] Fail to read file!\n", __FUNCTION__);
			return -1;
		}
	}

	return 0;
}

int INIT_EMPTY_BLOCK_LIST(int init_info)
{
	int i, j, k;
	unsigned int remains = 0;
	int ret;

	block_list* cur_empty_list = NULL;
	block_entry* cur_empty_entry = NULL;

	/* Allocate and Intialize the total number of empty blocks of each core */
	n_total_empty_blocks = (int64_t*)calloc(N_IO_CORES, sizeof(int64_t));
	for(i=0; i<N_IO_CORES; i++){
		n_total_empty_blocks[i] = 0;
	}

	if(init_info == 1){
		FILE* fp = fopen("./META/empty_block_list.dat","r");
		if(fp != NULL){
			for(i=0; i<N_FLASH; i++){
				for(j=0; j<N_PLANES_PER_FLASH; j++){

					/* Restore empty list of the plane from the file */
					cur_empty_list = &flash_i[i].plane_i[j].empty_list;
					ret = fread(cur_empty_list, sizeof(block_list), 1, fp);
					if(ret == -1){
						printf("ERROR[%s] Read empty block list fail!\n", __FUNCTION__);
						return -1;
					}

					/* Update the total number of empty blocks of the flash */
					flash_i[i].n_empty_blocks += cur_empty_list->n_blocks;
				}
			}

			for(i=0; i<N_FLASH; i++){
				for(j=0; j<N_PLANES_PER_FLASH; j++){

					/* Get current empty block list */
					cur_empty_list = &flash_i[i].plane_i[j].empty_list;

					/* Restore empty block list from file */
					remains = cur_empty_list->n_blocks;

					while(remains > 0){
						cur_empty_entry = (block_entry*)calloc(1, 
									sizeof(block_entry));
						if(cur_empty_entry == NULL){
							printf("ERROR[%s] Calloc fail\n", __FUNCTION__);
							return -1;
					}

						ret = fread(cur_empty_entry, sizeof(block_entry), 1, fp);
						if(ret == -1){
							printf("ERROR[%s] Read empty block entry fail!\n", __FUNCTION__);
							return -1;
						}

						cur_empty_entry->prev = NULL;
						cur_empty_entry->next = NULL;

						if(remains == cur_empty_list->n_blocks){
							cur_empty_list->head = cur_empty_entry;
							cur_empty_list->tail = cur_empty_entry;
						}					
						else{
							cur_empty_list->tail->next = cur_empty_entry;
							cur_empty_entry->prev = cur_empty_list->tail;
							cur_empty_list->tail = cur_empty_entry;
						}
						remains--;
					}
				}
			}

			fclose(fp);
			return 1;
		}
		else{
			printf("ERROR[%s] Fail to read file!\n", __FUNCTION__);
			return -1;
		}

	}
	else{
		/* First boot */
		for(i=0; i<N_FLASH; i++){
			for(j=0; j<N_PLANES_PER_FLASH; j++){

				/* Get current empty block list */
				cur_empty_list = &flash_i[i].plane_i[j].empty_list;

				for(k=0; k<N_BLOCKS_PER_PLANE; k++){

					/* Make new empty block entry */
					cur_empty_entry = (block_entry*)calloc(1,
								sizeof(block_entry));	
					if(cur_empty_entry == NULL){
						printf("ERROR[%s] Calloc fail\n", __FUNCTION__);
						return -1;
					}

					cur_empty_entry->pbn.path.flash = (int8_t)i;
					cur_empty_entry->pbn.path.plane = (int8_t)j;
					cur_empty_entry->pbn.path.block = (int16_t)k;
					cur_empty_entry->w_index = 0;
					cur_empty_entry->prev = NULL;					
					cur_empty_entry->next = NULL;					
	
					if(k == 0){
						cur_empty_list->head = cur_empty_entry;
						cur_empty_list->tail = cur_empty_entry;
					}
					else{
						cur_empty_list->tail->next = cur_empty_entry;
						cur_empty_entry->prev = cur_empty_list->tail;
						cur_empty_list->tail = cur_empty_entry;
					}
				}

				/* Initialize the number of empty blocks in the plane */
				cur_empty_list->n_blocks = (uint32_t)N_BLOCKS_PER_PLANE;
			}
			/* Initialize the number of empty blocks in the flash */
			flash_i[i].n_empty_blocks = (uint32_t)N_BLOCKS_PER_FLASH;
		}
		return 0;
	}
}

int INIT_VICTIM_BLOCK_LIST(int init_info)
{
	int i, j;
	unsigned int remains = 0;
	int ret;

	block_list* cur_victim_list = NULL;
	block_entry* cur_victim_entry = NULL;

	n_total_victim_blocks = (int64_t*)calloc(N_IO_CORES, sizeof(int64_t));
	for(i=0; i<N_IO_CORES; i++){
		n_total_victim_blocks[i] = 0;
	}

	if(init_info == 1){
		FILE* fp = fopen("./META/victim_block_list.dat","r");

		if(fp != NULL){
			/* Restore victim block list from file */
			for(i=0; i<N_FLASH; i++){
				for(j=0; j<N_PLANES_PER_FLASH; j++){

					cur_victim_list = &flash_i[i].plane_i[j].victim_list;
					ret = fread(cur_victim_list, sizeof(block_list), 1, fp);
					if(ret == -1){
						printf("ERROR[%s] Read victim block list fail!\n", __FUNCTION__);
						return -1;
					}
				}
			}

			/* Restore victim block entires from file */
			for(i=0; i<N_FLASH; i++){
				for(j=0; j<N_PLANES_PER_FLASH; j++){

					/* Get current victim block list */
					cur_victim_list = &flash_i[i].plane_i[j].victim_list;

					/* Restore empty block list from file */
					remains = cur_victim_list->n_blocks;

					while(remains > 0){
						cur_victim_entry = (block_entry*)calloc(1, 
								sizeof(block_entry));
						if(cur_victim_entry == NULL){
							printf("ERROR[%s] Calloc fail\n", __FUNCTION__);
							return -1;
						}

						ret = fread(cur_victim_entry, sizeof(block_entry), 1, fp);
						if(ret == -1){
							printf("ERROR[%s] Read victim block entry fail!\n", __FUNCTION__);
							return -1;
						}

						cur_victim_entry->prev = NULL;
						cur_victim_entry->next = NULL;

						if(remains == cur_victim_list->n_blocks){
							cur_victim_list->head = cur_victim_entry;
							cur_victim_list->tail = cur_victim_entry;
						}					
						else{
							cur_victim_list->tail->next = cur_victim_entry;
							cur_victim_entry->prev = cur_victim_list->tail;
							cur_victim_list->tail = cur_victim_entry;
						}
						remains--;
					}
				}
			}
			fclose(fp);
			return 1;
		}
		else{
			printf("ERROR[%s] Fail to read file!\n", __FUNCTION__);
			return -1;
		}
	}
	else{
		/* First boot */
		for(i=0; i<N_FLASH; i++){
			for(j=0; j<N_PLANES_PER_FLASH; j++){

				/* Get current victim block list */
				cur_victim_list = &flash_i[i].plane_i[j].victim_list;

				/* Initialize the victim block list */
				cur_victim_list->head = NULL;
				cur_victim_list->tail = NULL;
				cur_victim_list->n_blocks = 0;
			}
		}
		return 0;
	}
}

void TERM_FLASH_INFO(void)
{
	/* Free plane info */
	TERM_PLANE_INFO();

	/* Free flash info */
	free(flash_i);
}

void TERM_PLANE_INFO(void)
{
	int i;

	/* Terminate variables in plane info structure */
	TERM_VICTIM_BLOCK_LIST();
	TERM_EMPTY_BLOCK_LIST();
	TERM_BLOCK_STATE_TABLE();
	TERM_INVERSE_MAPPING_TABLE();

	/* Free plane info */
	for(i=0; i<N_FLASH; i++){
		free(flash_i[i].plane_i);	
	}
}

void TERM_INVERSE_MAPPING_TABLE(void)
{
	int i, j;

	FILE* fp = fopen("./META/inverse_mapping.dat", "w");
	if(fp==NULL){
		printf("ERROR[%s] File open fail\n", __FUNCTION__);
		return;
	}

	/* Write inverse mapping table to file */
	for(i=0; i<N_FLASH; i++){
		for(j=0; j<N_PLANES_PER_FLASH; j++){

			/* Write inverse mapping table of current plane to file */
			fwrite(flash_i[i].plane_i[j].inverse_mapping_table, 
					sizeof(int64_t), N_PAGES_PER_PLANE, fp);

			/* Free the inverse mapping table */
			free(flash_i[i].plane_i[j].inverse_mapping_table);
		}
	}
	fclose(fp);
}

void TERM_BLOCK_STATE_TABLE(void)
{
	int i, j;
	FILE* fp;

	TERM_VALID_ARRAY();

	fp = fopen("./META/block_state_table.dat","w");
	if(fp==NULL){
		printf("ERROR[%s] File open fail\n", __FUNCTION__);
		return;
	}

	/* Write block status table to the file */
	for(i=0; i<N_FLASH; i++){
		for(j=0; j<N_PLANES_PER_FLASH; j++){

			fwrite(flash_i[i].plane_i[j].block_state_table, 
					sizeof(block_state_entry), N_BLOCKS_PER_PLANE, fp);
		}
	}
	fclose(fp);

	/* Free current block status table */
	for(i=0; i<N_FLASH; i++){
		for(j=0; j<N_PLANES_PER_FLASH; j++){

			free(flash_i[i].plane_i[j].block_state_table);
		}
	}
}

void TERM_VALID_ARRAY(void)
{
	int i, j, k;
	uint32_t nbytes = (BITMAP_SIZE / N_BITS) + 1;
	block_state_entry* cur_bs_entry;

	FILE* fp = fopen("./META/valid_array.dat","w");

	for(i=0; i<N_FLASH; i++){
		for(j=0; j<N_PLANES_PER_FLASH; j++){

			/* Get block state entry of current plane */
			cur_bs_entry = (block_state_entry*)flash_i[i].plane_i[j].block_state_table;

			for(k=0; k<N_BLOCKS_PER_PLANE; k++){

				/* Write current valild array to file */
				fwrite(cur_bs_entry->valid_array, 
						sizeof(uint32_t), nbytes, fp);

				/* Free current valid array */
				DESTROY_BITMAP(cur_bs_entry->valid_array);

				cur_bs_entry += 1;
			}
		}
	}
	fclose(fp);
}

void TERM_EMPTY_BLOCK_LIST(void)
{
	int i, j;
	uint32_t remains = 0;

	block_list* cur_empty_list;
	block_entry* cur_empty_entry;
	block_entry* temp_empty_entry;

	FILE* fp = fopen("./META/empty_block_list.dat","w");
	if(fp==NULL){
		printf("ERROR[%s] File open fail\n", __FUNCTION__);
	}

	/* Write empty block list to file */
	for(i=0; i<N_FLASH; i++){
		for(j=0; j<N_PLANES_PER_FLASH; j++){

			cur_empty_list = &flash_i[i].plane_i[j].empty_list;
			fwrite(cur_empty_list, sizeof(block_list), 1, fp);
		}
	}

	/* Write empty block entries to file */
	for(i=0; i<N_FLASH; i++){
		for(j=0; j<N_PLANES_PER_FLASH; j++){

			/* Get current empty block list */
			cur_empty_list = &flash_i[i].plane_i[j].empty_list;

			/* Restore empty block list from file */
			remains = cur_empty_list->n_blocks;

			cur_empty_entry = cur_empty_list->head;
			while(remains > 0){
				if(cur_empty_entry == NULL){
					printf("ERROR[%s] Write NULL empty entry!\n",
							__FUNCTION__);
					return;
				}

				/* Write current empty entry to file */
				fwrite(cur_empty_entry, sizeof(block_entry), 1, fp);

				/* Get next empty entry */
				temp_empty_entry = cur_empty_entry;	
				cur_empty_entry = cur_empty_entry->next;

				/* Free the empty entry */
				free(temp_empty_entry);

				remains--;
			}
		}
	}
	fclose(fp);
}

void TERM_VICTIM_BLOCK_LIST(void)
{
	int i, j;
	uint32_t remains = 0;

	block_list* cur_victim_list;
	block_entry* cur_victim_entry;
	block_entry* temp_victim_entry;

	FILE* fp = fopen("./META/victim_block_list.dat","w");
	if(fp==NULL){
		printf("ERROR[%s] File open fail\n", __FUNCTION__);
	}

	/* Write victim block list to file */
	for(i=0; i<N_FLASH; i++){
		for(j=0; j<N_PLANES_PER_FLASH; j++){

			cur_victim_list = &flash_i[i].plane_i[j].victim_list;
			fwrite(cur_victim_list, sizeof(block_list), 1, fp);
		}
	}

	/* Write victim block entries to file */
	for(i=0; i<N_FLASH; i++){
		for(j=0; j<N_PLANES_PER_FLASH; j++){

			/* Get current victim block list */
			cur_victim_list = &flash_i[i].plane_i[j].victim_list;

			/* Restore empty block list from file */
			remains = cur_victim_list->n_blocks;

			cur_victim_entry = cur_victim_list->head;
			while(remains > 0){
				if(cur_victim_entry == NULL){
					printf("ERROR[%s] Write NULL victim entry!\n",
							__FUNCTION__);
					return;
				}

				/* Write current victim entry to file */
				fwrite(cur_victim_entry, sizeof(block_entry), 1, fp);

				/* Get next victim entry */	
				temp_victim_entry = cur_victim_entry;
				cur_victim_entry = cur_victim_entry->next;

				/* Free the victim entry */
				free(temp_victim_entry);

				remains--;
			}
		}
	}
	fclose(fp);
}


block_entry* GET_EMPTY_BLOCK(int core_id, pbn_t index, int mode)
{
	int ret;

	if(n_total_empty_blocks[core_id] == 0){
		printf("ERROR[%s] There is no empty block\n", __FUNCTION__);
		return NULL;
	}

	flash_info* cur_flash;
	plane_info* cur_plane;

	block_entry* cur_empty_block;
	block_list* cur_empty_list;

	while(n_total_empty_blocks[core_id] != 0){

		if(mode == MODE_OVERALL){

			/* Get empty block list of the plane */
			cur_flash = vs_core[core_id].flash_i;
			cur_plane = &cur_flash->plane_i[cur_flash->plane_index];
			cur_empty_list = &cur_plane->empty_list;

			/* Check wheter the flash or plane is available */
			if(gc_mode == FLASH_GC){
				ret = IS_AVAILABLE_FLASH(cur_flash);
				if(ret == FAIL){
					/* Check the other flash */
					vs_core[core_id].flash_i = cur_flash->next_flash;
					continue;			
				}				
			}
			else if(gc_mode == PLANE_GC){
				ret = IS_AVAILABLE_PLANE(cur_plane);
				if(ret == FAIL){
					/* Check the other plane */
					cur_flash->plane_index += 1;
					if(cur_flash->plane_index == N_PLANES_PER_FLASH){
						cur_flash->plane_index = 0;
					}

					continue;
				}
			}

			if(cur_empty_list->n_blocks != 0){

				cur_empty_block = cur_empty_list->head;
			
				/* Update the empty list index of the core */	
				cur_flash->plane_index += 1;
				if(cur_flash->plane_index == N_PLANES_PER_FLASH){
					cur_flash->plane_index = 0;
				}
				vs_core[core_id].flash_i = cur_flash->next_flash;
			
				/* Return the block pointer */
				return cur_empty_block;
			}
			else if(cur_flash->n_empty_blocks != 0){

				/* Check the other planes in the same flash */
				cur_flash->plane_index += 1;
				if(cur_flash->plane_index == N_PLANES_PER_FLASH){
					cur_flash->plane_index = 0;
				}

				continue;
			}
			else{
				/* Check the other flash */
				vs_core[core_id].flash_i = cur_flash->next_flash;

				continue;
			}
		}
		else if(mode == MODE_INFLASH){
			
			/* Get empty block list of the plane */
			cur_flash = &flash_i[index.path.flash];
			cur_plane = &cur_flash->plane_i[cur_flash->plane_index];
			cur_empty_list = &cur_plane->empty_list;

			if(cur_flash->n_empty_blocks == 0){
				/* If there is no empty block in the flash memory, return fail */
				printf("ERROR[%s]-INFLASH There is no empty block\n",__FUNCTION__);
				return NULL;
			}
			else{
				if(cur_empty_list->n_blocks != 0){
					cur_empty_block = cur_empty_list->head;

					/* Update the plane index of the flash */
					cur_flash->plane_index += 1;
					if(cur_flash->plane_index == N_PLANES_PER_FLASH){
						cur_flash->plane_index = 0;
					}
				}
				else{
					/* Check next plane in the flash */
					cur_flash->plane_index += 1;
					if(cur_flash->plane_index == N_PLANES_PER_FLASH){
						cur_flash->plane_index = 0;
					}

					continue;
				}

				return cur_empty_block;
			}	
		}
		else if(mode == MODE_INPLANE){
	
			/* Get empty block list of the plane */
			cur_plane = &flash_i[index.path.flash].plane_i[index.path.plane];
			cur_empty_list = &cur_plane->empty_list;

			if(cur_empty_list->n_blocks == 0){
				/* If there is no empty block in the plane, return fail */
				printf("ERROR[%s]-INPLANE There is no empty block\n",__FUNCTION__);
				return NULL;
			}
			else{
				return cur_empty_list->head;
			}	
		}
	}

	printf("ERROR[%s] There is no empty block\n", __FUNCTION__);
	return NULL;
}

int INSERT_EMPTY_BLOCK(int core_id, block_entry* new_empty_block)
{
	int8_t flash_nb;
	int8_t plane_nb;

	flash_info* cur_flash;
	plane_info* cur_plane;
	block_list* empty_list;

	/* Get address */
	flash_nb = new_empty_block->pbn.path.flash;
	plane_nb = new_empty_block->pbn.path.plane;

	/* Get flash & plane structure */
	cur_flash = &flash_i[flash_nb];
	cur_plane = &cur_flash->plane_i[plane_nb];
	empty_list = &cur_plane->empty_list;

	/* Update block entry */
	new_empty_block->w_index = 0;
	new_empty_block->prev = NULL;
	new_empty_block->next = NULL;

	/* Update empty block list */
	if(empty_list->n_blocks == 0){
		empty_list->head = new_empty_block;
		empty_list->tail = new_empty_block;
	}
	else{
		empty_list->tail->next = new_empty_block;
		new_empty_block->prev = empty_list->tail;
		empty_list->tail = new_empty_block;
	}

	/* Update the total number of victim blocks */
	empty_list->n_blocks++;
	cur_flash->n_empty_blocks++;
	n_total_empty_blocks[core_id]++;

	return SUCCESS;
}

int INSERT_VICTIM_BLOCK(int core_id, block_entry* new_victim_block)
{
	int8_t flash_nb;
	int8_t plane_nb;

	flash_info* cur_flash;
	plane_info* cur_plane;
	block_list* victim_list;

	/* Get address */
	flash_nb = new_victim_block->pbn.path.flash;
	plane_nb = new_victim_block->pbn.path.plane;

	/* Get flash & plane structure */
	cur_flash = &flash_i[flash_nb];
	cur_plane = &cur_flash->plane_i[plane_nb];
	victim_list = &cur_plane->victim_list;

	new_victim_block->prev = NULL;
	new_victim_block->next = NULL;

	/* Update victim block list */
	if(victim_list->n_blocks == 0){
		victim_list->head = new_victim_block;
		victim_list->tail = new_victim_block;
	}
	else{
		victim_list->tail->next = new_victim_block;
		new_victim_block->prev = victim_list->tail;
		victim_list->tail = new_victim_block;
	}

	/* Update the total number of victim blocks */
	victim_list->n_blocks++;
	n_total_victim_blocks[core_id]++;

	return SUCCESS;
}

int POP_EMPTY_BLOCK(int core_id, block_entry* empty_block)
{
	int8_t flash_nb;
	int8_t plane_nb;

	flash_info* cur_flash;
	plane_info* cur_plane;
	block_list* empty_list;

	/* Get address */
	flash_nb = empty_block->pbn.path.flash;
	plane_nb = empty_block->pbn.path.plane;
	
	/* Get flash & plane structure */
	cur_flash = &flash_i[flash_nb];
	cur_plane = &cur_flash->plane_i[plane_nb];
	empty_list = &cur_plane->empty_list;

	/* Pop the empty block from the empty list */
	if(empty_list->n_blocks == 1){
		empty_list->head = NULL;
		empty_list->tail = NULL;
	}
	else if(empty_block == empty_list->head){
		empty_list->head = empty_block->next;
		empty_list->head->prev = NULL;
	}
	else if(empty_block == empty_list->tail){
		empty_list->tail = empty_block->prev;
		empty_list->tail->next = NULL;
	}
	else{
		empty_block->prev->next = empty_block->next;
		empty_block->next->prev = empty_block->prev;
	}

	/* Reset the next and prev pointer */
	empty_block->prev = NULL;
	empty_block->next = NULL;

	/* Update the total number of empty blocks */
	empty_list->n_blocks--;
	cur_flash->n_empty_blocks--;
	n_total_empty_blocks[core_id]--;

	return SUCCESS;
}

int POP_VICTIM_BLOCK(int core_id, block_entry* victim_block)
{
	int8_t flash_nb;
	int8_t plane_nb;

	flash_info* cur_flash;
	plane_info* cur_plane;
	block_list* victim_list;

	/* Get address */
	flash_nb = victim_block->pbn.path.flash;
	plane_nb = victim_block->pbn.path.plane;
	
	/* Get flash & plane structure */
	cur_flash = &flash_i[flash_nb];
	cur_plane = &cur_flash->plane_i[plane_nb];
	victim_list = &cur_plane->victim_list;

	/* Pop the victim block from the victim list */
	if(victim_list->n_blocks == 1){
		victim_list->head = NULL;
		victim_list->tail = NULL;
	}
	else if(victim_block == victim_list->head){
		victim_list->head = victim_block->next;
		victim_list->head->prev = NULL;
	}
	else if(victim_block == victim_list->tail){
		victim_list->tail = victim_block->prev;
		victim_list->tail->next = NULL;
	}
	else{
		victim_block->prev->next = victim_block->next;
		victim_block->next->prev = victim_block->prev;
	}

	/* Reset the next and prev pointer */
	victim_block->prev = NULL;
	victim_block->next = NULL;

	/* Update the total number of victim blocks */
	victim_list->n_blocks--;
	n_total_victim_blocks[core_id]--;

	return SUCCESS;
}

block_state_entry* GET_BLOCK_STATE_ENTRY(pbn_t pbn){

	int32_t flash_nb = (int32_t)pbn.path.flash;
	int32_t plane_nb = (int32_t)pbn.path.plane;
	int32_t block_nb = (int32_t)pbn.path.block;

	block_state_entry* bs_entry;

	bs_entry = (block_state_entry*)flash_i[flash_nb].plane_i[plane_nb].block_state_table;
	bs_entry = bs_entry + block_nb;

	return bs_entry;
}

int64_t GET_INVERSE_MAPPING_INFO(ppn_t ppn)
{
	int64_t flash_nb = (int64_t)ppn.path.flash;
	int64_t plane_nb = (int64_t)ppn.path.plane;
	int64_t index = (int64_t)(ppn.path.block * N_PAGES_PER_BLOCK + ppn.path.page);

	return flash_i[flash_nb].plane_i[plane_nb].inverse_mapping_table[index];
}

int UPDATE_INVERSE_MAPPING(ppn_t ppn,  int64_t lpn)
{
	int64_t flash_nb = (int64_t)ppn.path.flash;
	int64_t plane_nb = (int64_t)ppn.path.plane;
	int64_t index = (int64_t)(ppn.path.block * N_PAGES_PER_BLOCK + ppn.path.page);

	if(index < 0 || index > N_PAGES_PER_PLANE){
		printf("ERROR[%s] wrong index: block %d, page %d\n",
				__FUNCTION__, ppn.path.block, ppn.path.page);
	}

	flash_i[flash_nb].plane_i[plane_nb].inverse_mapping_table[index] = lpn;

	return SUCCESS;
}

int UPDATE_BLOCK_STATE(int core_id, pbn_t pbn, int type)
{
        int i;
        block_state_entry* bs_entry = GET_BLOCK_STATE_ENTRY(pbn);

	bs_entry->type = type;
	
        if(type == EMPTY_BLOCK){

		/* Fill zeros to the valid array */
                for(i=0; i<N_PAGES_PER_BLOCK; i++){
                        UPDATE_BLOCK_STATE_ENTRY(core_id, pbn, i, INVALID);
                }

		/* Initialize the number of valid pages */
		bs_entry->n_valid_pages = 0;
        }

        return SUCCESS;
}

int UPDATE_BLOCK_STATE_ENTRY(int core_id, pbn_t pbn, int32_t offset, int valid)
{
	int32_t flash = (int32_t)pbn.path.flash;
	int32_t plane = (int32_t)pbn.path.plane;
	int32_t block = (int32_t)pbn.path.block;

	/* Check the validity of the address */
	if(flash >= N_FLASH || plane >= N_PLANES_PER_FLASH
			|| block >= N_BLOCKS_PER_PLANE || offset >= N_PAGES_PER_BLOCK){
		printf("ERROR[%s] Wrong physical address (valid: %d): f %d, p %d, b %d, offset %d\n", 
				__FUNCTION__, valid, flash, plane, block, offset);
		return FAIL;
	}

	block_state_entry* bs_entry = GET_BLOCK_STATE_ENTRY(pbn);

	if(bs_entry->core_id != core_id){
		pthread_mutex_lock(&bs_entry->lock);
	}

	bitmap_t valid_array = bs_entry->valid_array;

	if(valid == VALID){
		SET_BITMAP_MASK(valid_array, offset);
		bs_entry->n_valid_pages++;
	}
	else if(valid == INVALID){
		CLEAR_BITMAP_MASK(valid_array, offset);
		bs_entry->n_valid_pages--;
	}
	else{
		printf("ERROR[%s] Wrong valid value\n", __FUNCTION__);
	}

	if(bs_entry->core_id != core_id){
		pthread_mutex_unlock(&bs_entry->lock);
	}

	return SUCCESS;
}

int COUNT_BLOCK_STATE_ENTRY(pbn_t pbn)
{
	int i;
	int count = 0;
	
	block_state_entry* bs_entry = GET_BLOCK_STATE_ENTRY(pbn);
	bitmap_t valid_array = bs_entry->valid_array;

	for(i=0; i<N_PAGES_PER_BLOCK; i++){
		if(TEST_BITMAP_MASK(valid_array, i)){
			count++;
		}
	}

	return count;
}

int IS_AVAILABLE_FLASH(flash_info* flash_i)
{
	int i, ret;
	plane_info* cur_plane = flash_i->plane_i;

	/* If one of the planes is processing GC (or needed), 
			the flash is not available */
	for(i=0; i<N_PLANES_PER_FLASH; i++){
		ret = IS_AVAILABLE_PLANE(&cur_plane[i]);
		if(ret == FAIL)
			return FAIL;
	}

	return SUCCESS;
}

int IS_AVAILABLE_PLANE(plane_info* cur_plane)
{
	int state = GET_PLANE_STATE(cur_plane);

	/* If the plane is processing GC (or needed), 
			the plane is not available */
	if(state == ON_GC)
		return FAIL;

	return SUCCESS;
}

int GET_PLANE_STATE(plane_info* cur_plane)
{
	int state;

	pthread_mutex_lock(&cur_plane->state_lock);
	state = cur_plane->p_state;
	pthread_mutex_unlock(&cur_plane->state_lock);

	return state;
}

void UPDATE_PLANE_STATE(int core_id, plane_info* cur_plane, int state)
{
	int old_state = GET_PLANE_STATE(cur_plane);
	if(old_state == state)
		return;

	pthread_mutex_lock(&cur_plane->state_lock);
	cur_plane->p_state = state;
	pthread_mutex_unlock(&cur_plane->state_lock);

	/* Update GC related info of the core */
	if(old_state == NEED_BGGC)
		DECREASE_N_BGGC_PLANES(core_id);
	else if(old_state == NEED_FGGC)
		DECREASE_N_FGGC_PLANES(core_id);

	if(state == NEED_BGGC)
		INCREASE_N_BGGC_PLANES(core_id);
	else if(state == NEED_FGGC){
		INCREASE_N_FGGC_PLANES(core_id);
	}
}
