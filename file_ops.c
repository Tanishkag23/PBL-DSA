#include "file_ops.h"

void saveToFile(Node* head, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        printf("Error opening file for writing!\n");
        return;
    }

    Node* temp = head;
    while (temp != NULL) {
        fprintf(file, "%d %d %d %d %.2f %s %s %s\n", 
                temp->data.id,
                temp->data.date.day, temp->data.date.month, temp->data.date.year,
                temp->data.amount,
                temp->data.type,
                temp->data.category,
                temp->data.description);
        temp = temp->next;
    }

    fclose(file);
    printf("Data saved successfully to %s\n", filename);
}

void loadFromFile(Node** head, const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("No existing data found. Starting fresh.\n");
        return;
    }

    Transaction t;
    
    while (fscanf(file, "%d %d %d %d %lf %s %s %[^\n]", 
                  &t.id, 
                  &t.date.day, &t.date.month, &t.date.year, 
                  &t.amount, 
                  t.type, 
                  t.category, 
                  t.description) == 8) {
        addNode(head, t);
    }

    fclose(file);
    printf("Data loaded successfully from %s\n", filename);
}
