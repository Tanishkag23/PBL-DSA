// structures.h
#ifndef STRUCTURES_H
#define STRUCTURES_H

#define MAX_EXPENSES 1000
#define MAX_DESC 100
#define MAX_CAT 40
#define DATA_FILE "expenses.csv"
#define USERS_FILE "users.csv"
#define INCOMES_FILE "incomes.csv"
/* budgets storage per user,category */
#define BUDGETS_FILE "budgets.csv"

/* DSA enhancements */
#define CAT_HASH_SIZE 257

typedef struct {
    char category[MAX_CAT];
    double budget; /* 0 means no budget set */
} CategoryBudget;

typedef struct {
    int id;                 
    double amount;          
    int date;                
    char category[MAX_CAT];
    char description[MAX_DESC];
} Expense;

extern Expense expenses[MAX_EXPENSES];
extern int expense_count;
extern int next_id_for_user; 
extern double current_income;

#endif 
