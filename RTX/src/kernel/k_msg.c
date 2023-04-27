/**
 * @file:   k_msg.c
 * @brief:  kernel message passing routines
 * @author: Yiqing Huang
 * @date:   2020/10/09
 */

#include "k_msg.h"
#include "k_mem.h"
#include "k_task.h"

//#ifdef DEBUG_0
#include "printf.h"
//#endif /* ! DEBUG_0 */

int k_mbx_create(size_t size) {
//#ifdef DEBUG_0
//    printf("k_mbx_create: size = %d\r\n", size);
//#endif /* DEBUG_0 */


    if ((gp_current_task->mbx != NULL) || (MIN_MBX_SIZE > size)){
    	return RTX_ERR;
    }

    gp_current_task->mbx = k_mem_alloc(size);

	if (gp_current_task->mbx == NULL){
		return RTX_ERR;
	}

    gp_current_task->mbx_size = size;

    gp_current_task->head = NULL;
    gp_current_task->tail = NULL;

    gp_current_task->mbx_used = 0;

    return RTX_OK;
}

task_t head_tid = 0;

int k_send_msg(task_t receiver_tid, const void *buf) {

//    printf("k_send_msg: receiver_tid = %d, buf=0x%x, length=%d\r\n", receiver_tid, buf, ((RTX_MSG_HDR *)buf)->length);
    TCB *recv_tsk = &g_tcbs[receiver_tid];

    if ((recv_tsk == NULL) ||
			(recv_tsk->state == DORMANT) ||
			recv_tsk->mbx == NULL ||
			buf == NULL ||
			((RTX_MSG_HDR *)buf)->length < MIN_MSG_SIZE ||
		recv_tsk->mbx_used + ((RTX_MSG_HDR *)(buf))->length + sizeof(SENDER_INFO) > recv_tsk->mbx_size)
	{
		return RTX_ERR;
	}

//    RTX_MSG_HDR hdr;
//    memcpy(&hdr, buf, sizeof(RTX_MSG_HDR));


    size_t empty_space_to_head = (char *)recv_tsk->head - (char *)recv_tsk->mbx;

    if ((char *)recv_tsk->mbx + recv_tsk->mbx_size - (char *)recv_tsk->tail+recv_tsk->tail->length + sizeof(SENDER_INFO) < ((RTX_MSG_HDR *)(buf))->length + sizeof(SENDER_INFO)) {

    	RTX_MSG_HDR *start = recv_tsk->head;
    	RTX_MSG_HDR *old_tail = recv_tsk->tail;

    	for (int i = 0; i < recv_tsk->mbx_used; i++){
			*((char *)recv_tsk->mbx+i) = *((char *)start+i);
		}

    	recv_tsk->head = recv_tsk->mbx;
    	recv_tsk->tail = (RTX_MSG_HDR *)((char *)old_tail - empty_space_to_head);
    }

    // first msg to be put in the task's mailbox
    if (recv_tsk->head == NULL){
    	// address to the mailbox
    	RTX_MSG_HDR *header = recv_tsk->mbx;
    	// length doesn't include sender_info
    	for (int i=0;i<((RTX_MSG_HDR *)(buf))->length;i++){
    		*((char *)header+i) = *((char *)buf+i);
    		//printf("%x\n", *((char *)header+i));
    	}

    	recv_tsk->head = header;

    	SENDER_INFO *sender_info_addr = (SENDER_INFO *)((char *)header + ((RTX_MSG_HDR *)(buf))->length);
    	SENDER_INFO sender_info_ref = {.sender_tid=gp_current_task->tid};

    	for (int i=0;i<sizeof(SENDER_INFO);i++){
			*((char *)sender_info_addr+i) = *((char *)(&sender_info_ref)+i);
		}

//    	*sender_info_addr = sender_info_ref;

    	recv_tsk->tail = header;

    	recv_tsk->mbx_used += ((RTX_MSG_HDR *)(buf))->length + sizeof(SENDER_INFO);
    } else {
    	RTX_MSG_HDR *header = (RTX_MSG_HDR *)((char *)recv_tsk->tail+recv_tsk->tail->length+sizeof(SENDER_INFO));

		for (int i=0;i<((RTX_MSG_HDR *)(buf))->length;i++){
			*((char *)header+i) = *((char *)buf+i);
			//printf("%x\n", *((char *)header+i));
		}

		recv_tsk->tail = header;

		SENDER_INFO *sender_info_addr = (SENDER_INFO *)((char *)header + ((RTX_MSG_HDR *)(buf))->length);
		SENDER_INFO sender_info_ref = {.sender_tid=gp_current_task->tid};

		for (int i=0;i<sizeof(SENDER_INFO);i++){
			*((char *)sender_info_addr+i) = *((char *)(&sender_info_ref)+i);
		}

//		*sender_info_addr = sender_info_ref;

		recv_tsk->mbx_used += ((RTX_MSG_HDR *)(buf))->length + sizeof(SENDER_INFO);
    }

    // missing wrap around
    if (recv_tsk->state == BLK_MSG){
    	recv_tsk->state = READY;
//    	printf("send1\n");
//    	print_queue();
    	insert_queue(recv_tsk);
//    	printf("send2\n");
//		print_queue();
    	g_num_active_tasks++;

    	// if recv_task has higher priority (lower number), yield the current_task
    	if (recv_tsk->prio < gp_current_task->prio){
			remove_queue_specific_tcb(gp_current_task->tid);
			insert_queue(gp_current_task);
		}

    	k_tsk_run_new();
    }

    return 0;
}

