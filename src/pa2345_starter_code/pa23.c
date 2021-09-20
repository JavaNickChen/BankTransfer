#include "banking.h"
#include "common.h"
#include "ipc.h"
#include "pa2345.h"
#include "utils.h"

#include "stdlib.h"
#include "stdio.h"
#include "unistd.h"
#include "errno.h"
#include "fcntl.h"
#include "string.h"
#include "wait.h"

int writer[MAX_PROCESS_ID + 1][MAX_PROCESS_ID + 1];
int reader[MAX_PROCESS_ID + 1][MAX_PROCESS_ID + 1];
int LOCAL_ID = PARENT_ID;
int PID;
int parent_PID;
int childProcess_nums = 0;
int initBalance[MAX_PROCESS_ID + 1];
BalanceState nowState;
BalanceHistory balanceHistory;
AllHistory allHistory;

// type:
//      child process: STARTED, DONE
//      Parent Process:  STARTED, DONE, ACK and Balance_History
void getAllMessage(int type, int childProcessNums)
{
    sleep(1);
    printf("[local_id %d][getAllMessage] Get into getAllMessage()\n", LOCAL_ID);
    char buf[MAX_MESSAGE_LEN];
    memset(buf, 0, sizeof(buf));
    Message *msg = (Message *)buf;
    int count = 0;
    int isRead[MAX_PROCESS_ID + 1] = {0};
    int targetNums = 0;
    if(LOCAL_ID == PARENT_ID){
        targetNums = childProcessNums;
    } else{
        targetNums = childProcessNums - 1;
    }

    while(count < targetNums)
    {
        for(int i = 1; i < childProcessNums + 1; i++)
        {
            if(LOCAL_ID == i)
                continue;
            int fd = reader[LOCAL_ID][i];
            // there 0 means TRUE that successfully get data from pipe; otherwise, some exception occurs.
            if(!isRead[i]){
                int tmp = receive(&fd, i, msg);
                if(tmp == 0){ // successfully read data from pipe.
                    switch (type) {
                        case STARTED:
                        {
                            if(msg->s_header.s_type == type){
                                isRead[i] = 1;
                                count++;
                                printf("[local_id %d][getAllMessage] Success (count: %d). Get STARTED Message\n", LOCAL_ID, count);
                            }
                            break;
                        }
                        case DONE:{
                            if(msg->s_header.s_type == type){
                                isRead[i] = 1;
                                count++;
                                printf("[local_id %d][getAllMessage] Success (count: %d). Get DONE Message\n", LOCAL_ID, count);
                            }

                            break;
                        }
                        case STOP:{
                            if(msg->s_header.s_type == type){
                                isRead[i] = 1;
                                count++;
                                printf("[local_id %d][getAllMessage] Success (count: %d). Get STOP Message\n", LOCAL_ID, count);
                            }

                            break;
                        }
                        case BALANCE_HISTORY:{
                            if(msg->s_header.s_type == type){
                                isRead[i] = 1;
                                count++;
                                printf("[local_id %d][getAllMessage] Success (count: %d). Get BALANCE_HISTORY Message\n", LOCAL_ID, count);
                            }

                            allHistory.s_history[i].s_history_len = ((BalanceHistory *)(msg->s_payload))->s_history_len;
                            //allHistory.s_history[i].s_history = ((BalanceHistory *)(msg->s_payload))->s_history;
                            memcpy(allHistory.s_history[i].s_history, ((BalanceHistory *)(msg->s_payload))->s_history, sizeof(BalanceState) * ((BalanceHistory *)(msg->s_payload))->s_history_len);
                            allHistory.s_history[i].s_id = ((BalanceHistory *)(msg->s_payload))->s_id;
                            break;
                        }
                        case ACK:{
                            if(msg->s_header.s_type == type){
                                isRead[i] = 1;
                                count++;
                                printf("[local_id %d][getAllMessage] Success (count: %d). Get ACK Message\n", LOCAL_ID, count);
                            }
                        }
                    }
                } else if(tmp == 1){
                    printf("[local_id %d][getAllMessage] Read from pipe failed.\n", LOCAL_ID);
                } else if (tmp == 2){ // pipe is empty, try again later.
                    printf("[local_id %d][getAllMessage] There is no data from Process %d to Process %d now. Please wait.\n", LOCAL_ID, i, LOCAL_ID);
                }
            }
        }
        sleep(2);
    }
}

