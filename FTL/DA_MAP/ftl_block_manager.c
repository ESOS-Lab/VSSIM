// File: ftl_block_manager.c
// Date: 2014. 12. 05.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"

void* block_state_table;
void* empty_block_list;
int64_t total_empty_block_nb;
unsigned int empty_block_list_index;

void INIT_BLOCK_STATE_TABLE(int meta_read)
{
	/* Allocation Memory for block state table */
	block_state_table = (void*)calloc(BLOCK_MAPPING_ENTRY_NB, sizeof(block_state_entry));
	if(block_state_table == NULL){
		printf("ERROR[%s] Calloc mapping table fail\n",__FUNCTION__);
		return;
	}

	if(meta_read == SUCCESS){
		READ_METADATA_FROM_NAND(block_state_table, sizeof(block_state_entry)*BLOCK_MAPPING_ENTRY_NB);
	}
	else{

		int i;
		block_state_entry* curr_b_s_entry = (block_state_entry*)block_state_table;

		/* Intialize the block state table */
		for(i=0;i<BLOCK_MAPPING_ENTRY_NB;i++){
			curr_b_s_entry->lbn		= -1;
			curr_b_s_entry->type		= EMPTY_BLOCK;
			curr_b_s_entry->valid_page_nb	= 0;

			curr_b_s_entry->start_offset	= -1;
			curr_b_s_entry->write_limit	= -1;

			curr_b_s_entry->rp_block_pbn	= -1;
			curr_b_s_entry->rp_root_pbn	= -1;

			curr_b_s_entry += 1;
		}
	}

	/* Initialize page inverse mapping table */
	INIT_PM_INVERSE_TABLE(meta_read);
}

void INIT_PM_INVERSE_TABLE(int meta_read)
{
	int i;
	block_state_entry* curr_b_s_entry = (block_state_entry*)block_state_table;

	if(meta_read == SUCCESS){

		/* Intialize the block state table from NAND */
		for(i=0;i<BLOCK_MAPPING_ENTRY_NB;i++){
			if(curr_b_s_entry->pm_inv_table != NULL){
				curr_b_s_entry->pm_inv_table = (int32_t*)calloc(PAGE_NB, sizeof(int32_t));
				READ_METADATA_FROM_NAND((void*)curr_b_s_entry->pm_inv_table, sizeof(int32_t)*PAGE_NB);
			}

			curr_b_s_entry += 1;
		}
	}
	else{
		/* Intialize the block state table */
		for(i=0;i<BLOCK_MAPPING_ENTRY_NB;i++){
			curr_b_s_entry->pm_inv_table	= NULL;
			curr_b_s_entry += 1;
		}
	}
}

