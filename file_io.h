#ifndef FILE_IO_H
#define FILE_IO_H


void load_user_expenses(const char *username);

int save_user_expenses(const char *username);

void load_user_income(const char *username);   
int save_user_income(const char *username);    

#endif
