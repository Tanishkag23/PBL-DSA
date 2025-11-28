#include "common.h"
#include "linkedlist.h"
#include "stack.h"
#include "queue.h"
#include "bst.h"
#include "file_ops.h"
#include "utils.h"

int getNextId(Node* head) {
    int maxId = 0;
    Node* temp = head;
    while (temp != NULL) {
        if (temp->data.id > maxId) {
            maxId = temp->data.id;
        }
        temp = temp->next;
    }
    return maxId + 1;
}

void printUsage() {
    printf("Usage: expense_tracker <filename> <command> [args...]\n");
    printf("Commands:\n");
    printf("  add <day> <month> <year> <amount> <type> <category> <description>\n");
    printf("  delete <id>\n");
    printf("  sort_amount\n");
    printf("  sort_date\n");
    printf("  search <type> <value>\n");
    printf("  analysis\n");
    printf("  suggest <username> <text>\n");
    printf("  view_suggestions\n");
    printf("  delete_suggestion <line_number>\n");
    printf("  reply_user <username> <text>\n");
    printf("  view_replies <username>\n");
    printf("  undo\n");
    printf("  recurring <day> <month> <year> <amount> <type> <category> <description>\n");
    printf("  process_recurring\n");
    printf("  view_recurring\n");
}

void rebuildBST(Node* head, BSTNode** bstRoot) {
    freeBST(*bstRoot);
    *bstRoot = NULL;
    Node* temp = head;
    while (temp != NULL) {
        *bstRoot = insertBST(*bstRoot, temp->data);
        temp = temp->next;
    }
}

void cmdAdd(Node** head, StackNode** undoStack, BSTNode** bstRoot, char* filename, Transaction t) {
    addNode(head, t);
    push(undoStack, t, OP_ADD);
    saveStack(*undoStack, "undo_stack.txt");
    saveToFile(*head, filename);
    *bstRoot = insertBST(*bstRoot, t);
    printf("Transaction added successfully. ID: %d\n", t.id);
}

void cmdDelete(Node** head, StackNode** undoStack, BSTNode** bstRoot, char* filename, int id) {
    Node* nodeToDelete = findNode(*head, id);
    if (nodeToDelete) {
        Transaction t = nodeToDelete->data;
        if (deleteNode(head, id)) {
            push(undoStack, t, OP_DELETE);
            saveStack(*undoStack, "undo_stack.txt");
            saveToFile(*head, filename);
            rebuildBST(*head, bstRoot);
            printf("Transaction %d deleted successfully.\n", id);
        }
    } else {
        printf("Error: Transaction %d not found.\n", id);
    }
}

void cmdUndo(Node** head, StackNode** undoStack, BSTNode** bstRoot, char* filename) {
    if (isStackEmpty(*undoStack)) {
        printf("Nothing to undo.\n");
    } else {
        OperationType opType;
        Transaction t = pop(undoStack, &opType);
        
        if (opType == OP_ADD) {
            deleteNode(head, t.id);
            printf("Undo: Removed transaction %d.\n", t.id);
        } else if (opType == OP_DELETE) {
            addNode(head, t);
            printf("Undo: Restored transaction %d.\n", t.id);
        }
        saveToFile(*head, filename);
        saveStack(*undoStack, "undo_stack.txt");
        rebuildBST(*head, bstRoot);
    }
}

void cmdProcessRecurring(Node** head, StackNode** undoStack, Queue* recurringQueue, BSTNode** bstRoot, char* filename) {
    if (isQueueEmpty(recurringQueue)) {
        printf("No recurring payments to process.\n");
    } else {
        Transaction t = dequeue(recurringQueue);
        t.id = getNextId(*head);
        
        cmdAdd(head, undoStack, bstRoot, filename, t);
        saveQueue(recurringQueue, "recurring.txt");
        printf("Processed recurring payment: %s - %.2f\n", t.description, t.amount);
    }
}

