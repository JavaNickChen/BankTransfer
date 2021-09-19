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

int writer[MAX_PROCESS_ID + 1][MAX_PROCESS_ID + 1];
int reader[MAX_PROCESS_ID + 1][MAX_PROCESS_ID + 1];
int LOCAL_ID = PARENT_ID;
int PID;
int parent_PID;
int childProcess_nums = 0;
int initBalance[MAX_PROCESS_ID + 1];
BalanceState nowState;
BalanceHistory balanceHistory;

void getAllMessage(int type, int targetNums)
{
    char buf[MAX_MESSAGE_LEN];
    Message *msg = (Message *)buf;
    int count = 0;
    int isRead[MAX_PROCESS_ID + 1] = {0};
    ProcessInformation pi = {
            .local_id = LOCAL_ID,
            .fileDescriptor = 0;
    };

    while(count < targetNums)
    {
        for(int i = 1; i < targetNums + 1; i++)
        {
            if(LOCAL_ID == i)
                continue;
            int fd = reader[LOCAL_ID][i];
            // there 0 means TRUE that successfully get data from pipe; otherwise, some exception occurs.
            if(!isRead[i]){
                int tmp = receive(&fd, i, msg);
                if(tmp == 0){ // successfully read data from pipe.
                    if(msg->s_header.s_type == STARTED){
                        isRead[i] = 1;
                        count++;
                    }
                } else if(tmp == 1){
                    printf("Read from pipe failed.\n");
                } else if (tmp == 2){ // pipe is empty, try again later.
                    printf("There is no data from Process %d to Process %d now. Please wait.\n", i, LOCAL_ID);
                }

            }
        }
    }
}

void dealWithTransferAndStop()
{
    char buf[MAX_MESSAGE_LEN];
    memset(buf, 0, sizeof(buf));
    Message *msg = (Message *)buf;

    while(1) {
        // loop for read message possibly from other child process or Parent process.
        for(int i = 0; i < childProcess_nums + 1; i++){
            if(LOCAL_ID == i)
                continue;

            int fd = reader[LOCAL_ID][i];
            int res = receive(&fd, i, msg);
            if((res == 2) || (res == 1))
                continue;

            switch (msg->s_header.s_type) {
                case STOP :
                    return 0;
                case TRANSFER :
                {
                    TransferOrder *transferOrder = (TransferOrder *)(msg->s_payload);
                    ProcessDetail pd = {
                            .belong = i,
                            .money = transferOrder->s_amount,
                            .state = nowState,
                            .localID = LOCAL_ID,
                            .srcID = transferOrder->s_src,
                            .dstID = transferOrder->s_dst,
                            .belong = LOCAL_ID
                    };
                    if(i == PARENT_ID){
                        // get TRANSFER message from parent process and transfer money.
                        int balance = nowState.s_balance;
                        nowState.s_balance = balance - transferOrder->s_amount;
                        nowState.s_time = get_physical_time();


                        constructMessage(TRANSFER, buf, &pd);
                        int writeFd = writer[LOCAL_ID][transferOrder->s_dst];
                        send(&writeFd, transferOrder->s_dst, msg);

                    } else {
                        // get TRANSFER message from child process and send ACK message to Parent Process.
                        nowState.s_balance = nowState.s_balance + transferOrder->s_amount;
                        nowState.s_time = get_physical_time();

                        int writeFd = writer[LOCAL_ID][PARENT_ID];
                        memset(buf, 0, sizeof(buf));
                        constructMessage(ACK, buf, &pd);
                        send(&writeFd, PARENT_ID, msg);

                    }
                    balanceHistory.s_history[balanceHistory.s_history_len] = nowState;
                    balanceHistory.s_history_len = balanceHistory.s_history_len + 1;
                }
            }

        }
    }
}
void doParentWork()
{
    // 1. getAllStartedMessage();
    // 2. bank_robbery();
    // 3. sendStopMessage();
    // 4. collectBalanceHistories();
    AllHistory allHistory;
    allHistory.s_history_len = childProcess_nums;

    // 5. print_history();
}

void doChildWork()
{
    char buffer[MAX_MESSAGE_LEN + 1];

    timestamp_t startTime = get_physical_time();

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
            .belong = LOCAL_ID
    };
    //1. sendStartedMessage();
    constructMessage(STARTED, buffer, &pd);
    send_multicast(&LOCAL_ID, msg);
    //2. getAllStartedMessage();
    getAllMessage(STARTED, childProcess_nums);
    //3. deal with TRANSFER and STOP message.
    dealWithTransferAndStop();
    //4. sendDoneMessage();
    memset(buffer, 0, sizeof(buffer));
    constructMessage(DONE, buffer, &pd);
    send_multicast(&LOCAL_ID, msg);
    //5. getAllDoneMessage();
    getAllMessage(DONE, childProcess_nums);
    //6. sendBalanceHistory();
    memset(buffer, 0, sizeof(buffer));
    pd.balanceHistory = &balanceHistory;
    constructMessage(BALANCE_HISTORY, buffer, &pd);
    int writeFd = writer[LOCAL_ID][PARENT_ID];
    send(&writeFd, PARENT_ID, msg);
}


int main(int argc, char *argv[])
{
    getProcessNums(argc, argv);
    getInitBalance(argv);

    FILE *filePointer = NULL;
    if((filePointer = fopen(events_log, "a+")) == NULL){
        printf("NULL file pointer\n");
        exit(1);
    }


    /* create the topology of pipes before create the children process by fork(). */
    for(int i = 0; i < process_nums + 1; i++){
        for(int j = 0; j < process_nums + 1; j++){
            if(i == j)
                continue;

            int fd[2];
            if(pipe(fd) == -1)
            {
                printf("ERROR occurs when pipe()\n");
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

    parent_PID = getpid();
    /* Create the children processes by fork(). */
    for(int i = 1; i < process_nums + 1; i++)
    {
        pid_t pid = fork();
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
    // If ParentProcess
    if(LOCAL_ID == PARENT_ID)
    {
        doParentWork();
    }else if(LOCAL_ID != PARENT_ID)
    {
        doChildWork();
    }


    return 0;
}


