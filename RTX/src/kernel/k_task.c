/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTOS LAB
 *
 *                     Copyright 2020-2021 Yiqing Huang
 *                          All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice and the following disclaimer.
 *
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 ****************************************************************************
 */

/**************************************************************************//**
 * @file        k_task.c
 * @brief       task management C file
 *              l2
 * @version     V1.2021.01
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @attention   assumes NO HARDWARE INTERRUPTS
 * @details     The starter code shows one way of implementing context switching.
 *              The code only has minimal sanity check.
 *              There is no stack overflow check.
 *              The implementation assumes only two simple privileged task and
 *              NO HARDWARE INTERRUPTS.
 *              The purpose is to show how context switch could be done
 *              under stated assumptions.
 *              These assumptions are not true in the required RTX Project!!!
 *              Understand the assumptions and the limitations of the code before
 *              using the code piece in your own project!!!
 *
 *****************************************************************************/

//#include "VE_A9_MP.h"
#include "Serial.h"
#include "k_task.h"
#include "k_mem.h"
#include "k_rtx.h"

//#ifdef DEBUG_0
#include "printf.h"
//#endif /* DEBUG_0 */

/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */

TCB             *gp_current_task = NULL;	// the current RUNNING task
TCB             g_tcbs[MAX_TASKS];			// an array of TCBs
RTX_TASK_INFO   g_null_task_info;			// The null task info
U32             g_num_active_tasks = 0;		// number of non-dormant tasks

/*---------------------------------------------------------------------------
The memory map of the OS image may look like the following:

                       RAM_END+---------------------------+ High Address
                              |                           |
                              |                           |
                              |    Free memory space      |
                              |   (user space stacks      |
                              |         + heap)           |
                              |                           |
                              |                           |
                              |                           |
 &Image$$ZI_DATA$$ZI$$Limit-->|---------------------------|-----+-----
                              |         ......            |     ^
                              |---------------------------|     |
                              |      U_STACK_SIZE         |     |
             g_p_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |  other kernel proc stacks |     |
                              |---------------------------|     |
                              |      U_STACK_SIZE         |  OS Image
              g_p_stacks[2]-->|---------------------------|     |
                              |      U_STACK_SIZE         |     |
              g_p_stacks[1]-->|---------------------------|     |
                              |      U_STACK_SIZE         |     |
              g_p_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |                           |  OS Image
                              |---------------------------|     |
                              |      K_STACK_SIZE         |     |                
             g_k_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |     other kernel stacks   |     |                              
                              |---------------------------|     |
                              |      K_STACK_SIZE         |  OS Image
              g_k_stacks[2]-->|---------------------------|     |
                              |      K_STACK_SIZE         |     |                      
              g_k_stacks[1]-->|---------------------------|     |
                              |      K_STACK_SIZE         |     |
              g_k_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |---------------------------|     |
                              |        TCBs               |  OS Image
                      g_tcbs->|---------------------------|     |
                              |        global vars        |     |
                              |---------------------------|     |
                              |                           |     |          
                              |                           |     |
                              |                           |     |
                              |                           |     V
                     RAM_START+---------------------------+ Low Address
    
---------------------------------------------------------------------------*/ 

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */

/**************************************************************************//**
 * @brief   scheduler, pick the TCB of the next to run task
 *
 * @return  TCB pointer of the next to run task
 * @post    gp_curret_task is updated
 *
 *****************************************************************************/




TCB *g_head = NULL;

TCB *active_task;
TCB *ready_queue[MAX_TASKS];
int queue_size = 0;
int front = 0;
int back = -1;

void print_queue(void){
	//printf("inside print queue\n");
	printf("====\n");
	TCB *curr = g_head;

	while (curr != NULL){
		TCB *TCB = curr;

		printf("tid: %d, prio: %d, state: %d\n", TCB->tid, TCB->prio, TCB->state);

		curr = curr->next;
	}
	printf("====\n");
	//printf("exited print queue\n");
}

