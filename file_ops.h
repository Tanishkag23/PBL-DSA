#ifndef FILE_OPS_H
#define FILE_OPS_H

#include "common.h"
#include "linkedlist.h"

void saveToFile(Node* head, const char* filename);
void loadFromFile(Node** head, const char* filename);

#endif