void INIT_EMPTY_BLOCK_LIST(int meta_read)
{
	int ret;
	int i, j, k;
	int phy_block_nb;

	empty_block_entry* curr_entry;
	empty_block_root* curr_root;

	/* Allocation Memory for empty block list */
	empty_block_list = (void*)calloc(EMPTY_TABLE_ENTRY_NB, sizeof(empty_block_root));
	if(empty_block_list == NULL){
		printf("ERROR[%s] Calloc mapping table fail\n",__FUNCTION__);
		return;
	}

	if(meta_read == SUCCESS){

		READ_METADATA_FROM_NAND(empty_block_list, (sizeof(empty_block_root))*(EMPTY_TABLE_ENTRY_NB));

		void* temp_page = calloc(1, PAGE_SIZE);
		void* temp_page_ptr = temp_page;
		int temp_count = 0;
		int metablock_count = 0;
		total_empty_block_nb = 0;

		curr_root = (empty_block_root*)empty_block_list;

		/* Read entry from NAND */
		for(i=0; i<PLANES_PER_FLASH; i++){
			for(j=0; j<FLASH_NB; j++){
				total_empty_block_nb += curr_root->empty_block_nb;
				k = curr_root->empty_block_nb;

				if(k != 0 && i == 0 && j == 0){
					READ_METADATA_FROM_NAND(temp_page, PAGE_SIZE);
					temp_count = 0;
					temp_page_ptr = temp_page;
				}

				while(k > 0){
					if(temp_count == (PAGE_SIZE/sizeof(unsigned int))){
						READ_METADATA_FROM_NAND(temp_page, PAGE_SIZE);
						temp_count = 0;
						temp_page_ptr = temp_page;
					}

					curr_entry = (empty_block_entry*)calloc(1, sizeof(empty_block_entry));
					if(curr_entry == NULL){
						printf("ERROR[%s] Calloc fail\n", __FUNCTION__);
						break;
					}
					
					memcpy(&curr_entry->phy_block_nb, temp_page_ptr, sizeof(unsigned int));
					if(curr_entry->phy_block_nb < 0 || curr_entry->phy_block_nb >= BLOCK_MAPPING_ENTRY_NB){

						printf("ERROR[%s] Wrong block number. \n", __FUNCTION__);
						break;	
					}
					
					/* Check if the phy block is meta block */
					ret = IS_META_BLOCK(curr_entry->phy_block_nb);
					if(ret == SUCCESS){
						free(curr_entry);
						metablock_count++;
					}
					else{
						curr_entry->next = NULL;

						if(k == curr_root->empty_block_nb){
							curr_root->head = curr_entry;
							curr_root->tail = curr_entry;
						}
						else{
							curr_root->tail->next = curr_entry;
							curr_root->tail = curr_entry;
						}
					}
					temp_page_ptr += sizeof(unsigned int);
					temp_count++;
					k--;
				}
				curr_root->empty_block_nb -= metablock_count;
				metablock_count = 0;
				curr_root += 1;
			}
		}

		empty_block_list_index = 0;
		free(temp_page);
	}
	else{
		curr_root = (empty_block_root*)empty_block_list;

		/* Intialize the empty block list */
		for(i=0;i<PLANES_PER_FLASH;i++){
			for(j=0;j<FLASH_NB;j++){

				curr_root->empty_block_nb = 0;
				for(k=i;k<BLOCK_NB;k+=PLANES_PER_FLASH){

					phy_block_nb = j * BLOCK_NB + k;

					/* Remove Meta index block */
					if(phy_block_nb == 0){
						continue;
					}

					curr_entry = (empty_block_entry*)calloc(1, sizeof(empty_block_entry));	
					if(curr_entry == NULL){
						printf("ERROR[%s] Calloc fail\n",__FUNCTION__);
						break;
					}

					curr_entry->phy_block_nb = phy_block_nb;
					curr_entry->next = NULL;

					if(curr_root->empty_block_nb == 0){
						curr_root->head = curr_entry;
						curr_root->tail = curr_entry;
					}
					else{
						curr_root->tail->next = curr_entry;
						curr_root->tail = curr_entry;
					}
					UPDATE_BLOCK_STATE(phy_block_nb, EMPTY_BLOCK);

					curr_root->empty_block_nb++;
				}
				curr_root += 1;
			}
		}
		total_empty_block_nb = (int64_t)BLOCK_MAPPING_ENTRY_NB - 1;
		empty_block_list_index = 0;
	}
}

void TERM_BLOCK_STATE_TABLE(void)
{
	/* Write the block state table to NAND */
	WRITE_METADATA_TO_NAND(block_state_table, sizeof(block_state_entry)*BLOCK_MAPPING_ENTRY_NB);

	/* Write page inverse table to NAND */
	TERM_PM_INVERSE_TABLE();

	/* Free the memory for block state table */
	free(block_state_table);
}