int insert_queue(TCB *d){
	//printf("inserting 0x%x\n", d);

	if (d == NULL){
		return RTX_ERR;
	}

	// if there's nothing in the queue or d has higher priority (lower value) than what's already in the queue
	if (g_head == NULL || (g_head != NULL && d->prio < g_head->prio)){
//		//printf("%x, %x, %d, %d\n",g_head, tmp, d->prio, g_head->TCB->prio);
		d->next = g_head;
		g_head = d;
	} else { // if d has lower priority than the head
		TCB *curr = g_head;
		while (curr != NULL){
			//
			if (curr->next != NULL && d->prio < curr->next->prio){
				//printf("a\n");
				d->next = curr->next;
				curr->next = d;

				break;
			} else if (curr->next == NULL){ // if its at the very end
				//printf("sdfsdf\n");
				d->next = curr->next;
				curr->next = d;

				break;
			}

			curr = curr->next;
		}
	}

	// print_queue();

	return RTX_OK;
}

TCB *remove_queue(void){
	//printf("removing 0x%x\n", g_head);
	TCB *front_item = g_head;
	g_head = front_item->next;
	front_item -> next = NULL;

	return front_item;
}

void remove_queue_specific_tcb(task_t tid){
	TCB *curr = g_head;
	TCB *prev = NULL;
	while (curr != NULL){
		if (curr->tid == tid){
			if (prev == NULL){
				g_head = curr->next;
			} else {
				prev->next = curr->next;
			}
			////printf("%d\n", tid);

			break;
		}

		prev = curr;
		curr = curr->next;
	}
//	print_queue();
}

TCB *scheduler(void)
{
//        task_t tid = gp_current_task->tid;
//        return &g_tcbs[(++tid)%g_num_active_tasks];

    // if g_head has something, return its TCB
	//printf("in scheduler\n");
	return (g_head != NULL ? g_head : &g_tcbs[0]);
}



/**************************************************************************//**
 * @brief       initialize all boot-time tasks in the system,
 *
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       task_info   boot-time task information structure pointer
 * @param       num_tasks   boot-time number of tasks
 * @pre         memory has been properly initialized
 * @post        none
 *
 * @see         k_tsk_create_new
 *****************************************************************************/

extern void kcd_task(void);

int k_tsk_init(RTX_TASK_INFO *task_info, int num_tasks)
{

	for (int i=1;i<MAX_TASKS;i++){
		g_tcbs[i].state = DORMANT;
	}

    extern U32 SVC_RESTORE;

    RTX_TASK_INFO *p_taskinfo = &g_null_task_info;
    g_num_active_tasks = 0;

    if (num_tasks > MAX_TASKS) {
    	return RTX_ERR;
    }

    // create the first task
    TCB *p_tcb = &g_tcbs[0];
    p_tcb->prio     = PRIO_NULL;
    p_tcb->priv     = 1;
    p_tcb->tid      = TID_NULL;
    p_tcb->state    = RUNNING;
    p_tcb->mbx		= NULL;
    p_tcb->mbx_size = 0;
    p_tcb->head		= NULL;
    p_tcb->tail 	= NULL;

    g_num_active_tasks++;
    gp_current_task = p_tcb;

    // create the rest of the tasks
    p_taskinfo = task_info;
    for ( int i = 0; i < num_tasks; i++ ) {
        TCB *p_tcb = p_taskinfo->ptask != &kcd_task ? &g_tcbs[i+1] : &g_tcbs[TID_KCD];
        if (k_tsk_create_new(p_taskinfo, p_tcb, (p_taskinfo->ptask != &kcd_task ? i+1 : TID_KCD)) == RTX_OK) {
            insert_queue(p_tcb);
        	g_num_active_tasks++;
        }
        p_taskinfo++;
    }
    return RTX_OK;
}
/**************************************************************************//**
 * @brief       initialize a new task in the system,
 *              one dummy kernel stack frame, one dummy user stack frame
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       p_taskinfo  task information structure pointer
 * @param       p_tcb       the tcb the task is assigned to
 * @param       tid         the tid the task is assigned to
 *
 * @details     From bottom of the stack,
 *              we have user initial context (xPSR, PC, SP_USR, uR0-uR12)
 *              then we stack up the kernel initial context (kLR, kR0-kR12)
 *              The PC is the entry point of the user task
 *              The kLR is set to SVC_RESTORE
 *              30 registers in total
 *
 *****************************************************************************/
