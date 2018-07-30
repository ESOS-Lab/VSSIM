// File: firm_buffer_manager.c
// Date: 18-Sep-2017
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2017
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "qemu/osdep.h"
#include "hw/block/block.h"
#include "hw/hw.h"
#include "hw/pci/msix.h"
#include "hw/pci/pci.h"
#include "sysemu/sysemu.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "sysemu/block-backend.h"

#include "common.h"
#include "hw/block/nvme.h"

event_queue* e_queue;
event_queue* candidate_e_queue;
event_queue* complete_e_queue;

/* Pointer for the last read entry in the event queue */
event_queue_entry* last_read_entry;

/* Global variable for the FIRM IO BUF THREAD and locks for the event queue*/
pthread_mutex_t eq_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cq_lock = PTHREAD_MUTEX_INITIALIZER;

/* Global variable for the write buffer */
int N_WRITE_BUF = 2;	//TEMP
vssim_io_buffer* vssim_w_buf;
vssim_io_buffer vssim_r_buf;
vssim_io_buffer vssim_discard_buf;

void INIT_IO_BUFFER(void)
{
	/* Allocation event queue structure */
	e_queue = (event_queue*)calloc(1, sizeof(event_queue));
	if(e_queue == NULL){
		printf("ERROR [%s] Allocation event queue fail.\n",__FUNCTION__);
		return;
	}
	else{	/* Initialization event queue structure */
		e_queue->entry_nb = 0;
		e_queue->head = NULL;
		e_queue->tail = NULL;
	}

	/* Allocation candidate event queue structure */
	candidate_e_queue = (event_queue*)calloc(1, sizeof(event_queue));
	if(candidate_e_queue == NULL){
		printf("ERROR [%s] Allocation event queue fail.\n",__FUNCTION__);
		return;
	}
	else{	/* Initialization event queue structure */
		candidate_e_queue->entry_nb = 0;
		candidate_e_queue->head = NULL;
		candidate_e_queue->tail = NULL;
	}


	/* Allocation completed event queue structure */
	complete_e_queue = (event_queue*)calloc(1, sizeof(event_queue));
	if(complete_e_queue == NULL){
		printf("ERROR [%s] Allocation completed event queue fail.\n",__FUNCTION__);
		return;
	}
	else{	/* Initialization event queue structure */
		complete_e_queue->entry_nb = 0;
		complete_e_queue->head = NULL;
		complete_e_queue->tail = NULL;
	}

	/* Allocation Write Buffer in DRAM */
	INIT_VSSIM_WRITE_BUFFER();

	/* Allocation Read Buffer in DRAM */
	INIT_VSSIM_READ_BUFFER();

	/* Allocation Discard Buffer in DRAM */
	INIT_VSSIM_DISCARD_BUFFER();

	/* Initialization Buffer Pointers */
	last_read_entry = NULL;
}


void INIT_VSSIM_WRITE_BUFFER(void)
{
	int i;

	/* Allocate write buffer structure */
	vssim_w_buf = (vssim_io_buffer*)calloc(sizeof(vssim_io_buffer), N_WRITE_BUF);
	if(vssim_w_buf == NULL){
		printf("ERROR[%s] Allocation write buffer structure fail!\n", __FUNCTION__);
		return;
	}

	for(i=0; i<N_WRITE_BUF; i++){
		/* Allocate write buffer  */
		vssim_w_buf[i].addr = (void*)calloc(N_WB_SECTORS, SECTOR_SIZE);
		if(vssim_w_buf[i].addr == NULL){
			printf("ERROR [%s] Allocation write buffer fail.\n",__FUNCTION__);
			return;
		}

		/* Initialize write buffer structure */
		vssim_w_buf[i].n_empty_sectors = N_WB_SECTORS;
		vssim_w_buf[i].ftl_ptr = vssim_w_buf[i].addr;
		vssim_w_buf[i].host_ptr = vssim_w_buf[i].addr;
		vssim_w_buf[i].buffer_end = vssim_w_buf[i].addr 
				+ N_WB_SECTORS*SECTOR_SIZE;
		vssim_w_buf[i].is_full = 0;
		vssim_w_buf[i].t_last_flush = 0;
		pthread_mutex_init(&vssim_w_buf[i].lock, NULL);	
		pthread_cond_init(&vssim_w_buf[i].ready, NULL);	
	}
}


