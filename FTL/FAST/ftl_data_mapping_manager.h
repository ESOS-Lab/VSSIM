// Copyright(c)2013 
//
// Hanyang University, Seoul, Korea
// Embedded Software Systems Lab. All right reserved

#ifndef _DATA_MAPPING_MANAGER_H_
#define _DATA_MAPPING_MANAGER_H_

typedef struct data_block_mapping_entry
{
	unsigned int phy_flash_nb;
	unsigned int phy_block_nb;

}data_block_mapping_entry;

void INIT_DATA_BLOCK_MAPPING(void);
void TERM_DATA_BLOCK_MAPPING(void);

int GET_DATA_BLOCK_MAPPING_INFO(unsigned int log_flash_nb, unsigned int log_block_nb, unsigned int* phy_flash_nb, unsigned int* phy_block_nb);
data_block_mapping_entry* GET_DATA_BLOCK_MAPPING_ENTRY(unsigned int log_flash_nb, unsigned int log_block_nb);

int UPDATE_DATA_BLOCK_MAPPING(unsigned int log_flash_nb, unsigned int log_block_nb, unsigned int phy_flash_nb, unsigned int phy_block_nb);

int FIND_PAGE_IN_DATA_BLOCK(int64_t sector_nb, unsigned int* phy_flash_nb, unsigned int* phy_block_nb, unsigned int* phy_page_nb);
#endif