int k_tsk_create_new(RTX_TASK_INFO *p_taskinfo, TCB *p_tcb, task_t tid)
{
    extern U32 SVC_RESTORE;

    U32 *sp;

    if (p_taskinfo == NULL || p_tcb == NULL)
    {
        return RTX_ERR;
    }

    p_tcb->tid = tid;
    p_tcb->state = READY;
    p_tcb->prio = p_taskinfo->prio;
    p_tcb->priv = p_taskinfo->priv;
    p_tcb->u_stack_size = p_taskinfo->u_stack_size;
    p_tcb->k_stack_size = p_taskinfo->k_stack_size;
    p_tcb->ptask = p_taskinfo->ptask;
    p_tcb->mbx_size = p_taskinfo->rt_mbx_size;
    p_tcb->mbx = NULL;

    /*---------------------------------------------------------------
     *  Step1: allocate kernel stack for the task
     *         stacks grows down, stack base is at the high address
     * -------------------------------------------------------------*/

    ///////sp = g_k_stacks[tid] + (K_STACK_SIZE >> 2) ;
    sp = k_alloc_k_stack(tid);

    // 8B stack alignment adjustment
    if ((U32)sp & 0x04) {   // if sp not 8B aligned, then it must be 4B aligned
        sp--;               // adjust it to 8B aligned
    }

    /*-------------------------------------------------------------------
     *  Step2: create task's user/sys mode initial context on the kernel stack.
     *         fabricate the stack so that the stack looks like that
     *         task executed and entered kernel from the SVC handler
     *         hence had the user/sys mode context saved on the kernel stack.
     *         This fabrication allows the task to return
     *         to SVC_Handler before its execution.
     *
     *         16 registers listed in push order
     *         <xPSR, PC, uSP, uR12, uR11, ...., uR0>
     * -------------------------------------------------------------*/

    // if kernel task runs under SVC mode, then no need to create user context stack frame for SVC handler entering
    // since we never enter from SVC handler in this case
    // uSP: initial user stack
    if ( p_taskinfo->priv == 0 ) { // unprivileged task
        // xPSR: Initial Processor State
        *(--sp) = INIT_CPSR_USER;
        // PC contains the entry point of the user/privileged task
        *(--sp) = (U32) (p_taskinfo->ptask);

        //********************************************************************//
        //*** allocate user stack from the user space, not implemented yet ***//
        //********************************************************************//
        *(--sp) = (U32) k_alloc_p_stack(tid);

        if (sp == NULL) {
        	return RTX_ERR;
        }


        p_tcb->u_stack_hi = *sp;

        // uR12, uR11, ..., uR0
        for ( int j = 0; j < 13; j++ ) {
            *(--sp) = 0x0;
        }
    }


    /*---------------------------------------------------------------
     *  Step3: create task kernel initial context on kernel stack
     *
     *         14 registers listed in push order
     *         <kLR, kR0-kR12>
     * -------------------------------------------------------------*/
    if ( p_taskinfo->priv == 0 ) {
        // user thread LR: return to the SVC handler
        *(--sp) = (U32) (&SVC_RESTORE);
    } else {
        // kernel thread LR: return to the entry point of the task
        *(--sp) = (U32) (p_taskinfo->ptask);
    }

    // kernel stack R0 - R12, 13 registers
    for ( int j = 0; j < 13; j++) {
        *(--sp) = 0x0;
    }

    // kernel stack CPSR
    *(--sp) = (U32) INIT_CPSR_SVC;
    p_tcb->ksp = sp;

    return RTX_OK;
}