void INIT_VSSIM_READ_BUFFER(void)
{
	/* Allocate read buffer  */
	vssim_r_buf.addr = (void*)calloc(N_RB_SECTORS, SECTOR_SIZE);
	if(vssim_r_buf.addr == NULL){
		printf("ERROR [%s] Allocation read buffer fail.\n",__FUNCTION__);
		return;
	}

	/* Initialize write buffer structure */
	vssim_r_buf.n_empty_sectors = N_RB_SECTORS;
	vssim_r_buf.ftl_ptr = vssim_r_buf.addr;
	vssim_r_buf.host_ptr = vssim_r_buf.addr;
	vssim_r_buf.buffer_end = vssim_r_buf.addr + N_RB_SECTORS*SECTOR_SIZE;
	vssim_r_buf.is_full = 0;
}


void INIT_VSSIM_DISCARD_BUFFER(void)
{
	/* Allocate discard buffer  */
	vssim_discard_buf.addr = (void*)calloc(N_DISCARD_BUF_SECTORS, SECTOR_SIZE);
	if(vssim_discard_buf.addr == NULL){
		printf("ERROR [%s] Allocation discard buffer fail.\n",__FUNCTION__);
		return;
	}

	/* Initialize write buffer structure */
	vssim_discard_buf.n_empty_sectors = N_RB_SECTORS;
	vssim_discard_buf.ftl_ptr = vssim_discard_buf.addr;
	vssim_discard_buf.host_ptr = vssim_discard_buf.addr;
	vssim_discard_buf.buffer_end = vssim_discard_buf.addr 
					+ N_DISCARD_BUF_SECTORS*SECTOR_SIZE;
	vssim_discard_buf.is_full = 0;
}


void TERM_IO_BUFFER(void)
{
	int i;

	/* Deallocate IO Buffer */
	for(i=0; i<N_WRITE_BUF; i++){
		free(vssim_w_buf[i].addr);
	}	

	free(vssim_r_buf.addr);
	free(vssim_discard_buf.addr);

	/* Deallocate event queue */
	free(e_queue);
	free(candidate_e_queue);
	free(complete_e_queue);
}

void FIRM_READ_EVENT(event_queue_entry* r_entry)
{
	uint32_t length = r_entry->length;

	/* Check whether the current read buffer can afford the read request */
	while(vssim_r_buf.n_empty_sectors < length){
		/* busy waiting */
	}

	/* Record current read buffer address */
	r_entry->buf = vssim_r_buf.ftl_ptr;
	INCREASE_RB_FTL_POINTER(length);

	/* Insert the current event to the candidate event queue */
	INSERT_TO_CANDIDATE_EVENT_QUEUE(r_entry);

	/* Insert the current event to the per-core queue */
	INSERT_RW_TO_PER_CORE_EVENT_QUEUE(r_entry, -1);

	/* Wakeup all io threads */
	WAKEUP_ALL_IO_THREADS();
}

void FIRM_WRITE_EVENT(event_queue_entry* w_entry, bool flush)
{
	uint32_t length = w_entry->length;
	uint32_t remain_sectors = 0;
	static int write_buffer_index = 0;

	pthread_mutex_lock(&vssim_w_buf[write_buffer_index].lock);

	/* Check whether the current write buffer can afford the write request */
	remain_sectors = (uint32_t)((vssim_w_buf[write_buffer_index].buffer_end
				- vssim_w_buf[write_buffer_index].host_ptr)/SECTOR_SIZE);
	if(remain_sectors < length){

		/* Mark write buffer flag as full */
		vssim_w_buf[write_buffer_index].is_full = 1;

#ifdef IO_CORE_DEBUG
		printf("[%s] %lu-th write event: now %d-th write buffer is full\n",
				__FUNCTION__, w_entry->seq_nb, write_buffer_index);
#endif

		pthread_mutex_unlock(&vssim_w_buf[write_buffer_index].lock);

		/* Wake up the IO thread */
		WAKEUP_ALL_IO_THREADS();

		/* Use another write buffer */	
		write_buffer_index++;
		if(write_buffer_index == N_WRITE_BUF){
			write_buffer_index = 0;
		}

		pthread_mutex_lock(&vssim_w_buf[write_buffer_index].lock);
	}

	/* If current buffer is also full, wait till the buffer is empty */	
	while(vssim_w_buf[write_buffer_index].is_full == 1){
#ifdef IO_CORE_DEBUG
		printf("[%s] %lu-th write event: wait, %d-th write buffer is also full\n",
				__FUNCTION__, w_entry->seq_nb, write_buffer_index);
#endif
		pthread_cond_wait(&vssim_w_buf[write_buffer_index].ready,
				&vssim_w_buf[write_buffer_index].lock);
	}

	/* Record current write buffer address */
	w_entry->buf = vssim_w_buf[write_buffer_index].host_ptr;
 
	/* Read Data from Host to write buffer  */
	DMA_FROM_HOST_TO_BUFFER(w_entry, write_buffer_index);

	/* Release the lock for the current write buffer */
	pthread_mutex_unlock(&vssim_w_buf[write_buffer_index].lock);

	w_entry->flush = flush;

	/* Insert the current event to the per-core queue */
	INSERT_RW_TO_PER_CORE_EVENT_QUEUE(w_entry, write_buffer_index);

	if(flush){
		/* Insert the current event to the candidate event queue */
		INSERT_TO_CANDIDATE_EVENT_QUEUE(w_entry);
	}
	else{
#ifdef IO_CORE_DEBUG
		printf("[%s] %lu-th event: write to buffer and completed!\n",
				__FUNCTION__, w_entry->seq_nb);
#endif
		/* Return immediately to the host */
		UPDATE_EVENT_STATE(w_entry, COMPLETED);	
	}

	/* Wake up the IO thread */
	WAKEUP_ALL_IO_THREADS();
}


