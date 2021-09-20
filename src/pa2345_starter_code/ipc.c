#include "ipc.h"
#include "unistd.h"
#include "utils.h"
#include "stdio.h"
#include "errno.h"
#include "string.h"

/* it is needed to be modified the next two functions.*/
int send(void * self, local_id dst, const Message * msg)
{
    int8_t *fd = (int8_t *)self;
    uint16_t write_len = strlen(msg->s_payload) + sizeof(msg->s_header) + 1;
    uint16_t written_len = 0;

    int tmp = 0;
    while(written_len < write_len)
    {
        tmp = write(*fd, msg + written_len, write_len - written_len);
        if(tmp == -1)
        {
            if(errno == EAGAIN){
                printf("[send] %m\n", errno);
                return 2;
            }
            else{
                printf("[send] %m\n", errno);
                return 1;
            }
        }
        written_len += tmp;
    }
    printf("[send] send %d bytes, required %d bytes.\n", written_len, write_len);
    // If success, return 0.
    return 0;
}
/*Because the function doesn't know which pipes are written (if 'EAGAIN' happens), and it's more convenient to finish all sending task.*/
int send_multicast(void * self, const Message * msg)
{

    ProcessDetail *pd = (ProcessDetail *)self;
    //printf("[send_multicast][local_id %d] start to multi send.\n", pd->localID);
    switch (msg->s_header.s_type) {
        case STARTED:printf("[local_id %d][send_multicast] send STARTED Message\n", pd->localID);break;
        case DONE:printf("[local_id %d][send_multicast] send DONE Message\n", pd->localID);break;
        case STOP:printf("[local_id %d][send_multicast] send STOP Message\n", pd->localID);break;
        case BALANCE_HISTORY:printf("[local_id %d][send_multicast] send BALANCE_HISTORY Message\n", pd->localID);break;
        case ACK:printf("[local_id %d][send_multicast] send ACK Message\n", pd->localID);break;
    }
    int isWritten[MAX_PROCESS_ID + 1]= {0};
    int tmp = 0;
    int count = 0;
    while(count < pd->childProcessNums){
        //sleep(1);
        for(int8_t i = 0; i < pd->childProcessNums + 1; i++)
        {
            if(pd->belong == i)
                continue;
            if(!isWritten[i]){
                tmp = send(&(pd->fdArrayPointer[i]), i, msg);
                if(tmp == 2)
                {
                    printf("[local_id %d][send_multicast] Pipe is full when send_multicast().Please try again later\n", pd->localID);
                    return 2;
                } else if(tmp == 1){
                    printf("[local_id %d][send_multicast] ERROR occurs when send_multicast().\n", pd->localID);
                    return 1;
                } else{
                    isWritten[i] = 1;
                    count++;
                    printf("[local_id %d][send_multicast] Success(count: %d). Send message from %d process to %d process\n", pd->localID, count, pd->localID, i);
                }
            }
        }
    }
    printf("[local_id %d][send_multicast] Success. Finish to multi send.\n", pd->localID);
    // 0 on success. Non-zero on error.
    return 0;
}

int receive(void * self, local_id from, Message * msg){
    sleep(1);
    int *fd = (int *)self;
    uint16_t recv_len = sizeof(MessageHeader);
    uint16_t recved_len = 0;
    printf("[receive] Going to get MessageHeader with fd: %d from %d, %d bytes required.\n", *fd, from, recv_len);
    int tmp = 0;
    while(recved_len < recv_len)
    {
        tmp = read(*fd, msg + recved_len, recv_len - recved_len);
        if(tmp == -1)
        {
            if(errno == EAGAIN)
                return 2; // pipe is empty, try again later.
            else
            {
                printf("ERROR: %m\n", errno);
                return 1; // error occurs when read.
            }

        }
        recved_len += tmp;
    }
    printf("[receive] Finish get messageHeader from %d. Got: %d bytes, required:%d bytes\n", from, recved_len, recv_len);
    //printf("[receive] Going to get MessageBody\n");
    recv_len = msg->s_header.s_payload_len;
    recved_len = 0;
    memset(msg->s_payload, 0, MAX_PAYLOAD_LEN);
    printf("[receive] Going to get messageBody from %d. Got: %d bytes, required:%d bytes\n", from, recved_len, recv_len);
    while(recved_len < recv_len)
    {
        // why it is not allowed to pass (msg+sizeof(MessageHeader)) instead of msg->s_payload.
        tmp = read(*fd, msg->s_payload + recved_len, recv_len - recved_len);
        //printf("[receive] tmp: %d\n", tmp);
        if(tmp == -1)
        {
            if(errno == EAGAIN)
                return 2; // pipe is empty, try again later.
            else
            {
                printf("ERROR: %m\n", errno);
                return 1; // error occurs when read.
            }
        }
        recved_len += tmp;
    }
    printf("[receive] Finish get messageBody from %d. Got: %d bytes, required: %d bytes\n", from, recved_len, recv_len);

    return 0;
}



