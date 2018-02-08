// File: ftl_inverse_mapping_manager.c
// Date: 2018. 02. 08.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2018
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved



#include "common.h"

void* inverse_block_mapping_table_start;

void* empty_block_table_start;
unsigned int empty_block_table_index;
int64_t total_empty_block_nb;

void INIT_INVERSE_BLOCK_MAPPING(void)
{
	/* Allocation Memory for Data Block Mapping Table */
	inverse_block_mapping_table_start = (void*)calloc(BLOCK_MAPPING_ENTRY_NB, sizeof(inverse_block_mapping_entry));
	if(inverse_block_mapping_table_start == NULL){
		printf("ERROR[INIT_INVERSE_BLOCK_MAPPING_TABLE] Calloc mapping table fail\n");
		return;
	}

	/* Initialization Data Block Mapping Table */
	FILE* fp = fopen("./data/inverse_block_mapping.dat","r");
	if(fp != NULL){
		fread(inverse_block_mapping_table_start, sizeof(inverse_block_mapping_entry), BLOCK_MAPPING_ENTRY_NB, fp);
	}
	else{
		int i;
		inverse_block_mapping_entry* curr_mapping_entry = (inverse_block_mapping_entry*)inverse_block_mapping_table_start;
		char* valid_array;

		for(i=0;i<BLOCK_MAPPING_ENTRY_NB;i++){
			valid_array = (char*)calloc(PAGE_NB, sizeof(char));
			if(valid_array == NULL){
				printf("ERROR[INIT_INVERSE_BLOCK_MAPPING] Calloc valid array fail\n");
			}
			memset(valid_array, 0, PAGE_NB);

			curr_mapping_entry->log_flash_nb 	= FLASH_NB;
			curr_mapping_entry->log_block_nb 	= 0;
			curr_mapping_entry->type		= EMPTY_BLOCK;
			curr_mapping_entry->valid_page_nb	= 0;
			curr_mapping_entry->valid_array		= valid_array;
			curr_mapping_entry += 1;
		}
	}
}

void INIT_VALID_ARRAY(void)
{
	int i;
	inverse_block_mapping_entry* curr_mapping_entry = (inverse_block_mapping_entry*)inverse_block_mapping_table_start;
	char* valid_array;

	FILE* fp = fopen("./data/valid_array.dat","r");
	if(fp != NULL){
		for(i=0;i<BLOCK_MAPPING_ENTRY_NB;i++){
			valid_array = (char*)calloc(PAGE_NB, sizeof(char));
			fread(valid_array, sizeof(char), PAGE_NB, fp);
			curr_mapping_entry->valid_array = valid_array;

			curr_mapping_entry += 1;
		}
	}
	else{
		for(i=0;i<BLOCK_MAPPING_ENTRY_NB;i++){
			valid_array = (char*)calloc(PAGE_NB, sizeof(char));
			memset(valid_array,0,PAGE_NB);
			curr_mapping_entry->valid_array = valid_array;

			curr_mapping_entry += 1;
		}
	}
}

void INIT_EMPTY_BLOCK_LIST(void)
{
	int i, j, k;
	empty_block_entry* curr_entry;
	empty_block_root* curr_root;

	empty_block_table_start = (void*)calloc(PLANES_PER_FLASH * FLASH_NB, sizeof(empty_block_root));
	if(empty_block_table_start == NULL){
		printf("ERROR[INIT_EMPTY_BLOCK_LIST] Calloc mapping table fail\n");
		return;									       
       	}

	FILE* fp = fopen("./data/empty_block_list.dat","r");

	if(fp != NULL){
		total_empty_block_nb = 0;
		fread(empty_block_table_start,sizeof(empty_block_root),PLANES_PER_FLASH*FLASH_NB, fp);
		curr_root = (empty_block_root*)empty_block_table_start;

		for(i=0;i<PLANES_PER_FLASH;i++){

			for(j=0;j<FLASH_NB;j++){
				total_empty_block_nb += curr_root->empty_block_nb;
				k = curr_root->empty_block_nb;

				while(k > 0){
					curr_entry = (empty_block_entry*)calloc(1, sizeof(empty_block_entry));
					if(curr_entry == NULL){
						printf("ERROR[INIT_EMPTY_BLOCK_LIST] Calloc fail\n");
						break;					
					}
					fread(curr_entry, sizeof(empty_block_entry), 1, fp);
					curr_entry->next = NULL;

					if(k == curr_root->empty_block_nb){
						curr_root->next = curr_entry;
						curr_root->tail = curr_entry;
					}
					else{
						curr_root->tail->next = curr_entry;
						curr_root->tail = curr_entry;
					}
					k--;
				}
				curr_root += 1;
			}
		}
		empty_block_table_index = 0;
	}
	else{
		curr_root = (empty_block_root*)empty_block_table_start;		
		for(i=0;i<PLANES_PER_FLASH;i++){
		
			for(j=0;j<FLASH_NB;j++){

				for(k=i;k<BLOCK_NB;k+=PLANES_PER_FLASH){

					curr_entry = (empty_block_entry*)calloc(1, sizeof(empty_block_entry));
					if(curr_entry == NULL){
						printf("ERROR[INIT_EMPTY_BLOCK_LIST] Calloc fail\n");
						break;
					}
					if(k==i){
						curr_root->next = curr_entry;
						curr_root->tail = curr_entry;
						
						curr_root->tail->phy_flash_nb = j;
						curr_root->tail->phy_block_nb = k;
					}
					else{
						curr_root->tail->next = curr_entry;
						curr_root->tail = curr_entry;

						curr_root->tail->phy_flash_nb = j;
						curr_root->tail->phy_block_nb = k;
					}
				}
				curr_root->empty_block_nb = (unsigned int)EACH_EMPTY_BLOCK_ENTRY_NB;
				curr_root += 1;
			}
		}
		total_empty_block_nb = (int64_t)BLOCK_MAPPING_ENTRY_NB;
		empty_block_table_index = 0;
	}
}