void FIRM_DISCARD_EVENT(event_queue_entry* dc_entry)
{	
	uint32_t length = dc_entry->length;

	/* Check whether the current read buffer can afford the read request */
	while(vssim_discard_buf.n_empty_sectors < length){
		/* busy waiting */
	}

	/* Record current read buffer address */
	dc_entry->buf = vssim_discard_buf.host_ptr;

	/* Read Data from Host to Write buffer  */
	DMA_FROM_HOST_TO_BUFFER(dc_entry, -1);

	/* Insert the current event to the candidate event queue */
	INSERT_TO_CANDIDATE_EVENT_QUEUE(dc_entry);

	/* Insert the current event to the per-core queue */
	INSERT_DISCARD_TO_PER_CORE_EVENT_QUEUE(dc_entry);
}

void FIRM_FLUSH_EVENT(event_queue_entry* eq_entry)
{
	int io_type;
	event_queue_entry* cur_entry = NULL;

	pthread_mutex_lock(&eq_lock);

	while(e_queue->entry_nb != 0){

		/* Get new IO event */
		cur_entry = DEQUEUE_IO();
		if(cur_entry == NULL)
			break;

		io_type = cur_entry->io_type;

		if(io_type == READ){
			FIRM_READ_EVENT(cur_entry);
		}
		else if(io_type == WRITE){
			FIRM_WRITE_EVENT(cur_entry, true);
		}
		else if(io_type == DISCARD){
			FIRM_DISCARD_EVENT(cur_entry);	
		}
		else if(io_type == FLUSH){
			cur_entry->flush = false;
			UPDATE_EVENT_STATE(cur_entry, COMPLETED);
		}
	}	

	pthread_mutex_unlock(&eq_lock);

	eq_entry->n_child = N_IO_CORES;

	/* Insert the current event to the candidate event queue */
	eq_entry->flush = true;
	INSERT_TO_CANDIDATE_EVENT_QUEUE(eq_entry);

	/* Insert the current event to the per-core queue */
	INSERT_FLUSH_TO_PER_CORE_EVENT_QUEUE(eq_entry);

	/* Wakeup all io threads */
	WAKEUP_ALL_IO_THREADS();

	/* Wait the completion of the flush event */
	WAIT_FLUSH_COMPLETION();
}

void WAIT_FLUSH_COMPLETION(void)
{
	int core_id;

#ifdef IO_CORE_DEBUG
	printf("[%s] Firm thread start wait flush completion...\n", __FUNCTION__);
#endif

	for(core_id=0; core_id<N_IO_CORES; core_id++){
		
		while(TEST_FLUSH_FLAG(core_id)){

		}
	}

#ifdef IO_CORE_DEBUG
	printf("[%s] Wait flush end\n", __FUNCTION__);
#endif
}

void* CHECK_WRITE_BUFFER(int core_id, uint64_t lba, uint32_t read_sects)
{
	int i;

	uint64_t end_lba = lba + read_sects - 1;
	uint64_t cur_start_lba;
	uint64_t cur_end_lba;

	core_req_queue* cur_w_queue;
	core_req_entry* cur_w_entry;

	for(i=0; i<N_WRITE_BUF; i++){
		cur_w_queue = &vs_core[core_id].write_queue[i];

		pthread_mutex_lock(&cur_w_queue->lock);

		cur_w_entry= cur_w_queue->head;

		while(cur_w_entry != NULL){

			cur_start_lba = cur_w_entry->sector_nb;
			cur_end_lba = cur_start_lba + cur_w_entry->length -1;

			if(cur_start_lba <= lba
				&& cur_end_lba >= end_lba){

				pthread_mutex_unlock(&cur_w_queue->lock);
				
				return cur_w_entry->buf;
			}

			cur_w_entry = cur_w_entry->next;
		}

		pthread_mutex_unlock(&cur_w_queue->lock);
	}	

	return NULL;
}


