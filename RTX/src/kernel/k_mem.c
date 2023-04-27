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

/**************************************************************************/ /**
																			  * @file        k_mem.c
																			  * @brief       Kernel Memory Management API C Code
																			  *
																			  * @version     V1.2021.01.lab2
																			  * @authors     Yiqing Huang
																			  * @date        2021 JAN
																			  *
																			  * @note        skeleton code
																			  *
																			  *****************************************************************************/

/**
 * @brief:  k_mem.c kernel API implementations, this is only a skeleton.
 * @author: Yiqing Huang
 */

#include "k_mem.h"
#include "Serial.h"
// #ifdef DEBUG_0
#include "printf.h"
// #endif  /* DEBUG_0 */

/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */
// kernel stack size, referred by startup_a9.s
const U32 g_k_stack_size = K_STACK_SIZE;
// task proc space stack size in bytes, referred by system_a9.cs
const U32 g_p_stack_size = U_STACK_SIZE;

// task kernel stacks
U32 g_k_stacks[MAX_TASKS][K_STACK_SIZE >> 2] __attribute__((aligned(8)));

// process stack for tasks in SYS mode
U32 g_p_stacks[MAX_TASKS][U_STACK_SIZE >> 2] __attribute__((aligned(8)));

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */

U32 *k_alloc_k_stack(task_t tid)
{
	return g_k_stacks[tid + 1];
}

U32 *k_alloc_p_stack(task_t tid)
{

	TCB *TCB = &g_tcbs[tid];

	void *user_space_sp = k_mem_alloc(TCB->u_stack_size);
	U32 user_space_sp_hi = (U32) user_space_sp + TCB->u_stack_size;

//	return g_p_stacks[tid + 1];
	return (U32*)user_space_sp_hi;
//	return k_
}

void traverse_linked_list(void);

struct Node
{
	unsigned int variable;
	U8 tid;
};

struct Node *head;
struct Node *furthest = NULL;

unsigned int total_bytes;
unsigned int total_bytes_used = 0;
unsigned int overhead_size;
struct Node * RAM_END_ROUNDED = (struct Node *) (RAM_END + 1);

char mem_initialized = 0;

#define MIN_SIZE 8

unsigned int round_up_nearest_eight(unsigned int num)
{
	return (num % MIN_SIZE) == 0 ? num : (num + MIN_SIZE) - (num % MIN_SIZE);
}



int k_mem_init(void)
{
	unsigned int end_addr = (unsigned int)&Image$$ZI_DATA$$ZI$$Limit; // start of the actual usable memory

	//  printf("k_mem_init: image ends at 0x%x\r\n", end_addr);
	//  printf("k_mem_init: RAM ends at 0x%x\r\n", RAM_END); // end of the actual usable memory

	total_bytes = (unsigned int) RAM_END_ROUNDED - end_addr;
	total_bytes_used = 0;

//	printf("total bytes free: %d\n", total_bytes);
	head = (struct Node *)(end_addr);

//	printf("head:0x%x\n", head);

	struct Node head_ref = {.variable = total_bytes, .tid = NULL};

	*head = head_ref;

	overhead_size = sizeof(struct Node);

	mem_initialized = 1;

	return RTX_OK;
}

void *k_mem_alloc(size_t size)
{
//	printf("k_mem_alloc: requested memory size = %d\r\n", size);

	if (size == 0 || mem_initialized == 0)
	{
		printf("k_mem_alloc returned NULL\n");
		return NULL;
	}

	size = round_up_nearest_eight(size);

	size_t actual_size_needed = size + overhead_size;

	//	printf("rounded: %d\n", size);

	// 	printf("overhead size: %d\n", overhead_size);

	if (total_bytes_used + size + overhead_size > total_bytes)
	{
		// no more bytes left
//		printf("size: %d, overhead_size: %d\n", size, overhead_size);
//		 printf("No space\n");
		return NULL;
	}

	struct Node *curr = head;
	while (curr < RAM_END_ROUNDED)
	{
//		printf("At 1\n");
		size_t block_size = curr->variable & 0x7FFFFFFF; // AND function with the right 31 bits set to 1
		char allocated = curr->variable >> 31;			 // allocated if the left most bit, 1 or 0
		struct Node *next = (struct Node *)((char *)curr + block_size);
		//printf("current:0x%x\nblock_size:%d\n", curr, block_size);
//		printf("At 2\n");
		if (allocated == 0 && ((actual_size_needed + overhead_size <= block_size ||	(block_size == actual_size_needed))))
		{ // If there is space for the allocation
			struct Node *new_alloc_block;
			new_alloc_block = (struct Node *)((char *)curr);
//			printf("At 3\n");
			// set the 1st bit to 1, and rest of the 31 bits to the size of the block
			struct Node new_alloc_block_ref = {.variable = (1U << 31) | (0x7FFFFFFF & (actual_size_needed)), .tid = gp_current_task->tid};

			// add new node
			if ((char *)curr + actual_size_needed <= ((char *)(RAM_END_ROUNDED)) - (overhead_size + MIN_SIZE)
				&& block_size - actual_size_needed > MIN_SIZE + overhead_size)
			{
//				printf("At 4\n");
				struct Node *new_free_block;
				new_free_block = (struct Node *)((char *)curr + actual_size_needed);


				// block size is set to a value with size and overhead size added up
				new_free_block->variable = (0 << 31) | (block_size - actual_size_needed);
				new_free_block->tid = NULL;

//				if (head == curr)
//				{
//					head = new_free_block;
////					 printf("new head at 0x%x\n", head);
//				}
			}
//			printf("At 5\n");
			*new_alloc_block = new_alloc_block_ref;

//			 printf("allocated at 0x%x! +overhead=0x%x\n", curr, (char *)curr + overhead_size);
			// printf("new_free_block: 0x%x\n", new_free_block);
			total_bytes_used += actual_size_needed;

//			if (total_bytes_used == total_bytes){
//				head = RAM_END_ROUNDED;
////				printf("new head at 0x%x\n", head);
//			}
//			head = total_bytes_used == total_bytes ? RAM_END_ROUNDED : head;

//			 printf("total bytes used: %d/%d\n", total_bytes_used, total_bytes);
			// printf("success\n");

//			traverse_linked_list();

			return (char *)curr + overhead_size;
//			printf("At 6\n");
		}
		//printf("Curr: %x\n", curr);
		curr = next;
		//printf("Curr After: %x\n", curr);
//		printf("At 7\n");
	}


//	printf("sdfsdf\n");

	return NULL;
}