void TERM_PM_INVERSE_TABLE(void)
{
	int i;
	block_state_entry* curr_b_s_entry = (block_state_entry*)block_state_table;

	/* Write the valid array data to NAND */
	for(i=0;i<BLOCK_MAPPING_ENTRY_NB;i++){
		if(curr_b_s_entry->pm_inv_table != NULL){
			WRITE_METADATA_TO_NAND((void*)curr_b_s_entry->pm_inv_table, sizeof(int32_t)*PAGE_NB);
			free(curr_b_s_entry->pm_inv_table);
		}
		curr_b_s_entry += 1;
	}

}

void TERM_EMPTY_BLOCK_LIST(void)
{
	int i, j, k;

	empty_block_entry* curr_entry;
	empty_block_root* curr_root;

	/* Write root table to NAND */
	WRITE_METADATA_TO_NAND(empty_block_list, (sizeof(empty_block_root))*(EMPTY_TABLE_ENTRY_NB));
	void* temp_page = calloc(1,PAGE_SIZE);
	void* temp_page_ptr = temp_page;
	int temp_count = 0;

	curr_root = (empty_block_root*)empty_block_list;

	/* Write entry ro NAND */
	for(i=0; i<PLANES_PER_FLASH; i++){
		for(j=0; j<FLASH_NB; j++){
			k = curr_root->empty_block_nb;
			if(k != 0){
				curr_entry = (empty_block_entry*)curr_root->head;
			}

			while(k > 0){

				memcpy(temp_page_ptr, &curr_entry->phy_block_nb, sizeof(unsigned int));
				temp_page_ptr += sizeof(unsigned int);
				temp_count++;

				if(k != 1){
					curr_entry = curr_entry->next;
				}
				k--;

				if(temp_count == (PAGE_SIZE/sizeof(unsigned int))){
					WRITE_METADATA_TO_NAND(temp_page, PAGE_SIZE);
					temp_count = 0;
					temp_page_ptr = temp_page;
				}
			}
			curr_root += 1;
		}
	}

	if(temp_count != 0){
		WRITE_METADATA_TO_NAND(temp_page, PAGE_SIZE);
	}
}

int IS_META_BLOCK(unsigned int pbn)
{
	meta_block_entry* meta_entry = meta_root->head;
	int i;
	int ret = FAIL;

	for(i=0; i<meta_root->n_meta_block; i++){
		if(meta_entry->pbn == pbn){
			ret = SUCCESS;
		}
	}

	return ret;
}

empty_block_entry* GET_EMPTY_BLOCK(int mode, int mapping_index)
{
	/* If there is no empty block, return fail */
	if(total_empty_block_nb == 0){
		printf("ERROR[%s] There is no empty block. mode:%d, mapping_index:%d \n", __FUNCTION__, mode, mapping_index);
		return NULL;
	}

	int input_mapping_index = mapping_index;
	empty_block_entry* curr_empty_block;
	empty_block_root* curr_root_entry;

	while(total_empty_block_nb != 0){

		/* Select an empty block across the flash memories */
		if(mode == VICTIM_OVERALL){
			curr_root_entry = (empty_block_root*)empty_block_list + empty_block_list_index;

			if(curr_root_entry->empty_block_nb == 0){
				/* If the root entry has no empty block, move the index */
				empty_block_list_index++;
				if(empty_block_list_index == EMPTY_TABLE_ENTRY_NB){
					empty_block_list_index = 0;
				}
				continue;
			}
			else{
				curr_empty_block = curr_root_entry->head;

				/* Update Empty Block List */
				if(curr_root_entry->empty_block_nb == 1){
					curr_root_entry->head = NULL;
					curr_root_entry->empty_block_nb = 0;
				}
				else{
					curr_root_entry->head = curr_empty_block->next;
					curr_root_entry->empty_block_nb -= 1;
				}
				
				/* Update The total number of empty block */
				total_empty_block_nb--;

				/* Move the empty block table index*/
				empty_block_list_index++;
				if(empty_block_list_index == EMPTY_TABLE_ENTRY_NB){
					empty_block_list_index = 0;
				}
				
				return curr_empty_block;
			}
		}
		/* Select an empty block in the specified flash memory */
		else if(mode == VICTIM_INCHIP){
			curr_root_entry = (empty_block_root*)empty_block_list + mapping_index;

			/* If the flash memory has no empty block, */
			if(curr_root_entry->empty_block_nb == 0){
				/* If the flash memory has multiple planes, move index */
				if(PLANES_PER_FLASH != 1){
					mapping_index++;
        	                        if(mapping_index % PLANES_PER_FLASH == 0){
                	                        mapping_index = mapping_index - (PLANES_PER_FLASH-1);
                        	        }
					if(mapping_index == input_mapping_index){
						printf("ERROR[%s] There is no empty block\n",__FUNCTION__);
						return NULL;				
					}
				}
				/* If there is no empty block in the flash memory, return fail */
				else{
#ifdef FTL_DEBUG
					printf("ERROR[%s]-INCHIP There is no empty block\n",__FUNCTION__);
#endif
					return NULL;				
				}

				continue;
			}
			else{
				curr_empty_block = curr_root_entry->head;
					
				/* Update Empty Block List */
				if(curr_root_entry->empty_block_nb == 1){
					curr_root_entry->head = NULL;
					curr_root_entry->empty_block_nb = 0;
				}
				else{
					curr_root_entry->head = curr_empty_block->next;
					curr_root_entry->empty_block_nb -= 1;
				}

				/* Update The total number of empty block */
				total_empty_block_nb--;

				return curr_empty_block;
			}	
		}
	}

	printf("ERROR[%s] There is no empty block\n",__FUNCTION__);
	return NULL;
}

