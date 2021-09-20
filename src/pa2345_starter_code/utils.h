#include "ipc.h"
#include "banking.h"

#ifndef BANKTRANSFER_UTILS_H
#define BANKTRANSFER_UTILS_H

#endif //BANKTRANSFER_UTILS_H

typedef struct {
    BalanceState state;
    local_id localID;
    uint16_t PID;
    uint16_t parentID;
    uint16_t belong; // which process the struct variable from.
    uint16_t srcID; // in transfer, the one whose money is out.
    uint16_t dstID; //  in transfer, the one whose balance increase.
    uint16_t money; // in transfer, the amount for transfer.
    BalanceHistory *balanceHistory;
    int *fdArrayPointer;
    uint16_t childProcessNums;
} __attribute__((packed)) ProcessDetail;


int getProcessNums(int argc, char *argv[]);
void getInitBalance(char *argv[], int childProcessNums, int *initBalance);
void constructMessage(int type, char *buf, ProcessDetail *processDetail);


