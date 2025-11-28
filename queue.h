#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"

typedef struct QueueNode {
    Transaction data;
    struct QueueNode* next;
} QueueNode;

typedef struct {
    QueueNode *front, *rear;
} Queue;

Queue* createQueue();
void enqueue(Queue* q, Transaction data);
Transaction dequeue(Queue* q);
int isQueueEmpty(Queue* q);
void displayQueue(Queue* q);
void freeQueue(Queue* q);
void saveQueue(Queue* q, const char* filename);
void loadQueue(Queue* q, const char* filename);

#endif
