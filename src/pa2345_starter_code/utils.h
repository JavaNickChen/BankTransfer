#include "ipc.h"
#include "banking.h"

#ifndef BANKTRANSFER_UTILS_H
#define BANKTRANSFER_UTILS_H

#endif //BANKTRANSFER_UTILS_H

typedef struct {
    BalanceState state;
    local_id localID;
    int PID;
    int parentID;
    int belong; // which process the struct variable from.
    int srcID; // in transfer, the one whose money is out.
    int dstID; //  in transfer, the one whose balance increase.
    int money; // in transfer, the amount for transfer.
    BalanceHistory *balanceHistory;
} __attribute__((packed)) ProcessDetail;


void getProcessNums(int argc, char *argv[]);
void getInitBalance(char *argv[]);
void constructMessage(int type, char *buf, ProcessDetail processDetail);