/**************************************************************************//**
 * @brief       switching kernel stacks of two TCBs
 * @param:      p_tcb_old, the old tcb that was in RUNNING
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task is pointing to a valid TCB
 *              gp_current_task->state = RUNNING
 *              gp_crrent_task != p_tcb_old
 *              p_tcb_old == NULL or p_tcb_old->state updated
 * @note:       caller must ensure the pre-conditions are met before calling.
 *              the function does not check the pre-condition!
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 *****************************************************************************/
__asm void k_tsk_switch(TCB *p_tcb_old)
{
        PUSH    {R0-R12, LR}
        MRS 	R1, CPSR
        PUSH 	{R1}
        STR     SP, [R0, #TCB_KSP_OFFSET]   ; save SP to p_old_tcb->ksp
        LDR     R1, =__cpp(&gp_current_task);
        LDR     R2, [R1]
        LDR     SP, [R2, #TCB_KSP_OFFSET]   ; restore ksp of the gp_current_task
        POP		{R0}
        MSR		CPSR_cxsf, R0
        POP     {R0-R12, PC}
}


/**************************************************************************//**
 * @brief       run a new thread. The caller becomes READY and
 *              the scheduler picks the next ready to run task.
 * @return      RTX_ERR on error and zero on success
 * @pre         gp_current_task != NULL && gp_current_task == RUNNING
 * @post        gp_current_task gets updated to next to run task
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *****************************************************************************/
int k_tsk_run_new(void)
{
    TCB *p_tcb_old = NULL;

    if (gp_current_task == NULL) {
    	return RTX_ERR;
    }

    p_tcb_old = gp_current_task;
    gp_current_task = scheduler();
    
    if ( gp_current_task == NULL  ) {
        gp_current_task = p_tcb_old;        // revert back to the old task
        return RTX_ERR;
    }

//    print_queue();

    // at this point, gp_current_task != NULL and p_tcb_old != NULL
    if (gp_current_task != p_tcb_old) {
        gp_current_task->state = RUNNING;   // change state of the to-be-switched-in  tcb
        if (p_tcb_old->state != DORMANT && p_tcb_old->state != BLK_MSG){
            p_tcb_old->state = READY;           // change state of the to-be-switched-out tcb
        }
//        print_queue();
        k_tsk_switch(p_tcb_old);            // switch stacks
    }

    return RTX_OK;
}

/**************************************************************************//**
 * @brief       yield the cpu
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task != NULL &&
 *              gp_current_task->state = RUNNING
 * @post        gp_current_task gets updated to next to run task
 * @note:       caller must ensure the pre-conditions before calling.
 *****************************************************************************/
int k_tsk_yield(void)
{

	remove_queue_specific_tcb(gp_current_task->tid);
	insert_queue(gp_current_task);

//	if (g_head != NULL && g_head->next != NULL){
//   		if (g_head->TCB->prio >= g_head->next->TCB->prio){
//   			remove_queue_specific_tcb(gp_current_task->tid);
//   			insert_queue(gp_current_task);
//   		}
//   	}

//	if (g_head != NULL && g_head->TCB->prio < gp_current_task->prio){
//		remove_queue_specific_tcb(gp_current_task->tid);
//		insert_queue(gp_current_task);
//	}

    return k_tsk_run_new();
}


/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB2
 *===========================================================================
 */

int total_stack_size_used = 0;

int k_tsk_create(task_t *task, void (*task_entry)(void), U8 prio, U16 stack_size)
{
// Task conditions
    if (task == NULL || task_entry == NULL || prio == PRIO_NULL || prio == PRIO_RT ||
        stack_size < U_STACK_SIZE || stack_size % 8 != 0 || g_num_active_tasks > MAX_TASKS)
    {
        return RTX_ERR;
    }

    int tid_to_be_assigned = -1;
    for (int i = 1; i < MAX_TASKS; i++)
    {
        if (g_tcbs[i].state == DORMANT)
        {
            tid_to_be_assigned = i;
            break;
        }
    }

    if (tid_to_be_assigned == -1)
    {
        return RTX_ERR;
    }

    TCB *TCB = &g_tcbs[tid_to_be_assigned];
    TCB->tid = tid_to_be_assigned;
    TCB->prio = prio;
    TCB->state = READY;
    TCB->priv = 0;
    TCB->ptask = task_entry;
    TCB->k_stack_size = K_STACK_SIZE;
    TCB->u_stack_size = stack_size;
    TCB->mbx		= NULL;
    TCB->mbx_size 	= 0;
    TCB->head		= NULL;
    TCB->tail 		= NULL;


    RTX_TASK_INFO task_info = {
        .ptask = task_entry,
        .k_stack_size = K_STACK_SIZE,
        .u_stack_size = stack_size,
        .tid = tid_to_be_assigned,
        .prio = prio,
        .state = READY,
        .priv = 0,
		.rt_mbx_size = 0
    };

    if (k_tsk_create_new(&task_info, TCB, tid_to_be_assigned) == RTX_ERR)
    {
    	TCB->state = DORMANT;
        return RTX_ERR;
    }

    insert_queue(TCB);
	//print_queue();

	if (prio < gp_current_task->prio){
		remove_queue_specific_tcb(gp_current_task->tid);
		insert_queue(gp_current_task);
	}

	//print_queue();

    *task = tid_to_be_assigned;

    g_num_active_tasks++;



    k_tsk_run_new();

    return RTX_OK;
}

void k_tsk_exit(void) 
{
//#ifdef DEBUG_0
    //printf("k_tsk_exit: entering...\n\r");

//	printf("gp: %x, g_head: %x\n", gp_current_task, g_head);

    gp_current_task->state = DORMANT;
    k_mem_dealloc((void *)(gp_current_task->u_stack_hi - gp_current_task->u_stack_size));

    gp_current_task->u_stack_hi = NULL;

    g_num_active_tasks--;

    if (gp_current_task->mbx != NULL){
    	k_mem_dealloc(gp_current_task->mbx);
    	gp_current_task->mbx = NULL;
    }

    remove_queue();

    //printf("before run new\n");
    k_tsk_run_new();
    //printf("after run new\n");
//#endif /* DEBUG_0 */
}


int k_tsk_set_prio(task_t task_id, U8 prio) 
{
//#ifdef DEBUG_0
    //printf("k_tsk_set_prio: entering...\n\r");
    //printf("task_id = %d, prio = %d.\n\r", task_id, prio);

    // error check
//    print_queue();


    if (!(PRIO_RT < prio || prio < PRIO_NULL) || (gp_current_task->priv == 0 && g_tcbs[task_id].priv == 1)){
    	return RTX_ERR;
    }
    // change prio
    if((g_tcbs[task_id].state != BLK_MSG) && (g_tcbs[task_id].state != DORMANT)) {
    	remove_queue_specific_tcb(task_id);
    }


    g_tcbs[task_id].prio = prio;

    if((g_tcbs[task_id].state != BLK_MSG) && (g_tcbs[task_id].state != DORMANT)) {
        insert_queue(&g_tcbs[task_id]);
    }


    // if you are not trying to change the current task's prio (Priority change I)
    if((g_tcbs[task_id].state != BLK_MSG) && (g_tcbs[task_id].state != DORMANT)) {
		if (task_id != gp_current_task->tid){
			if (prio < gp_current_task->prio){
				remove_queue_specific_tcb(gp_current_task->tid);
				insert_queue(gp_current_task);
			}
		}
		k_tsk_run_new();
    }

//    print_queue();
//    printf("k_tsk_set_prio: exiting...\n\r");
//#endif /* DEBUG_0 */
    return RTX_OK;    
}

int k_tsk_get_info(task_t task_id, RTX_TASK_INFO *buffer)
{
//#ifdef DEBUG_0
    //printf("k_tsk_get_info: entering...\n\r");
    //printf("task_id = %d, buffer = 0x%x.\n\r", task_id, buffer);
//#endif /* DEBUG_0 */
    if (buffer == NULL || task_id >= MAX_TASKS) {
        return RTX_ERR;
    }

    TCB *TCB = &g_tcbs[task_id];
    /* The code fills the buffer with some fake task information.
       You should fill the buffer with correct information    */
    buffer->tid = TCB->tid;
    buffer->prio = TCB->prio;
    buffer->state = TCB->state;
    buffer->priv = TCB->priv;
    buffer->ptask = TCB->ptask;
    buffer->k_stack_size = TCB->k_stack_size;
    buffer->u_stack_size = TCB->u_stack_size;

    //printf("completed get info \n");
    return RTX_OK;     
}

task_t k_tsk_get_tid(void)
{
//#ifdef DEBUG_0
    //printf("k_tsk_get_tid: entering...\n\r");
//#endif /* DEBUG_0 */
    return gp_current_task->tid;
}

int k_tsk_ls(task_t *buf, int count){
#ifdef DEBUG_0
    //printf("k_tsk_ls: buf=0x%x, count=%d\r\n", buf, count);
#endif /* DEBUG_0 */
    return 0;
}

/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB4
 *===========================================================================
 */

int k_tsk_create_rt(task_t *tid, TASK_RT *task)
{
    return 0;
}

void k_tsk_done_rt(void) {
#ifdef DEBUG_0
    //printf("k_tsk_done: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

void k_tsk_suspend(TIMEVAL *tv)
{
#ifdef DEBUG_0
    //printf("k_tsk_suspend: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