void interactiveMenu(Node** head, StackNode** undoStack, Queue* recurringQueue, BSTNode** bstRoot, char* filename) {
    int choice;
    while (1) {
        printf("\n--- Expense Tracker Menu ---\n");
        printf("1. Add Transaction\n");
        printf("2. Delete Transaction\n");
        printf("3. View Transactions\n");
        printf("4. Undo Last Action\n");
        printf("5. Search\n");
        printf("6. Sort\n");
        printf("7. Analysis\n");
        printf("8. Recurring Payments\n");
        printf("9. Suggestions\n");
        printf("0. Exit\n");
        printf("Enter choice: ");
        scanf("%d", &choice);

        if (choice == 0) break;

        switch (choice) {
            case 1: {
                Transaction t;
                t.id = getNextId(*head);
                printf("Enter Date (DD MM YYYY): ");
                scanf("%d %d %d", &t.date.day, &t.date.month, &t.date.year);
                printf("Enter Amount: ");
                scanf("%lf", &t.amount);
                printf("Enter Type (Income/Expense): ");
                scanf("%s", t.type);
                printf("Enter Category: ");
                scanf("%s", t.category);
                printf("Enter Description: ");
                while(getchar() != '\n'); 
                fgets(t.description, MAX_DESC, stdin);
                t.description[strcspn(t.description, "\n")] = 0;

                cmdAdd(head, undoStack, bstRoot, filename, t);
                break;
            }
            case 2: {
                int id;
                printf("Enter ID to delete: ");
                scanf("%d", &id);
                cmdDelete(head, undoStack, bstRoot, filename, id);
                break;
            }
            case 3:
                displayList(*head);
                break;
            case 4:
                cmdUndo(head, undoStack, bstRoot, filename);
                break;
            case 5: {
                int searchChoice;
                printf("Search by: 1. Amount, 2. ID, 3. Description: ");
                scanf("%d", &searchChoice);
                if (searchChoice == 1) {
                    double amt;
                    printf("Enter Amount: ");
                    scanf("%lf", &amt);
                    searchBST(*bstRoot, amt);
                } else if (searchChoice == 2) {
                    int id;
                    printf("Enter ID: ");
                    scanf("%d", &id);
                    Node* res = findNode(*head, id);
                    if (res) printf("Found: ID: %d, Amount: %.2f, Desc: %s\n", res->data.id, res->data.amount, res->data.description);
                    else printf("Not found.\n");
                } else if (searchChoice == 3) {
                    char desc[MAX_DESC];
                    printf("Enter Description: ");
                    scanf("%s", desc);
                    Node* temp = *head;
                    int found = 0;
                    while (temp != NULL) {
                        if (strstr(temp->data.description, desc) != NULL) {
                            printf("Found: ID: %d, Amount: %.2f, Desc: %s\n", temp->data.id, temp->data.amount, temp->data.description);
                            found = 1;
                        }
                        temp = temp->next;
                    }
                    if (!found) printf("No match.\n");
                }
                break;
            }
            case 6: {
                int sortChoice;
                printf("Sort by: 1. Amount, 2. Date: ");
                scanf("%d", &sortChoice);
                if (sortChoice == 1) sortTransactionsByAmount(head);
                else if (sortChoice == 2) sortTransactionsByDate(head);
                saveToFile(*head, filename);
                printf("Sorted.\n");
                break;
            }
            case 7:
                getCategoryTotals(*head);
                break;
            case 8: {
                int rChoice;
                printf("1. Schedule New, 2. View, 3. Process Next: ");
                scanf("%d", &rChoice);
                if (rChoice == 1) {
                    Transaction t;
                    t.id = getNextId(*head);
                    printf("Enter Date (DD MM YYYY): ");
                    scanf("%d %d %d", &t.date.day, &t.date.month, &t.date.year);
                    printf("Enter Amount: ");
                    scanf("%lf", &t.amount);
                    printf("Enter Type: ");
                    scanf("%s", t.type);
                    printf("Enter Category: ");
                    scanf("%s", t.category);
                    printf("Enter Description: ");
                    while(getchar() != '\n');
                    fgets(t.description, MAX_DESC, stdin);
                    t.description[strcspn(t.description, "\n")] = 0;

                    enqueue(recurringQueue, t);
                    saveQueue(recurringQueue, "recurring.txt");
                    printf("Scheduled.\n");
                } else if (rChoice == 2) {
                    displayQueue(recurringQueue);
                } else if (rChoice == 3) {
                    cmdProcessRecurring(head, undoStack, recurringQueue, bstRoot, filename);
                }
                break;
            }
            case 9: {
                printf("Suggestions feature is primarily CLI based for Admin/User separation.\n");
                break;
            }
            default:
                printf("Invalid choice.\n");
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    Node* head = NULL;
    BSTNode* bstRoot = NULL;
    StackNode* undoStack = NULL;
    Queue* recurringQueue = createQueue();
    
    char* filename = argv[1];
    loadFromFile(&head, filename);
    loadQueue(recurringQueue, "recurring.txt");
    loadStack(&undoStack, "undo_stack.txt");

    Node* temp = head;
    while (temp != NULL) {
        bstRoot = insertBST(bstRoot, temp->data);
        temp = temp->next;
    }

    if (argc == 2) {
        interactiveMenu(&head, &undoStack, recurringQueue, &bstRoot, filename);
        freeList(head);
        freeBST(bstRoot);
        freeStack(undoStack);
        freeQueue(recurringQueue);
        return 0;
    }

    char* command = argv[2];

    if (strcmp(command, "add") == 0) {
        if (argc < 10) {
            printf("Error: Missing arguments for add.\n");
            return 1;
        }
        Transaction t;
        t.id = getNextId(head);
        t.date.day = atoi(argv[3]);
        t.date.month = atoi(argv[4]);
        t.date.year = atoi(argv[5]);
        t.amount = atof(argv[6]);
        strncpy(t.type, argv[7], MAX_TYPE);
        strncpy(t.category, argv[8], MAX_CAT);
        strncpy(t.description, argv[9], MAX_DESC);

        cmdAdd(&head, &undoStack, &bstRoot, filename, t);

    } else if (strcmp(command, "delete") == 0) {
        if (argc < 4) {
            printf("Error: Missing ID for delete.\n");
            return 1;
        }
        int id = atoi(argv[3]);
        cmdDelete(&head, &undoStack, &bstRoot, filename, id);

    } else if (strcmp(command, "sort_amount") == 0) {
        sortTransactionsByAmount(&head);
        saveToFile(head, filename);
        printf("Sorted by amount and saved.\n");

    } else if (strcmp(command, "sort_date") == 0) {
        sortTransactionsByDate(&head);
        saveToFile(head, filename);
        printf("Sorted by date and saved.\n");

    } else if (strcmp(command, "search") == 0) {
        if (argc < 5) {
            printf("Error: Usage: search <type> <value>\n");
            return 1;
        }
        char* searchType = argv[3];
        
        if (strcmp(searchType, "amount") == 0) {
            double amount = atof(argv[4]);
            searchBST(bstRoot, amount);
        } else if (strcmp(searchType, "id") == 0) {
            int id = atoi(argv[4]);
            Node* res = findNode(head, id);
            if (res) {
                printf("Found: ID: %d, Amount: %.2f, Desc: %s\n", res->data.id, res->data.amount, res->data.description);
            } else {
                printf("Transaction with ID %d not found.\n", id);
            }
        } else if (strcmp(searchType, "description") == 0) {
            char* desc = argv[4];
            Node* temp = head;
            int found = 0;
            while (temp != NULL) {
                if (strstr(temp->data.description, desc) != NULL) {
                    printf("Found: ID: %d, Amount: %.2f, Desc: %s\n", temp->data.id, temp->data.amount, temp->data.description);
                    found = 1;
                }
                temp = temp->next;
            }
            if (!found) printf("No transactions found matching '%s'.\n", desc);
        } else {
            printf("Error: Unknown search type '%s'. Supported: amount, id, description.\n", searchType);
        }

    } else if (strcmp(command, "analysis") == 0) {
        getCategoryTotals(head);

    } else if (strcmp(command, "suggest") == 0) {
        if (argc < 5) {
            printf("Error: Usage: suggest <username> <text>\n");
            return 1;
        }
        char* username = argv[3];
        FILE* fp = fopen("suggestions.txt", "a");
        if (fp) {
            fprintf(fp, "%s: ", username);
            for (int i = 4; i < argc; i++) {
                fprintf(fp, "%s ", argv[i]);
            }
            fprintf(fp, "\n");
            fclose(fp);
            printf("Suggestion submitted successfully.\n");
        } else {
            printf("Error: Could not open suggestions file.\n");
        }

    } else if (strcmp(command, "view_suggestions") == 0) {
        FILE* fp = fopen("suggestions.txt", "r");
        if (fp) {
            char line[256];
            int lineNum = 1;
            while (fgets(line, sizeof(line), fp)) {
                printf("%d. %s", lineNum++, line);
            }
            fclose(fp);
        } else {
            printf("No suggestions found.\n");
        }

    } else if (strcmp(command, "delete_suggestion") == 0) {
        if (argc < 4) {
            printf("Error: Usage: delete_suggestion <line_number>\n");
            return 1;
        }
        int lineToDelete = atoi(argv[3]);
        FILE* fp = fopen("suggestions.txt", "r");
        FILE* tempFp = fopen("temp_suggestions.txt", "w");
        
        if (fp && tempFp) {
            char line[256];
            int currentLine = 1;
            int deleted = 0;
            while (fgets(line, sizeof(line), fp)) {
                if (currentLine != lineToDelete) {
                    fputs(line, tempFp);
                } else {
                    deleted = 1;
                }
                currentLine++;
            }
            fclose(fp);
            fclose(tempFp);
            remove("suggestions.txt");
            rename("temp_suggestions.txt", "suggestions.txt");
            if (deleted) printf("Suggestion deleted successfully.\n");
            else printf("Suggestion line %d not found.\n", lineToDelete);
        } else {
            printf("Error: Could not open file for deletion.\n");
        }

    } else if (strcmp(command, "reply_user") == 0) {
        if (argc < 5) {
            printf("Error: Usage: reply_user <username> <text>\n");
            return 1;
        }
        char* targetUser = argv[3];
        char filename_reply[100];
        sprintf(filename_reply, "replies_%s.txt", targetUser);
        
        FILE* fp = fopen(filename_reply, "a");
        if (fp) {
            fprintf(fp, "Admin Reply: ");
            for (int i = 4; i < argc; i++) {
                fprintf(fp, "%s ", argv[i]);
            }
            fprintf(fp, "\n");
            fclose(fp);
            printf("Reply sent to %s.\n", targetUser);
        } else {
            printf("Error: Could not open reply file.\n");
        }

    } else if (strcmp(command, "view_replies") == 0) {
        if (argc < 4) {
             printf("Error: Usage: view_replies <username>\n");
             return 1;
        }
        char* username = argv[3];
        char filename_reply[100];
        sprintf(filename_reply, "replies_%s.txt", username);
        
        FILE* fp = fopen(filename_reply, "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                printf("%s", line);
            }
            fclose(fp);
        } else {
            printf("No new messages.\n");
        }

    } else if (strcmp(command, "undo") == 0) {
        cmdUndo(&head, &undoStack, &bstRoot, filename);

    } else if (strcmp(command, "recurring") == 0) {
        if (argc < 10) {
            printf("Error: Missing arguments for recurring.\n");
            return 1;
        }
        Transaction t;
        t.id = getNextId(head);
        t.date.day = atoi(argv[3]);
        t.date.month = atoi(argv[4]);
        t.date.year = atoi(argv[5]);
        t.amount = atof(argv[6]);
        strncpy(t.type, argv[7], MAX_TYPE);
        strncpy(t.category, argv[8], MAX_CAT);
        strncpy(t.description, argv[9], MAX_DESC);

        enqueue(recurringQueue, t);
        saveQueue(recurringQueue, "recurring.txt");
        printf("Recurring payment scheduled.\n");

    } else if (strcmp(command, "process_recurring") == 0) {
        cmdProcessRecurring(&head, &undoStack, recurringQueue, &bstRoot, filename);

    } else if (strcmp(command, "view_recurring") == 0) {
        displayQueue(recurringQueue);

    } else {
        printf("Unknown command: %s\n", command);
        printUsage();
        return 1;
    }

    freeList(head);
    freeBST(bstRoot);
    freeStack(undoStack);
    freeQueue(recurringQueue);

    return 0;
}
