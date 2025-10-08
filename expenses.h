// expenses.h
#ifndef EXPENSES_H
#define EXPENSES_H

#include "structures.h"

/* Initialize globals for a particular user */
void init_expenses_for_user(const char *username);

/* Expense operations */
int add_expense(double amount, int date, const char *category, const char *desc);
int update_expense(int id, double amount, int date, const char *category, const char *desc);
int delete_expense(int id);
void display_all();

/* sorting */
void sort_by_amount(int ascending); /* 1 = asc, 0 = desc */
void sort_by_date(int newest_first); /* 1 = newest first */

/* search */
Expense* find_expense_by_id(int id);

/* helper: returns number of loaded expenses */
int get_expense_count();

/* Generate report (prompts for income, outputs category-wise totals, saves report) */
void generate_report();

#endif
