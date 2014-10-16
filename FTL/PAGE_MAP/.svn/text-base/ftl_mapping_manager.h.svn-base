#ifndef _MAPPING_MANAGER_H_
#define _MAPPING_MANAGER_H_

extern void* mapping_table_start;
extern void* block_table_start;

extern unsigned int flash_index;
extern unsigned int* plane_index;
extern unsigned int* block_index;

typedef struct mapping_table_entry
{
	unsigned int phy_flash_nb;
	unsigned int phy_block_nb;
	unsigned int phy_page_nb;
}mapping_table_entry;

void INIT_MAPPING_TABLE(void);
void TERM_MAPPING_TABLE(void);

int GET_MAPPING_INFO(int64_t lba, unsigned int* phy_flash_nb, unsigned int* phy_block_nb, unsigned int* phy_page_nb);
int GET_NEW_PAGE(unsigned int* new_phy_flash_nb, unsigned int* new_phy_block_nb, unsigned int* new_phy_page_nb);
mapping_table_entry* GET_MAPPING_ENTRY(int64_t lba);

int UPDATE_OLD_PAGE_MAPPING(int64_t lba);
int UPDATE_NEW_PAGE_MAPPING(int64_t lba, unsigned int new_phy_flash_nb, unsigned int new_phy_block_nb, unsigned int new_phy_page_nb);

#endif
