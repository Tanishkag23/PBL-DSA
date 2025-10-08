// expenses.c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "expenses.h"
#include "file_io.h"   /* for load/save functions */
#include "structures.h"

/* globals */
Expense expenses[MAX_EXPENSES];
int expense_count = 0;
int next_id_for_user = 1;
static char current_user[100] = {0};

/* Initialize: clear arrays and set username */
void init_expenses_for_user(const char *username) {
    expense_count = 0;
    next_id_for_user = 1;
    if (username) {
        strncpy(current_user, username, sizeof(current_user)-1);
        current_user[sizeof(current_user)-1] = '\0';
    } else {
        current_user[0] = '\0';
    }
    /* Load user's expenses from file (file_io will populate expenses[] and expense_count and set next_id_for_user) */
    load_user_expenses(current_user); /* defined in file_io.c */
}

/* Add expense for current user */
int add_expense(double amount, int date, const char *category, const char *desc) {
    if (expense_count >= MAX_EXPENSES) return -1;
    Expense *e = &expenses[expense_count++];
    e->id = next_id_for_user++;
    e->amount = amount;
    e->date = date;
    strncpy(e->category, category ? category : "", MAX_CAT);
    e->category[MAX_CAT-1] = '\0';
    strncpy(e->description, desc ? desc : "", MAX_DESC);
    e->description[MAX_DESC-1] = '\0';
    return e->id;
}

/* Find by id */
Expense* find_expense_by_id(int id) {
    for (int i = 0; i < expense_count; i++) {
        if (expenses[i].id == id) return &expenses[i];
    }
    return NULL;
}

/* Update */
int update_expense(int id, double amount, int date, const char *category, const char *desc) {
    Expense *e = find_expense_by_id(id);
    if (!e) return 0;
    e->amount = amount;
    e->date = date;
    if (category) { strncpy(e->category, category, MAX_CAT); e->category[MAX_CAT-1] = '\0'; }
    if (desc) { strncpy(e->description, desc, MAX_DESC); e->description[MAX_DESC-1] = '\0'; }
    return 1;
}

/* Delete (compact array) */
int delete_expense(int id) {
    int idx = -1;
    for (int i = 0; i < expense_count; i++) if (expenses[i].id == id) { idx = i; break; }
    if (idx == -1) return 0;
    for (int j = idx; j + 1 < expense_count; j++) expenses[j] = expenses[j+1];
    expense_count--;
    return 1;
}

/* Display */
void display_all() {
    if (expense_count == 0) {
        printf("No expenses found for user %s.\n", current_user);
        return;
    }
    printf("\nID | Amount    | Date     | Category           | Description\n");
    printf("-----------------------------------------------------------------\n");
    for (int i = 0; i < expense_count; i++) {
        Expense *e = &expenses[i];
        printf("%-3d| %-9.2f| %-9d| %-18s| %s\n",
               e->id, e->amount, e->date, e->category, e->description);
    }
    printf("\n");
}

/* Sorting comparators */
static int cmp_amount_asc(const void *a, const void *b) {
    const Expense *ea = (const Expense*) a;
    const Expense *eb = (const Expense*) b;
    if (ea->amount < eb->amount) return -1;
    if (ea->amount > eb->amount) return 1;
    return 0;
}
static int cmp_amount_desc(const void *a, const void *b) { return -cmp_amount_asc(a,b); }
static int cmp_date_desc(const void *a, const void *b) {
    const Expense *ea = (const Expense*) a;
    const Expense *eb = (const Expense*) b;
    if (ea->date > eb->date) return -1;
    if (ea->date < eb->date) return 1;
    return 0;
}
static int cmp_date_asc(const void *a, const void *b) {
    return -cmp_date_desc(a,b);
}

/* Sort functions */
void sort_by_amount(int ascending) {
    if (expense_count <= 1) return;
    qsort(expenses, expense_count, sizeof(Expense), ascending ? cmp_amount_asc : cmp_amount_desc);
}
void sort_by_date(int newest_first) {
    if (expense_count <= 1) return;
    qsort(expenses, expense_count, sizeof(Expense), newest_first ? cmp_date_desc : cmp_date_asc);
}

int get_expense_count() {
    return expense_count;
}

/* ------------------ Report Generation ------------------
   - Prompts user for total income/pocket money
   - Aggregates expenses by category from in-memory expenses[]
   - Prints totals, balance
   - Suggests saving if balance < 1000
   - Writes report to <username>_report.txt
*/
void generate_report() {
    if (strlen(current_user) == 0) {
        printf("No user loaded. Cannot generate report.\n");
        return;
    }

    char income_s[100];
    double income = 0.0;
    printf("Enter your total income / pocket money for the period: ");
    if (!fgets(income_s, sizeof(income_s), stdin)) { printf("Input error.\n"); return; }
    income = atof(income_s);

    /* Aggregate categories */
    char categories[MAX_EXPENSES][MAX_CAT];
    double totals[MAX_EXPENSES];
    int cat_count = 0;

    double total_expenses = 0.0;
    for (int i = 0; i < expense_count; i++) {
        Expense *e = &expenses[i];
        total_expenses += e->amount;
        int found = -1;
        for (int j = 0; j < cat_count; j++) {
            if (strcmp(categories[j], e->category) == 0) { found = j; break; }
        }
        if (found == -1) {
            strncpy(categories[cat_count], e->category, MAX_CAT);
            categories[cat_count][MAX_CAT-1] = '\0';
            totals[cat_count] = e->amount;
            cat_count++;
        } else {
            totals[found] += e->amount;
        }
    }

    double balance = income - total_expenses;

    /* Print report to console */
    printf("\n===== Expense Report for %s =====\n", current_user);
    printf("Total Income / Pocket money: %.2f\n", income);
    printf("Total Expenses: %.2f\n", total_expenses);
    printf("Balance: %.2f\n", balance);
    printf("\nExpenses by category:\n");
    if (cat_count == 0) printf("  (No recorded expenses)\n");
    for (int i = 0; i < cat_count; i++) {
        printf("  %s : %.2f\n", categories[i], totals[i]);
    }

    if (balance < 1000.0) {
        double needed = 1000.0 - balance;
        printf("\n⚠️  Warning: Your balance is less than 1000.\n");
        printf("   You should save at least %.2f more to keep a 1000 buffer.\n", needed);
    } else {
        printf("\n✅ Good: Your balance is %.2f (>= 1000).\n", balance);
    }

    /* Save report to file */
    char filename[200];
    snprintf(filename, sizeof(filename), "%s_report.txt", current_user);
    FILE *f = fopen(filename, "w");
    if (f) {
        fprintf(f, "Expense Report for %s\n", current_user);
        fprintf(f, "Total Income: %.2f\n", income);
        fprintf(f, "Total Expenses: %.2f\n", total_expenses);
        fprintf(f, "Balance: %.2f\n\n", balance);
        fprintf(f, "Expenses by category:\n");
        for (int i = 0; i < cat_count; i++) {
            fprintf(f, "%s,%.2f\n", categories[i], totals[i]);
        }
        if (balance < 1000.0) {
            double needed = 1000.0 - balance;
            fprintf(f, "\nWarning: Balance less than 1000. Save at least %.2f\n", needed);
        } else {
            fprintf(f, "\nGood: Balance >= 1000\n");
        }
        fclose(f);
        printf("\nReport saved to %s\n", filename);
    } else {
        printf("Failed to write report file %s\n", filename);
    }
}
