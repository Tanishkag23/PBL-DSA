/* To compile:
   gcc -std=c99 -o expense_tracker main.c
   ./expense_tracker
   Or paste into any online C compiler.
   Note: C files must be compiled to run. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* simple HTTP server headers */
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ================== typedefs + globals ================== */

/* Transaction structure as specified */
typedef struct Transaction {
    int id;                    /* unique identifier */
    char username[30];         /* who owns the transaction */
    char date[11];             /* YYYY-MM-DD */
    char type[8];              /* "INCOME" or "EXPENSE" */
    char category[30];         /* category name */
    double amount;             /* amount >= 0 */
    char currency[4];          /* e.g., INR, USD */
    char description[100];     /* short text */
} Transaction;

/* Action structure for undo/redo stack */
typedef struct Action {
    char actionType[8];  /* "ADD", "DELETE", "EDIT" */
    Transaction before;  /* previous state (for EDIT/DELETE) */
    Transaction after;   /* new state (for EDIT/ADD) */
} Action;

/* Doubly linked list node storing transaction index */
typedef struct ListNode {
    int txIndex;                /* index into transactions array */
    struct ListNode *prev;      /* previous node */
    struct ListNode *next;      /* next node */
} ListNode;

/* BST node keyed by category with total spent and linked list of items */
typedef struct CategoryNode {
    char category[30];
    double totalSpent;          /* accumulated total spent for this category (expenses) */
    ListNode *head;             /* doubly linked list head */
    ListNode *tail;             /* doubly linked list tail */
    struct CategoryNode *left;
    struct CategoryNode *right;
} CategoryNode;

/* ======== Graph (Adjacency Matrix) ======== */
#define MAX_CATEGORIES 50
static int adj[MAX_CATEGORIES][MAX_CATEGORIES]; /* adjacency matrix weights */
static char graphCategories[MAX_CATEGORIES][30]; /* category names stored */
static int graphUsed[MAX_CATEGORIES];            /* flags for used slots */
static int graphNodeCount = 0;                   /* count of used nodes */

/* ======== Global configuration ======== */
#define MAX_TRANSACTIONS 1000
#define MAX_HISTORY 1000
#define MAX_RECURRING 100

static Transaction transactions[MAX_TRANSACTIONS];
static int transactionCount = 0;
static int nextId = 1; /* next id assigned to new transactions */

static Action undoStack[MAX_HISTORY];
static int undoTop = -1;
static Action redoStack[MAX_HISTORY];
static int redoTop = -1;

static Transaction recurringQueue[MAX_RECURRING];
static int rqFront = 0, rqRear = -1, rqCount = 0;

static char currentUser[30] = "";
static int isLoggedIn = 0;

/* File paths */
static const char *USERS_FILE = "users.txt";
static const char *EXPENSES_FILE = "expenses.txt";
static const char *INCOME_FILE = "income.txt";
static const char *RECURRING_FILE = "recurring.txt";
static const char *HISTORY_FILE = "history.txt";
static const char *BUDGETS_FILE = "budgets.txt"; /* username,category,amount */

/* ========= Forward declarations (used by HTTP section) ========= */
int login_user(const char *username, const char *password);
int signup_user(const char *username, const char *password);
int add_transaction(const Transaction *tIn);
int find_transaction_index_linear(int id);
int delete_transaction_by_id(int id, Transaction *outDeleted);
double parse_double(const char *buf, int *ok);

/* ================== Utility Helpers ================== */

/* Purpose: Trim trailing newline from a string produced by fgets.
   Inputs: char *s - string buffer
   Outputs: None (modifies buffer in-place)
   Time Complexity: O(n) where n is string length */
void trim_newline(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    if (len > 0 && s[len - 1] == '\n') s[len - 1] = '\0';
}

/* Purpose: Read a line safely from stdin into buffer.
   Inputs: char *buf - destination, int size - buffer size
   Outputs: None (fills buffer), ensures null-termination
   Time Complexity: O(n) for input length */
void read_line(char *buf, int size) {
    if (!buf || size <= 0) return;
    if (fgets(buf, size, stdin) == NULL) {
        buf[0] = '\0';
        return;
    }
    trim_newline(buf);
}

/* Purpose: Validate date format YYYY-MM-DD with basic checks.
   Inputs: const char *date
   Outputs: int (1 valid, 0 invalid)
   Time Complexity: O(1) */
int is_valid_date(const char *date) {
    if (!date) return 0;
    if (strlen(date) != 10) return 0;
    if (date[4] != '-' || date[7] != '-') return 0;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) continue;
        if (date[i] < '0' || date[i] > '9') return 0;
    }
    /* Simple range checks (not exhaustive) */
    int y = (date[0]-'0')*1000 + (date[1]-'0')*100 + (date[2]-'0')*10 + (date[3]-'0');
    int m = (date[5]-'0')*10 + (date[6]-'0');
    int d = (date[8]-'0')*10 + (date[9]-'0');
    if (y < 1900 || y > 2100) return 0;
    if (m < 1 || m > 12) return 0;
    if (d < 1 || d > 31) return 0; /* Simplified day check */
    return 1;
}

/* Purpose: Check non-empty string.
   Inputs: const char *s
   Outputs: int (1 if non-empty, else 0)
   Time Complexity: O(1) */
int is_non_empty(const char *s) {
    return s && s[0] != '\0';
}

/* Purpose: Parse integer from input buffer using sscanf safely.
   Inputs: const char *buf
   Outputs: int parsed integer, returns 0 if parse fails
   Time Complexity: O(1) */
int parse_int(const char *buf, int *ok) {
    int v = 0; *ok = 0;
    if (buf && sscanf(buf, "%d", &v) == 1) { *ok = 1; }
    return v;
}

/* Purpose: Parse double from input buffer using sscanf safely.
   Inputs: const char *buf
   Outputs: double parsed value, sets ok flag
   Time Complexity: O(1) */
double parse_double(const char *buf, int *ok) {
    double v = 0.0; *ok = 0;
    if (buf && sscanf(buf, "%lf", &v) == 1) { *ok = 1; }
    return v;
}

/* Purpose: Simple modulo-based string hash for indexing categories.
   Inputs: const char *s, int mod
   Outputs: int in [0, mod-1]
   Time Complexity: O(n) for string length */
int hashString(const char *s, int mod) {
    unsigned long sum = 0;
    for (int i = 0; s && s[i]; i++) sum += (unsigned long)(unsigned char)s[i];
    return (int)(sum % (unsigned long)mod);
}

/* ================== FILE OPS ================== */

/* Purpose: Ensure all required data files exist (create empty if missing).
   Inputs: None
   Outputs: None (creates files if needed)
   Time Complexity: O(1) */
void ensure_files_exist(void) {
    const char *files[] = {USERS_FILE, EXPENSES_FILE, INCOME_FILE, RECURRING_FILE, HISTORY_FILE, BUDGETS_FILE};
    for (int i = 0; i < 6; i++) {
        FILE *f = fopen(files[i], "r");
        if (!f) {
            f = fopen(files[i], "w");
            if (f) fclose(f);
        } else {
            fclose(f);
        }
    }
}

/* ================== Budgets storage ================== */
typedef struct BudgetRec {
    char username[30];
    char category[30];
    double amount;
} BudgetRec;

/* get budget amount for (user, category); returns -1 if none */
double get_budget_amount(const char *user, const char *category) {
    FILE *f = fopen(BUDGETS_FILE, "r");
    if (!f) return -1.0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        BudgetRec b; memset(&b, 0, sizeof(b));
        int scanned = sscanf(line, "%29[^,],%29[^,],%lf", b.username, b.category, &b.amount);
        if (scanned == 3) {
            if (strcmp(b.username, user) == 0 && strcmp(b.category, category) == 0) { fclose(f); return b.amount; }
        }
    }
    fclose(f);
    return -1.0;
}

/* set/update budget; if amount==0, remove */
int set_budget_amount(const char *user, const char *category, double amount) {
    /* Read all, rewrite */
    FILE *fr = fopen(BUDGETS_FILE, "r");
    BudgetRec list[256]; int n = 0; char line[256];
    if (fr) {
        while (fgets(line, sizeof(line), fr) && n < 256) {
            BudgetRec b; memset(&b, 0, sizeof(b));
            int scanned = sscanf(line, "%29[^,],%29[^,],%lf", b.username, b.category, &b.amount);
            if (scanned == 3) list[n++] = b;
        }
        fclose(fr);
    }
    /* modify in memory */
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(list[i].username, user) == 0 && strcmp(list[i].category, category) == 0) {
            found = 1;
            if (amount == 0.0) {
                /* remove by shifting */
                for (int j = i; j < n-1; j++) list[j] = list[j+1];
                n--;
            } else {
                list[i].amount = amount;
            }
            break;
        }
    }
    if (!found && amount > 0.0 && n < 256) {
        BudgetRec b; memset(&b, 0, sizeof(b));
        strncpy(b.username, user, sizeof(b.username)-1);
        strncpy(b.category, category, sizeof(b.category)-1);
        b.amount = amount;
        list[n++] = b;
    }
    /* write back */
    FILE *fw = fopen(BUDGETS_FILE, "w");
    if (!fw) return 0;
    for (int i = 0; i < n; i++) {
        fprintf(fw, "%s,%s,%.2f\n", list[i].username, list[i].category, list[i].amount);
    }
    fclose(fw);
    return 1;
}

