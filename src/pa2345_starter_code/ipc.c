#include "ipc.h"
#include "unistd.h"
#include "utils.h"
#include "stdio.h"
#include "errno.h"

/* it is needed to be modified the next two functions.*/
int send(void * self, local_id dst, const Message * msg)
{
    int8_t *source_id = (int8_t *)self;
    int write_fd = writerFd[*source_id][dst];
    int8_t write_len = strlen(msg->s_payload) + sizeof(msg->s_header);
    int8_t written_len = 0;
    int8_t tmp = 0;
    while(written_len < write_len)
    {
        tmp = write(write_fd, msg + written_len, write_len - written_len);
        if(tmp < 0)
        {
            printf("ERROR occurs when send()\n");
            return 1;
        }
        written_len += tmp;
    }

    // If success, return 0.
    return 0;
}

int send_multicast(void * self, const Message * msg)
{
    int8_t *source_id = (int8_t *)self;

    for(int8_t i = 0; i < nums_child_process + 1; i++)
    {
        if(*source_id == i)
            continue;
        if(send(self, i, msg))
        {
            printf("ERROR occurs when send_multicast().\n");
            return 1;
        }
    }
    // 0 on success. Non-zero on error.
    return 0;
}



int receive(void * self, local_id from, Message * msg){
    int8_t *fd = (int8_t *)self;
    int8_t recv_len = sizeof(MessageHeader);
    int8_t recved_len = 0;

    int tmp = 0;
    while(recved_len < recv_len)
    {
        tmp = read(*fd, msg + recved_len, recv_len - recved_len);
        if(tmp == -1)
        {
            if(errno == EAGAIN)
                return 2; // pipe is empty, try again later.
            else
                return 1; // error occurs when read.
        }
        recved_len += tmp;
    }

    recv_len = ((Message *)msg)->s_header.s_payload_len;
    recved_len = 0;
    while(recved_len < recv_len)
    {
        tmp = read(*fd, msg + sizeof(MessageHeader) + recved_len, recv_len - recved_len);
        if(tmp == -1)
        {
            if(errno == EAGAIN)
                return 2; // pipe is empty, try again later.
            else
                return 1; // error occurs when read.
        }
        recved_len += tmp;
    }
    return 0;
}



