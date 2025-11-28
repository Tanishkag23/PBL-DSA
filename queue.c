#include "queue.h"

Queue* createQueue() {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    q->front = q->rear = NULL;
    return q;
}

void enqueue(Queue* q, Transaction data) {
    QueueNode* temp = (QueueNode*)malloc(sizeof(QueueNode));
    temp->data = data;
    temp->next = NULL;

    if (q->rear == NULL) {
        q->front = q->rear = temp;
        return;
    }

    q->rear->next = temp;
    q->rear = temp;
}

Transaction dequeue(Queue* q) {
    Transaction empty = {0};
    if (q->front == NULL) {
        return empty;
    }

    QueueNode* temp = q->front;
    Transaction data = temp->data;
    q->front = q->front->next;

    if (q->front == NULL) {
        q->rear = NULL;
    }

    free(temp);
    return data;
}

int isQueueEmpty(Queue* q) {
    return q->front == NULL;
}

void displayQueue(Queue* q) {
    if (isQueueEmpty(q)) {
        printf("No upcoming recurring payments.\n");
        return;
    }

    printf("\n--- Upcoming Recurring Payments ---\n");
    printf("%-12s %-10s %-15s %-20s\n", "Date", "Amount", "Category", "Description");
    printf("----------------------------------------------------------\n");

    QueueNode* temp = q->front;
    while (temp != NULL) {
        printf("%02d/%02d/%04d   %-10.2f %-15s %-20s\n", 
               temp->data.date.day, temp->data.date.month, temp->data.date.year, 
               temp->data.amount, 
               temp->data.category, 
               temp->data.description);
        temp = temp->next;
    }
    printf("----------------------------------------------------------\n");
}

void freeQueue(Queue* q) {
    while (!isQueueEmpty(q)) {
        dequeue(q);
    }
    free(q);
}

void saveQueue(Queue* q, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Could not open file %s for writing.\n", filename);
        return;
    }

    QueueNode* temp = q->front;
    while (temp != NULL) {
        fprintf(fp, "%d %d %d %d %.2f %s %s %s\n", 
            temp->data.id,
            temp->data.date.day, temp->data.date.month, temp->data.date.year,
            temp->data.amount,
            temp->data.type,
            temp->data.category,
            temp->data.description);
        temp = temp->next;
    }
    fclose(fp);
}

void loadQueue(Queue* q, const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        return;
    }

    Transaction t;
    while (fscanf(fp, "%d %d %d %d %lf %s %s %s", 
            &t.id,
            &t.date.day, &t.date.month, &t.date.year,
            &t.amount,
            t.type,
            t.category,
            t.description) == 8) {
        enqueue(q, t);
    }
    fclose(fp);
}
