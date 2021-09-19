#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include "ipc.h"
#include "pa1.h"

unsigned int process;
int in[10][10];
int out[10][10];
FILE *fp;
local_id temp;

int send(void * self, local_id dst, const Message * msg){
    if(msg->s_header.s_magic != MESSAGE_MAGIC){
        return -1;
    }else{
        write(out[temp][dst], &msg->s_header, sizeof(MessageHeader));
        write(out[temp][dst], &msg->s_payload, msg->s_header.s_payload_len);
    }
    return 0;
}

int send_multicast(void * self, const Message * msg){
    for(local_id id = 0; id < process; id++){
        send(self, id, msg);
    }
    return 0;
}

static int read_bytes(int pipe_cell, void *buf, int num_bytes) {
	int allRead = 0;
    int reBytes = num_bytes;    
    while(reBytes > 0){
        int readBytes = read(pipe_cell, ((char *)buf) + allRead, reBytes);
        if(readBytes > 0){
            reBytes -= readBytes;
            allRead += readBytes;
        }else{
            return -1;
        }
    }    
    return allRead;
}

int receive(void * self, local_id from, Message * msg){
    if(from >= process){        
        return 3;
    }
    read_bytes(in[from][temp], &msg->s_header, sizeof(MessageHeader));
    read_bytes(in[from][temp], &msg->s_payload, msg->s_header.s_payload_len);
    
    if(msg->s_header.s_magic != MESSAGE_MAGIC){
        return 2;
    }
    return 0;
}

void receive_message(local_id id_temp, unsigned int childProcess){
    for(int i = 1; i <= childProcess; i++){
        Message msg;
        if(i != id_temp){
            receive(NULL, i, &msg);
        }
    }
}

void send_message(local_id id_temp, int16_t type, const char* const log) {
    if (id_temp != PARENT_ID){
        Message msg = { .s_header = { .s_magic = MESSAGE_MAGIC, .s_type = type, }, };
        printf(log, temp, getpid(), getppid());
        fprintf(fp, log, temp, getpid());
        sprintf(msg.s_payload, log, id_temp, getpid(), getppid());
        msg.s_header.s_payload_len = strlen(msg.s_payload);
        send_multicast(NULL, &msg);
    }
}

void close_unused_pipes() {
    for (unsigned int i = 0; i < process; i++) {
        for (unsigned int j = 0; j < process; j++) {
            if (i != temp && j != temp && i != j) {
                close(out[i][j]);
                close(in[i][j]);
            }
            if (i == temp && j != temp) {
                close(in[i][j]);
            }
            if (j == temp && i != temp) {
                close(out[i][j]);
            }
        }
    }
}

int main(int argc, char** argv){
	unsigned int childProcess;
    fp = fopen("events.log", "w");
	int par;
	while((par = getopt(argc, argv, "p:?")) != -1){
		switch(par){
			case 'p':
				childProcess = strtoul(optarg, NULL, 10);
				break;
			case '?':
				printf("-p X, X is the number of the processes, range from 0 to 10!\n");
				return 1;;
			default:
				return 1;
		}
	}

	process = childProcess + 1;
    pid_t pids[11]; //to store the pids;
	//creat pipes

	for(int i = 0; i < process; i++){
		for(int j = 0; j < process; j++){
			if(i != j){
				int fd[2];
				pipe(fd);
				in[i][j] = fd[0];//read
				out[i][j] = fd[1];//write
			}
		}
	}

	pids[PARENT_ID] = getpid();
	//create processes
	for(int i = 1; i <= childProcess; i++){
		int pid = fork();
		if(pid != -1){
			if(pid == 0){
				temp = i;
				break;
			}else{
				temp = PARENT_ID;
			}
		}else{
			printf("It is error to create prcess!\n");
		}
	}
	close_unused_pipes();

    send_message(temp, STARTED, log_started_fmt);
    receive_message(temp, childProcess);
    printf(log_received_all_started_fmt, temp);
    fprintf(fp, log_received_all_started_fmt, temp);
    send_message(temp, DONE, log_done_fmt);
    printf(log_received_all_done_fmt, temp);

    if (temp == PARENT_ID) {
        for (int i = 1; i <= process; i++) {
            waitpid(pids[i], NULL, 0);
        }
    }
    fclose(fp);
    return 0;
}