/* Because VSSIM only imposes IO delay,
 * this function does not perform data copy operation.
 * lock for the buffer should be held before calling this function.
 */
void DMA_FROM_HOST_TO_BUFFER(event_queue_entry* eq_entry, int w_buf_index)
{
	int index = 0;
	int io_type = eq_entry->io_type;
	uint32_t nlb = eq_entry->length;
	uint32_t trans_nlb = 0;
	uint32_t total_trans_nlb = 0;
	uint64_t cur_addr = 0;
	uint64_t cur_len = 0;
	uint64_t offset = 0;
	void* cur_buf = eq_entry->buf; 
	void* src;

	NvmeRequest* req = (NvmeRequest*)eq_entry->opaque;	
	QEMUSGList qsg = req->qsg;
	int nsg = qsg.nsg;

	while(index != nsg){

		/* 1. Parsing Sglist */	
		cur_addr = qsg.sg[index].base;
		cur_len = qsg.sg[index].len; // byte
	
		/* 2. Calculate the number of sectors to transfer */
		offset = cur_addr % SECTOR_SIZE;	// offset in the sector
		trans_nlb = cur_len / SECTOR_SIZE; 	// # of sectors
		if (cur_len % SECTOR_SIZE != 0)
			trans_nlb += 1;

		/* 3. Do DMA from host to device */
		src = dma_memory_map(qsg.as, cur_addr, &cur_len, DMA_DIRECTION_TO_DEVICE);
		if(!src){
			printf("ERROR[%s] dma_memory_map() fail!\n", __FUNCTION__);
			return;
		}
		cur_buf = cur_buf + offset;
		memcpy(cur_buf, src, cur_len);

		cur_buf += cur_len;

		/* 4. Release the source memory */
		dma_memory_unmap(qsg.as, src, cur_len, DMA_DIRECTION_TO_DEVICE, cur_len);

		/* 5. Increase host pointer */
		if(io_type == WRITE){
			INCREASE_WB_HOST_POINTER(trans_nlb, w_buf_index);	
		}
		else if(io_type == DISCARD){
			INCREASE_DISCARD_BUF_HOST_POINTER(trans_nlb);	
		}

		total_trans_nlb += trans_nlb;
		index++;
	}

	/* Error check */
	if(io_type == WRITE && nlb != total_trans_nlb){
		printf("ERROR[%s] DMA fail: nlb %u != trans_nlb %u\n",
			__FUNCTION__, nlb, trans_nlb);
	}
}


void BUF_FILL_ZEROS(event_queue_entry* eq_entry)
{
	int index = 0;
	int io_type = eq_entry->io_type;
	uint32_t nlb = eq_entry->length;
	uint32_t trans_nlb = 0;
	uint32_t total_trans_nlb = 0;
	uint64_t cur_addr = 0;
	uint64_t cur_len = 0;
	//uint64_t offset = 0;
	void* src;

	NvmeRequest* req = (NvmeRequest*)eq_entry->opaque;	
	QEMUSGList qsg = req->qsg;
	int nsg = qsg.nsg;

	while(index != nsg){

		/* 1. Parsing Sglist */	
		cur_addr = qsg.sg[index].base;
		cur_len = qsg.sg[index].len; // byte
	
		/* 2. Calculate the number of sectors to transfer */
		// offset = cur_addr % SECTOR_SIZE;	// offset in the sector
		trans_nlb = cur_len / SECTOR_SIZE; 	// # of sectors
		if (cur_len % SECTOR_SIZE != 0)
			trans_nlb += 1;

		/* 3. Do DMA from host to device */
		src = dma_memory_map(qsg.as, cur_addr, &cur_len, DMA_DIRECTION_TO_DEVICE);
		if(!src){
			printf("ERROR[%s] dma_memory_map() fail!\n", __FUNCTION__);
			return;
		}

		memset(src, 0, cur_len);

		/* 4. Release the source memory */
		dma_memory_unmap(qsg.as, src, cur_len, DMA_DIRECTION_TO_DEVICE, cur_len);

		total_trans_nlb += trans_nlb;
		index++;
	}

	/* Error check */
	if(io_type == WRITE && nlb != total_trans_nlb){
		printf("ERROR[%s] fail: nlb %u != trans_nlb %u\n",
			__FUNCTION__, nlb, trans_nlb);
	}
}


