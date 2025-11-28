#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include "common.h"

typedef struct Node {
    Transaction data;
    struct Node* next;
} Node;

Node* createNode(Transaction data);
void addNode(Node** head, Transaction data);
int deleteNode(Node** head, int id);
void displayList(Node* head);
void freeList(Node* head);
Node* findNode(Node* head, int id);

#endif