int INSERT_EMPTY_BLOCK(int32_t new_empty_pbn)
{
	if(new_empty_pbn < 0 || new_empty_pbn >= FLASH_NB * BLOCK_NB){
		printf("ERROR[%s] new empty block %d \n", __FUNCTION__, new_empty_pbn);
		return FAIL;
	}

	int mapping_index;
	int plane_nb;
	empty_block_root* curr_root_entry;
	empty_block_entry* new_empty_block;

	/* Allocate memory for new empty block entry */
	new_empty_block = (empty_block_entry*)calloc(1, sizeof(empty_block_entry));
	if(new_empty_block == NULL){
		printf("ERROR[%s] Alloc new empty block fail\n",__FUNCTION__);
		return FAIL;
	}

	/* Init New empty block */
	new_empty_block->phy_block_nb = new_empty_pbn;
	new_empty_block->next = NULL;

	/* Calculate the mapping index for empty block table */
	plane_nb = CALC_BLOCK_FROM_PBN(new_empty_pbn) % PLANES_PER_FLASH;
	mapping_index = plane_nb * FLASH_NB + CALC_FLASH_FROM_PBN(new_empty_pbn);
	curr_root_entry = (empty_block_root*)empty_block_list + mapping_index;

	/* Insert new empty block to empty block table */
	if(curr_root_entry->empty_block_nb == 0){
		curr_root_entry->head = new_empty_block;
		curr_root_entry->tail = new_empty_block;
		curr_root_entry->empty_block_nb = 1;
	}
	else{
		curr_root_entry->tail->next = new_empty_block;
		curr_root_entry->tail = new_empty_block;
		curr_root_entry->empty_block_nb++;
	}

	/* Update the number of total empty blocks*/
	total_empty_block_nb++;

	return SUCCESS;
}

/* Get the block state entry of the pbn */
block_state_entry* GET_BLOCK_STATE_ENTRY(int32_t pbn)
{
	return ((block_state_entry*)block_state_table + pbn);
}

