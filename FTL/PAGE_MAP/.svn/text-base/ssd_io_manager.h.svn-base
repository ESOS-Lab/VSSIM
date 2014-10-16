#ifndef _SSD_IO_MANAGER_H
#define _SSD_IO_MANAGER_H

extern int old_channel_nb;

int64_t get_usec(void);

int SSD_IO_INIT(void);

int CELL_READ(unsigned int flash_nb, unsigned int block_nb, unsigned int page_nb);
int CELL_WRITE(unsigned int flash_nb, unsigned int block_nb, unsigned int page_nb);
int BLOCK_ERASE(unsigned int flash_nb, unsigned int block_nb);

int CHANNEL_ACCESS_W(int channel);
int CHANNEL_CHECK(int channel);
int CHANNEL_ACCESS_R(int channel);

int REG_ACCESS(int channel, int flag);


#endif