event_queue_entry* DEQUEUE_IO(void)
{
	/* Exception handling */
	if(e_queue->entry_nb == 0){
		printf("ERROR[%s] There is no event. \n", __FUNCTION__);
		return NULL;
	}
	else if(e_queue->entry_nb != 0 && e_queue->head == NULL){
		printf("ERROR[%s] Although there are %d entries, \
				head has NULL pointer.\n", __FUNCTION__, \
				e_queue->entry_nb);
		return NULL;
	}

	/* Get IO from the event queue header */
	event_queue_entry* eq_entry = e_queue->head;

	/* Deallocation event queue entry */
	e_queue->entry_nb--;
	if(e_queue->entry_nb == 0){
		e_queue->head = NULL;
		e_queue->tail = NULL;
	}
	else{
		e_queue->head = eq_entry->next;
	}

	/* Update last read entry pointer */
	if(eq_entry == last_read_entry){
		last_read_entry = NULL;
	}
	
	/* Initialized prev/next pointer */
	eq_entry->prev = NULL;
	eq_entry->next = NULL;

#ifdef IO_CORE_DEBUG
	printf("[%s] dequeue %lu-th event, now %d remain\n",
		__FUNCTION__, eq_entry->seq_nb, e_queue->entry_nb);
#endif

	return eq_entry;
}


void INSERT_TO_CANDIDATE_EVENT_QUEUE(event_queue_entry* new_entry)
{
	/* Get the out event queue lock */
	pthread_mutex_lock(&cq_lock);

	/* Insert new out event entry */
	if(candidate_e_queue->entry_nb == 0){
		candidate_e_queue->head = new_entry;
		candidate_e_queue->tail = new_entry;
	}
	else{
		candidate_e_queue->tail->next = new_entry;
		new_entry->prev = candidate_e_queue->tail;
		candidate_e_queue->tail = new_entry;
	}

	/* Increase the number of out event entry number */	
	candidate_e_queue->entry_nb++;
	
	/* Release the out event queue lock */
	pthread_mutex_unlock(&cq_lock);
}

/* This function should be called after cq_lock is held */
void REMOVE_FROM_CANDIDATE_EVENT_QUEUE(event_queue_entry* eq_entry)
{
	/* Re-construct the event queue */
	if(candidate_e_queue->entry_nb == 1){
		candidate_e_queue->head = NULL;
		candidate_e_queue->tail = NULL;
	}
	else if(candidate_e_queue->head == eq_entry){
		candidate_e_queue->head = eq_entry->next;
		candidate_e_queue->head->prev = NULL;
	}
	else if(candidate_e_queue->tail == eq_entry){
		candidate_e_queue->tail = eq_entry->prev;
		candidate_e_queue->tail->next = NULL;
	}
	else{
		eq_entry->prev->next = eq_entry->next;
		eq_entry->next->prev = eq_entry->prev;
	}
	
	/* Release the memory */
	free(eq_entry);

	/* Decrease the number of out event entry number */	
	candidate_e_queue->entry_nb--;
}

void ENQUEUE_IO(event_queue_entry* new_entry)
{
	int io_type = new_entry->io_type;

#ifdef IO_CORE_DEBUG
	printf("[%s] %lu-th event (io_type %d, length %u) is inserted \n",
			__FUNCTION__, new_entry->seq_nb, io_type, new_entry->length);
#endif

	/* Get the event queue lock */
	pthread_mutex_lock(&eq_lock);

	if(e_queue->entry_nb == 0){
		e_queue->head = new_entry;
		e_queue->tail = new_entry;

		if(io_type == READ)
			last_read_entry = new_entry;
	}
	else if(io_type == FLUSH){
		new_entry->next = e_queue->head;
		e_queue->head = new_entry;
	}
	else{
		/* Enqueue new read entry:
		 * - Read requests have higher priority than write request
		 */
		if(io_type == READ){
			/* If there is no read event */
			if(last_read_entry == NULL){
				new_entry->next = e_queue->head;
				e_queue->head->prev = new_entry;
				e_queue->head = new_entry;
			}
			/* If there is a read event */
			else{
				if(last_read_entry == e_queue->tail){
					e_queue->tail->next = new_entry;
					new_entry->prev = e_queue->tail;
					e_queue->tail = new_entry;
				}
				else{
					new_entry->next = last_read_entry->next;
					last_read_entry->next->prev = new_entry;
					last_read_entry->next = new_entry;
					new_entry->prev = last_read_entry;
				}
			}
			last_read_entry = new_entry;
		}
		/* Enqueue new write or new discard entry */
		else if(io_type == WRITE || io_type == DISCARD){
			e_queue->tail->next = new_entry;
			new_entry->prev = e_queue->tail;
			e_queue->tail = new_entry;
		}
		else{
			printf("ERROR[%s] Wrong I/O type!\n", __FUNCTION__);
			return;
		}
	}

	/* Increase the number of events */
	e_queue->entry_nb++;

	/* Release the event queue lock */
	pthread_mutex_unlock(&eq_lock);
}

