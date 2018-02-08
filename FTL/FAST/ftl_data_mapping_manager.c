// Copyright(c)2013 
//
// Hanyang University, Seoul, Korea
// Embedded Software Systems Lab. All right reserved

#include "common.h"

void* data_block_mapping_table_start;

void INIT_DATA_BLOCK_MAPPING(void)
{
	/* Allocation Memory for Data Block Mapping Table */
	data_block_mapping_table_start = (void*)calloc(DATA_MAPPING_ENTRY_NB, sizeof(data_block_mapping_entry));
	if(data_block_mapping_table_start == NULL){
		printf("ERROR[INIT_DATA_BLOCK_MAPPING_TABLE] Calloc mapping table fail\n");
		return;
	}

	/* Initialization Data Block Mapping Table */
	FILE* fp = fopen("./data/block_mapping.dat","r");
	if(fp != NULL){
		fread(data_block_mapping_table_start, sizeof(data_block_mapping_entry), DATA_MAPPING_ENTRY_NB, fp);
	}
	else{
		int i;
		data_block_mapping_entry* curr_mapping_entry = (data_block_mapping_entry*)data_block_mapping_table_start;
		for(i=0;i<DATA_MAPPING_ENTRY_NB;i++){
			curr_mapping_entry->phy_flash_nb 	= FLASH_NB;
			curr_mapping_entry->phy_block_nb 	= 0;

			curr_mapping_entry += 1;
		}
	}
}

void TERM_DATA_BLOCK_MAPPING(void)
{
	FILE* fp = fopen("./data/block_mapping.dat","w");
	if(fp == NULL){
		printf("ERROR[TERM_DATA_BLOCK_MAPPING] File open fail\n");
		return;
	}

	/* Write the mapping table to file */
	fwrite(data_block_mapping_table_start, sizeof(data_block_mapping_entry), DATA_MAPPING_ENTRY_NB, fp);

	/* Free the mapping table memory */
	free(data_block_mapping_table_start);
}

int GET_DATA_BLOCK_MAPPING_INFO(unsigned int log_flash_nb, unsigned int log_block_nb, unsigned int* phy_flash_nb, unsigned int* phy_block_nb)
{
	data_block_mapping_entry* data_mapping_entry = GET_DATA_BLOCK_MAPPING_ENTRY(log_flash_nb, log_block_nb);

	*phy_flash_nb = data_mapping_entry->phy_flash_nb;
	*phy_block_nb = data_mapping_entry->phy_block_nb;

	return SUCCESS;
}

data_block_mapping_entry* GET_DATA_BLOCK_MAPPING_ENTRY(unsigned int log_flash_nb, unsigned int log_block_nb){

	int64_t data_mapping_index = log_flash_nb * BLOCK_NB + log_block_nb;

	data_block_mapping_entry* data_mapping_entry = (data_block_mapping_entry*)data_block_mapping_table_start + data_mapping_index;

	return data_mapping_entry;
}


int UPDATE_DATA_BLOCK_MAPPING(unsigned int log_flash_nb, unsigned int log_block_nb, unsigned int phy_flash_nb, unsigned int phy_block_nb)
{
	data_block_mapping_entry* data_mapping_entry = GET_DATA_BLOCK_MAPPING_ENTRY(log_flash_nb, log_block_nb);

	data_mapping_entry->phy_flash_nb = phy_flash_nb;
	data_mapping_entry->phy_block_nb = phy_block_nb;

	return SUCCESS;
}

int FIND_PAGE_IN_DATA_BLOCK(int64_t sector_nb, unsigned int* phy_flash_nb, unsigned int* phy_block_nb, unsigned int* phy_page_nb)
{
        int64_t log_page_nb_i = sector_nb / (int64_t)SECTORS_PER_PAGE;
        int64_t log_block_nb_i = log_page_nb_i / (int64_t)PAGE_NB;

        unsigned int log_flash_nb_r;
        unsigned int log_block_nb_r;
        unsigned int log_page_nb_r;

	unsigned int data_flash_nb;
	unsigned int data_block_nb;

        log_flash_nb_r = (unsigned int)(log_block_nb_i / (int64_t)BLOCK_NB);
        log_block_nb_r = (unsigned int)(log_block_nb_i % (int64_t)BLOCK_NB);
        log_page_nb_r  = (unsigned int)(log_page_nb_i % (int64_t)PAGE_NB);

	GET_DATA_BLOCK_MAPPING_INFO(log_flash_nb_r, log_block_nb_r, &data_flash_nb, &data_block_nb);
	
	if(data_flash_nb != FLASH_NB){

		*phy_flash_nb = data_flash_nb;
		*phy_block_nb = data_block_nb;
		*phy_page_nb = log_page_nb_r;

		return SUCCESS;
	}

	return FAIL;
}