int UPDATE_BLOCK_STATE(int32_t pbn, int block_type)
{
	int i;
        block_state_entry* b_s_entry = GET_BLOCK_STATE_ENTRY(pbn);
        b_s_entry->type = block_type;

	/* If the new state of the block is empty, initialize metadata of the block */ 	
        if(block_type == EMPTY_BLOCK){

		/* Update the state of the pages in the block to empty state (0)  */
                for(i=0;i<PAGE_NB;i++){
                        UPDATE_BLOCK_STATE_ENTRY(pbn, i, 0);
                }

		/* De-allocate the page inverse mapping */
		if(b_s_entry->pm_inv_table != NULL){
			free(b_s_entry->pm_inv_table);
			b_s_entry->pm_inv_table = NULL;
		}

		b_s_entry->lbn = -1;
		b_s_entry->start_offset = -1;
		b_s_entry->write_limit = -1;

		b_s_entry->rp_root_pbn = -1;
		b_s_entry->rp_block_pbn = -1;
        }

        return SUCCESS;
}

int UPDATE_BLOCK_STATE_ENTRY(int32_t pbn, int32_t block_offset, int valid)
{
	if(pbn >= BLOCK_MAPPING_ENTRY_NB || block_offset >= PAGE_NB){
		printf("ERROR[%s] Wrong physical address. (%d, %d)\n", __FUNCTION__, pbn, block_offset);
		return FAIL;
	}

	int i;
	int valid_count = 0;
	int32_t ppn = pbn * PAGE_NB + block_offset;
	int32_t start_ppn = pbn * PAGE_NB;
	/* Get the block state Info */
	block_state_entry* state_entry = GET_BLOCK_STATE_ENTRY(pbn);

	/* Update the page state */
	if(valid == VALID){

		SET_BIT(valid_bitmap, ppn);

		/* Check write limit and update*/
		if(state_entry->write_limit < block_offset){
			state_entry->write_limit = block_offset;
		}
		else{
			printf("ERROR[%s] Is this possible? pbn:%d->%d %d\n", __FUNCTION__, pbn, state_entry->write_limit, block_offset);
		}
	}
	else if(valid == INVALID){
		RESET_BIT(valid_bitmap, ppn);
	}
	else if(valid == 0){
		RESET_BIT(valid_bitmap, ppn);
	}
	else{
		printf("ERROR[%s] Wrong valid value\n",__FUNCTION__);
	}

	/* Update valid_page_nb */
	for(i=0;i<PAGE_NB;i++){
		if(GET_BIT(valid_bitmap, start_ppn + i) == 1){
			valid_count++;
		}
	}
	state_entry->valid_page_nb = valid_count;

	return SUCCESS;
}



void UPDATE_MULTIPLE_BLOCK_STATE_ENTRIES(int32_t sector_nb, unsigned int length, int32_t pbn, int valid)
{
	int32_t block_offset;
	int32_t lba = sector_nb;
	int32_t start_offset, real_offset;

	unsigned int remain = length;
	unsigned int left_skip = sector_nb % SECTORS_PER_PAGE;
	unsigned int right_skip;
	unsigned int write_sects;

	int32_t lpn;		// Logical Page Number
	int32_t lbn;		// Logical Block Number

	block_state_entry* b_s_entry = GET_BLOCK_STATE_ENTRY(pbn);
	start_offset = b_s_entry->start_offset;
	if(start_offset == -1){
		printf("ERROR[%s] start offset is not initialized\n", __FUNCTION__);
		return;
	}

	while(remain > 0){

		if(remain > SECTORS_PER_PAGE - left_skip){
			right_skip = 0;
		}
		else{
			right_skip = SECTORS_PER_PAGE - left_skip - remain;
		}

		write_sects = SECTORS_PER_PAGE - left_skip - right_skip;

		/* Calculate logical address and block offset */
		lpn = lba / (int32_t)SECTORS_PER_PAGE;
		lbn = lpn / PAGE_NB;
		block_offset = lpn % (int32_t)PAGE_NB;
		real_offset = block_offset - start_offset;

		if(real_offset >= 0){
			UPDATE_BLOCK_STATE_ENTRY(pbn, real_offset, valid);
		}

		lba += write_sects;
		remain -= write_sects;
		left_skip = 0;
	}
}
