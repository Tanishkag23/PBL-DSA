// file_io.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file_io.h"
#include "structures.h"
#include "expenses.h"

/* externs from expenses.c */
extern Expense expenses[];
extern int expense_count;
extern int next_id_for_user;

/* Helper: safe string chomp */
static void chomp(char *s) { if (!s) return; s[strcspn(s, "\n")] = '\0'; }

/* Expected CSV format per line:
   username,id,amount,date,category,description\n
*/

/* Load only lines that match username */
void load_user_expenses(const char *username) {
    expense_count = 0;
    next_id_for_user = 1;
    if (!username || strlen(username) == 0) return;

    FILE *f = fopen(DATA_FILE, "r");
    if (!f) return; /* no file yet */

    char line[1024];
    int max_id = 0;
    while (fgets(line, sizeof(line), f)) {
        char *cpy = strdup(line);
        char *u = strtok(cpy, ",");
        if (!u) { free(cpy); continue; }
        if (strcmp(u, username) == 0) {
            /* parse remaining fields */
            char *id_s = strtok(NULL, ",");
            char *amount_s = strtok(NULL, ",");
            char *date_s = strtok(NULL, ",");
            char *category = strtok(NULL, ",");
            char *description = strtok(NULL, "\n");
            if (!id_s || !amount_s || !date_s) { free(cpy); continue; }
            int id = atoi(id_s);
            double amount = atof(amount_s);
            int date = atoi(date_s);
            if (!category) category = "";
            if (!description) description = "";
            chomp(category); chomp(description);

            if (expense_count < MAX_EXPENSES) {
                Expense *e = &expenses[expense_count++];
                e->id = id;
                e->amount = amount;
                e->date = date;
                strncpy(e->category, category, MAX_CAT); e->category[MAX_CAT-1] = '\0';
                strncpy(e->description, description, MAX_DESC); e->description[MAX_DESC-1] = '\0';
                if (id > max_id) max_id = id;
            }
        }
        free(cpy);
    }
    fclose(f);
    next_id_for_user = max_id + 1;
    if (next_id_for_user < 1) next_id_for_user = 1;
}

/* Save: rewrite full file, but keep other users' records unchanged.
   Simple approach:
   - Read existing file and store lines for other users in memory
   - Write back all other-users lines, then write current user's expenses
*/
int save_user_expenses(const char *username) {
    /* collect other users' lines */
    FILE *f = fopen(DATA_FILE, "r");
    char **other_lines = NULL;
    size_t other_count = 0;
    if (f) {
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            /* check username at start */
            char *line_copy = strdup(line);
            char *u = strtok(line_copy, ",");
            if (!u) { free(line_copy); continue; }
            if (strcmp(u, username) != 0) {
                other_lines = realloc(other_lines, sizeof(char*)*(other_count+1));
                other_lines[other_count++] = strdup(line);
            }
            free(line_copy);
        }
        fclose(f);
    }

    /* open for write (overwrite) */
    f = fopen(DATA_FILE, "w");
    if (!f) {
        for (size_t i=0;i<other_count;i++) free(other_lines[i]);
        free(other_lines);
        return 0;
    }

    /* write other users' lines first */
    for (size_t i=0;i<other_count;i++) {
        fputs(other_lines[i], f);
        free(other_lines[i]);
    }
    free(other_lines);

    /* then write current user's expenses */
    for (int i = 0; i < expense_count; i++) {
        Expense *e = &expenses[i];
        /* username,id,amount,date,category,description\n */
        /* Note: no escaping of commas in text fields (keep simple) */
        fprintf(f, "%s,%d,%.2f,%d,%s,%s\n",
                username, e->id, e->amount, e->date, e->category, e->description);
    }

    fclose(f);
    return 1;
}
