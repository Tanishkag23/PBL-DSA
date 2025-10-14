#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "expenses.h"
#include "file_io.h"  
#include "structures.h"

Expense expenses[MAX_EXPENSES];
int expense_count = 0;
int next_id_for_user = 1;
double current_income = 0.0; 
static char current_user[100] = {0};
static CategoryBudget budgets[MAX_EXPENSES];
static int budgets_count = 0;

void init_expenses_for_user(const char *username) {
    expense_count = 0;
    next_id_for_user = 1;
    current_income = 0.0;
    budgets_count = 0;
    if (username) {
        strncpy(current_user, username, sizeof(current_user)-1);
        current_user[sizeof(current_user)-1] = '\0';
    } else {
        current_user[0] = '\0';
    }
    load_user_expenses(current_user);
    load_user_income(current_user);
    load_user_budgets(current_user);
}

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

Expense* find_expense_by_id(int id) {
    for (int i = 0; i < expense_count; i++) {
        if (expenses[i].id == id) return &expenses[i];
    }
    return NULL;
}

int update_expense(int id, double amount, int date, const char *category, const char *desc) {
    Expense *e = find_expense_by_id(id);
    if (!e) return 0;
    e->amount = amount;
    e->date = date;
    if (category) { strncpy(e->category, category, MAX_CAT); e->category[MAX_CAT-1] = '\0'; }
    if (desc) { strncpy(e->description, desc, MAX_DESC); e->description[MAX_DESC-1] = '\0'; }
    return 1;
}

int delete_expense(int id) {
    int idx = -1;
    for (int i = 0; i < expense_count; i++) if (expenses[i].id == id) { idx = i; break; }
    if (idx == -1) return 0;
    for (int j = idx; j + 1 < expense_count; j++) expenses[j] = expenses[j+1];
    expense_count--;
    return 1;
}

/* --------- KMP-based description search --------- */
static void kmp_build_lps(const char *pat, int *lps) {
    int m = (int)strlen(pat);
    int len = 0; lps[0] = 0;
    for (int i = 1; i < m; ) {
        if (pat[i] == pat[len]) { lps[i++] = ++len; }
        else if (len != 0) { len = lps[len-1]; }
        else { lps[i++] = 0; }
    }
}

static int kmp_search(const char *text, const char *pat) {
    int n = (int)strlen(text);
    int m = (int)strlen(pat);
    if (m == 0) return 1;
    int *lps = (int*)malloc(sizeof(int)*m);
    if (!lps) return 0;
    kmp_build_lps(pat, lps);
    int i = 0, j = 0;
    int found = 0;
    while (i < n) {
        if (text[i] == pat[j]) { i++; j++; if (j == m) { found = 1; break; } }
        else if (j != 0) { j = lps[j-1]; }
        else { i++; }
    }
    free(lps);
    return found;
}

int search_description(const char *pattern) {
    int matches = 0;
    printf("\nID | Amount    | Date     | Category           | Description\n");
    printf("-----------------------------------------------------------------\n");
    for (int i = 0; i < expense_count; i++) {
        if (kmp_search(expenses[i].description, pattern)) {
            printf("%-3d| %-9.2f| %-9d| %-18s| %s\n",
                   expenses[i].id, expenses[i].amount, expenses[i].date, expenses[i].category, expenses[i].description);
            matches++;
        }
    }
    printf("\n");
    return matches;
}

/* --------- Category helpers --------- */
int count_by_category(const char *category) {
    int c = 0;
    for (int i = 0; i < expense_count; i++) if (strcmp(expenses[i].category, category) == 0) c++;
    return c;
}

void list_by_category(const char *category) {
    printf("\nID | Amount    | Date     | Category           | Description\n");
    printf("-----------------------------------------------------------------\n");
    for (int i = 0; i < expense_count; i++) {
        Expense *e = &expenses[i];
        if (strcmp(e->category, category) == 0) {
            printf("%-3d| %-9.2f| %-9d| %-18s| %s\n", e->id, e->amount, e->date, e->category, e->description);
        }
    }
    printf("\n");
}

/* --------- Date range total via binary search + prefix sums --------- */
static int cmp_date_only(const void *a, const void *b) {
    const Expense *ea = (const Expense*)a;
    const Expense *eb = (const Expense*)b;
    if (ea->date < eb->date) return -1;
    if (ea->date > eb->date) return 1;
    return 0;
}

double total_in_date_range(int from_yyyymmdd, int to_yyyymmdd) {
    if (expense_count == 0) return 0.0;
    Expense *copy = (Expense*)malloc(sizeof(Expense) * expense_count);
    if (!copy) return 0.0;
    memcpy(copy, expenses, sizeof(Expense)*expense_count);
    qsort(copy, expense_count, sizeof(Expense), cmp_date_only);
    double *prefix = (double*)malloc(sizeof(double)*(expense_count+1));
    if (!prefix) { free(copy); return 0.0; }
    prefix[0] = 0.0;
    for (int i = 0; i < expense_count; i++) prefix[i+1] = prefix[i] + copy[i].amount;
    int lo = 0, hi = expense_count;
    while (lo < hi) { int mid = (lo+hi)/2; if (copy[mid].date < from_yyyymmdd) lo = mid+1; else hi = mid; }
    int left = lo;
    lo = 0; hi = expense_count;
    while (lo < hi) { int mid = (lo+hi)/2; if (copy[mid].date <= to_yyyymmdd) lo = mid+1; else hi = mid; }
    int right = lo; 
    double ans = 0.0;
    if (left < right) ans = prefix[right] - prefix[left];
    free(prefix);
    free(copy);
    return ans;
}

