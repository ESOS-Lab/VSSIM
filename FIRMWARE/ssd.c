// File: ssd.c
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

#include "ssd.h"
#include "common.h"

int64_t get_usec_nvme(void)
{
        int64_t t = 0;
        struct timeval tv;
        struct timezone tz;

        gettimeofday(&tv, &tz);
        t = tv.tv_sec;
        t *= 1000000;
        t += tv.tv_usec;

        return t;
}


void SSD_INIT(void)
{
//	vssim_exit = false;
	FTL_INIT();
}


void SSD_TERM(void)
{	
	FTL_TERM();
//	vssim_exit = true;
}


event_queue_entry* SSD_NVME_READ(uint64_t slba, uint32_t nlb, NvmeRequest *req,
		void(*cb)(void *opaque, int ret))
{
	return SSD_RW(READ, slba, nlb, req, cb);
}


event_queue_entry* SSD_NVME_WRITE(uint64_t slba, uint32_t nlb, NvmeRequest *req,
		void(*cb)(void *opaque, int ret))
{
	if(nlb > N_WB_SECTORS){
		printf("ERROR[%s] the size of the write event (%u) exceeds the write buffer (%d), please increase the write buffer size in the ssd configuration \n", __FUNCTION__, nlb, N_WB_SECTORS);
		return NULL;
	}

	return SSD_RW(WRITE, slba, nlb, req, cb);
}


event_queue_entry* SSD_NVME_FLUSH(uint64_t slba, uint32_t nlb, NvmeRequest *req,
		void(*cb)(void *opaque, int ret))
{
	event_queue_entry* new_eq_entry = NULL;

	/* Create new I/O event */
	new_eq_entry = CREATE_NEW_EVENT(FLUSH, slba, nlb, (void*)req, cb);

	/* Insert new I/O event to the event queue*/
	ENQUEUE_IO(new_eq_entry);

	/* Wake up the firmware io buffer thread */
	pthread_cond_signal(&eq_ready);

	return new_eq_entry;
}


event_queue_entry* SSD_RW(int io_type, uint64_t slba, uint32_t nlb, void* opaque, CallbackFunc *cb)
{
	event_queue_entry* new_eq_entry = NULL;

	/* Create new I/O event */
	new_eq_entry = CREATE_NEW_EVENT(io_type, slba, nlb, opaque, cb);

	/* Insert new I/O event to the event queue*/
	ENQUEUE_IO(new_eq_entry);

	/* Wake up the firmware io buffer thread */
	pthread_cond_signal(&eq_ready);

	return new_eq_entry;
}


void SSD_DSM_DISCARD(NvmeRequest *req, uint32_t nr)
{
	event_queue_entry* new_eq_entry = NULL;

	/* Create new discard event */
	new_eq_entry = CREATE_NEW_EVENT(DISCARD, 0, nr, req, NULL);
	
	/* Insert new discard event to the event queue*/
	ENQUEUE_IO(new_eq_entry);

	/* Wake up the firmware io buffer thread */
	pthread_cond_signal(&eq_ready);
}


int IS_SSD_TRIM_ENABLED(void)
{
	return DSM_TRIM_ENABLE;
}