event_queue_entry* CREATE_NEW_EVENT(int io_type, uint64_t slba, uint32_t nlb, void* opaque, CallbackFunc *cb)
{
	static uint64_t seq_nb = 0;

	/* Allocate new event entry */
	event_queue_entry* new_eq_entry = calloc(1, sizeof(event_queue_entry));
	if(new_eq_entry == NULL){
		printf("[%s] Allocation new event fail.\n", __FUNCTION__);
		return NULL;
	}

	/* Allocate sequence number for this event */
	new_eq_entry->seq_nb = seq_nb;
	seq_nb++;

	/* Initialize new event */
	new_eq_entry->io_type = io_type;
	new_eq_entry->valid = VALID;
	new_eq_entry->sector_nb = slba;
	new_eq_entry->length = nlb;
	new_eq_entry->cb = cb;
	new_eq_entry->opaque = opaque;
	new_eq_entry->buf = NULL;
	new_eq_entry->n_child = 0;
	new_eq_entry->n_completed = 0;
	new_eq_entry->n_trimmed = 0;
	new_eq_entry->e_state = WAIT_CHILD;

	pthread_mutex_init(&new_eq_entry->lock, NULL);
	new_eq_entry->flush = false;

	new_eq_entry->t_start = get_usec();
	new_eq_entry->n_pages = 0;

	new_eq_entry->prev = NULL;
	new_eq_entry->next = NULL;

	return new_eq_entry;
}

/* This function should be called after eq_entry->lock is already held. */
void UPDATE_EVENT_STATE(event_queue_entry* eq_entry, enum event_state state)
{
	enum event_state old_state = eq_entry->e_state;

	if(old_state == state){
		printf("ERROR[%s] %lu-th event (%d io_type, f %d, nch %d, ncp %d, nt %d) is already in the state %d\n", 
				__FUNCTION__, eq_entry->seq_nb, eq_entry->io_type, 
				eq_entry->flush, 
				eq_entry->n_child,
				eq_entry->n_completed,
				eq_entry->n_trimmed,
				old_state);
		return;
	}

	eq_entry->e_state = state;
}


int GET_EVENT_STATE(event_queue_entry* eq_entry)
{
	enum event_state e_state;

	pthread_mutex_lock(&eq_entry->lock);
	e_state = eq_entry->e_state;
	pthread_mutex_unlock(&eq_entry->lock);

	return e_state;
}

int GET_N_IO_PAGES(uint64_t sector_nb, uint32_t length)
{
	uint32_t remains = length;
	uint32_t sects = 0;
	uint64_t left_skip = sector_nb % SECTORS_PER_PAGE;
	uint64_t right_skip = 0;
	uint32_t n_pages = 0;

	while(remains > 0){
		
		if(remains > SECTORS_PER_PAGE - left_skip){
			right_skip = 0;
		}
		else{
			right_skip = SECTORS_PER_PAGE - left_skip - remains;
		}
		sects = SECTORS_PER_PAGE - left_skip - right_skip;

		/* Update the local variables */
		n_pages++;
		remains -= sects;
		left_skip = 0;
	}

	return n_pages;
}

int GET_WRITE_BUFFER_TO_FLUSH(int core_id, int* w_buf_index)
{
	int64_t t_now = get_usec();

	int i = 0;
	int ret_index = *w_buf_index;

	do{
		pthread_mutex_lock(&vssim_w_buf[ret_index].lock);		

		if(vs_core[core_id].write_queue[ret_index].entry_nb != 0 
				&& (vssim_w_buf[ret_index].is_full == 1
				|| (t_now - vssim_w_buf[ret_index].t_last_flush 
				> FLUSH_TIMEOUT_USEC))){

#ifdef IO_CORE_DEBUG
			printf("[%s] core %d: decide to flush %d write buffer: %d %d %ld\n",
					__FUNCTION__, core_id, ret_index,
					vssim_w_buf[ret_index].n_empty_sectors,
					vssim_w_buf[ret_index].is_full,
					t_now - vssim_w_buf[ret_index].t_last_flush);
#endif

			*w_buf_index = ret_index;

			vssim_w_buf[ret_index].is_full = 1;

			pthread_mutex_unlock(&vssim_w_buf[ret_index].lock);

			return SUCCESS;
		}

		pthread_mutex_unlock(&vssim_w_buf[ret_index].lock);

		i++;
		ret_index++;
		if(ret_index == N_WRITE_BUF){
			ret_index = 0;
		}	

	}while(i != N_WRITE_BUF);

#ifdef IO_CORE_DEBUG
	printf("[%s] core %d: There is no write buffer to flush\n",
				__FUNCTION__, core_id);
#endif
	return FAIL;
}