/* --------- Top-K categories using min-heap --------- */
typedef struct { double total; char cat[MAX_CAT]; } CatTotal;
static void heap_swap(CatTotal *a, CatTotal *b) { CatTotal t = *a; *a = *b; *b = t; }
static void heap_up(CatTotal *h, int idx) {
    while (idx > 0) { int p = (idx-1)/2; if (h[p].total <= h[idx].total) break; heap_swap(&h[p], &h[idx]); idx = p; }
}
static void heap_down(CatTotal *h, int n, int idx) {
    while (1) { int l = idx*2+1, r = idx*2+2, s = idx; if (l<n && h[l].total < h[s].total) s = l; if (r<n && h[r].total < h[s].total) s = r; if (s==idx) break; heap_swap(&h[s], &h[idx]); idx = s; }
}

void top_k_categories(int k) {
    if (k <= 0) { printf("Invalid k\n"); return; }
    char cats[MAX_EXPENSES][MAX_CAT];
    double totals[MAX_EXPENSES];
    int ccount = 0;
    for (int i = 0; i < expense_count; i++) {
        Expense *e = &expenses[i];
        int pos = -1;
        for (int j = 0; j < ccount; j++) if (strcmp(cats[j], e->category) == 0) { pos = j; break; }
        if (pos == -1) { strncpy(cats[ccount], e->category, MAX_CAT); cats[ccount][MAX_CAT-1] = '\0'; totals[ccount] = e->amount; ccount++; }
        else { totals[pos] += e->amount; }
    }
    if (ccount == 0) { printf("No categories.\n"); return; }
    if (k > ccount) k = ccount;
    CatTotal *heap = (CatTotal*)malloc(sizeof(CatTotal)*k);
    int hsize = 0;
    for (int i = 0; i < ccount; i++) {
        CatTotal ct; ct.total = totals[i]; strncpy(ct.cat, cats[i], MAX_CAT); ct.cat[MAX_CAT-1] = '\0';
        if (hsize < k) { heap[hsize] = ct; heap_up(heap, hsize); hsize++; }
        else if (ct.total > heap[0].total) { heap[0] = ct; heap_down(heap, hsize, 0); }
    }
    CatTotal *out = (CatTotal*)malloc(sizeof(CatTotal)*hsize);
    int osz = 0;
    while (hsize > 0) { out[osz++] = heap[0]; heap[0] = heap[hsize-1]; hsize--; heap_down(heap, hsize, 0); }
    for (int i = osz-1; i >= 0; i--) printf("%s : %.2f\n", out[i].cat, out[i].total);
    free(out);
    free(heap);
}

/* --------- Budgets per category --------- */
void set_category_budget(const char *category, double amount) {
    if (!category) return;
    for (int i = 0; i < budgets_count; i++) if (strcmp(budgets[i].category, category) == 0) { budgets[i].budget = amount; return; }
    if (budgets_count < MAX_EXPENSES) {
        strncpy(budgets[budgets_count].category, category, MAX_CAT);
        budgets[budgets_count].category[MAX_CAT-1] = '\0';
        budgets[budgets_count].budget = amount;
        budgets_count++;
    }
}

double get_category_budget(const char *category) {
    for (int i = 0; i < budgets_count; i++) if (strcmp(budgets[i].category, category) == 0) return budgets[i].budget;
    return 0.0;
}

static double category_total(const char *category) {
    double s = 0.0; for (int i = 0; i < expense_count; i++) if (strcmp(expenses[i].category, category) == 0) s += expenses[i].amount; return s;
}

void show_budget_alerts() {
    for (int i = 0; i < budgets_count; i++) {
        double b = budgets[i].budget; if (b <= 0) continue;
        double t = category_total(budgets[i].category);
        if (t >= b) printf("Budget exceeded for %s: %.2f / %.2f\n", budgets[i].category, t, b);
        else if (t >= 0.8*b) printf("Budget warning for %s: %.2f / %.2f (>=80%%)\n", budgets[i].category, t, b);
    }
}

/* Helpers for file I/O budgets persistence */
int get_budgets(CategoryBudget *out, int max_items) {
    int n = budgets_count; if (n > max_items) n = max_items;
    for (int i = 0; i < n; i++) out[i] = budgets[i];
    return n;
}
void clear_budgets() { budgets_count = 0; }

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

void set_user_income(double income) {
    current_income = income;
    save_user_income(current_user);
}
double get_user_income() { return current_income; }

void generate_report() {
    if (strlen(current_user) == 0) {
        printf("No user loaded. Cannot generate report.\n");
        return;
    }

    char income_s[100];
    if (current_income <= 0.0) {
        printf("You have not set monthly income yet.\n");
        printf("Enter your total income / pocket money for the period: ");
        if (!fgets(income_s, sizeof(income_s), stdin)) { printf("Input error.\n"); return; }
        double income_val = atof(income_s);
        if (income_val > 0) {
            current_income = income_val;
            save_user_income(current_user);
        }
    }

    double income = current_income;

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
    } else if (balance < 2000.0) {
        printf("\n⚠️  Try to save more. Balance less than 2000.\n");
    } else if (balance < 3000.0) {
        printf("\n⚠️  Use money wisely. Balance less than 3000.\n");
    } else {
        printf("\n✅ Good: Your balance is %.2f (>= 3000).\n", balance);
    }

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
        } else if (balance < 2000.0) {
            fprintf(f, "\nTry to save more. Balance less than 2000.\n");
        } else if (balance < 3000.0) {
            fprintf(f, "\nUse money wisely. Balance less than 3000.\n");
        } else {
            fprintf(f, "\nGood: Balance >= 3000\n");
        }
        fclose(f);
        printf("\nReport saved to %s\n", filename);
    } else {
        printf("Failed to write report file %s\n", filename);
    }
}
