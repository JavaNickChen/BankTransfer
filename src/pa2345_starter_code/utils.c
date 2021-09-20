#include "utils.h"
#include "getopt.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "ipc.h"
#include "pa2345.h"


// 'buf' stores the data which will be write to other process.
void constructMessage(int type, char *buf, ProcessDetail *pd)
{
    if(type != TRANSFER && pd->belong != PARENT_ID)
        memset(buf, 0, MAX_MESSAGE_LEN);
    Message *msg = (Message *)buf;
    MessageHeader *mHeader = (MessageHeader *)buf;
    mHeader->s_magic = MESSAGE_MAGIC;

    mHeader->s_local_time = 0;
    int8_t mHeaderLen = sizeof(MessageHeader);

    switch (type) {
        case STARTED:{
            mHeader->s_type = STARTED;
            sprintf(msg->s_payload, log_started_fmt, pd->state.s_time, pd->localID, pd->PID, pd->parentID, pd->state.s_balance);
            break;
        }
        case DONE:{
            mHeader->s_type = DONE;
            sprintf(msg->s_payload, log_done_fmt, pd->state.s_time, pd->localID, pd->state.s_balance);
            break;
        }
        case TRANSFER:{
            mHeader->s_type = TRANSFER;
            if(pd->belong == PARENT_ID){
                // the Parent Process transfer money from Csrc to Cdst process.
                TransferOrder  transferOrder = {
                        .s_src = pd->srcID,
                        .s_dst = pd->dstID,
                        .s_amount = pd->money
                };
                memcpy(msg->s_payload, &transferOrder, sizeof(TransferOrder));
                //sprintf(msg->s_payload, log_transfer_out_fmt, pd->state.s_time, pd->localID, pd->money, pd->dstID);
            }
//            else{
//
//                sprintf(msg->s_payload, log_transfer_in_fmt, pd->state.s_time, pd->dstID, pd->money, pd->srcID);
//            }
            break;
        }
        case ACK:{
            mHeader->s_type = ACK;
            sprintf(msg->s_payload, "This is an ACK-type message.");
            break;
        }
        case STOP:{
            mHeader->s_type = STOP;
            sprintf(msg->s_payload, "This is an STOP-type message.");
            break;
        }
        case BALANCE_HISTORY:{
            mHeader->s_type = BALANCE_HISTORY;
            memcpy(msg->s_payload, pd->balanceHistory, sizeof(BalanceHistory));
        }
        default:{
            sprintf(msg->s_payload, "This is filled by default.");
        }
//        case STOP:{
//
//            break;
//        }
    }
    mHeader->s_payload_len = strlen(msg->s_payload);
    int8_t messageLen = strlen(msg->s_payload) + mHeaderLen + 1;
    buf[messageLen - 1] = '\0';
}

int getProcessNums(int argc, char *argv[])
{
    int ch;
    int childProcess_nums = 0;
    if((ch = getopt(argc, argv, "p:")) != -1)
    {
        switch (ch) {
            case 'p':
                childProcess_nums = optarg[0] - '0';
                printf("%d child process will be created.\n", childProcess_nums);
                break;
            default:
                printf("No such option supported.\n");
        }
    }
    else{
        printf("Can't get the number of child process\n");
        exit(1);
    }
    return childProcess_nums;
}
void getInitBalance(char *argv[], int childProcessNums, int *initBalance)
{
    for(int i = 1; i < childProcessNums + 1; i++)
    {
        initBalance[i] = atoi(argv[i + 3]);
    }
}
