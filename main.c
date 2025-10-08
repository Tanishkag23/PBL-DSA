// main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "auth.h"
#include "expenses.h"
#include "file_io.h"
#include "structures.h"

/* helper to read input safely */
static void read_line(char *buf, int size) {
    if (!fgets(buf, size, stdin)) { buf[0] = '\0'; return; }
    buf[strcspn(buf, "\n")] = '\0';
}

static void show_menu() {
    printf("\n=== Expense Tracker ===\n");
    printf("1. Add expense\n");
    printf("2. Update expense\n");
    printf("3. Delete expense\n");
    printf("4. Display all expenses (your previous entries)\n");
    printf("5. Sort by amount (ascending)\n");
    printf("6. Sort by date (newest first)\n");
    printf("7. Save data\n");
    printf("8. Logout and Exit\n");
    printf("9. Generate Report\n");
    printf("Choose option: ");
}

int main() {
    /* 1. Authentication */
    char *username = auth_prompt();
    if (!username) {
        printf("Goodbye.\n");
        return 0;
    }

    /* 2. Initialize expenses for this user (loads previous user's data) */
    init_expenses_for_user(username);
    printf("Loaded %d expense(s) for user %s. Next ID = %d\n", get_expense_count(), username, next_id_for_user);

    /* Main loop */
    while (1) {
        show_menu();
        char opt_str[16];
        read_line(opt_str, sizeof(opt_str));
        int choice = atoi(opt_str);

        if (choice == 1) {
            char amt_s[50], date_s[20], cat[MAX_CAT], desc[MAX_DESC];
            printf("Enter amount: ");
            read_line(amt_s, sizeof(amt_s));
            double amt = atof(amt_s);
            printf("Enter date (yyyymmdd): ");
            read_line(date_s, sizeof(date_s));
            int date = atoi(date_s);
            printf("Enter category: ");
            read_line(cat, sizeof(cat));
            printf("Enter description: ");
            read_line(desc, sizeof(desc));
            int id = add_expense(amt, date, cat, desc);
            if (id == -1) printf("Failed to add expense (full).\n");
            else printf("Added expense with ID %d\n", id);
        }
        else if (choice == 2) {
            char id_s[20];
            printf("Enter ID to update: ");
            read_line(id_s, sizeof(id_s));
            int id = atoi(id_s);
            Expense *e = find_expense_by_id(id);
            if (!e) { printf("No expense with ID %d\n", id); continue; }
            printf("Current: Amount=%.2f Date=%d Cat=%s Desc=%s\n", e->amount, e->date, e->category, e->description);
            char amt_s[50], date_s[20], cat[MAX_CAT], desc[MAX_DESC];
            printf("New amount: "); read_line(amt_s, sizeof(amt_s)); double amt = atof(amt_s);
            printf("New date (yyyymmdd): "); read_line(date_s, sizeof(date_s)); int date = atoi(date_s);
            printf("New category: "); read_line(cat, sizeof(cat));
            printf("New description: "); read_line(desc, sizeof(desc));
            if (update_expense(id, amt, date, cat, desc)) printf("Updated.\n"); else printf("Update failed.\n");
        }
        else if (choice == 3) {
            char id_s[20];
            printf("Enter ID to delete: "); read_line(id_s, sizeof(id_s));
            int id = atoi(id_s);
            if (delete_expense(id)) printf("Deleted ID %d\n", id); else printf("Not found.\n");
        }
        else if (choice == 4) {
            display_all();
        }
        else if (choice == 5) {
            sort_by_amount(1);
            printf("Sorted by amount ascending.\n");
        }
        else if (choice == 6) {
            sort_by_date(1);
            printf("Sorted by date (newest first).\n");
        }
        else if (choice == 7) {
            if (save_user_expenses(username)) printf("Saved %d records for %s to %s\n", get_expense_count(), username, DATA_FILE);
            else printf("Save failed.\n");
        }
        else if (choice == 8) {
            /* save before exit */
            if (save_user_expenses(username)) printf("Saved before exit.\n");
            printf("Goodbye, %s!\n", username);
            break;
        }
        else if (choice == 9) {
            generate_report();
        }
        else {
            printf("Invalid option.\n");
        }
    }

    return 0;
}
