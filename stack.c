#include "stack.h"

void push(StackNode** top, Transaction data, OperationType type) {
    StackNode* newNode = (StackNode*)malloc(sizeof(StackNode));
    if (!newNode) {
        printf("Stack Overflow\n");
        return;
    }
    newNode->data = data;
    newNode->type = type;
    newNode->next = *top;
    *top = newNode;
}

Transaction pop(StackNode** top, OperationType* type) {
    Transaction empty = {0};
    if (isStackEmpty(*top)) {
        printf("Stack Underflow\n");
        return empty;
    }
    StackNode* temp = *top;
    Transaction data = temp->data;
    if (type) *type = temp->type;
    *top = (*top)->next;
    free(temp);
    return data;
}

int isStackEmpty(StackNode* top) {
    return top == NULL;
}

void freeStack(StackNode* top) {
    StackNode* temp;
    while (top != NULL) {
        temp = top;
        top = top->next;
        free(temp);
    }
}

void saveStack(StackNode* top, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Could not open file %s for writing.\n", filename);
        return;
    }

    StackNode* temp = top;
    while (temp != NULL) {
        fprintf(fp, "%d %d %d %d %.2f %s %s %s %d\n", 
            temp->data.id,
            temp->data.date.day, temp->data.date.month, temp->data.date.year,
            temp->data.amount,
            temp->data.type,
            temp->data.category,
            temp->data.description,
            temp->type);
        temp = temp->next;
    }
    fclose(fp);
}

void loadStack(StackNode** top, const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        return;
    }

    Transaction t;
    int opTypeInt;
    StackNode* tempStack = NULL;
    while (fscanf(fp, "%d %d %d %d %lf %s %s %s %d", 
            &t.id,
            &t.date.day, &t.date.month, &t.date.year,
            &t.amount,
            t.type,
            t.category,
            t.description,
            &opTypeInt) == 9) {
        push(&tempStack, t, (OperationType)opTypeInt);
    }
    fclose(fp);

    while (!isStackEmpty(tempStack)) {
        OperationType op;
        Transaction data = pop(&tempStack, &op);
        push(top, data, op);
    }
}