void DO_PER_CORE_READ(int core_id)
{
	int ret;

	core_req_entry* cr_entry;
	core_req_queue* r_queue = &vs_core[core_id].read_queue;

	pthread_mutex_lock(&r_queue->lock);

	/* Dequeue the read request */
	cr_entry = r_queue->head;
	r_queue->head = cr_entry->next;
	r_queue->entry_nb--;

	/* Init per-core read queue */
	if(r_queue->entry_nb == 0){
		r_queue->head = NULL;
		r_queue->tail = NULL;
	}

#ifdef IO_CORE_DEBUG
	printf("[%s] core %d: %lu-th event dequeue\n",
			__FUNCTION__, core_id, cr_entry->parent->seq_nb);
#endif

	pthread_mutex_unlock(&r_queue->lock);

	/* Read data from Flash memory */
	ret = FTL_READ(core_id, cr_entry->sector_nb, cr_entry->length);
	
	/* If this request is for the trimmed data */
	if(ret == TRIMMED){
		cr_entry->is_trimmed = true;
	}

	/* post processing */
	END_PER_CORE_READ_REQUEST(cr_entry);
}

void DO_PER_CORE_DISCARD(int core_id)
{
	core_req_entry* cr_entry;
	core_req_queue* discard_queue = &vs_core[core_id].discard_queue;

	pthread_mutex_lock(&discard_queue->lock);

	/* Dequeue the discard request */
	cr_entry = discard_queue->head;
	discard_queue->head = cr_entry->next;
	discard_queue->entry_nb--;

	/* Init per-core discard queue */
	if(discard_queue->entry_nb == 0){
		discard_queue->head = NULL;
		discard_queue->tail = NULL;
	}

	pthread_mutex_unlock(&discard_queue->lock);

	/* Discard LBAs */
	FTL_DISCARD(core_id, cr_entry->sector_nb, cr_entry->length);

	/* post processing */
	END_PER_CORE_DISCARD_REQUEST(cr_entry);
}

void FLUSH_WRITE_BUFFER(int core_id, int w_buf_index)
{
	int n_total_pages = 0;
	core_req_entry* cr_entry;
	core_req_queue* cur_w_queue = &vs_core[core_id].write_queue[w_buf_index];

#ifdef GET_WB_LAT_INFO
	int64_t wb_lat_start = get_usec();
	int64_t wb_lat_end;
#endif
#ifdef GET_CH_UTIL_INFO
	int n_ch_util = 0;	
	double ch_util = 0;	
#endif

	pthread_mutex_lock(&cur_w_queue->lock);

	int n_entries = cur_w_queue->entry_nb;
	int n_remain_pages = 0;
	int n_wait_pages = 0;

#ifdef DEL_FIRM_OVERHEAD
	bool first_entry = true;
	int64_t remains;
#endif

	cr_entry = cur_w_queue->head;

	while(n_entries != 0){

#ifdef IO_CORE_DEBUG
		printf("[%s] core %d: %lu-th event dequeue\n",
			__FUNCTION__, core_id, cr_entry->seq_nb);
#endif
		/* Write data to Flash memory */
		cr_entry->n_pages = FTL_WRITE(core_id, cr_entry->sector_nb, cr_entry->length);

		/* Get next cr_entry */
#ifdef IO_CORE_DEBUG
		printf("[%s] core %d: %lu-th event FTL write complete\n",
			__FUNCTION__, core_id, cr_entry->seq_nb);
#endif
		n_total_pages += cr_entry->n_pages;

		cr_entry = cr_entry->next;
		n_entries--;
	}	

	/* post processing */
	while(cur_w_queue->entry_nb != 0){

		/* Dequeue the write request */
		cr_entry = cur_w_queue->head;
		cur_w_queue->head = cr_entry->next;
		cur_w_queue->entry_nb--;

		n_wait_pages = cr_entry->n_pages;

		if(n_remain_pages != 0){
			if(cr_entry->n_pages <= n_remain_pages){
				n_wait_pages = 0;
				n_remain_pages -= cr_entry->n_pages;
			}
			else{
				n_wait_pages -= n_remain_pages;
				n_remain_pages = 0;
			}
		}

#ifdef DEL_FIRM_OVERHEAD
		/* Remove firm overhead */
		if(first_entry){
			remains = SET_FIRM_OVERHEAD(core_id, WRITE, (get_usec()-cr_entry->t_start));
		}
		else
			remains = SET_FIRM_OVERHEAD(core_id, WRITE, remains);
#endif

		/* Wait until all flash io are completed */
		if(n_wait_pages != 0)
			n_remain_pages += WAIT_FLASH_IO(core_id, WRITE, n_wait_pages);

#ifdef DEL_FIRM_OVERHEAD
		/* Recover the init delay */
		SET_FIRM_OVERHEAD(core_id, WRITE, 0);
		first_entry = false;
#endif

		END_PER_CORE_WRITE_REQUEST(cr_entry, w_buf_index);
	}

	/* Init the per core write queue */
	cur_w_queue->head = NULL;
	cur_w_queue->tail = NULL;

#ifdef GET_WB_LAT_INFO
	wb_lat_end = get_usec();
	fprintf(fp_wb_lat, "%ld\t%d\n", wb_lat_end - wb_lat_start, n_total_pages);
#endif
#ifdef GET_CH_UTIL_INFO
	if(n_total_pages > vs_core[core_id].n_channel)
		n_ch_util = vs_core[core_id].n_channel;
	else
		n_ch_util = n_total_pages;

	ch_util = (double) n_ch_util / vs_core[core_id].n_channel;

	fprintf(fp_ch_util, "W\t%d\t%d\t%d\t%lf\n", core_id, n_ch_util, n_total_pages, ch_util);
#endif

	/* If needed, perform foreground GC */
	FGGC_CHECK(core_id);

	pthread_mutex_unlock(&cur_w_queue->lock);
}


