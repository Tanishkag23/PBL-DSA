#ifndef EXPENSES_H
#define EXPENSES_H

#include "structures.h"

void init_expenses_for_user(const char *username);

int add_expense(double amount, int date, const char *category, const char *desc);
int update_expense(int id, double amount, int date, const char *category, const char *desc);
int delete_expense(int id);
void display_all();

void sort_by_amount(int ascending); 
void sort_by_date(int newest_first); 

Expense* find_expense_by_id(int id);

int get_expense_count();

void set_user_income(double income);
double get_user_income();

void generate_report();

/* DSA enhancements */
int count_by_category(const char *category);
void list_by_category(const char *category);

/* KMP substring search in descriptions; returns number of matches printed */
int search_description(const char *pattern);

/* Date range total using binary search + prefix sums over a sorted copy */
double total_in_date_range(int from_yyyymmdd, int to_yyyymmdd);

/* Top-K categories by total spend using a min-heap */
void top_k_categories(int k);

/* Budgets per category */
void set_category_budget(const char *category, double amount);
double get_category_budget(const char *category);
void show_budget_alerts();

/* Budgets persistence */
void load_user_budgets(const char *username);
int save_user_budgets(const char *username);

/* Helpers for budgets I/O */
int get_budgets(CategoryBudget *out, int max_items);
void clear_budgets();

#endif
