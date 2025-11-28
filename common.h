#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_DESC 100
#define MAX_CAT 50
#define MAX_TYPE 10

typedef struct {
    int day;
    int month;
    int year;
} Date;

typedef struct {
    int id;
    Date date;
    double amount;
    char type[MAX_TYPE];
    char category[MAX_CAT];
    char description[MAX_DESC];
} Transaction;

static inline Date createDate(int d, int m, int y) {
    Date date = {d, m, y};
    return date;
}

#endif
