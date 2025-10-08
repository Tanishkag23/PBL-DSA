// structures.h
#ifndef STRUCTURES_H
#define STRUCTURES_H

#define MAX_EXPENSES 1000
#define MAX_DESC 100
#define MAX_CAT 40
#define DATA_FILE "expenses.csv"
#define USERS_FILE "users.csv"

/* Expense record */
typedef struct {
    int id;                   /* unique id for this user's expenses (starts at 1) */
    double amount;            /* amount */
    int date;                 /* yyyymmdd integer (e.g., 20251008) */
    char category[MAX_CAT];
    char description[MAX_DESC];
} Expense;

/* Global arrays (defined in expenses.c) */
extern Expense expenses[MAX_EXPENSES];
extern int expense_count;
extern int next_id_for_user; /* next id to assign for the logged-in user */

#endif // STRUCTURES_H
