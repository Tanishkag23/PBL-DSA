#ifndef UTILS_H
#define UTILS_H

#include "common.h"
#include "linkedlist.h"

void sortTransactionsByAmount(Node** head);
void sortTransactionsByDate(Node** head);
void getCategoryTotals(Node* head);

#endif