void dealWithTransferAndStop()
{
    printf("[dealWithTransferAndStop][local_id %d] Enter dealWithTransferAndStop()\n", LOCAL_ID);
    char buf[MAX_MESSAGE_LEN];
    memset(buf, 0, MAX_MESSAGE_LEN);
    Message *msg = (Message *)buf;
    int isTransferOut = 0;
    int isTransferIn = 0;
    int isStop = 0;

    while(1) {
        sleep(1);
        if(isTransferOut && isTransferIn && isStop)
            break;
        // loop for read message possibly from other child process or Parent process.
        for(int i = 0; i < childProcess_nums + 1; i++){
            if(LOCAL_ID == i)
                continue;

            memset(buf, 0, MAX_MESSAGE_LEN);
            int fd = reader[LOCAL_ID][i];
            int res = receive(&fd, i, msg);

            printf("[dealWithTransferAndStop]readFrom: %d process, fd: %d, res: %d\n", i, fd, res);
            if((res == 2) || (res == 1))
                continue;


            switch (msg->s_header.s_type) {
                case STOP :
                {
                    printf("[dealWithTransferAndStop][local_id %d] Get STOP message\n", LOCAL_ID);
                    isStop = 1;
                    break;
                }

                case TRANSFER :
                {
                    TransferOrder *transferOrder = (TransferOrder *)(msg->s_payload);
                    ProcessDetail pd = {
                            .money = transferOrder->s_amount,
                            .state = nowState,
                            .localID = LOCAL_ID,
                            .srcID = transferOrder->s_src,
                            .dstID = transferOrder->s_dst,
                            .belong = LOCAL_ID
                    };
                    if(i == PARENT_ID){
                        printf("[dealWithTransferAndStop][local_id %d] Get TRANSFER message from Parent from %d process to %d process with amount %d\n", LOCAL_ID, pd.srcID, pd.dstID, pd.money);
                        // get TRANSFER message from parent process and transfer money.
                        int balance = nowState.s_balance;
                        nowState.s_balance = balance - transferOrder->s_amount;
                        nowState.s_time = get_physical_time();

                        //constructMessage(TRANSFER, buf, &pd);
                        int writeFd = writer[LOCAL_ID][transferOrder->s_dst];
                        send(&writeFd, transferOrder->s_dst, msg);
                        isTransferOut = 1;

                    } else {
                        // get TRANSFER message from child process and send ACK message to Parent Process.
                        printf("[dealWithTransferAndStop][local_id %d] Get TRANSFER message from %d process with amount %d\n", LOCAL_ID, pd.srcID,  pd.money);
                        nowState.s_balance = nowState.s_balance + transferOrder->s_amount;
                        nowState.s_time = get_physical_time();

                        int writeFd = writer[LOCAL_ID][PARENT_ID];
                        //memset(buf, 0, sizeof(buf));
                        //constructMessage(ACK, buf, &pd);
                        msg->s_header.s_type = ACK;
                        send(&writeFd, PARENT_ID, msg);
                        isTransferIn = 1;

                    }
                    balanceHistory.s_history[balanceHistory.s_history_len] = nowState;
                    balanceHistory.s_history_len = balanceHistory.s_history_len + 1;
                }
            }
        }
    }
    printf("[dealWithTransferAndStop][local_id %d] Finish dealWithTransferAndStop()\n", LOCAL_ID);
}