/* produce budgets list for user as JSON array */
char* budgets_json(const char *user) {
    FILE *f = fopen(BUDGETS_FILE, "r");
    if (!f) { char *s = (char*)malloc(3); strcpy(s, "[]"); return s; }
    char *buf = (char*)malloc(8192); buf[0] = '\0'; strcat(buf, "[");
    int first = 1; char line[256];
    while (fgets(line, sizeof(line), f)) {
        BudgetRec b; memset(&b, 0, sizeof(b));
        int scanned = sscanf(line, "%29[^,],%29[^,],%lf", b.username, b.category, &b.amount);
        if (scanned == 3 && strcmp(b.username, user) == 0) {
            if (!first) strcat(buf, ","); first = 0;
            char item[256]; snprintf(item, sizeof(item), "{\"category\":\"%s\",\"amount\":%.2f}", b.category, b.amount);
            strcat(buf, item);
        }
    }
    fclose(f);
    strcat(buf, "]");
    return buf;
}

/* ================== Summaries ================== */
void compute_totals_for_user(const char *user, double *income, double *expense) {
    double ti = 0.0, te = 0.0;
    for (int i = 0; i < transactionCount; i++) {
        if (strcmp(transactions[i].username, user) == 0) {
            if (strcmp(transactions[i].type, "INCOME") == 0) ti += transactions[i].amount;
            else te += transactions[i].amount;
        }
    }
    if (income) *income = ti; if (expense) *expense = te;
}

/* category totals JSON for expenses only */
char* category_totals_json(const char *user) {
    typedef struct CT { char cat[30]; double sum; } CT;
    CT arr[128]; int n = 0;
    for (int i = 0; i < transactionCount; i++) {
        Transaction *t = &transactions[i];
        if (strcmp(t->username, user) != 0) continue;
        if (strcmp(t->type, "EXPENSE") != 0) continue;
        int idx = -1;
        for (int j = 0; j < n; j++) if (strcmp(arr[j].cat, t->category) == 0) { idx = j; break; }
        if (idx < 0) { idx = n++; strncpy(arr[idx].cat, t->category, sizeof(arr[idx].cat)-1); arr[idx].sum = 0.0; }
        arr[idx].sum += t->amount;
    }
    char *buf = (char*)malloc(8192); buf[0] = '\0'; strcat(buf, "[");
    for (int i = 0; i < n; i++) {
        if (i > 0) strcat(buf, ",");
        char item[256]; snprintf(item, sizeof(item), "{\"category\":\"%s\",\"total\":%.2f}", arr[i].cat, arr[i].sum);
        strcat(buf, item);
    }
    strcat(buf, "]");
    return buf;
}

/* ================== Tiny URL helpers ================== */
void url_decode(char *s) {
    if (!s) return;
    char *p = s; char *o = s;
    while (*p) {
        if (*p == '+') { *o++ = ' '; p++; }
        else if (*p == '%' && p[1] && p[2]) {
            int hi = (p[1] >= 'A' ? (p[1] & ~32) - 'A' + 10 : p[1] - '0');
            int lo = (p[2] >= 'A' ? (p[2] & ~32) - 'A' + 10 : p[2] - '0');
            *o++ = (char)((hi<<4) | lo); p += 3;
        } else { *o++ = *p++; }
    }
    *o = '\0';
}

int get_param(const char *query, const char *key, char *out, int outsz) {
    if (!query || !key || !out || outsz <= 0) return 0;
    const char *p = query;
    size_t klen = strlen(key);
    while (p && *p) {
        const char *eq = strstr(p, "=");
        const char *amp = strstr(p, "&");
        if (!eq) break;
        size_t keylen = (size_t)(eq - p);
        if (keylen == klen && strncmp(p, key, klen) == 0) {
            size_t valLen = amp ? (size_t)(amp - (eq + 1)) : strlen(eq + 1);
            if (valLen >= (size_t)outsz) valLen = (size_t)outsz - 1;
            strncpy(out, eq + 1, valLen); out[valLen] = '\0'; url_decode(out); return 1;
        }
        p = amp ? (amp + 1) : NULL;
    }
    return 0;
}

void send_http(int fd, const char *status, const char *ctype, const char *body) {
    char header[512];
    int blen = body ? (int)strlen(body) : 0;
    snprintf(header, sizeof(header), "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
             status ? status : "200 OK", ctype ? ctype : "text/plain", blen);
    send(fd, header, strlen(header), 0);
    if (blen > 0) send(fd, body, blen, 0);
}

int serve_file(int fd, const char *relpath) {
    char path[256]; snprintf(path, sizeof(path), "web/%s", relpath);
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc(sz);
    fread(buf, 1, sz, f); fclose(f);
    const char *ctype = "text/plain";
    if (strstr(relpath, ".html")) ctype = "text/html";
    else if (strstr(relpath, ".css")) ctype = "text/css";
    else if (strstr(relpath, ".js")) ctype = "application/javascript";
    send_http(fd, "200 OK", ctype, buf);
    free(buf);
    return 1;
}

