#ifndef FILE_IO_H
#define FILE_IO_H


void load_user_expenses(const char *username);

int save_user_expenses(const char *username);

void load_user_income(const char *username);   
int save_user_income(const char *username);    

/* budgets: username,category,budget */
void load_user_budgets(const char *username);
int save_user_budgets(const char *username);

#endif