void CAL_NEW_EMPTY_BLOCK_ADDR(int64_t count, unsigned int* phy_flash_nb, unsigned int* phy_block_nb)
{
	*phy_flash_nb = count % FLASH_NB;
	*phy_block_nb = count / FLASH_NB;
}

void TERM_INVERSE_BLOCK_MAPPING(void)
{
	FILE* fp = fopen("./data/inverse_block_mapping.dat","w");
	if(fp==NULL){
		printf("ERROR[TERM_INVERSE_BLOCK_MAPPING] FILE open fail\n");
		return;
	}

	/* Write The inverse block table to file */
	fwrite(inverse_block_mapping_table_start, sizeof(inverse_block_mapping_entry), BLOCK_MAPPING_ENTRY_NB, fp);

	/* Free The inverse block table memory */
	free(inverse_block_mapping_table_start);
}

void TERM_VALID_ARRAY(void)
{
	int i;
	inverse_block_mapping_entry* curr_mapping_entry = (inverse_block_mapping_entry*)inverse_block_mapping_table_start;
	char* valid_array;

	FILE* fp = fopen("./data/valid_array.dat","w");
	if(fp == NULL){
		printf("ERROR[TERM_VALID_ARRAY] File open fail\n");
		return;
	}

	for(i=0;i<BLOCK_MAPPING_ENTRY_NB;i++){
		valid_array = curr_mapping_entry->valid_array;
		fwrite(valid_array, sizeof(char), PAGE_NB, fp);
		curr_mapping_entry += 1;
	}
}

void TERM_EMPTY_BLOCK_LIST(void)
{
	int i, j, k;
	empty_block_entry* curr_entry;
	empty_block_root* curr_root;

	FILE* fp = fopen("./data/empty_block_list.dat","w");
	if(fp==NULL){
		printf("ERROR[TERM_EMPTY_BLOCK_LIST] File open fail\n");
	}

	fwrite(empty_block_table_start,sizeof(empty_block_root),PLANES_PER_FLASH*FLASH_NB, fp);
	curr_root = (empty_block_root*)empty_block_table_start;

	for(i=0;i<PLANES_PER_FLASH;i++){

		for(j=0;j<FLASH_NB;j++){

			k = curr_root->empty_block_nb;
			if(k != 0){
				curr_entry = (empty_block_entry*)curr_root->next;
			}
			while(k > 0){
				fwrite(curr_entry, sizeof(empty_block_entry), 1, fp);
				if(k != 1){
					curr_entry = curr_entry->next;
				}
				k--;
			}
			curr_root += 1;
		}
	}
}

int GET_EMPTY_BLOCK(unsigned int* phy_flash_nb, unsigned int* phy_block_nb)
{
	if(total_empty_block_nb == 0){
		printf("ERROR[GET_NEW_BLOCK] There is no empty block\n");
		return FAIL;
	}else{
		empty_block_entry* curr_empty_block;
		empty_block_root* curr_root_entry;
		while(1){
			curr_root_entry = (empty_block_root*)empty_block_table_start + empty_block_table_index;

			if(curr_root_entry->empty_block_nb == 0){
				empty_block_table_index++;
				if(empty_block_table_index == EMPTY_BLOCK_TABLE_NB){
					empty_block_table_index = 0;
				}
				continue;
			}
			else{
				curr_empty_block = curr_root_entry->next;

				*phy_flash_nb = curr_empty_block->phy_flash_nb;
				*phy_block_nb = curr_empty_block->phy_block_nb;

				if(curr_root_entry->empty_block_nb == 1){
					curr_root_entry->next = NULL;
					curr_root_entry->tail = NULL;
					curr_root_entry->empty_block_nb = 0;
				}
				else{
					curr_root_entry->next = curr_empty_block->next;
					curr_root_entry->empty_block_nb--;
				}
				free(curr_empty_block);
				total_empty_block_nb--;
				empty_block_table_index++;

				if(empty_block_table_index == EMPTY_BLOCK_TABLE_NB){
					empty_block_table_index = 0;
				}
				return SUCCESS;
			}
		}
	}
}