void handle_api(int fd, const char *method, const char *path, const char *query, const char *body) {
    if (strcmp(path, "/api/login") == 0 && strcmp(method, "POST") == 0) {
        char u[32] = "", p[32] = "";
        if (body) {
            get_param(body, "username", u, sizeof(u));
            get_param(body, "password", p, sizeof(p));
        }
        int ok = login_user(u, p);
        if (ok) { isLoggedIn = 1; strncpy(currentUser, u, sizeof(currentUser)-1); }
        char resp[128]; snprintf(resp, sizeof(resp), "{\"ok\":%d,\"user\":\"%s\"}", ok, ok ? currentUser : "");
        send_http(fd, ok ? "200 OK" : "401 Unauthorized", "application/json", resp);
        return;
    }
    if (strcmp(path, "/api/signup") == 0 && strcmp(method, "POST") == 0) {
        char u[32] = "", p[32] = "";
        if (body) { get_param(body, "username", u, sizeof(u)); get_param(body, "password", p, sizeof(p)); }
        int ok = signup_user(u, p);
        char resp[128]; snprintf(resp, sizeof(resp), "{\"ok\":%d}", ok);
        send_http(fd, ok ? "200 OK" : "400 Bad Request", "application/json", resp);
        return;
    }
    if (strcmp(path, "/api/me") == 0) {
        char resp[128]; snprintf(resp, sizeof(resp), "{\"loggedIn\":%d,\"user\":\"%s\"}", isLoggedIn, currentUser);
        send_http(fd, "200 OK", "application/json", resp);
        return;
    }
    if (strcmp(path, "/api/dashboard") == 0) {
        double ti=0, te=0; compute_totals_for_user(currentUser, &ti, &te);
        char *cats = category_totals_json(currentUser);
        char bodybuf[8192]; snprintf(bodybuf, sizeof(bodybuf), "{\"income\":%.2f,\"totalExpenses\":%.2f,\"balance\":%.2f,\"byCategory\":%s}", ti, te, ti-te, cats);
        free(cats);
        send_http(fd, "200 OK", "application/json", bodybuf);
        return;
    }
    if (strcmp(path, "/api/expenses/list") == 0) {
        char filterCat[64] = "", filterDesc[64] = "";
        if (query) { get_param(query, "category", filterCat, sizeof(filterCat)); get_param(query, "desc", filterDesc, sizeof(filterDesc)); }
        char *buf = (char*)malloc(16384); buf[0] = '\0'; strcat(buf, "["); int first = 1;
        for (int i = 0; i < transactionCount; i++) {
            Transaction *t = &transactions[i];
            if (strcmp(t->username, currentUser) != 0) continue;
            if (strcmp(t->type, "EXPENSE") != 0) continue;
            if (is_non_empty(filterCat) && strcmp(t->category, filterCat) != 0) continue;
            if (is_non_empty(filterDesc) && !strstr(t->description, filterDesc)) continue;
            if (!first) strcat(buf, ","); first = 0;
            char item[512]; snprintf(item, sizeof(item), "{\"id\":%d,\"amount\":%.2f,\"date\":\"%s\",\"category\":\"%s\",\"description\":\"%s\"}", t->id, t->amount, t->date, t->category, t->description);
            strcat(buf, item);
        }
        strcat(buf, "]");
        send_http(fd, "200 OK", "application/json", buf);
        free(buf);
        return;
    }
    if (strcmp(path, "/api/expenses/add") == 0 && strcmp(method, "POST") == 0) {
        char amountStr[32] = "", dateStr[32] = "", category[64] = "", desc[128] = "", currency[8] = "INR";
        if (body) {
            get_param(body, "amount", amountStr, sizeof(amountStr));
            get_param(body, "date", dateStr, sizeof(dateStr));
            get_param(body, "category", category, sizeof(category));
            get_param(body, "description", desc, sizeof(desc));
            get_param(body, "currency", currency, sizeof(currency));
        }
        int okAmt=0; double amt = parse_double(amountStr, &okAmt);
        /* convert YYYYMMDD -> YYYY-MM-DD if needed */
        char dateFmt[16] = "";
        if (strlen(dateStr) == 8) {
            snprintf(dateFmt, sizeof(dateFmt), "%.4s-%.2s-%.2s", dateStr, dateStr+4, dateStr+6);
        } else {
            strncpy(dateFmt, dateStr, sizeof(dateFmt)-1);
        }
        Transaction t; memset(&t, 0, sizeof(t));
        strncpy(t.username, currentUser, sizeof(t.username)-1);
        strncpy(t.type, "EXPENSE", sizeof(t.type)-1);
        strncpy(t.date, dateFmt, sizeof(t.date)-1);
        strncpy(t.category, category, sizeof(t.category)-1);
        t.amount = amt; strncpy(t.currency, currency, sizeof(t.currency)-1);
        strncpy(t.description, desc, sizeof(t.description)-1);
        int ok = add_transaction(&t);
        char resp[128]; snprintf(resp, sizeof(resp), "{\"ok\":%d,\"id\":%d}", ok, ok ? (nextId-1) : -1);
        send_http(fd, ok ? "200 OK" : "400 Bad Request", "application/json", resp);
        return;
    }
    if (strcmp(path, "/api/transactions/delete") == 0) {
        char idStr[16] = ""; int okId = 0; int id = 0;
        if (query && get_param(query, "id", idStr, sizeof(idStr))) id = parse_int(idStr, &okId);
        Transaction removed; int ok = 0;
        if (okId) {
            int idx = find_transaction_index_linear(id);
            if (idx >= 0 && strcmp(transactions[idx].username, currentUser) == 0) ok = delete_transaction_by_id(id, &removed);
        }
        char resp[128]; snprintf(resp, sizeof(resp), "{\"ok\":%d}", ok);
        send_http(fd, ok ? "200 OK" : "400 Bad Request", "application/json", resp);
        return;
    }
    if (strcmp(path, "/api/budgets/list") == 0) {
        char *bj = budgets_json(currentUser);
        send_http(fd, "200 OK", "application/json", bj);
        free(bj);
        return;
    }
    if (strcmp(path, "/api/budgets/set") == 0 && strcmp(method, "POST") == 0) {
        char cat[64]="", amtStr[32]=""; if (body) { get_param(body, "category", cat, sizeof(cat)); get_param(body, "amount", amtStr, sizeof(amtStr)); }
        int okAmt=0; double amt=parse_double(amtStr,&okAmt); if (!okAmt || !is_non_empty(cat)) { send_http(fd, "400 Bad Request", "application/json", "{\"ok\":0}"); return; }
        int ok = set_budget_amount(currentUser, cat, amt);
        char resp[64]; snprintf(resp, sizeof(resp), "{\"ok\":%d}", ok);
        send_http(fd, ok?"200 OK":"400 Bad Request", "application/json", resp);
        return;
    }
    /* unknown */
    send_http(fd, "404 Not Found", "application/json", "{\"error\":\"Not found\"}");
}

void handle_client(int fd) {
    char req[16384]; int n = recv(fd, req, sizeof(req)-1, 0); if (n <= 0) { close(fd); return; }
    req[n] = '\0';
    /* parse first line */
    char method[8] = "", url[1024] = "", proto[16] = "";
    sscanf(req, "%7s %1023s %15s", method, url, proto);
    char *p = url; char *qmark = strchr(p, '?'); char path[1024] = ""; char query[1024] = "";
    if (qmark) { strncpy(path, p, (size_t)(qmark-p)); path[qmark-p]='\0'; strncpy(query, qmark+1, sizeof(query)-1); }
    else { strncpy(path, p, sizeof(path)-1); }
    /* body extraction */
    char *body = strstr(req, "\r\n\r\n"); if (body) body += 4; else body = NULL;
    if (strcmp(path, "/") == 0) { serve_file(fd, "index.html"); close(fd); return; }
    if (strcmp(path, "/styles.css") == 0) { serve_file(fd, "styles.css"); close(fd); return; }
    if (strcmp(path, "/app.js") == 0) { serve_file(fd, "app.js"); close(fd); return; }
    if (strncmp(path, "/api/", 5) == 0) { handle_api(fd, method, path, query[0]?query:NULL, body); close(fd); return; }
    send_http(fd, "404 Not Found", "text/plain", "Not found"); close(fd);
}

int start_server(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }
    int yes = 1; setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr)); addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_ANY); addr.sin_port = htons(port);
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); close(sock); return 1; }
    if (listen(sock, 16) < 0) { perror("listen"); close(sock); return 1; }
    printf("Server listening at http://127.0.0.1:%d/\n", port);
    while (1) {
        struct sockaddr_in cli; socklen_t clilen = sizeof(cli);
        int fd = accept(sock, (struct sockaddr*)&cli, &clilen);
        if (fd < 0) { perror("accept"); continue; }
        handle_client(fd);
    }
    return 0;
}

/* Purpose: Append a history record describing an action.
   Inputs: const char *actionLabel, const Transaction *a, const Transaction *b, const char *note
   Outputs: None (appends to history.txt)
   Time Complexity: O(1) per call */
void append_history(const char *actionLabel, const Transaction *a, const Transaction *b, const char *note) {
    FILE *f = fopen(HISTORY_FILE, "a");
    if (!f) return;
    fprintf(f, "%s|user=%s|A_id=%d|B_id=%d|note=%s\n",
            actionLabel ? actionLabel : "",
            currentUser,
            a ? a->id : -1,
            b ? b->id : -1,
            note ? note : "");
    fclose(f);
}

/* Purpose: Save all transactions to expenses.txt and income.txt immediately.
   Inputs: None
   Outputs: None (writes files)
   Time Complexity: O(n) where n is transactionCount */
void save_all_transactions(void) {
    FILE *fe = fopen(EXPENSES_FILE, "w");
    FILE *fi = fopen(INCOME_FILE, "w");
    if (!fe || !fi) { if (fe) fclose(fe); if (fi) fclose(fi); return; }
    for (int i = 0; i < transactionCount; i++) {
        Transaction *t = &transactions[i];
        FILE *out = (strcmp(t->type, "EXPENSE") == 0) ? fe : fi;
        fprintf(out, "%d,%s,%s,%s,%s,%.2f,%s,%s\n",
                t->id, t->username, t->date, t->type, t->category, t->amount, t->currency, t->description);
    }
    fclose(fe);
    fclose(fi);
}

/* Purpose: Load transactions from expenses.txt and income.txt into array.
   Inputs: None
   Outputs: None (fills transactions[], updates transactionCount and nextId)
   Time Complexity: O(n) for total lines */
void load_transactions(void) {
    transactionCount = 0;
    int maxId = 0;
    const char *files[2] = {EXPENSES_FILE, INCOME_FILE};
    for (int k = 0; k < 2; k++) {
        FILE *f = fopen(files[k], "r");
        if (!f) continue;
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            Transaction t; memset(&t, 0, sizeof(t));
            /* Parse CSV: id,username,date,type,category,amount,currency,description */
            int id; char username[30], date[11], type[8], category[30], currency[4], description[100];
            double amount;
            int scanned = sscanf(line, "%d,%29[^,],%10[^,],%7[^,],%29[^,],%lf,%3[^,],%99[^\n]",
                                 &id, username, date, type, category, &amount, currency, description);
            if (scanned == 8 && transactionCount < MAX_TRANSACTIONS) {
                t.id = id;
                strncpy(t.username, username, sizeof(t.username)-1);
                strncpy(t.date, date, sizeof(t.date)-1);
                strncpy(t.type, type, sizeof(t.type)-1);
                strncpy(t.category, category, sizeof(t.category)-1);
                t.amount = amount;
                strncpy(t.currency, currency, sizeof(t.currency)-1);
                strncpy(t.description, description, sizeof(t.description)-1);
                transactions[transactionCount++] = t;
                if (id > maxId) maxId = id;
            }
        }
        fclose(f);
    }
    nextId = maxId + 1;
}

/* Purpose: Load recurring queue from recurring.txt.
   Inputs: None
   Outputs: None (fills circular queue)
   Time Complexity: O(n) */
