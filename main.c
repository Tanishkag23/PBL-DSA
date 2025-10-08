#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "auth.h"
#include "expenses.h"
#include "file_io.h"
#include "structures.h"

static void read_line(char *buf, int size) {
    if (!fgets(buf, size, stdin)) { buf[0] = '\0'; return; }
    buf[strcspn(buf, "\n")] = '\0';
}

static void show_menu() {
    printf("\n=== Expense Tracker ===\n");
    printf("1. Add expense(s) and set monthly income\n");
    printf("2. Update expense\n");
    printf("3. Delete expense\n");
    printf("4. Display all expenses (your previous entries)\n");
    printf("5. Sort by amount (ascending)\n");
    printf("6. Sort by date (newest first)\n");
    printf("7. Save data\n");
    printf("8. Logout and Exit\n");
    printf("9. Generate Report\n");
    printf("10. Update monthly income\n");
    printf("Choose option: ");
}

int main() {
    char *username = auth_prompt();
    if (!username) {
        printf("Goodbye.\n");
        return 0;
    }

    init_expenses_for_user(username);
    printf("Loaded %d expense(s) for user %s. Next ID = %d. Current income = %.2f\n",
           get_expense_count(), username, next_id_for_user, get_user_income());

    while (1) {
        show_menu();
        char opt_str[16];
        read_line(opt_str, sizeof(opt_str));
        int choice = atoi(opt_str);

        if (choice == 1) {
            char ans[10];
            printf("Do you want to set/update monthly income? (y/n): ");
            read_line(ans, sizeof(ans));
            if (ans[0] == 'y' || ans[0] == 'Y') {
                char inc_s[50];
                printf("Enter monthly income/pocket money: ");
                read_line(inc_s, sizeof(inc_s));
                double inc = atof(inc_s);
                if (inc > 0) {
                    set_user_income(inc); 
                    printf("Income set to %.2f and saved.\n", inc);
                } else {
                    printf("Invalid income; skipped.\n");
                }
            }

            char n_s[20];
            printf("How many expense items do you want to add now? (enter 0 to cancel): ");
            read_line(n_s, sizeof(n_s));
            int n = atoi(n_s);
            double session_total = 0.0;
            for (int i = 0; i < n; i++) {
                char amt_s[50], date_s[20], cat[MAX_CAT], desc[MAX_DESC];
                printf("\nItem %d:\n", i+1);
                printf("Enter amount: "); read_line(amt_s, sizeof(amt_s)); double amt = atof(amt_s);
                printf("Enter date (yyyymmdd): "); read_line(date_s, sizeof(date_s)); int date = atoi(date_s);
                printf("Enter category (e.g., Rent, Food): "); read_line(cat, sizeof(cat));
                printf("Enter description: "); read_line(desc, sizeof(desc));
                int id = add_expense(amt, date, cat, desc);
                if (id == -1) {
                    printf("Failed to add expense (storage full).\n");
                    break;
                } else {
                    printf("Added expense ID %d\n", id);
                    session_total += amt;
                }
            }

            double total_expenses = 0.0;
            for (int i = 0; i < expense_count; i++) total_expenses += expenses[i].amount;
            double income = get_user_income();
            double balance = income - total_expenses;

            printf("\nSummary after additions:\n");
            printf("Monthly income: %.2f\n", income);
            printf("Total expenses (all recorded): %.2f\n", total_expenses);
            printf("Balance: %.2f\n", balance);

            if (balance < 1000.0) {
                printf("ALERT: Save money — balance < 1000.\n");
            } else if (balance < 2000.0) {
                printf("Suggestion: Try to save more — balance < 2000.\n");
            } else if (balance < 3000.0) {
                printf("Suggestion: Use money wisely — balance < 3000.\n");
            } else {
                printf("Good: You have healthy balance.\n");
            }
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
            printf("Enter ID to delete (see Display to view IDs): "); read_line(id_s, sizeof(id_s));
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
            if (save_user_income(username)) printf("Income saved to %s\n", INCOMES_FILE);
        }
        else if (choice == 8) {
            if (save_user_expenses(username)) printf("Saved before exit.\n");
            save_user_income(username);
            printf("Goodbye, %s!\n", username);
            break;
        }
        else if (choice == 9) {
            generate_report();
        }
        else if (choice == 10) {
            char inc_s[50];
            printf("Enter new monthly income to update: ");
            read_line(inc_s, sizeof(inc_s));
            double inc = atof(inc_s);
            if (inc > 0) {
                set_user_income(inc);
                printf("Income updated and saved as %.2f\n", inc);
            } else printf("Invalid income entered.\n");
        }
        else {
            printf("Invalid option.\n");
        }
    }

    return 0;
}
