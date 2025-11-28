#include "utils.h"

Node* sortedMergeAmount(Node* a, Node* b);
Node* sortedMergeDate(Node* a, Node* b);
void splitList(Node* source, Node** frontRef, Node** backRef);

void mergeSortAmount(Node** headRef) {
    Node* head = *headRef;
    Node* a;
    Node* b;

    if ((head == NULL) || (head->next == NULL)) {
        return;
    }

    splitList(head, &a, &b);

    mergeSortAmount(&a);
    mergeSortAmount(&b);

    *headRef = sortedMergeAmount(a, b);
}

Node* sortedMergeAmount(Node* a, Node* b) {
    Node* result = NULL;

    if (a == NULL) return b;
    if (b == NULL) return a;

    if (a->data.amount <= b->data.amount) {
        result = a;
        result->next = sortedMergeAmount(a->next, b);
    } else {
        result = b;
        result->next = sortedMergeAmount(a, b->next);
    }
    return result;
}

void mergeSortDate(Node** headRef) {
    Node* head = *headRef;
    Node* a;
    Node* b;

    if ((head == NULL) || (head->next == NULL)) {
        return;
    }

    splitList(head, &a, &b);

    mergeSortDate(&a);
    mergeSortDate(&b);

    *headRef = sortedMergeDate(a, b);
}

Node* sortedMergeDate(Node* a, Node* b) {
    Node* result = NULL;

    if (a == NULL) return b;
    if (b == NULL) return a;

    int date1 = a->data.date.year * 10000 + a->data.date.month * 100 + a->data.date.day;
    int date2 = b->data.date.year * 10000 + b->data.date.month * 100 + b->data.date.day;

    if (date1 <= date2) {
        result = a;
        result->next = sortedMergeDate(a->next, b);
    } else {
        result = b;
        result->next = sortedMergeDate(a, b->next);
    }
    return result;
}

void splitList(Node* source, Node** frontRef, Node** backRef) {
    Node* fast;
    Node* slow;
    slow = source;
    fast = source->next;

    while (fast != NULL) {
        fast = fast->next;
        if (fast != NULL) {
            slow = slow->next;
            fast = fast->next;
        }
    }
    *frontRef = source;
    *backRef = slow->next;
    slow->next = NULL;
}

void sortTransactionsByAmount(Node** head) {
    mergeSortAmount(head);
    printf("Transactions sorted by Amount.\n");
}

void sortTransactionsByDate(Node** head) {
    mergeSortDate(head);
    printf("Transactions sorted by Date.\n");
}

void getCategoryTotals(Node* head) {
    if (head == NULL) return;

    double totalIncome = 0;
    double totalExpense = 0;

    Node* temp = head;
    while (temp != NULL) {
        if (strcmp(temp->data.type, "Income") == 0) {
            totalIncome += temp->data.amount;
        } else if (strcmp(temp->data.type, "Expense") == 0) {
            totalExpense += temp->data.amount;
        }
        temp = temp->next;
    }

    printf("\n--- Financial Summary ---\n");
    printf("Total Income:  %.2f\n", totalIncome);
    printf("Total Expense: %.2f\n", totalExpense);
    printf("Net Savings:   %.2f\n", totalIncome - totalExpense);
    printf("-------------------------\n");
}