void load_recurring_queue(void) {
    rqFront = 0; rqRear = -1; rqCount = 0;
    FILE *f = fopen(RECURRING_FILE, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        Transaction t; memset(&t, 0, sizeof(t));
        int id; char username[30], date[11], type[8], category[30], currency[4], description[100];
        double amount;
        int scanned = sscanf(line, "%d,%29[^,],%10[^,],%7[^,],%29[^,],%lf,%3[^,],%99[^\n]",
                             &id, username, date, type, category, &amount, currency, description);
        if (scanned == 8 && rqCount < MAX_RECURRING) {
            t.id = id; /* id in recurring is placeholder; not used for actual tx */
            strncpy(t.username, username, sizeof(t.username)-1);
            strncpy(t.date, date, sizeof(t.date)-1);
            strncpy(t.type, type, sizeof(t.type)-1);
            strncpy(t.category, category, sizeof(t.category)-1);
            t.amount = amount;
            strncpy(t.currency, currency, sizeof(t.currency)-1);
            strncpy(t.description, description, sizeof(t.description)-1);
            rqRear = (rqRear + 1) % MAX_RECURRING;
            recurringQueue[rqRear] = t;
            rqCount++;
        }
    }
    fclose(f);
}

/* Purpose: Save recurring queue to recurring.txt.
   Inputs: None
   Outputs: None (writes file)
   Time Complexity: O(n) */
void save_recurring_queue(void) {
    FILE *f = fopen(RECURRING_FILE, "w");
    if (!f) return;
    for (int i = 0; i < rqCount; i++) {
        int idx = (rqFront + i) % MAX_RECURRING;
        Transaction *t = &recurringQueue[idx];
        fprintf(f, "%d,%s,%s,%s,%s,%.2f,%s,%s\n",
                t->id, t->username, t->date, t->type, t->category, t->amount, t->currency, t->description);
    }
    fclose(f);
}

/* ================== USER AUTH ================== */

/* Purpose: Check if a username already exists in users.txt.
   Inputs: const char *username
   Outputs: int (1 exists, 0 not found)
   Time Complexity: O(n) for number of users */
int user_exists(const char *username) {
    FILE *f = fopen(USERS_FILE, "r");
    if (!f) return 0;
    char line[128];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char u[30], p[30];
        int scanned = sscanf(line, "%29[^,],%29[^\n]", u, p);
        if (scanned == 2) {
            if (strcmp(u, username) == 0) { found = 1; break; }
        }
    }
    fclose(f);
    return found;
}

/* Purpose: Signup a new user (plain-text).
   Inputs: const char *username, const char *password
   Outputs: int (1 success, 0 fail if exists or file error)
   Time Complexity: O(n) for duplicates check */
int signup_user(const char *username, const char *password) {
    if (!is_non_empty(username) || !is_non_empty(password)) return 0;
    if (user_exists(username)) return 0;
    FILE *f = fopen(USERS_FILE, "a");
    if (!f) return 0;
    fprintf(f, "%s,%s\n", username, password);
    fclose(f);
    return 1;
}

/* Purpose: Login user against users.txt.
   Inputs: const char *username, const char *password
   Outputs: int (1 valid, 0 invalid)
   Time Complexity: O(n) for number of users */
int login_user(const char *username, const char *password) {
    FILE *f = fopen(USERS_FILE, "r");
    if (!f) return 0;
    char line[128];
    int ok = 0;
    while (fgets(line, sizeof(line), f)) {
        char u[30], p[30];
        int scanned = sscanf(line, "%29[^,],%29[^\n]", u, p);
        if (scanned == 2) {
            if (strcmp(u, username) == 0 && strcmp(p, password) == 0) { ok = 1; break; }
        }
    }
    fclose(f);
    return ok;
}

/* ================== ARRAYS / TRANSACTIONS ================== */

/* Purpose: Validate a Transaction's fields.
   Inputs: const Transaction *t
   Outputs: int (1 valid, 0 invalid)
   Time Complexity: O(1) */
int validate_transaction(const Transaction *t) {
    if (!t) return 0;
    if (!is_non_empty(t->username)) return 0;
    if (!is_valid_date(t->date)) return 0;
    if (!(strcmp(t->type, "INCOME") == 0 || strcmp(t->type, "EXPENSE") == 0)) return 0;
    if (!is_non_empty(t->category)) return 0;
    if (t->amount < 0.0) return 0;
    if (strlen(t->currency) != 3) return 0;
    return 1;
}

/* Purpose: Find transaction index by id using linear search.
   Inputs: int id
   Outputs: int index (>=0) or -1 if not found
   Time Complexity: O(n) */
int find_transaction_index_linear(int id) {
    for (int i = 0; i < transactionCount; i++) {
        if (transactions[i].id == id) return i;
    }
    return -1;
}

/* Purpose: Add a transaction (assigns new id) and persist immediately.
   Inputs: const Transaction *tIn
   Outputs: int (1 success, 0 fail)
   Time Complexity: O(1) amortized (array append) + O(n) save */
int add_transaction(const Transaction *tIn) {
    if (!tIn) return 0;
    Transaction t = *tIn;
    t.id = nextId++;
    if (!validate_transaction(&t)) return 0;
    if (transactionCount >= MAX_TRANSACTIONS) return 0;
    transactions[transactionCount++] = t;
    save_all_transactions();
    Action a; memset(&a, 0, sizeof(a)); strcpy(a.actionType, "ADD"); a.after = t;
    undoStack[++undoTop] = a; redoTop = -1; /* clear redo on new action */
    append_history("ADD", NULL, &t, "new transaction added");
    return 1;
}

/* Purpose: Add an existing transaction with its id (used in redo/undo).
   Inputs: const Transaction *t (id must be set)
   Outputs: int (1 success, 0 fail)
   Time Complexity: O(1) amortized + O(n) save */
int add_transaction_existing(const Transaction *t) {
    if (!t) return 0;
    Transaction x = *t;
    if (!validate_transaction(&x)) return 0;
    if (transactionCount >= MAX_TRANSACTIONS) return 0;
    transactions[transactionCount++] = x;
    if (x.id >= nextId) nextId = x.id + 1;
    save_all_transactions();
    return 1;
}

/* Purpose: Delete transaction by id and persist.
   Inputs: int id, Transaction *outDeleted
   Outputs: int (1 success, 0 fail)
   Time Complexity: O(n) due to search and shift */
int delete_transaction_by_id(int id, Transaction *outDeleted) {
    int idx = find_transaction_index_linear(id);
    if (idx < 0) return 0;
    Transaction removed = transactions[idx];
    for (int i = idx; i < transactionCount - 1; i++) {
        transactions[i] = transactions[i + 1];
    }
    transactionCount--;
    save_all_transactions();
    if (outDeleted) *outDeleted = removed;
    Action a; memset(&a, 0, sizeof(a)); strcpy(a.actionType, "DELETE"); a.before = removed;
    undoStack[++undoTop] = a; redoTop = -1;
    append_history("DELETE", &removed, NULL, "transaction deleted");
    return 1;
}

/* Purpose: Edit transaction by id and persist.
   Inputs: int id, const Transaction *newT, Transaction *outOld
   Outputs: int (1 success, 0 fail)
   Time Complexity: O(n) due to search */
int edit_transaction_by_id(int id, const Transaction *newT, Transaction *outOld) {
    int idx = find_transaction_index_linear(id);
    if (idx < 0 || !newT) return 0;
    Transaction before = transactions[idx];
    Transaction after = *newT; after.id = id;
    if (!validate_transaction(&after)) return 0;
    transactions[idx] = after;
    save_all_transactions();
    if (outOld) *outOld = before;
    Action a; memset(&a, 0, sizeof(a)); strcpy(a.actionType, "EDIT"); a.before = before; a.after = after;
    undoStack[++undoTop] = a; redoTop = -1;
    append_history("EDIT", &before, &after, "transaction edited");
    return 1;
}

/* Purpose: Compare transactions by date (YYYY-MM-DD)
   Inputs: const Transaction *a, const Transaction *b
   Outputs: int (<0 if a<b, 0 equal, >0 if a>b)
   Time Complexity: O(1) */
int compare_by_date(const Transaction *a, const Transaction *b) {
    return strcmp(a->date, b->date);
}

/* Purpose: Merge function for merge sort by date.
   Inputs: Transaction *arr, int l, int m, int r
   Outputs: None (arr sorted between l..r)
   Time Complexity: O(n) */
void merge_by_date(Transaction *arr, int l, int m, int r) {
    int n1 = m - l + 1, n2 = r - m;
    Transaction *L = (Transaction *)malloc(sizeof(Transaction) * n1);
    Transaction *R = (Transaction *)malloc(sizeof(Transaction) * n2);
    for (int i = 0; i < n1; i++) L[i] = arr[l + i];
    for (int j = 0; j < n2; j++) R[j] = arr[m + 1 + j];
    int i = 0, j = 0, k = l;
    while (i < n1 && j < n2) {
        if (compare_by_date(&L[i], &R[j]) <= 0) arr[k++] = L[i++];
        else arr[k++] = R[j++];
    }
    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];
    free(L); free(R);
}