void INCREASE_RB_FTL_POINTER(uint32_t n_sectors)
{
	pthread_mutex_lock(&vssim_r_buf.lock);

	while(n_sectors != 0){
		vssim_r_buf.ftl_ptr += SECTOR_SIZE;
		vssim_r_buf.n_empty_sectors--;

		if(vssim_r_buf.ftl_ptr
				== vssim_r_buf.buffer_end){
			vssim_r_buf.ftl_ptr 
				= vssim_r_buf.addr;
		}

		n_sectors--;
	}

	pthread_mutex_unlock(&vssim_r_buf.lock);
}

void INCREASE_RB_HOST_POINTER(uint32_t n_sectors)
{
	pthread_mutex_lock(&vssim_r_buf.lock);

	while(n_sectors != 0){
		vssim_r_buf.host_ptr += SECTOR_SIZE;
		vssim_r_buf.n_empty_sectors++;

		if(vssim_r_buf.host_ptr
				== vssim_r_buf.buffer_end){
			vssim_r_buf.host_ptr 
				= vssim_r_buf.addr;
		}

		n_sectors--;
	}

	pthread_mutex_unlock(&vssim_r_buf.lock);
}


/* 
 * lock for the buffer should be held before calling this function.
 */
void INCREASE_WB_HOST_POINTER(uint32_t n_sectors, int w_buf_index)
{
	while(n_sectors != 0){
		vssim_w_buf[w_buf_index].host_ptr += SECTOR_SIZE;
		vssim_w_buf[w_buf_index].n_empty_sectors--;

		/* Notify exception */
		if(vssim_w_buf[w_buf_index].host_ptr >
				vssim_w_buf[w_buf_index].buffer_end){

			printf("ERROR[%s] %d Write buffer host pointer exceeds the buffer range! \n", __FUNCTION__, w_buf_index);
			break;
		}

		n_sectors--;
	}

	if(vssim_w_buf[w_buf_index].n_empty_sectors == 0 ||
				vssim_w_buf[w_buf_index].host_ptr 
				== vssim_w_buf[w_buf_index].buffer_end){
		vssim_w_buf[w_buf_index].is_full = 1;
	}
}


/* 
 * lock for the buffer should be held before calling this function.
 */
void INCREASE_DISCARD_BUF_FTL_POINTER(uint32_t n_sectors)
{
	while(n_sectors != 0){
		vssim_discard_buf.ftl_ptr += SECTOR_SIZE;
		vssim_discard_buf.n_empty_sectors++;

		if(vssim_discard_buf.ftl_ptr
				== vssim_discard_buf.buffer_end){
			vssim_discard_buf.ftl_ptr 
				= vssim_discard_buf.addr;
		}

		n_sectors--;
	}
}


void INCREASE_DISCARD_BUF_HOST_POINTER(uint32_t n_sectors)
{
	while(n_sectors != 0){
		vssim_discard_buf.host_ptr += SECTOR_SIZE;
		vssim_discard_buf.n_empty_sectors--;

		if(vssim_discard_buf.host_ptr
				== vssim_discard_buf.buffer_end){
			vssim_discard_buf.host_ptr 
				= vssim_discard_buf.addr;
		}

		n_sectors--;
	}
}


int COUNT_READ_EVENT(void)
{
	int count = 1;
	event_queue_entry* e_q_entry = NULL;

	if(last_read_entry == NULL){
		return 0;
	}
	else{
		e_q_entry = e_queue->head;
		while(e_q_entry != last_read_entry){
			count++;

			e_q_entry = e_q_entry->next;
		}
	}
	return count;
}
