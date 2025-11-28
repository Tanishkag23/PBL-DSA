#ifndef STACK_H
#define STACK_H

#include "common.h"

typedef enum {
    OP_ADD,
    OP_DELETE
} OperationType;

typedef struct StackNode {
    Transaction data;
    OperationType type;
    struct StackNode* next;
} StackNode;

void push(StackNode** top, Transaction data, OperationType type);
Transaction pop(StackNode** top, OperationType* type);
int isStackEmpty(StackNode* top);
void freeStack(StackNode* top);
void saveStack(StackNode* top, const char* filename);
void loadStack(StackNode** top, const char* filename);

#endif