/* Purpose: Merge sort transactions by date.
   Inputs: Transaction *arr, int l, int r
   Outputs: None (arr sorted)
   Time Complexity: O(n log n) */
void merge_sort_by_date(Transaction *arr, int l, int r) {
    if (l >= r) return;
    int m = l + (r - l) / 2;
    merge_sort_by_date(arr, l, m);
    merge_sort_by_date(arr, m + 1, r);
    merge_by_date(arr, l, m, r);
}

/* Purpose: Merge function for merge sort by amount.
   Inputs: Transaction *arr, int l, int m, int r
   Outputs: None
   Time Complexity: O(n) */
void merge_by_amount(Transaction *arr, int l, int m, int r) {
    int n1 = m - l + 1, n2 = r - m;
    Transaction *L = (Transaction *)malloc(sizeof(Transaction) * n1);
    Transaction *R = (Transaction *)malloc(sizeof(Transaction) * n2);
    for (int i = 0; i < n1; i++) L[i] = arr[l + i];
    for (int j = 0; j < n2; j++) R[j] = arr[m + 1 + j];
    int i = 0, j = 0, k = l;
    while (i < n1 && j < n2) {
        if (L[i].amount <= R[j].amount) arr[k++] = L[i++];
        else arr[k++] = R[j++];
    }
    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];
    free(L); free(R);
}

/* Purpose: Merge sort by amount.
   Inputs: Transaction *arr, int l, int r
   Outputs: None
   Time Complexity: O(n log n) */
void merge_sort_by_amount(Transaction *arr, int l, int r) {
    if (l >= r) return;
    int m = l + (r - l) / 2;
    merge_sort_by_amount(arr, l, m);
    merge_sort_by_amount(arr, m + 1, r);
    merge_by_amount(arr, l, m, r);
}

/* ================== STACK (UNDO/REDO) ================== */

/* Purpose: Push action onto undo stack.
   Inputs: const Action *a
   Outputs: None
   Time Complexity: O(1) */
void push_undo(const Action *a) {
    if (!a) return;
    if (undoTop < MAX_HISTORY - 1) undoStack[++undoTop] = *a;
}

/* Purpose: Pop action from undo stack.
   Inputs: int *ok - output flag
   Outputs: Action; sets ok to 1 if popped, else 0
   Time Complexity: O(1) */
Action pop_undo(int *ok) {
    Action a; memset(&a, 0, sizeof(a));
    if (undoTop >= 0) { *ok = 1; a = undoStack[undoTop--]; }
    else { *ok = 0; }
    return a;
}

/* Purpose: Push action onto redo stack.
   Inputs: const Action *a
   Outputs: None
   Time Complexity: O(1) */
void push_redo(const Action *a) {
    if (!a) return;
    if (redoTop < MAX_HISTORY - 1) redoStack[++redoTop] = *a;
}

/* Purpose: Pop action from redo stack.
   Inputs: int *ok
   Outputs: Action; sets ok accordingly
   Time Complexity: O(1) */
Action pop_redo(int *ok) {
    Action a; memset(&a, 0, sizeof(a));
    if (redoTop >= 0) { *ok = 1; a = redoStack[redoTop--]; }
    else { *ok = 0; }
    return a;
}

/* Purpose: Perform undo of last action. Updates files immediately.
   Inputs: None
   Outputs: None (prints status)
   Time Complexity: O(n) in worst case due to edits */
void perform_undo(void) {
    int ok = 0; Action a = pop_undo(&ok);
    if (!ok) { printf("Nothing to undo.\n"); return; }
    if (strcmp(a.actionType, "ADD") == 0) {
        Transaction removed;
        if (delete_transaction_by_id(a.after.id, &removed)) {
            push_redo(&a);
            append_history("UNDO_ADD", NULL, &a.after, "undo add");
            printf("Undo ADD: removed id %d\n", a.after.id);
        } else printf("Undo ADD failed.\n");
    } else if (strcmp(a.actionType, "DELETE") == 0) {
        if (add_transaction_existing(&a.before)) {
            push_redo(&a);
            append_history("UNDO_DELETE", &a.before, NULL, "undo delete");
            printf("Undo DELETE: restored id %d\n", a.before.id);
        } else printf("Undo DELETE failed.\n");
    } else if (strcmp(a.actionType, "EDIT") == 0) {
        Transaction old;
        if (edit_transaction_by_id(a.after.id, &a.before, &old)) {
            push_redo(&a);
            append_history("UNDO_EDIT", &a.before, &a.after, "undo edit");
            printf("Undo EDIT: reverted id %d\n", a.after.id);
        } else printf("Undo EDIT failed.\n");
    } else {
        printf("Unknown action for undo.\n");
    }
}

/* Purpose: Perform redo of last undone action.
   Inputs: None
   Outputs: None (prints status)
   Time Complexity: O(n) in worst case */
void perform_redo(void) {
    int ok = 0; Action a = pop_redo(&ok);
    if (!ok) { printf("Nothing to redo.\n"); return; }
    if (strcmp(a.actionType, "ADD") == 0) {
        if (add_transaction_existing(&a.after)) {
            push_undo(&a);
            append_history("REDO_ADD", NULL, &a.after, "redo add");
            printf("Redo ADD: re-added id %d\n", a.after.id);
        } else printf("Redo ADD failed.\n");
    } else if (strcmp(a.actionType, "DELETE") == 0) {
        Transaction removed;
        if (delete_transaction_by_id(a.before.id, &removed)) {
            push_undo(&a);
            append_history("REDO_DELETE", &a.before, NULL, "redo delete");
            printf("Redo DELETE: re-removed id %d\n", a.before.id);
        } else printf("Redo DELETE failed.\n");
    } else if (strcmp(a.actionType, "EDIT") == 0) {
        Transaction old;
        if (edit_transaction_by_id(a.before.id, &a.after, &old)) {
            push_undo(&a);
            append_history("REDO_EDIT", &a.before, &a.after, "redo edit");
            printf("Redo EDIT: re-applied id %d\n", a.before.id);
        } else printf("Redo EDIT failed.\n");
    } else {
        printf("Unknown action for redo.\n");
    }
}

/* ================== QUEUE (RECURRING) ================== */

/* Purpose: Enqueue a recurring payment (circular queue).
   Inputs: const Transaction *t
   Outputs: int (1 success, 0 fail if full)
   Time Complexity: O(1) */
int enqueue_recurring(const Transaction *t) {
    if (rqCount >= MAX_RECURRING) return 0;
    rqRear = (rqRear + 1) % MAX_RECURRING;
    recurringQueue[rqRear] = *t;
    rqCount++;
    save_recurring_queue();
    append_history("QUEUE_ENQ", t, NULL, "enqueued recurring");
    return 1;
}

/* Purpose: Dequeue next recurring payment.
   Inputs: Transaction *out (next item)
   Outputs: int (1 success, 0 fail if empty)
   Time Complexity: O(1) */
int dequeue_recurring(Transaction *out) {
    if (rqCount <= 0) return 0;
    Transaction t = recurringQueue[rqFront];
    rqFront = (rqFront + 1) % MAX_RECURRING;
    rqCount--;
    if (out) *out = t;
    save_recurring_queue();
    append_history("QUEUE_DEQ", &t, NULL, "dequeued recurring");
    return 1;
}

/* ================== LINKED LIST ================== */

/* Purpose: Build a doubly linked list of transactions for a given category (current user).
   Inputs: const char *category
   Outputs: ListNode* head (must be freed)
   Time Complexity: O(n) */
ListNode* build_list_for_category(const char *category) {
    ListNode *head = NULL, *tail = NULL;
    for (int i = 0; i < transactionCount; i++) {
        if (strcmp(transactions[i].username, currentUser) == 0 && strcmp(transactions[i].category, category) == 0) {
            ListNode *node = (ListNode *)malloc(sizeof(ListNode));
            node->txIndex = i; node->prev = tail; node->next = NULL;
            if (!head) head = node; else tail->next = node;
            tail = node;
        }
    }
    return head;
}

/* Purpose: Free a doubly linked list.
   Inputs: ListNode *head
   Outputs: None
   Time Complexity: O(k) where k is list length */
void free_list(ListNode *head) {
    ListNode *cur = head;
    while (cur) { ListNode *n = cur->next; free(cur); cur = n; }
}

/* ================== CATEGORY BST ================== */

/* Purpose: Create a new CategoryNode.
   Inputs: const char *category
   Outputs: CategoryNode *
   Time Complexity: O(1) */