int k_mem_dealloc(void *ptr)
{
//	printf("k_mem_dealloc: freeing 0x%x\r\n", (U32) ptr);

	struct Node *to_be_deallocated = (struct Node *)((char *)ptr - overhead_size); // points to the start of the block
	unsigned int end_addr = (unsigned int)&Image$$ZI_DATA$$ZI$$Limit;

	if (ptr == NULL || (int)to_be_deallocated < end_addr || (int)to_be_deallocated > RAM_END)
	{
//		printf("dealloc error\n");
		return RTX_ERR;
	}




	char to_be_deallocated_allocated = to_be_deallocated->variable >> 31;
	if (to_be_deallocated_allocated != 1 || to_be_deallocated->tid != gp_current_task->tid)
	{
//		printf("dealloc a\n");
		return RTX_ERR;
	}

//	 printf("node=0x%0x\n", to_be_deallocated);

	size_t to_be_deallocated_block_size = to_be_deallocated->variable & 0x7FFFFFFF;

	struct Node *to_be_deallocated_next = (struct Node *)((char *)to_be_deallocated + to_be_deallocated_block_size);

	total_bytes_used -= to_be_deallocated_block_size;
//	printf("total bytes used: %d\n", total_bytes_used);
	// add node

	struct Node *new_free_block;
	new_free_block = (struct Node *)(to_be_deallocated);
	struct Node new_free_block_ref = {.variable = (0 << 31) | ((to_be_deallocated_block_size)), .tid = NULL};
//		printf("dealloc next: 0x%x\n", to_be_deallocated_next);
	if (to_be_deallocated_next < RAM_END_ROUNDED && to_be_deallocated_next->variable >> 31 == 0)
	{ // if the next node is deallocated as well we have to merge that
//			 printf("merge next\n");
		size_t to_be_deallocated_next_block_size = to_be_deallocated_next->variable & 0x7FFFFFFF;
		new_free_block_ref.variable = (0 << 31) | ((to_be_deallocated_block_size + to_be_deallocated_next_block_size));
		new_free_block_ref.tid = NULL;
	}
//		printf("sdfsdfsdf\n");
	*new_free_block = new_free_block_ref;
	to_be_deallocated_block_size = new_free_block->variable & 0x7FFFFFFF;

	struct Node *curr = head;

//		printf("head: 0x%x\n", head);

	while (curr < to_be_deallocated)
	{ // while loop until you want to reach the previous node to what we want to deallocate

		size_t block_size = curr->variable & 0x7FFFFFFF;
		char allocated = curr->variable >> 31;
		struct Node *next = (struct Node *)((char *)curr + block_size);
//		printf("curr: %x, block size: %d, next: %x\n", curr, block_size, next);

		if (block_size == 0){
			printf("====block====\n");
			traverse_linked_list();
//			printf("====\n");
			break;
		}

		if (allocated == 0 && next == to_be_deallocated) // if the next node IS the node to be deallocated, merge to the current node
		{
			struct Node *new_free_block2;
			new_free_block2 = (struct Node *)(curr);
			struct Node new_free_block_ref2 = {.variable = (0 << 31) | ((block_size + to_be_deallocated_block_size)), .tid = NULL};

			*new_free_block2 = new_free_block_ref2;
//			printf("merge prev\n");
			return RTX_OK;
		}

		curr = next;
	}

//	traverse_linked_list();
//	printf("deallocedaaa\n");
	return RTX_OK;

}

// for debugging purposes, not used in the actual function
void traverse_linked_list(void)
{
	unsigned int end_addr = (unsigned int)&Image$$ZI_DATA$$ZI$$Limit;
	struct Node *curr = (struct Node *)end_addr;
	while (curr < RAM_END_ROUNDED)
	{
		size_t block_size = curr->variable & 0x7FFFFFFF;
		char allocated = curr->variable >> 31;
		struct Node *next = (struct Node *)((char *)curr + block_size);

		printf("current: 0x%x, block size: %d, next: 0x%x, allocated:%d\n", curr, block_size, next, allocated);

		curr = next;
	}
}

int k_mem_count_extfrag(size_t size)
{
	unsigned int memory_region_counter = 0;

	struct Node *curr = head;
	while (curr < RAM_END_ROUNDED)
	{
		size_t block_size = curr->variable & 0x7FFFFFFF;
		char allocated = curr->variable >> 31;
		struct Node *next = (struct Node *)((char *)curr + block_size);

		if (allocated == 0 && block_size < size)
		{
			memory_region_counter++;
		}

		curr = next;
	}

	return memory_region_counter;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
