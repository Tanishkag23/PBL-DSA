// file_io.h
#ifndef FILE_IO_H
#define FILE_IO_H

/* Loads expenses for given username into expenses[] (in expenses.c).
   This function is responsible for reading DATA_FILE, filtering rows for username,
   populating expenses[] and setting expense_count and next_id_for_user.
*/
void load_user_expenses(const char *username);

/* Saves current in-memory expenses[] for the username into DATA_FILE.
   For simplicity this function rewrites the whole file: it reads existing lines for other users
   and writes back them plus the current user's updated records.
*/
int save_user_expenses(const char *username);

#endif