CategoryNode* new_category_node(const char *category) {
    CategoryNode *n = (CategoryNode *)malloc(sizeof(CategoryNode));
    if (!n) return NULL;
    strncpy(n->category, category, sizeof(n->category)-1);
    n->category[sizeof(n->category)-1] = '\0';
    n->totalSpent = 0.0;
    n->head = NULL; n->tail = NULL;
    n->left = NULL; n->right = NULL;
    return n;
}

/* Purpose: Insert a transaction into BST under its category.
   Inputs: CategoryNode *root, const char *category, double amount, int txIndex
   Outputs: CategoryNode * (root possibly updated)
   Time Complexity: O(h) where h is tree height */
CategoryNode* bst_insert(CategoryNode *root, const char *category, double amount, int txIndex) {
    if (!root) {
        CategoryNode *n = new_category_node(category);
        if (!n) return NULL;
        /* attach first list node */
        ListNode *ln = (ListNode *)malloc(sizeof(ListNode));
        if (!ln) return n;
        ln->txIndex = txIndex; ln->prev = NULL; ln->next = NULL;
        n->head = n->tail = ln;
        /* Only expenses contribute to totalSpent */
        if (strcmp(transactions[txIndex].type, "EXPENSE") == 0) n->totalSpent += amount;
        return n;
    }
    int cmp = strcmp(category, root->category);
    if (cmp == 0) {
        ListNode *ln = (ListNode *)malloc(sizeof(ListNode));
        if (!ln) return root;
        ln->txIndex = txIndex; ln->prev = root->tail; ln->next = NULL;
        if (!root->head) root->head = ln; else root->tail->next = ln;
        root->tail = ln;
        if (strcmp(transactions[txIndex].type, "EXPENSE") == 0) root->totalSpent += amount;
    } else if (cmp < 0) {
        root->left = bst_insert(root->left, category, amount, txIndex);
    } else {
        root->right = bst_insert(root->right, category, amount, txIndex);
    }
    return root;
}

/* Purpose: Build BST from current user's transactions.
   Inputs: None
   Outputs: CategoryNode *root
   Time Complexity: O(n log n) average (insert per tx) */
CategoryNode* build_category_bst(void) {
    CategoryNode *root = NULL;
    for (int i = 0; i < transactionCount; i++) {
        if (strcmp(transactions[i].username, currentUser) == 0) {
            root = bst_insert(root, transactions[i].category, transactions[i].amount, i);
        }
    }
    return root;
}

/* Purpose: Inorder traversal of BST to display category summary.
   Inputs: CategoryNode *root
   Outputs: None (prints)
   Time Complexity: O(n) where n is number of nodes */
void bst_inorder_print(CategoryNode *root) {
    if (!root) return;
    bst_inorder_print(root->left);
    /* count items */
    int count = 0; for (ListNode *p = root->head; p; p = p->next) count++;
    printf("Category: %s | Items: %d | Total Spent: %.2f\n", root->category, count, root->totalSpent);
    bst_inorder_print(root->right);
}

/* Purpose: Compute total spent recursively.
   Inputs: CategoryNode *root
   Outputs: double total sum
   Time Complexity: O(n) */
double bst_total_sum(CategoryNode *root) {
    if (!root) return 0.0;
    return bst_total_sum(root->left) + root->totalSpent + bst_total_sum(root->right);
}

/* Purpose: Free BST and its attached lists.
   Inputs: CategoryNode *root
   Outputs: None
   Time Complexity: O(n) */
void bst_free(CategoryNode *root) {
    if (!root) return;
    bst_free(root->left);
    bst_free(root->right);
    free_list(root->head);
    free(root);
}

/* ================== GRAPH ================== */

/* Purpose: Clear graph data structures.
   Inputs: None
   Outputs: None
   Time Complexity: O(V^2) for adjacency reset */
void clear_graph(void) {
    for (int i = 0; i < MAX_CATEGORIES; i++) {
        graphUsed[i] = 0;
        graphCategories[i][0] = '\0';
        for (int j = 0; j < MAX_CATEGORIES; j++) adj[i][j] = 0;
    }
    graphNodeCount = 0;
}

/* Purpose: Get or add category index using hashed table with linear probing.
   Inputs: const char *category
   Outputs: int index in [0..MAX_CATEGORIES-1], or -1 if full
   Time Complexity: O(k) where k is probe length */
int get_or_add_category_index(const char *category) {
    if (!category || !category[0]) return -1;
    int idx = hashString(category, MAX_CATEGORIES);
    for (int step = 0; step < MAX_CATEGORIES; step++) {
        int i = (idx + step) % MAX_CATEGORIES;
        if (!graphUsed[i]) {
            /* insert */
            strncpy(graphCategories[i], category, sizeof(graphCategories[i])-1);
            graphCategories[i][sizeof(graphCategories[i])-1] = '\0';
            graphUsed[i] = 1; graphNodeCount++;
            return i;
        }
        if (strcmp(graphCategories[i], category) == 0) return i;
    }
    return -1;
}

/* Purpose: Build graph by connecting categories that occur on the same date (current user).
   Inputs: None
   Outputs: None (fills adjacency)
   Time Complexity: O(n) with simple sequential pairing */
void build_graph_from_transactions(void) {
    clear_graph();
    /* For each transaction, ensure node exists */
    for (int i = 0; i < transactionCount; i++) {
        if (strcmp(transactions[i].username, currentUser) == 0) {
            get_or_add_category_index(transactions[i].category);
        }
    }
    /* Connect categories that appear consecutively on the same date */
    for (int i = 1; i < transactionCount; i++) {
        if (strcmp(transactions[i].username, currentUser) == 0 &&
            strcmp(transactions[i-1].username, currentUser) == 0 &&
            strcmp(transactions[i].date, transactions[i-1].date) == 0) {
            int a = get_or_add_category_index(transactions[i-1].category);
            int b = get_or_add_category_index(transactions[i].category);
            if (a >= 0 && b >= 0 && a != b) { adj[a][b] += 1; adj[b][a] += 1; }
        }
    }
}

/* Purpose: DFS helper (moved out of graph_dfs to avoid nested function)
   Inputs: node index u, visited array
   Outputs: prints nodes in DFS order
   Time Complexity: O(V^2) due to adjacency scan */
void dfs_visit_rec(int u, int visited[]) {
    visited[u] = 1;
    printf("%s ", graphCategories[u]);
    for (int v = 0; v < MAX_CATEGORIES; v++) {
        if (graphUsed[v] && adj[u][v] > 0 && !visited[v]) dfs_visit_rec(v, visited);
    }
}

/* Purpose: DFS traversal starting from node 0.
   Inputs: None
   Outputs: None (prints order)
   Time Complexity: O(V^2) due to adjacency matrix */
void graph_dfs(void) {
    int visited[MAX_CATEGORIES] = {0};
    /* find a starting node */
    int start = -1;
    for (int i = 0; i < MAX_CATEGORIES; i++) { if (graphUsed[i]) { start = i; break; } }
    if (start == -1) { printf("(empty graph)\n"); return; }
    dfs_visit_rec(start, visited);
    printf("\n");
}

/* Purpose: BFS traversal starting from node 0.
   Inputs: None
   Outputs: None (prints order)
   Time Complexity: O(V^2) */
void graph_bfs(void) {
    int visited[MAX_CATEGORIES] = {0};
    int q[MAX_CATEGORIES]; int front = 0, rear = -1;
    int start = -1;
    for (int i = 0; i < MAX_CATEGORIES; i++) { if (graphUsed[i]) { start = i; break; } }
    if (start == -1) { printf("(empty graph)\n"); return; }
    visited[start] = 1; q[++rear] = start;
    while (front <= rear) {
        int u = q[front++];
        printf("%s ", graphCategories[u]);
        for (int v = 0; v < MAX_CATEGORIES; v++) {
            if (graphUsed[v] && adj[u][v] > 0 && !visited[v]) { visited[v] = 1; q[++rear] = v; }
        }
    }
    printf("\n");
}

/* Purpose: Prim's MST demo over the category graph.
   Inputs: None
   Outputs: int total weight of MST (0 if trivial)
   Time Complexity: O(V^2) */
int graph_prim_mst(void) {
    /* collect indices of used nodes into an array */
    int idx[MAX_CATEGORIES]; int n = 0;
    for (int i = 0; i < MAX_CATEGORIES; i++) if (graphUsed[i]) idx[n++] = i;
    if (n <= 1) return 0;
    int selected[MAX_CATEGORIES] = {0}; selected[0] = 1;
    int total = 0;
    for (int e = 0; e < n - 1; e++) {
        int bestU = -1, bestV = -1, bestW = 1000000;
        for (int i = 0; i < n; i++) if (selected[i]) {
            int u = idx[i];
            for (int j = 0; j < n; j++) if (!selected[j]) {
                int v = idx[j]; int w = adj[u][v];
                if (w > 0 && w < bestW) { bestW = w; bestU = i; bestV = j; }
            }
        }
        if (bestU != -1 && bestV != -1) { selected[bestV] = 1; total += bestW; }
    }
    return total;
}

