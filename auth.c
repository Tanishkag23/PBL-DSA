#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "auth.h"
#include "structures.h"


static char logged_user[100] = {0};

static void chomp(char *s) {
    if (!s) return;
    s[strcspn(s, "\n")] = '\0';
}

static int find_user(const char *username, char *out_pass) {
    FILE *f = fopen(USERS_FILE, "r");
    if (!f) return 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *u = strtok(line, ",");
        char *p = strtok(NULL, "\n");
        if (!u || !p) continue;
        chomp(p);
        if (strcmp(u, username) == 0) {
            if (out_pass) strncpy(out_pass, p, 200);
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

static int save_user(const char *username, const char *password) {
    FILE *f = fopen(USERS_FILE, "a");
    if (!f) {
        printf("Failed to open users file for writing.\n");
        return 0;
    }
    fprintf(f, "%s,%s\n", username, password);
    fclose(f);
    return 1;
}

static void read_line(char *buf, int size) {
    if (!fgets(buf, size, stdin)) { buf[0] = '\0'; return; }
    chomp(buf);
}

char* auth_prompt() {
    char choice[10];
    printf("=== Welcome: Login or Signup ===\n");
    while (1) {
        printf("Type 'login' to sign in or 'signup' to create account (or 'exit' to quit): ");
        read_line(choice, sizeof(choice));
        if (strcmp(choice, "exit") == 0) return NULL;
        if (strcmp(choice, "signup") == 0) {
            char user[100], pass[100], pass2[100];
            printf("Choose a username: ");
            read_line(user, sizeof(user));
            if (strlen(user) == 0) { printf("Username cannot be empty.\n"); continue; }
            if (find_user(user, NULL)) { printf("Username already exists. Try login or pick another.\n"); continue; }
            printf("Choose a password: ");
            read_line(pass, sizeof(pass));
            printf("Confirm password: ");
            read_line(pass2, sizeof(pass2));
            if (strcmp(pass, pass2) != 0) { printf("Passwords do not match. Try again.\n"); continue; }
            if (save_user(user, pass)) {
                printf("Signup successful. You can now login.\n");
            } else {
                printf("Signup failed.\n");
            }
        } else if (strcmp(choice, "login") == 0) {
            char user[100], pass[100], stored_pass[200];
            printf("Username: ");
            read_line(user, sizeof(user));
            if (!find_user(user, stored_pass)) {
                printf("No such user. Try signup or try again.\n");
                continue;
            }
            printf("Password: ");
            read_line(pass, sizeof(pass));
            if (strcmp(pass, stored_pass) == 0) {
                strncpy(logged_user, user, sizeof(logged_user)-1);
                logged_user[sizeof(logged_user)-1] = '\0';
                printf("Login successful. Welcome, %s!\n\n", logged_user);
                return logged_user;
            } else {
                printf("Incorrect password. Try again.\n");
                continue;
            }
        } else {
            printf("Unknown option. Type 'login' or 'signup'.\n");
        }
    }
    return NULL;
}