void doParentWork()
{
    char buffer[MAX_MESSAGE_LEN];
    memset(buffer, 0, sizeof(buffer));
    Message *msg = (Message *)buffer;
    ProcessDetail pd = {
            .localID = LOCAL_ID,
            .childProcessNums = childProcess_nums,
            .belong = LOCAL_ID,
            .fdArrayPointer = writer[LOCAL_ID]
    };

    printf("[doParentWork][local_id %d] Finish init. start to get all STARTED message\n", LOCAL_ID);
    // 1. getAllStartedMessage();
    printf("[doParentWork][local_id %d] Going to getAll STARTED message\n", LOCAL_ID);
    getAllMessage(STARTED, childProcess_nums);
    // 2. bank_robbery();
    printf("[doParentWork][local_id %d] Going to do bank_robbery\n", LOCAL_ID);
    bank_robbery(NULL, childProcess_nums);

    printf("[doParentWork][local_id %d] Going to getAll ACK message\n", LOCAL_ID);
    getAllMessage(ACK, childProcess_nums);

    // 3. sendStopMessage();

    printf("[doParentWork][local_id %d] construct STOP message\n", LOCAL_ID);
    constructMessage(STOP, buffer, &pd);
    printf("[doParentWork][local_id %d] Going to multiSend STOP message\n", LOCAL_ID);
    send_multicast(&pd, msg);

    getAllMessage(DONE, childProcess_nums);

    // 4. collectBalanceHistories();
    //AllHistory allHistory;

    printf("[doParentWork][local_id %d] Going to getAll BALANCE_HISTORY message\n", LOCAL_ID);
    getAllMessage(BALANCE_HISTORY, childProcess_nums);
    allHistory.s_history_len = childProcess_nums;

    // 5. print_history();
    print_history(&allHistory);
}

void doChildWork()
{
    char buffer[MAX_MESSAGE_LEN + 1];
    memset(buffer, 0, sizeof(buffer));

    nowState.s_balance = initBalance[LOCAL_ID];
    nowState.s_time = get_physical_time();
    nowState.s_balance_pending_in = 0;

    balanceHistory.s_id = LOCAL_ID;
    balanceHistory.s_history_len = 1;
    balanceHistory.s_history[0] = nowState;

    Message *msg = (Message *)buffer;
    ProcessDetail pd = {
            .state = nowState,
            .localID = LOCAL_ID,
            .PID = PID,
            .parentID = parent_PID,
            .belong = LOCAL_ID,
            .childProcessNums = childProcess_nums,
            .fdArrayPointer = writer[LOCAL_ID]
    };
    printf("[local_id %d][doChildWork] Finish init. start to send STARTED message\n", LOCAL_ID);

    //1. sendStartedMessage();
    printf("[local_id %d][doChildWork] Construct STARTED message\n", LOCAL_ID);
    constructMessage(STARTED, buffer, &pd);
    printf("[local_id %d][doChildWork] Going to multi-send STARTED message\n", LOCAL_ID);
    send_multicast(&pd, msg);
    //2. getAllStartedMessage();
    printf("[local_id %d][doChildWork] Going to get all STARTED message\n", LOCAL_ID);
    getAllMessage(STARTED, childProcess_nums);
    //3. deal with TRANSFER and STOP message.
    printf("[local_id %d][doChildWork] Going to deal with TRANSFER and STOP message\n", LOCAL_ID);
    dealWithTransferAndStop();
    //4. sendDoneMessage();
    memset(buffer, 0, sizeof(buffer));
    constructMessage(DONE, buffer, &pd);
    send_multicast(&LOCAL_ID, msg);
    //5. getAllDoneMessage();
    printf("[doChildWork][local_id %d] Construct DONE message\n", LOCAL_ID);
    getAllMessage(DONE, childProcess_nums);
    //6. sendBalanceHistory();
    memset(buffer, 0, sizeof(buffer));
    pd.balanceHistory = &balanceHistory;
    printf("[doChildWork][local_id %d] Construct BALANCE_HISTORY message\n", LOCAL_ID);
    constructMessage(BALANCE_HISTORY, buffer, &pd);
    int writeFd = writer[LOCAL_ID][PARENT_ID];
    printf("[doChildWork][local_id %d] Going to send BALANCE_HISTORY message to Parent process\n", LOCAL_ID);
    send(&writeFd, PARENT_ID, msg);
}