/* Purpose: Show graph summary with DFS, BFS and Prim MST.
   Inputs: None
   Outputs: None (prints summary)
   Time Complexity: O(V^2) overall */
void show_graph_summary(void) {
    build_graph_from_transactions();
    printf("Graph Nodes (%d): ", graphNodeCount);
    for (int i = 0; i < MAX_CATEGORIES; i++) if (graphUsed[i]) printf("%s ", graphCategories[i]);
    printf("\nDFS: "); graph_dfs();
    printf("BFS: "); graph_bfs();
    int mst = graph_prim_mst();
    printf("Prim MST total weight: %d\n", mst);
}

/* ================== FILE OPS: REPORT GENERATION ================== */

/* Purpose: Export a simple report to report.txt for the current user.
   Inputs: None
   Outputs: None (writes file)
   Time Complexity: O(n) */
void export_report(void) {
    FILE *f = fopen("report.txt", "w");
    if (!f) { printf("Failed to write report.\n"); return; }
    double totalIncome = 0.0, totalExpense = 0.0;
    fprintf(f, "Report for %s\n", currentUser);
    fprintf(f, "ID,Date,Type,Category,Amount,Currency,Description\n");
    for (int i = 0; i < transactionCount; i++) {
        if (strcmp(transactions[i].username, currentUser) == 0) {
            fprintf(f, "%d,%s,%s,%s,%.2f,%s,%s\n",
                    transactions[i].id, transactions[i].date, transactions[i].type,
                    transactions[i].category, transactions[i].amount, transactions[i].currency,
                    transactions[i].description);
            if (strcmp(transactions[i].type, "INCOME") == 0) totalIncome += transactions[i].amount;
            else totalExpense += transactions[i].amount;
        }
    }
    fprintf(f, "\nTotal Income: %.2f\n", totalIncome);
    fprintf(f, "Total Expense: %.2f\n", totalExpense);
    fprintf(f, "Net: %.2f\n", totalIncome - totalExpense);
    fclose(f);
    printf("Report exported to report.txt\n");
}

/* ================== MENU HELPERS ================== */

/* Purpose: Prompt user for a transaction (common fields).
   Inputs: Transaction *t, const char *type ("INCOME" or "EXPENSE")
   Outputs: int (1 success, 0 fail)
   Time Complexity: O(1) */
int prompt_transaction(Transaction *t, const char *type) {
    if (!t || !type) return 0;
    memset(t, 0, sizeof(*t));
    strncpy(t->username, currentUser, sizeof(t->username)-1);
    strncpy(t->type, type, sizeof(t->type)-1);

    char buf[128];
    printf("Enter date (YYYY-MM-DD): "); read_line(buf, sizeof(buf));
    if (!is_valid_date(buf)) { printf("Invalid date format.\n"); return 0; }
    strncpy(t->date, buf, sizeof(t->date)-1);

    printf("Enter category: "); read_line(buf, sizeof(buf));
    if (!is_non_empty(buf)) { printf("Category cannot be empty.\n"); return 0; }
    strncpy(t->category, buf, sizeof(t->category)-1);

    printf("Enter amount (non-negative): "); read_line(buf, sizeof(buf));
    int ok = 0; double amt = parse_double(buf, &ok);
    if (!ok || amt < 0.0) { printf("Invalid amount.\n"); return 0; }
    t->amount = amt;

    printf("Enter currency (3 letters, e.g., INR): "); read_line(buf, sizeof(buf));
    if (strlen(buf) != 3) { printf("Currency must be 3 letters.\n"); return 0; }
    strncpy(t->currency, buf, sizeof(t->currency)-1);

    printf("Enter description: "); read_line(buf, sizeof(buf));
    strncpy(t->description, buf, sizeof(t->description)-1);
    return 1;
}

/* Purpose: Show all transactions for current user, sorted by date.
   Inputs: None
   Outputs: None (prints)
   Time Complexity: O(n log n) for sort + O(n) print */
void show_all_transactions(void) {
    /* copy to temp array */
    Transaction *temp = (Transaction *)malloc(sizeof(Transaction) * transactionCount);
    int cnt = 0;
    for (int i = 0; i < transactionCount; i++) if (strcmp(transactions[i].username, currentUser) == 0) temp[cnt++] = transactions[i];
    if (cnt == 0) { printf("No transactions.\n"); free(temp); return; }
    merge_sort_by_date(temp, 0, cnt - 1);
    printf("ID   Date       Type     Category           Amount  Curr  Description\n");
    printf("-----------------------------------------------------------------------\n");
    for (int i = 0; i < cnt; i++) {
        printf("%-4d %-10s %-7s %-18s %8.2f %-4s %s\n",
               temp[i].id, temp[i].date, temp[i].type, temp[i].category,
               temp[i].amount, temp[i].currency, temp[i].description);
    }
    free(temp);
}

/* Purpose: Show transactions by category using doubly linked list.
   Inputs: None
   Outputs: None (prints)
   Time Complexity: O(n) */
void show_by_category(void) {
    char cat[32]; printf("Enter category to show: "); read_line(cat, sizeof(cat));
    if (!is_non_empty(cat)) { printf("Category cannot be empty.\n"); return; }
    ListNode *head = build_list_for_category(cat);
    if (!head) { printf("No transactions found for category '%s'.\n", cat); return; }
    printf("Transactions for category '%s':\n", cat);
    for (ListNode *p = head; p; p = p->next) {
        Transaction *t = &transactions[p->txIndex];
        printf("%-4d %-10s %-7s %-18s %8.2f %-4s %s\n",
               t->id, t->date, t->type, t->category, t->amount, t->currency, t->description);
    }
    free_list(head);
}

/* ================== SEED SAMPLE DATA ================== */

/* Purpose: Seed sample users and transactions if files are empty.
   Inputs: None
   Outputs: None (adds demo data, persists immediately)
   Time Complexity: O(1) for checks, O(k) for inserts */
void seed_sample_data(void) {
    /* check users.txt empty */
    FILE *fu = fopen(USERS_FILE, "r");
    int hasUser = 0;
    if (fu) {
        char line[64];
        if (fgets(line, sizeof(line), fu)) hasUser = 1;
        fclose(fu);
    }
    if (!hasUser) {
        signup_user("demo", "demo");
        printf("Seeded user: demo/demo\n");
    }
    /* if no transactions, add some */
    if (transactionCount == 0) {
        char oldUser[30]; strncpy(oldUser, currentUser, sizeof(oldUser)-1);
        strncpy(currentUser, "demo", sizeof(currentUser)-1);
        Transaction t;
        /* incomes */
        memset(&t, 0, sizeof(t)); strcpy(t.type, "INCOME"); strcpy(t.username, currentUser);
        strcpy(t.date, "2024-01-10"); strcpy(t.category, "Scholarship"); t.amount = 5000; strcpy(t.currency, "INR"); strcpy(t.description, "Scholarship Jan"); add_transaction(&t);
        strcpy(t.date, "2024-02-10"); strcpy(t.category, "Scholarship"); t.amount = 5000; strcpy(t.description, "Scholarship Feb"); add_transaction(&t);
        /* expenses */
        memset(&t, 0, sizeof(t)); strcpy(t.type, "EXPENSE"); strcpy(t.username, currentUser);
        strcpy(t.date, "2024-01-12"); strcpy(t.category, "Food"); t.amount = 300; strcpy(t.currency, "INR"); strcpy(t.description, "Cafeteria"); add_transaction(&t);
        strcpy(t.date, "2024-01-15"); strcpy(t.category, "Books"); t.amount = 1200; strcpy(t.description, "Semester books"); add_transaction(&t);
        strcpy(t.date, "2024-02-01"); strcpy(t.category, "Transport"); t.amount = 150; strcpy(t.description, "Bus pass"); add_transaction(&t);
        strncpy(currentUser, oldUser, sizeof(currentUser)-1);
        printf("Seeded sample transactions for user 'demo'.\n");
    }
}

/* ================== MENU ACTIONS ================== */

/* Purpose: Handle adding income.
   Inputs: None
   Outputs: None
   Time Complexity: O(1) per input, O(n) save */
void menu_add_income(void) {
    Transaction t; if (!prompt_transaction(&t, "INCOME")) return; add_transaction(&t);
}

/* Purpose: Handle adding expense.
   Inputs: None
   Outputs: None
   Time Complexity: O(1) per input, O(n) save */
void menu_add_expense(void) {
    Transaction t; if (!prompt_transaction(&t, "EXPENSE")) return; add_transaction(&t);
}

/* Purpose: Handle editing a transaction by id.
   Inputs: None
   Outputs: None
   Time Complexity: O(n) */
