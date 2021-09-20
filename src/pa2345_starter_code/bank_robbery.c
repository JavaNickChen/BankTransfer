/**
 * @file     bank_robbery.c
 * @Author   Michael Kosyakov and Evgeniy Ivanov (ifmo.distributedclass@gmail.com)
 * @date     March, 2014
 * @brief    Toy implementation of bank_robbery(), don't do it in real life ;)
 *
 * Students must not modify this file!
 */

#include "banking.h"
#include "stdio.h"

void bank_robbery(void * parent_data, local_id max_id)
{
    printf("[bank_robbery] In function bank_robbery()\n");
    for (int i = 1; i < max_id; ++i) {
        transfer(parent_data, i, i + 1, i);
    }
    if (max_id > 1) {
        transfer(parent_data, max_id, 1, 1);
    }
}
