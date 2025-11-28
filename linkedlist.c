#include "linkedlist.h"

Node* createNode(Transaction data) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (!newNode) {
        printf("Memory allocation failed!\n");
        return NULL;
    }
    newNode->data = data;
    newNode->next = NULL;
    return newNode;
}

void addNode(Node** head, Transaction data) {
    Node* newNode = createNode(data);
    if (!newNode) return;

    if (*head == NULL) {
        *head = newNode;
        return;
    }

    Node* temp = *head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = newNode;
}

int deleteNode(Node** head, int id) {
    if (*head == NULL) return 0;

    Node* temp = *head;
    Node* prev = NULL;

    if (temp != NULL && temp->data.id == id) {
        *head = temp->next;
        free(temp);
        return 1;
    }

    while (temp != NULL && temp->data.id != id) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL) return 0;

    prev->next = temp->next;
    free(temp);
    return 1;
}

Node* findNode(Node* head, int id) {
    Node* temp = head;
    while (temp != NULL) {
        if (temp->data.id == id) {
            return temp;
        }
        temp = temp->next;
    }
    return NULL;
}

void displayList(Node* head) {
    if (head == NULL) {
        printf("No transactions found.\n");
        return;
    }
    
    printf("\n%-5s %-12s %-10s %-10s %-15s %-20s\n", "ID", "Date", "Amount", "Type", "Category", "Description");
    printf("-------------------------------------------------------------------------------\n");
    
    Node* temp = head;
    while (temp != NULL) {
        printf("%-5d %02d/%02d/%04d   %-10.2f %-10s %-15s %-20s\n", 
               temp->data.id, 
               temp->data.date.day, temp->data.date.month, temp->data.date.year, 
               temp->data.amount, 
               temp->data.type, 
               temp->data.category, 
               temp->data.description);
        temp = temp->next;
    }
    printf("-------------------------------------------------------------------------------\n");
}

void freeList(Node* head) {
    Node* temp;
    while (head != NULL) {
        temp = head;
        head = head->next;
        free(temp);
    }
}