int INSERT_EMPTY_BLOCK(unsigned int phy_flash_nb, unsigned int phy_block_nb)
{
	int mapping_index;
	int plane_nb;

	empty_block_root* curr_root_entry;
	empty_block_entry* new_empty_block;

	new_empty_block = (empty_block_entry*)calloc(1, sizeof(empty_block_entry));
	if(new_empty_block == NULL){
		printf("ERROR[INSERT_EMPTY_BLOCK] Alloc new empty block fail\n");
		return FAIL;
	}

	/* Init New empty block */
	new_empty_block->phy_flash_nb = phy_flash_nb;
	new_empty_block->phy_block_nb = phy_block_nb;
	new_empty_block->next = NULL;

	plane_nb = phy_block_nb % PLANES_PER_FLASH;
	mapping_index = plane_nb * FLASH_NB + phy_flash_nb;

	curr_root_entry = (empty_block_root*)empty_block_table_start + mapping_index;
	if(curr_root_entry->empty_block_nb == 0){
		curr_root_entry->next = new_empty_block;
		curr_root_entry->tail = new_empty_block;
		curr_root_entry->empty_block_nb = 1;
	}
	else{
		curr_root_entry->tail->next = new_empty_block;
		curr_root_entry->tail = new_empty_block;
		curr_root_entry->empty_block_nb++;
	}
	total_empty_block_nb++;
	return SUCCESS;
}

inverse_block_mapping_entry* GET_INVERSE_BLOCK_MAPPING_ENTRY(unsigned int phy_flash_nb, unsigned int phy_block_nb){

	int64_t mapping_index = phy_flash_nb * BLOCK_NB + phy_block_nb;

	inverse_block_mapping_entry* mapping_entry = (inverse_block_mapping_entry*)inverse_block_mapping_table_start + mapping_index;

	return mapping_entry;
}

int UPDATE_INVERSE_BLOCK_MAPPING(unsigned int phy_flash_nb, unsigned int phy_block_nb, unsigned int log_flash_nb, unsigned int log_block_nb, int type)
{
	int i;
	inverse_block_mapping_entry* mapping_entry = GET_INVERSE_BLOCK_MAPPING_ENTRY(phy_flash_nb, phy_block_nb);

	mapping_entry->log_flash_nb 	= log_flash_nb;
	mapping_entry->log_block_nb 	= log_block_nb;
	mapping_entry->type		= type;

	if(type == EMPTY_BLOCK || type == EMPTY_DATA_BLOCK || type == EMPTY_SEQ_BLOCK || type == EMPTY_RAN_COLD_BLOCK || type == EMPTY_RAN_HOT_BLOCK){
		for(i=0;i<PAGE_NB;i++){
			UPDATE_INVERSE_BLOCK_VALIDITY(phy_flash_nb, phy_block_nb, i, 0);
		}
	}

	return SUCCESS;
}

int UPDATE_INVERSE_BLOCK_VALIDITY(unsigned int phy_flash_nb, unsigned int phy_block_nb, unsigned int phy_page_nb, int valid)
{
	if(phy_flash_nb >= FLASH_NB || phy_block_nb >= BLOCK_NB || phy_page_nb >= PAGE_NB){
		printf("ERROR[UPDATE_INVERSE_BLOCK_VALIDITY] Wrong physical address\n");
		return FAIL;
	}

	int i;
	int valid_count = 0;
	inverse_block_mapping_entry* mapping_entry = GET_INVERSE_BLOCK_MAPPING_ENTRY(phy_flash_nb, phy_block_nb);

	char* valid_array = mapping_entry->valid_array;

	if(valid == VALID){
		valid_array[phy_page_nb] = 'V';
	}
	else if(valid == INVALID){
		valid_array[phy_page_nb] = 'I';
	}
	else if(valid == 0){
		valid_array[phy_page_nb] = '0';
	}
	else{
		printf("ERROR[UPDATE_INVERSE_BLOCK_VALIDITY] Wrong valid value\n");
	}

	/* Update valid_page_nb */
	for(i=0;i<PAGE_NB;i++){
		if(valid_array[i] == 'V'){
			valid_count++;
		}
	}
	mapping_entry->valid_page_nb = valid_count;

	return SUCCESS;
}

void PRINT_VALID_ARRAY(unsigned int phy_flash_nb, unsigned int phy_block_nb)
{
	int i;
	int cnt = 0;
	inverse_block_mapping_entry* inverse_block_entry = GET_INVERSE_BLOCK_MAPPING_ENTRY(phy_flash_nb, phy_block_nb);

	printf("Type %d [%d][%d]valid array:\n",inverse_block_entry->type,  phy_flash_nb, phy_block_nb);
	for(i=0;i<PAGE_NB;i++){
		printf("%c ",inverse_block_entry->valid_array[i]);
		cnt++;
		if(cnt == 10){
			printf("\n");
		}
	}
	printf("\n");
}