void END_SSD_NVME_RW(event_queue_entry* eq_entry)
{
	if(eq_entry == NULL)
		return;

	int io_type = eq_entry->io_type;
	enum event_state e_state = eq_entry->e_state; 	

#ifdef MONITOR_ON
	int64_t t_start = eq_entry->t_start;
	int64_t t_end = 0;
	uint32_t n_pages = eq_entry->n_pages;
#endif

	/* Wait until the vssim IO process is completed */
	
#ifdef IO_CORE_DEBUG
	printf("[%s] Check %lu-th event (io_type %d)\n", __FUNCTION__, eq_entry->seq_nb, io_type);
#endif

	while(e_state != COMPLETED){
		e_state = GET_EVENT_STATE(eq_entry);
	}	

#ifdef IO_CORE_DEBUG
	printf("[%s] Complete %lu-th event\n", __FUNCTION__, eq_entry->seq_nb);
#endif

	/* Post processing */
	if(io_type == READ){
		END_SSD_NVME_READ(eq_entry);

#ifdef MONITOR_ON
		t_end = get_usec();
		UPDATE_LOG(LOG_READ_PAGE, n_pages);
		UPDATE_BW(READ, n_pages, t_end - t_start);
#endif
#ifdef GET_RW_LAT_INFO
	fprintf(fp_rw_lat,"R\t%ld\n", t_end - t_start);
#endif
	}
	else if(io_type == WRITE){
		END_SSD_NVME_WRITE(eq_entry);

#ifdef MONITOR_ON
		t_end = get_usec();
		UPDATE_LOG(LOG_WRITE_PAGE, n_pages);
		UPDATE_BW(WRITE, n_pages, t_end - t_start);		
#endif
#ifdef GET_RW_LAT_INFO
	fprintf(fp_rw_lat,"W\t%ld\n", t_end - t_start);
#endif
	}
	else if(io_type == FLUSH){
		END_SSD_NVME_FLUSH(eq_entry);
	}

	return;
}

void END_SSD_NVME_READ(event_queue_entry* eq_entry)
{
	enum event_state e_state;
	event_queue_entry* cur_eq_entry = NULL;
	event_queue_entry* temp_eq_entry = NULL;

	if(IS_SSD_TRIM_ENABLED() 
			&& eq_entry->n_child == eq_entry->n_trimmed){

		BUF_FILL_ZEROS(eq_entry);
	}

	if(eq_entry->buf == vssim_r_buf.host_ptr){

		/* Update the host pointer of the read buffer */
		INCREASE_RB_HOST_POINTER(eq_entry->length);

		/* Get the candidate event queue lock */
		pthread_mutex_lock(&cq_lock);

		cur_eq_entry = eq_entry->next;

		while(cur_eq_entry != NULL){

			e_state = GET_EVENT_STATE(cur_eq_entry);

			/* If the entry is completed, update the host pointer
				and remove from the candidate queue */
			if(e_state == DONE_READ_DMA){

				/* Update the host pointer of the read buffer */
				INCREASE_RB_HOST_POINTER(cur_eq_entry->length);

				temp_eq_entry = cur_eq_entry;
				cur_eq_entry = cur_eq_entry->next;

				/* Remove from the candidate event queue */
				REMOVE_FROM_CANDIDATE_EVENT_QUEUE(temp_eq_entry);
			}
			else{
				break;
			}
		}

		/* Remove from the candidate event queue */
		REMOVE_FROM_CANDIDATE_EVENT_QUEUE(eq_entry);

		/* Release the candidate event queue lock */
		pthread_mutex_unlock(&cq_lock);
	}
	else{
		/* If current event is middle of the candidate event queue, 
				just update the event state and return */
		pthread_mutex_lock(&eq_entry->lock);
		UPDATE_EVENT_STATE(eq_entry, DONE_READ_DMA);
		pthread_mutex_unlock(&eq_entry->lock);
	}

	return;
}


void END_SSD_NVME_WRITE(event_queue_entry* eq_entry)
{
	if(!eq_entry->flush){
		free(eq_entry);
		return;
	}

	pthread_mutex_lock(&cq_lock);

	/* Remove from the candidate event queue */
	REMOVE_FROM_CANDIDATE_EVENT_QUEUE(eq_entry);

	pthread_mutex_unlock(&cq_lock);
	
	return;
}


void END_SSD_NVME_FLUSH(event_queue_entry* eq_entry)
{
	if(!eq_entry->flush){
		free(eq_entry);
		return;
	}

	pthread_mutex_lock(&cq_lock);

	/* Remove from the candidate event queue */
	REMOVE_FROM_CANDIDATE_EVENT_QUEUE(eq_entry);

	pthread_mutex_unlock(&cq_lock);
	
	return;
}
