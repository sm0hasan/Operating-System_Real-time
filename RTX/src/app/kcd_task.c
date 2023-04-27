/* The KCD Task Template File */

#include "rtx.h"
#include "printf.h"
#include "Serial.h"

struct LLNode {
	struct LLNode *next;
	char chr;
};

struct LLNode *head_kcd = NULL;
struct LLNode *tail_kcd = NULL;
size_t num_keys = 0;

void insert_key(char d){
	struct LLNode *new = (struct LLNode *)(mem_alloc(sizeof(struct LLNode)));
	new->chr = d;

	if (head_kcd == NULL){
		head_kcd = new;
		tail_kcd = new;
		new->next = NULL;
	} else {
		tail_kcd->next = new;
		tail_kcd = new;
		tail_kcd->next = NULL;
	}

	num_keys++;
}

char remove_key(){
	struct LLNode *tmp = head_kcd;
	char ret = head_kcd->chr;
	head_kcd = head_kcd->next;
	mem_dealloc(tmp);

	num_keys--;

	return ret;
}

void print_keys(){
	struct LLNode *curr = head_kcd;

	while (curr != NULL){

		printf("%c\n", curr->chr);

		curr = curr->next;
	}
}

char commands[128];
task_t owners[128];

RTX_TASK_INFO info;
char identifier;

void kcd_task(void)
{

	for (int i=0;i<128;i++){
		commands[i] = '\0';
		owners[i] = 0;
	}


	mbx_create(KCD_MBX_SIZE);
	size_t recv_buf_size = sizeof(RTX_MSG_HDR) + sizeof(char);

	RTX_MSG_HDR *recv_buf = (RTX_MSG_HDR *)(mem_alloc(recv_buf_size));
	task_t recv_tid;

//	print_queue();

	while (1){
		recv_msg(&recv_tid, recv_buf, recv_buf_size);

		if (recv_buf->type == KCD_REG){
			if (recv_buf->length > sizeof(RTX_MSG_HDR) + sizeof(char)){
				continue;
			}

			char recv_chr = *((char *)recv_buf + sizeof(RTX_MSG_HDR));

			commands[recv_chr] = recv_chr;
			owners[recv_chr] = recv_tid;


		} else if (recv_buf->type == KEY_IN){
			char recv_chr = *((char *)recv_buf + sizeof(RTX_MSG_HDR));

			if (recv_chr != 13){
				insert_key(recv_chr);
			} else {
//				char *msg2 = mem_alloc(sizeof(RTX_MSG_HDR) + num_keys);
				RTX_MSG_HDR *msg = (RTX_MSG_HDR *)(mem_alloc(sizeof(RTX_MSG_HDR) + num_keys - 1));
				msg->length = sizeof(RTX_MSG_HDR) + num_keys - 1;
				msg->type = KCD_CMD;
				size_t command_length = num_keys;

				char percent_sign = remove_key();

				identifier = remove_key();
				*((char *)msg + sizeof(RTX_MSG_HDR)) = identifier;

				for (int i=1;i<command_length-1;i++){
					*((char *)msg + sizeof(RTX_MSG_HDR) + i) = remove_key();
				}

				if (percent_sign != '%' || command_length > 64){
					SER_PutStr(0, "Invalid Command\n");
					continue;
				}

				tsk_get_info(owners[identifier], &info);
//				printf("%d,%d\n", commands[identifier], info.state);
				if (commands[identifier] == '\0' || info.state == DORMANT){
					SER_PutStr(0, "Command cannot be processed\n");
					continue;
				}
				// receiver todo
				send_msg(owners[identifier], msg);

				mem_dealloc(msg);
			}
		}
	}

	mem_dealloc(recv_buf);
}
