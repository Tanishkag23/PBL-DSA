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

#endif