int k_recv_msg(task_t *sender_tid, void *buf, size_t len) {
//#ifdef DEBUG_0
//    printf("k_recv_msg: sender_tid  = 0x%x, buf=0x%x, len=%d\r\n", sender_tid, buf, len);
//#endif /* DEBUG_0 */

    if (gp_current_task->mbx == NULL || buf == NULL){
            return RTX_ERR;
	}

    if (gp_current_task->mbx_used == 0){
        	gp_current_task->state = BLK_MSG;
//        	printf("recv1\n");
//			print_queue();
        	remove_queue();
//        	printf("recv2\n");
//			print_queue();
        	g_num_active_tasks--;
//        	print_queue();
            k_tsk_run_new();
    }

	RTX_MSG_HDR *msg = gp_current_task->head;

	if(sender_tid != NULL) {
		SENDER_INFO *sender_tid_addr = ((SENDER_INFO *)((char *)gp_current_task->head + gp_current_task->head->length));

		for (int i=0;i<sizeof(task_t);i++){
			*((char *)sender_tid+i) = *((char *)(sender_tid_addr)+i);
		}
	}

	if (len >= msg->length){
		for (int i=0;i<msg->length;i++){
			*((char *)buf+i) = *((char *)msg+i);
//			printf("recv: %x\n", *((char *)buf+i));
		}
	}

	gp_current_task->mbx_used -= msg->length + sizeof(SENDER_INFO);

	if (gp_current_task->mbx_used == 0){
		gp_current_task->head = NULL;
		gp_current_task->tail = NULL;
	} else {
		gp_current_task->head = (RTX_MSG_HDR *)((char *)gp_current_task->head + gp_current_task->head->length + sizeof(SENDER_INFO));
	}

	if (len < msg->length){
		return RTX_ERR;
	}

	

    return RTX_OK;
}

int k_recv_msg_nb(task_t *sender_tid, void *buf, size_t len) {
#ifdef DEBUG_0
    printf("k_recv_msg_nb: sender_tid  = 0x%x, buf=0x%x, len=%d\r\n", sender_tid, buf, len);
#endif /* DEBUG_0 */
    return 0;
}

int k_mbx_ls(task_t *buf, int count) {
#ifdef DEBUG_0
    printf("k_mbx_ls: buf=0x%x, count=%d\r\n", buf, count);
#endif /* DEBUG_0 */
    return 0;
}