void menu_edit_transaction(void) {
    char buf[64]; printf("Enter id to edit: "); read_line(buf, sizeof(buf)); int ok = 0; int id = parse_int(buf, &ok);
    if (!ok) { printf("Invalid id.\n"); return; }
    int idx = find_transaction_index_linear(id);
    if (idx < 0 || strcmp(transactions[idx].username, currentUser) != 0) { printf("ID not found for current user.\n"); return; }
    Transaction t; memset(&t, 0, sizeof(t));
    printf("Edit type (INCOME/EXPENSE), current: %s: ", transactions[idx].type); read_line(buf, sizeof(buf));
    if (!(strcmp(buf, "INCOME") == 0 || strcmp(buf, "EXPENSE") == 0)) { printf("Invalid type.\n"); return; }
    strncpy(t.type, buf, sizeof(t.type)-1);
    strncpy(t.username, currentUser, sizeof(t.username)-1);
    printf("Edit date (YYYY-MM-DD), current: %s: "); printf("%s\n", transactions[idx].date); read_line(buf, sizeof(buf));
    if (!is_valid_date(buf)) { printf("Invalid date.\n"); return; } strncpy(t.date, buf, sizeof(t.date)-1);
    printf("Edit category, current: %s: "); printf("%s\n", transactions[idx].category); read_line(buf, sizeof(buf));
    if (!is_non_empty(buf)) { printf("Category cannot be empty.\n"); return; } strncpy(t.category, buf, sizeof(t.category)-1);
    printf("Edit amount, current: %.2f: ", transactions[idx].amount); read_line(buf, sizeof(buf));
    int ok2 = 0; double amt = parse_double(buf, &ok2); if (!ok2 || amt < 0.0) { printf("Invalid amount.\n"); return; } t.amount = amt;
    printf("Edit currency, current: %s: ", transactions[idx].currency); read_line(buf, sizeof(buf)); if (strlen(buf) != 3) { printf("Currency must be 3 letters.\n"); return; } strncpy(t.currency, buf, sizeof(t.currency)-1);
    printf("Edit description, current: %s: ", transactions[idx].description); read_line(buf, sizeof(buf)); strncpy(t.description, buf, sizeof(t.description)-1);
    Transaction old; if (edit_transaction_by_id(id, &t, &old)) printf("Edited id %d successfully.\n", id);
}

/* Purpose: Handle deleting a transaction by id.
   Inputs: None
   Outputs: None
   Time Complexity: O(n) */
void menu_delete_transaction(void) {
    char buf[64]; printf("Enter id to delete: "); read_line(buf, sizeof(buf)); int ok = 0; int id = parse_int(buf, &ok);
    if (!ok) { printf("Invalid id.\n"); return; }
    int idx = find_transaction_index_linear(id);
    if (idx < 0 || strcmp(transactions[idx].username, currentUser) != 0) { printf("ID not found for current user.\n"); return; }
    Transaction removed; if (delete_transaction_by_id(id, &removed)) printf("Deleted id %d.\n", id);
}

/* Purpose: Add a recurring payment.
   Inputs: None
   Outputs: None
   Time Complexity: O(1) */
void menu_add_recurring(void) {
    Transaction t; if (!prompt_transaction(&t, "EXPENSE")) return; /* recurring usually expenses */
    if (!enqueue_recurring(&t)) printf("Recurring queue is full.\n"); else printf("Recurring payment enqueued.\n");
}

/* Purpose: Pay next recurring (dequeue then add as transaction).
   Inputs: None
   Outputs: None
   Time Complexity: O(1) */
void menu_pay_next_recurring(void) {
    Transaction t; if (!dequeue_recurring(&t)) { printf("No recurring payments queued.\n"); return; }
    /* assign current user to ensure ownership */
    strncpy(t.username, currentUser, sizeof(t.username)-1);
    if (add_transaction(&t)) printf("Paid recurring: added transaction id %d.\n", nextId-1);
}

/* Purpose: Show BST inorder for current user's categories.
   Inputs: None
   Outputs: None
   Time Complexity: O(n) */
void menu_show_bst(void) {
    CategoryNode *root = build_category_bst();
    if (!root) { printf("No data to build BST.\n"); return; }
    bst_inorder_print(root);
    double total = bst_total_sum(root);
    printf("Total spent across categories: %.2f\n", total);
    bst_free(root);
}

/* Purpose: Show graph summary.
   Inputs: None
   Outputs: None
   Time Complexity: O(V^2) */
void menu_show_graph(void) { show_graph_summary(); }

/* Purpose: Undo last action.
   Inputs: None
   Outputs: None
   Time Complexity: O(n) worst-case */
void menu_undo(void) { perform_undo(); }

/* Purpose: Redo last undone action.
   Inputs: None
   Outputs: None
   Time Complexity: O(n) worst-case */
void menu_redo(void) { perform_redo(); }

/* ================== LOGIN & MAIN MENU ================== */

/* Purpose: Show authenticated menu for the current user.
   Inputs: None
   Outputs: None (loops until logout/exit)
   Time Complexity: Interactive */
void show_menu(void) {
    while (isLoggedIn) {
        printf("\n===== Expense Tracker Menu =====\n");
        printf("1. Add income\n");
        printf("2. Add expense\n");
        printf("3. Edit transaction\n");
        printf("4. Delete transaction\n");
        printf("5. Show all\n");
        printf("6. Show by category\n");
        printf("7. Add recurring payment\n");
        printf("8. Pay next recurring\n");
        printf("9. Undo\n");
        printf("10. Redo\n");
        printf("11. Show BST (inorder)\n");
        printf("12. Show Graph summary\n");
        printf("13. Export report\n");
        printf("14. Logout\n");
        printf("15. Exit (auto-save)\n");
        printf("Select option: ");
        char buf[32]; read_line(buf, sizeof(buf)); int ok = 0; int choice = parse_int(buf, &ok); if (!ok) choice = -1;
        switch (choice) {
            case 1: menu_add_income(); break;
            case 2: menu_add_expense(); break;
            case 3: menu_edit_transaction(); break;
            case 4: menu_delete_transaction(); break;
            case 5: show_all_transactions(); break;
            case 6: show_by_category(); break;
            case 7: menu_add_recurring(); break;
            case 8: menu_pay_next_recurring(); break;
            case 9: menu_undo(); break;
            case 10: menu_redo(); break;
            case 11: menu_show_bst(); break;
            case 12: menu_show_graph(); break;
            case 13: export_report(); break;
            case 14:
                save_all_transactions(); save_recurring_queue();
                isLoggedIn = 0; currentUser[0] = '\0'; printf("Logged out.\n");
                break;
            case 15:
                save_all_transactions(); save_recurring_queue();
                printf("Exiting. Bye!\n"); exit(0);
            default: printf("Invalid option.\n");
        }
    }
}

/* Purpose: Authentication prompt loop for login/signup.
   Inputs: None
   Outputs: None
   Time Complexity: Interactive */
void auth_loop(void) {
    while (!isLoggedIn) {
        printf("\n===== Login / Signup =====\n");
        printf("1. Login\n");
        printf("2. Signup\n");
        printf("3. Exit\n");
        printf("Select option: ");
        char buf[64]; read_line(buf, sizeof(buf)); int ok = 0; int choice = parse_int(buf, &ok); if (!ok) choice = -1;
        if (choice == 1) {
            char u[32], p[32];
            printf("Username: "); read_line(u, sizeof(u));
            printf("Password: "); read_line(p, sizeof(p));
            if (login_user(u, p)) { isLoggedIn = 1; strncpy(currentUser, u, sizeof(currentUser)-1); printf("Login successful. Welcome, %s!\n", currentUser); }
            else printf("Invalid credentials.\n");
        } else if (choice == 2) {
            char u[32], p[32];
            printf("Choose username: "); read_line(u, sizeof(u));
            printf("Choose password: "); read_line(p, sizeof(p));
            if (signup_user(u, p)) printf("Signup successful. You can now login.\n");
            else printf("Signup failed (username may exist).\n");
        } else if (choice == 3) {
            printf("Goodbye!\n"); exit(0);
        } else {
            printf("Invalid option.\n");
        }
    }
}

/* ================== MENU & MAIN ================== */

/* Purpose: Program entry point. Loads data, seeds samples, runs auth and menu.
   Inputs: int argc, char **argv
   Outputs: int exit code
   Time Complexity: Depends on user interaction */
int main(void) {
    ensure_files_exist();
    load_transactions();
    load_recurring_queue();
    seed_sample_data();
    /* switch to HTTP server mode; login handled via API */
    isLoggedIn = 0; currentUser[0] = '\0';
    int port = 8000;
    start_server(port);
    /* on shutdown, save */
    save_all_transactions();
    save_recurring_queue();
    return 0;
}