void transfer(void * parent_data, local_id src, local_id dst, balance_t amount)
{
    TransferOrder transferOrder = {
            .s_src = src,
            .s_dst = dst,
            .s_amount = amount
    };

    MessageHeader header = {
            .s_magic = MESSAGE_MAGIC,
            .s_local_time = get_physical_time(),
            .s_payload_len = (uint16_t)sizeof(TransferOrder),
            .s_type = TRANSFER
    };
    Message msg;
    msg.s_header = header;
    memset(msg.s_payload, 0, MAX_PAYLOAD_LEN);
    memcpy(msg.s_payload, &transferOrder, sizeof(TransferOrder));

    int writeFd = writer[PARENT_ID][src];
    if(send(&writeFd, src, &msg) == 0)
        printf("[transfer] Success. Finish to send TRANSFER message to %d process (transfer from %d process to %d process), required %lu bytes\n", transferOrder.s_src, transferOrder.s_src, transferOrder.s_dst, header.s_payload_len+sizeof(MessageHeader));
    else{
        printf("ERROR. See terminal line information.\n");
    }
}

int main(int argc, char * argv[])
{
    childProcess_nums = getProcessNums(argc, argv);
    getInitBalance(argv, childProcess_nums, initBalance);

    FILE *filePointer = NULL;
    if((filePointer = fopen(events_log, "a+")) == NULL){
        printf("[main][local_id %d] NULL file pointer\n", LOCAL_ID);
        exit(1);
    }


    /* create the topology of pipes before create the children process by fork(). */
    for(int i = 0; i < childProcess_nums + 1; i++){
        for(int j = 0; j < childProcess_nums + 1; j++){
            if(i == j)
                continue;

            int fd[2];
            if(pipe(fd) == -1)
            {
                printf("[main][local_id %d] ERROR occurs when pipe()\n", LOCAL_ID);
                exit(1);
            }
            // make the read() and write() non-blocking.
            int flagR = fcntl(fd[0], F_GETFL, 0);
            fcntl(fd[0], F_SETFL, flagR | O_NONBLOCK);
            int flagW = fcntl(fd[1], F_GETFL, 0);
            fcntl(fd[1], F_SETFL, flagW | O_NONBLOCK);

            reader[i][j] = fd[0];
            writer[j][i] = fd[1];

        }
    }
    printf("print writer:\n");
    for(int i = 0; i < childProcess_nums + 1; i++){
        for(int j = 0; j < childProcess_nums+1;j++){
            printf("%d  ", writer[i][j]);
        }
        printf("\n");
    }
    printf("print reader:\n");
    for(int i = 0; i < childProcess_nums + 1; i++){
        for(int j = 0; j < childProcess_nums+1;j++){
            printf("%d  ", reader[i][j]);
        }
        printf("\n");
    }

    parent_PID = getpid();
    /* Create the children processes by fork(). */
    for(int i = 1; i < childProcess_nums + 1; i++)
    {
        pid_t pid = fork();
        //printf("[fork()][local_id %d] Get process PID: %d\n", LOCAL_ID, pid);
        if(pid > 0)
        {
            LOCAL_ID = i;
            PID = pid;
            break;
        }
        else if(pid < 0)
        {
            printf("ERROR occurs when create %d-th child process.\n", i);
        }
    }
    // Note: The child process can run the rest code after the code "fork()", but the code is only one.
    // Note: The read() and write() function is non-blocking function.
    /* If ChildProcess
     *      send the STARTED-type message. Modify the code for loop because of non-blocking.
     *      receive the STARTED-type message from all other children processes.
     *
     *
     * */
    // If childProcess
    if(LOCAL_ID != PARENT_ID)
    {
        printf("[main][local_id %d] doChildWork()\n", LOCAL_ID);
        doChildWork();
    }
    // If ParentProcess
    if(LOCAL_ID == PARENT_ID)
    {
        printf("[main][local_id %d] doParentWork()\n", LOCAL_ID);
        doParentWork();
        int tmp;
        while (1) {
            tmp = wait(NULL);
            if (tmp == -1) {
                if (errno == EINTR) {    // 返回值为-1的时候有两种情况，一种是没有子进程了，还有一种是被中断了
                    continue;
                }
                break;
            }
        }
    }


    return 0;
}
