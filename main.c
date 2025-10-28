/*
  EXPENSE TRACKER (Very Beginner-Friendly)

  What this program does:
  - Starts a tiny web server at http://127.0.0.1:8000/
  - Serves the website files from the "web" folder
  - Stores data in simple text files (CSV-like)
  - Reloads data if files change, so manual edits reflect immediately

  Dashboard logic:
  - income = only your BASE_INCOME (one record per user)
  - totalExpenses = sum of your expenses
  - balance = income - totalExpenses
  - byCategory = expense totals by category

  How to build and run:
    1) Build: cc -O2 -Wall -Wextra -o expense_tracker main.c
    2) Run:   ./expense_tracker
    3) Open:  http://127.0.0.1:8000/

  Files used:
    - users.txt    : username,password
    - income.txt   : id,username,date,type,category,amount,currency,description
    - expenses.txt : same columns, but type is EXPENSE
    - budgets.txt  : username,category,amount
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <time.h>

/* ======================== Data ========================
   A single struct for both income and expense records.
*/
typedef struct Transaction {
    int id;                /* unique identifier */
    char username[30];     /* owner */
    char date[11];         /* YYYY-MM-DD */
    char type[8];          /* INCOME or EXPENSE */
    char category[30];
    double amount;         /* >=0 */
    char currency[4];      /* INR, USD */
    char description[100]; /* e.g., BASE_INCOME */
} Transaction;

#define MAX_TRANSACTIONS 2000
static Transaction transactions[MAX_TRANSACTIONS];
static int transactionCount = 0;
static int nextId = 1;

static char currentUser[30] = ""; /* set after login */
static int isLoggedIn = 0;

/* Files */
/* Use users.txt by default; support user.txt if present */
static const char *USERS_FILE_PRIMARY = "users.txt";     /* username,password */
static const char *USERS_FILE_ALT     = "user.txt";      /* optional alias */
static const char *EXPENSES_FILE = "expenses.txt";       /* CSV of EXPENSE */
static const char *INCOME_FILE   = "income.txt";         /* CSV of INCOME */
static const char *BUDGETS_FILE  = "budgets.txt";        /* username,category,amount */
static const char *HISTORY_FILE  = "history.txt";        /* time,user,action,detail */
static const char *RECURRING_FILE= "recurring.txt";      /* id,user,category,amount,currency,desc,period */

/* Track mtime to reflect external edits */
static time_t last_income_mtime = 0;
static time_t last_expenses_mtime = 0;

/* ======================== Helpers ========================
   Small helper functions to keep the code easy to read.
*/
static int is_non_empty(const char *s) { return s && s[0] != '\0'; }

static double parse_double(const char *buf, int *ok) {
    double v = 0.0; *ok = 0;
    if (buf && sscanf(buf, "%lf", &v) == 1) { *ok = 1; }
    return v;
}

/* Make sure all files exist. */
static int file_has_content(const char *path){
    FILE *f=fopen(path,"r"); if(!f) return 0; int c=fgetc(f); fclose(f); return c!=EOF;
}

static const char* users_file(void){
    /* Prefer user.txt if it exists and has content, otherwise use users.txt */
    if (file_has_content(USERS_FILE_ALT)) return USERS_FILE_ALT;
    return USERS_FILE_PRIMARY;
}

static void ensure_files_exist(void) {
    const char *files[] = {
        USERS_FILE_PRIMARY, USERS_FILE_ALT, EXPENSES_FILE,
        INCOME_FILE, BUDGETS_FILE, HISTORY_FILE, RECURRING_FILE
    };
    for (int i = 0; i < 4; i++) {
        FILE *f = fopen(files[i], "r");
        if (!f) { f = fopen(files[i], "w"); if (f) fclose(f); }
        else { fclose(f); }
    }
    for (int i = 4; i < 7; i++) {
        FILE *f = fopen(files[i], "r");
        if (!f) { f = fopen(files[i], "w"); if (f) fclose(f); }
        else { fclose(f); }
    }
}

/* Save in-memory transactions back to files. */
static void save_all_transactions(void) {
    FILE *fe = fopen(EXPENSES_FILE, "w");
    FILE *fi = fopen(INCOME_FILE, "w");
    if (!fe || !fi) { if (fe) fclose(fe); if (fi) fclose(fi); return; }
    for (int i = 0; i < transactionCount; i++) {
        Transaction *t = &transactions[i];
        FILE *out = (strcmp(t->type, "EXPENSE") == 0) ? fe : fi;
        fprintf(out, "%d,%s,%s,%s,%s,%.2f,%s,%s\n",
                t->id, t->username, t->date, t->type,
                t->category, t->amount, t->currency, t->description);
    }
    fclose(fe);
    fclose(fi);
}

/* Load transactions from files into memory. */
static void load_transactions(void) {
    transactionCount = 0;
    nextId = 1;
    char line[256];

    FILE *fi = fopen(INCOME_FILE, "r");
    if (fi) {
        while (fgets(line, sizeof(line), fi) && transactionCount < MAX_TRANSACTIONS) {
            Transaction t; memset(&t, 0, sizeof(t));
            int ok = sscanf(line,
                "%d,%29[^,],%10[^,],%7[^,],%29[^,],%lf,%3[^,],%99[^\n]",
                &t.id, t.username, t.date, t.type, t.category,
                &t.amount, t.currency, t.description);
            if (ok == 8) {
                transactions[transactionCount++] = t;
                if (t.id >= nextId) nextId = t.id + 1;
            }
        }
        fclose(fi);
    }

    FILE *fe = fopen(EXPENSES_FILE, "r");
    if (fe) {
        while (fgets(line, sizeof(line), fe) && transactionCount < MAX_TRANSACTIONS) {
            Transaction t; memset(&t, 0, sizeof(t));
            int ok = sscanf(line,
                "%d,%29[^,],%10[^,],%7[^,],%29[^,],%lf,%3[^,],%99[^\n]",
                &t.id, t.username, t.date, t.type, t.category,
                &t.amount, t.currency, t.description);
            if (ok == 8) {
                transactions[transactionCount++] = t;
                if (t.id >= nextId) nextId = t.id + 1;
            }
        }
        fclose(fe);
    }
}

/* Find an item in the array by id. */
static int find_index_by_id(int id) {
    for (int i = 0; i < transactionCount; i++) {
        if (transactions[i].id == id) return i;
    }
    return -1;
}

/* Add a new transaction and save. */
static int add_transaction(const Transaction *tIn) {
    if (!tIn || transactionCount >= MAX_TRANSACTIONS) return 0;
    Transaction t = *tIn;
    t.id = nextId++;
    transactions[transactionCount++] = t;
    save_all_transactions();
    return 1;
}

/* Edit an existing transaction and save. */
static int edit_transaction(int id, const Transaction *newT) {
    int idx = find_index_by_id(id);
    if (idx < 0 || !newT) return 0;
    Transaction t = *newT;
    t.id = id;
    transactions[idx] = t;
    save_all_transactions();
    return 1;
}

/* Delete a transaction by id and save. */
static int delete_transaction(int id) {
    int idx = find_index_by_id(id);
    if (idx < 0) return 0;
    for (int i = idx; i < transactionCount - 1; i++) {
        transactions[i] = transactions[i + 1];
    }
    transactionCount--;
    save_all_transactions();
    return 1;
}

/* ======================== Budgets ======================== */
typedef struct BudgetRec { char username[30]; char category[30]; double amount; } BudgetRec;

/* Set a budget amount for a user's category. */
static int set_budget_amount(const char *user, const char *category, double amount) {
    FILE *fr = fopen(BUDGETS_FILE, "r");
    BudgetRec list[256];
    int n = 0;
    char line[256];

    if (fr) {
        while (fgets(line, sizeof(line), fr) && n < 256) {
            BudgetRec b; memset(&b, 0, sizeof(b));
            int ok = sscanf(line, "%29[^,],%29[^,],%lf", b.username, b.category, &b.amount);
            if (ok == 3) list[n++] = b;
        }
        fclose(fr);
    }

    int found = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(list[i].username, user) == 0 && strcmp(list[i].category, category) == 0) {
            found = 1;
            if (amount == 0.0) {
                for (int j = i; j < n - 1; j++) list[j] = list[j + 1];
                n--;
            } else {
                list[i].amount = amount;
            }
            break;
        }
    }

    if (!found && amount > 0.0 && n < 256) {
        BudgetRec b; memset(&b, 0, sizeof(b));
        strncpy(b.username, user, sizeof(b.username) - 1);
        strncpy(b.category, category, sizeof(b.category) - 1);
        b.amount = amount;
        list[n++] = b;
    }

    FILE *fw = fopen(BUDGETS_FILE, "w");
    if (!fw) return 0;
    for (int i = 0; i < n; i++) fprintf(fw, "%s,%s,%.2f\n", list[i].username, list[i].category, list[i].amount);
    fclose(fw);
    return 1;
}

/* Return budgets for the given user as a JSON array (caller frees). */
static char* budgets_json(const char *user) {
    FILE *f = fopen(BUDGETS_FILE, "r");
    if (!f) { char *s = (char*)malloc(3); strcpy(s, "[]"); return s; }

    char *buf = (char*)malloc(8192);
    buf[0] = '\0';
    strcat(buf, "[");
    int first = 1;
    char line[256];

    while (fgets(line, sizeof(line), f)) {
        BudgetRec b; memset(&b, 0, sizeof(b));
        int ok = sscanf(line, "%29[^,],%29[^,],%lf", b.username, b.category, &b.amount);
        if (ok == 3 && strcmp(b.username, user) == 0) {
            if (!first) strcat(buf, ",");
            first = 0;
            char item[256];
            snprintf(item, sizeof(item), "{\"category\":\"%s\",\"amount\":%.2f}", b.category, b.amount);
            strcat(buf, item);
        }
    }
    fclose(f);
    strcat(buf, "]");
    return buf;
}

/* ======================== Totals ======================== */
static void compute_totals_for_user(const char *user, double *income, double *expense) {
    double ti = 0.0, te = 0.0;
    for (int i = 0; i < transactionCount; i++) {
        Transaction *t = &transactions[i];
        if (strcmp(t->username, user) != 0) continue;
        if (strcmp(t->type, "INCOME") == 0) ti += t->amount;
        else te += t->amount;
    }
    if (income) *income = ti;
    if (expense) *expense = te;
}

/* Get ONLY the base income (a single record per user). */
static double get_base_income_for_user(const char *user) {
    for (int i = transactionCount - 1; i >= 0; i--) {
        Transaction *t = &transactions[i];
        if (strcmp(t->username, user) == 0 &&
            strcmp(t->type, "INCOME") == 0 &&
            strcmp(t->description, "BASE_INCOME") == 0) {
            return t->amount;
        }
    }
    return 0.0;
}

/* Build a JSON array of category totals for EXPENSES (caller frees). */
static char* category_totals_json(const char *user) {
    typedef struct CT { char cat[30]; double sum; } CT;
    CT arr[128]; int n = 0;

    for (int i = 0; i < transactionCount; i++) {
        Transaction *t = &transactions[i];
        if (strcmp(t->username, user) != 0) continue;
        if (strcmp(t->type, "EXPENSE") != 0) continue;

        int idx = -1;
        for (int j = 0; j < n; j++) {
            if (strcmp(arr[j].cat, t->category) == 0) { idx = j; break; }
        }
        if (idx < 0) {
            idx = n++;
            strncpy(arr[idx].cat, t->category, sizeof(arr[idx].cat) - 1);
            arr[idx].sum = 0.0;
        }
        arr[idx].sum += t->amount;
    }

    char *buf = (char*)malloc(8192);
    buf[0] = '\0';
    strcat(buf, "[");
    for (int i = 0; i < n; i++) {
        if (i > 0) strcat(buf, ",");
        char item[256];
        snprintf(item, sizeof(item), "{\"category\":\"%s\",\"total\":%.2f}", arr[i].cat, arr[i].sum);
        strcat(buf, item);
    }
    strcat(buf, "]");
    return buf;
}

/* ======================== Tiny HTTP ========================
   Very small HTTP helpers: parse request, get parameters, and respond.
*/
static void url_decode(char *s){
    if(!s) return;
    char *p=s, *o=s;
    while(*p){
        if(*p=='+'){ *o++=' '; p++; }
        else if(*p=='%' && p[1] && p[2]){
            int hi=(p[1]>='A'?(p[1]&~32)-'A'+10:p[1]-'0');
            int lo=(p[2]>='A'?(p[2]&~32)-'A'+10:p[2]-'0');
            *o++=(char)((hi<<4)|lo);
            p+=3;
        } else { *o++=*p++; }
    }
    *o='\0';
}

/* Get "key=value" from query string or POST form body. */
static int get_param(const char *query, const char *key, char *out, int outsz){
    if(!query||!key||!out||outsz<=0) return 0;
    const char *p=query; size_t klen=strlen(key);
    while(p&&*p){
        const char *eq=strstr(p,"=");
        const char *amp=strstr(p,"&");
        if(!eq) break;
        size_t keylen=(size_t)(eq-p);
        if(keylen==klen && strncmp(p,key,klen)==0){
            size_t valLen=amp?(size_t)(amp-(eq+1)):strlen(eq+1);
            if(valLen>=(size_t)outsz) valLen=(size_t)outsz-1;
            strncpy(out,eq+1,valLen); out[valLen]='\0';
            url_decode(out); return 1;
        }
        p=amp?(amp+1):NULL;
    }
    return 0;
}

/* Send a simple HTTP response with a string body. */
static void send_http(int fd, const char *status, const char *ctype, const char *body){
    char header[512]; int blen=body?(int)strlen(body):0;
    snprintf(header,sizeof(header),
        "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
        status?status:"200 OK", ctype?ctype:"text/plain", blen);
    send(fd, header, strlen(header), 0);
    if (blen>0) send(fd, body, blen, 0);
}

/* Same as above but body length is already known. */
static void send_http_len(int fd, const char *status, const char *ctype, const char *body, int blen){
    char header[512]; if(blen<0) blen=0;
    snprintf(header,sizeof(header),
        "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
        status?status:"200 OK", ctype?ctype:"text/plain", blen);
    send(fd, header, strlen(header), 0);
    if(blen>0 && body) send(fd, body, blen, 0);
}

/* Serve a file from the "web" folder. */
static int serve_file(int fd, const char *relpath){
    char path[256]; snprintf(path,sizeof(path),"web/%s", relpath);
    FILE *f=fopen(path,"rb");
    if(!f){ send_http(fd, "404 Not Found", "text/plain", "Not found"); return 0; }
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    char *buf=(char*)malloc(sz>0?sz:1);
    size_t rd=fread(buf,1,sz,f);
    fclose(f);
    const char *ctype="text/plain";
    if(strstr(relpath,".html")) ctype="text/html";
    else if(strstr(relpath,".css")) ctype="text/css";
    else if(strstr(relpath,".js")) ctype="application/javascript";
    send_http_len(fd, "200 OK", ctype, buf, (int)rd);
    free(buf);
    return 1;
}

/* Auto-reload data when files change. */
static time_t get_mtime(const char *path){ struct stat st; if(stat(path,&st)==0) return st.st_mtime; return 0; }
static void reload_transactions_if_changed(void){
    time_t im=get_mtime(INCOME_FILE);
    time_t em=get_mtime(EXPENSES_FILE);
    if(im!=last_income_mtime || em!=last_expenses_mtime){
        load_transactions();
        last_income_mtime=im;
        last_expenses_mtime=em;
    }
}

/* ======================== API ========================
   Endpoints used by the website (web/app.js).
*/
static int signup_user(const char *username, const char *password){
    if(!is_non_empty(username)||!is_non_empty(password)) return 0;
    FILE *f=fopen(users_file(),"r"); char line[128];
    if(f){
        while(fgets(line,sizeof(line),f)){
            char u[32]="",p[32]="";
            if(sscanf(line,"%31[^,],%31[^\n]", u,p)==2){
                if(strcmp(u,username)==0){ fclose(f); return 0; }
            }
        }
        fclose(f);
    }
    f=fopen(users_file(),"a"); if(!f) return 0;
    fprintf(f, "%s,%s\n", username, password);
    fclose(f);
    return 1;
}

static int login_user(const char *username, const char *password){
    if(!is_non_empty(username)||!is_non_empty(password)) return 0;
    FILE *f=fopen(users_file(),"r"); if(!f) return 0; char line[128]; int ok=0;
    while(fgets(line,sizeof(line),f)){
        char u[32]="",p[32]="";
        if(sscanf(line,"%31[^,],%31[^\n]", u,p)==2){
            if(strcmp(u,username)==0 && strcmp(p,password)==0){ ok=1; break; }
        }
    }
    fclose(f);
    return ok;
}

/* ======================== History ======================== */
static void log_history(const char *user, const char *action, const char *detail){
    FILE *f=fopen(HISTORY_FILE,"a"); if(!f) return;
    time_t now=time(NULL); struct tm tm=*localtime(&now);
    char ts[32]; snprintf(ts,sizeof(ts),"%04d-%02d-%02d %02d:%02d:%02d",
        tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    fprintf(f, "%s,%s,%s,%s\n", ts, user?user:"", action?action:"", detail?detail:"");
    fclose(f);
}

static char* history_json(const char *user){
    FILE *f=fopen(HISTORY_FILE,"r"); if(!f){ char *s=(char*)malloc(3); strcpy(s,"[]"); return s; }
    char *buf=(char*)malloc(16384); buf[0]='\0'; strcat(buf,"["); int first=1;
    char line[512];
    while(fgets(line,sizeof(line),f)){
        char ts[32]="", u[32]="", act[64]="", det[256]="";
        int ok=sscanf(line, "%31[^,],%31[^,],%63[^,],%255[^\n]", ts,u,act,det);
        if(ok==4 && (!is_non_empty(user) || strcmp(u,user)==0)){
            if(!first) strcat(buf,","); first=0;
            char item[512]; snprintf(item,sizeof(item),"{\"time\":\"%s\",\"action\":\"%s\",\"detail\":\"%s\"}", ts, act, det);
            strcat(buf,item);
        }
    }
    fclose(f); strcat(buf,"]"); return buf;
}

static void handle_api(int fd, const char *method, const char *path, const char *query, const char *body){
    reload_transactions_if_changed();

    /* Login */
    if(strcmp(path,"/api/login")==0 && strcmp(method,"POST")==0){
        char u[32]="",p[32]="";
        if(body){ get_param(body,"username",u,sizeof(u)); get_param(body,"password",p,sizeof(p)); }
        int ok=login_user(u,p);
        if(ok){ isLoggedIn=1; strncpy(currentUser,u,sizeof(currentUser)-1); }
        log_history(ok?u:"", "login", ok?"success":"failure");
        char resp[128];
        snprintf(resp,sizeof(resp),"{\"ok\":%d,\"user\":\"%s\"}", ok, ok?currentUser:"");
        send_http(fd, ok?"200 OK":"401 Unauthorized", "application/json", resp);
        return;
    }

    /* Signup */
    if(strcmp(path,"/api/signup")==0 && strcmp(method,"POST")==0){
        char u[32]="",p[32]="";
        if(body){ get_param(body,"username",u,sizeof(u)); get_param(body,"password",p,sizeof(p)); }
        int ok=signup_user(u,p);
        log_history(u, "signup", ok?"created":"failed");
        char resp[64]; snprintf(resp,sizeof(resp),"{\"ok\":%d}", ok);
        send_http(fd, ok?"200 OK":"400 Bad Request", "application/json", resp);
        return;
    }

    /* Logout */
    if(strcmp(path,"/api/logout")==0 && strcmp(method,"POST")==0){
        log_history(currentUser, "logout", "ok");
        isLoggedIn=0; currentUser[0]='\0';
        send_http(fd, "200 OK", "application/json", "{\"ok\":1}");
        return;
    }

    /* Current user info */
    if(strcmp(path,"/api/me")==0){
        char resp[128];
        snprintf(resp,sizeof(resp),"{\"loggedIn\":%d,\"user\":\"%s\"}", isLoggedIn, currentUser);
        send_http(fd, "200 OK", "application/json", resp);
        return;
    }

    /* Dashboard */
    if(strcmp(path,"/api/dashboard")==0){
        double ti=0, te=0; compute_totals_for_user(currentUser,&ti,&te);
        double base=get_base_income_for_user(currentUser);
        char *cats=category_totals_json(currentUser);
        char bodybuf[8192];
        snprintf(bodybuf,sizeof(bodybuf),
            "{\"income\":%.2f,\"totalExpenses\":%.2f,\"balance\":%.2f,\"byCategory\":%s}",
            base, te, base-te, cats);
        free(cats);
        send_http(fd, "200 OK", "application/json", bodybuf);
        return;
    }

    /* List expenses with optional filters */
    if(strcmp(path,"/api/expenses/list")==0){
        char filterCat[64]="", filterDesc[64]="";
        if(query){ get_param(query,"category",filterCat,sizeof(filterCat)); get_param(query,"desc",filterDesc,sizeof(filterDesc)); }
        char *buf=(char*)malloc(16384); buf[0]='\0'; strcat(buf,"["); int first=1;
        for(int i=0;i<transactionCount;i++){
            Transaction *t=&transactions[i];
            if(strcmp(t->username,currentUser)!=0) continue;
            if(strcmp(t->type,"EXPENSE")!=0) continue;
            if(is_non_empty(filterCat)&&strcmp(t->category,filterCat)!=0) continue;
            if(is_non_empty(filterDesc)&&!strstr(t->description,filterDesc)) continue;
            if(!first) strcat(buf,","); first=0;
            char item[512];
            snprintf(item,sizeof(item),
                "{\"id\":%d,\"amount\":%.2f,\"date\":\"%s\",\"category\":\"%s\",\"description\":\"%s\"}",
                t->id, t->amount, t->date, t->category, t->description);
            strcat(buf,item);
        }
        strcat(buf,"]");
        send_http(fd, "200 OK", "application/json", buf);
        free(buf);
        return;
    }

    /* Add expense */
    if(strcmp(path,"/api/expenses/add")==0 && strcmp(method,"POST")==0){
        char amountStr[32]="", dateStr[32]="", category[64]="", desc[128]="", currency[8]="INR";
        if(body){
            get_param(body,"amount",amountStr,sizeof(amountStr));
            get_param(body,"date",dateStr,sizeof(dateStr));
            get_param(body,"category",category,sizeof(category));
            get_param(body,"description",desc,sizeof(desc));
            get_param(body,"currency",currency,sizeof(currency));
        }
        int okAmt=0; double amt=parse_double(amountStr,&okAmt);
        if(!okAmt || amt < 0.0 || !is_non_empty(category) || !is_non_empty(dateStr)){
            send_http(fd, "400 Bad Request", "application/json", "{\"ok\":0}");
            return;
        }
        char dateFmt[16]="";
        if(strlen(dateStr)==8){ /* YYYYMMDD -> YYYY-MM-DD */
            snprintf(dateFmt,sizeof(dateFmt),"%.4s-%.2s-%.2s", dateStr, dateStr+4, dateStr+6);
        } else {
            strncpy(dateFmt,dateStr,sizeof(dateFmt)-1);
        }
        Transaction t; memset(&t,0,sizeof(t));
        strncpy(t.username,currentUser,sizeof(t.username)-1);
        strncpy(t.type,"EXPENSE",sizeof(t.type)-1);
        strncpy(t.date,dateFmt,sizeof(t.date)-1);
        strncpy(t.category,category,sizeof(t.category)-1);
        t.amount=amt;
        strncpy(t.currency,currency,sizeof(t.currency)-1);
        strncpy(t.description,desc,sizeof(t.description)-1);
        int ok=add_transaction(&t);
        if(ok){ char det[128]; snprintf(det,sizeof(det),"expense %.2f %s", amt, category); log_history(currentUser, "add_expense", det); }
        char resp[128]; snprintf(resp,sizeof(resp),"{\"ok\":%d,\"id\":%d}", ok, ok?(nextId-1):-1);
        send_http(fd, ok?"200 OK":"400 Bad Request", "application/json", resp);
        return;
    }

    /* Delete by id */
    if(strcmp(path,"/api/transactions/delete")==0){
        char idStr[16]=""; if(query) get_param(query,"id",idStr,sizeof(idStr));
        int ok=0; int id=0; if(idStr[0]) ok=sscanf(idStr,"%d",&id)==1;
        if(!ok){ send_http(fd, "400 Bad Request", "application/json", "{\"ok\":0}"); return; }
        int okDel=delete_transaction(id);
        if(okDel){ char det[64]; snprintf(det,sizeof(det),"id=%d", id); log_history(currentUser, "delete_tx", det); }
        char resp[64]; snprintf(resp,sizeof(resp),"{\"ok\":%d}", okDel);
        send_http(fd, okDel?"200 OK":"400 Bad Request", "application/json", resp);
        return;
    }

    /* Budgets */
    if(strcmp(path,"/api/budgets/list")==0){
        char *bj=budgets_json(currentUser);
        send_http(fd, "200 OK", "application/json", bj);
        free(bj);
        return;
    }
    if(strcmp(path,"/api/budgets/set")==0 && strcmp(method,"POST")==0){
        char cat[64]="", amtStr[32]="";
        if(body){ get_param(body,"category",cat,sizeof(cat)); get_param(body,"amount",amtStr,sizeof(amtStr)); }
        int okAmt=0; double amt=parse_double(amtStr,&okAmt);
        if(!okAmt || !is_non_empty(cat)){
            send_http(fd, "400 Bad Request", "application/json", "{\"ok\":0}");
            return;
        }
        int ok=set_budget_amount(currentUser,cat,amt);
        if(ok){ char det[128]; snprintf(det,sizeof(det),"%s=%.2f", cat, amt); log_history(currentUser, "set_budget", det); }
        char resp[64]; snprintf(resp,sizeof(resp),"{\"ok\":%d}", ok);
        send_http(fd, ok?"200 OK":"400 Bad Request", "application/json", resp);
        return;
    }

    /* Set base income (overwrites single record) */
    if(strcmp(path,"/api/income/set")==0 && strcmp(method,"POST")==0){
        char amtStr[32]="", cat[64]="";
        if(body){ get_param(body,"amount",amtStr,sizeof(amtStr)); get_param(body,"category",cat,sizeof(cat)); }
        int okAmt=0; double amt=parse_double(amtStr,&okAmt);
        if(!okAmt || amt < 0.0){ send_http(fd, "400 Bad Request", "application/json", "{\"ok\":0}"); return; }
        if(!is_non_empty(cat)) strcpy(cat, "BASE");

        int idxFound=-1;
        for(int i=0;i<transactionCount;i++){
            Transaction *t=&transactions[i];
            if(strcmp(t->username,currentUser)==0 && strcmp(t->type,"INCOME")==0 && strcmp(t->description,"BASE_INCOME")==0){
                idxFound=i; break;
            }
        }
        int ok=0;
        if(idxFound>=0){
            Transaction newT=transactions[idxFound];
            newT.amount=amt;
            strncpy(newT.category,cat,sizeof(newT.category)-1);
            ok=edit_transaction(newT.id,&newT);
        } else {
            Transaction t; memset(&t,0,sizeof(t));
            strncpy(t.username,currentUser,sizeof(t.username)-1);
            strncpy(t.type,"INCOME",sizeof(t.type)-1);
            time_t now=time(NULL); struct tm tm=*localtime(&now); char today[11];
            snprintf(today,sizeof(today),"%04d-%02d-%02d", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday);
            strncpy(t.date,today,sizeof(t.date)-1);
            strncpy(t.category,cat,sizeof(t.category)-1);
            t.amount=amt;
            strncpy(t.currency,"INR",sizeof(t.currency)-1);
            strncpy(t.description,"BASE_INCOME",sizeof(t.description)-1);
            ok=add_transaction(&t);
        }
        if(ok){ char det[64]; snprintf(det,sizeof(det),"income=%.2f", amt); log_history(currentUser, "set_income", det); }
        char resp[128]; snprintf(resp,sizeof(resp),"{\"ok\":%d,\"income\":%.2f}", ok, amt);
        send_http(fd, ok?"200 OK":"400 Bad Request", "application/json", resp);
        return;
    }

    /* History */
    if(strcmp(path,"/api/history")==0){
        char *hj=history_json(currentUser);
        send_http(fd, "200 OK", "application/json", hj);
        free(hj);
        return;
    }

    /* ======================== Recurring ======================== */
    typedef struct RecurringRec { int id; char username[30]; char category[30]; double amount; char currency[4]; char description[100]; char period[16]; } RecurringRec;

    int handle_recurring_list=strcmp(path,"/api/recurring/list")==0;
    int handle_recurring_add =(strcmp(path,"/api/recurring/add")==0 && strcmp(method,"POST")==0);
    int handle_recurring_del =(strcmp(path,"/api/recurring/delete")==0);

    if(handle_recurring_list || handle_recurring_add || handle_recurring_del){
        /* Load all recurring entries into memory */
        RecurringRec list[512]; int n=0; int maxId=0; char line[512];
        FILE *fr=fopen(RECURRING_FILE,"r");
        if(fr){
            while(fgets(line,sizeof(line),fr) && n<512){
                RecurringRec r; memset(&r,0,sizeof(r));
                int ok=sscanf(line, "%d,%29[^,],%29[^,],%lf,%3[^,],%99[^,],%15[^\n]",
                    &r.id, r.username, r.category, &r.amount, r.currency, r.description, r.period);
                if(ok==7){ list[n++]=r; if(r.id>maxId) maxId=r.id; }
            }
            fclose(fr);
        }

        if(handle_recurring_list){
            char *buf=(char*)malloc(16384); buf[0]='\0'; strcat(buf,"["); int first=1;
            for(int i=0;i<n;i++){
                RecurringRec *r=&list[i]; if(strcmp(r->username,currentUser)!=0) continue;
                if(!first) strcat(buf,","); first=0;
                char item[512]; snprintf(item,sizeof(item),
                    "{\"id\":%d,\"category\":\"%s\",\"amount\":%.2f,\"currency\":\"%s\",\"description\":\"%s\",\"period\":\"%s\"}",
                    r->id, r->category, r->amount, r->currency, r->description, r->period);
                strcat(buf,item);
            }
            strcat(buf,"]");
            send_http(fd, "200 OK", "application/json", buf); free(buf); return;
        }

        if(handle_recurring_add){
            char cat[64]="", amtStr[32]="", desc[128]="", cur[8]="INR", period[16]="MONTHLY";
            if(body){
                get_param(body,"category",cat,sizeof(cat));
                get_param(body,"amount",amtStr,sizeof(amtStr));
                get_param(body,"description",desc,sizeof(desc));
                get_param(body,"currency",cur,sizeof(cur));
                get_param(body,"period",period,sizeof(period));
            }
            int okAmt=0; double amt=parse_double(amtStr,&okAmt);
            if(!okAmt || amt<=0.0 || !is_non_empty(cat)){
                send_http(fd, "400 Bad Request", "application/json", "{\"ok\":0}"); return;
            }
            RecurringRec r; memset(&r,0,sizeof(r));
            r.id=maxId+1; strncpy(r.username,currentUser,sizeof(r.username)-1);
            strncpy(r.category,cat,sizeof(r.category)-1);
            r.amount=amt; strncpy(r.currency,cur,sizeof(r.currency)-1);
            strncpy(r.description,desc,sizeof(r.description)-1);
            strncpy(r.period,period,sizeof(r.period)-1);
            list[n++]=r;
            FILE *fw=fopen(RECURRING_FILE,"w"); if(!fw){ send_http(fd, "500 Internal Server Error", "application/json", "{\"ok\":0}"); return; }
            for(int i=0;i<n;i++) fprintf(fw, "%d,%s,%s,%.2f,%s,%s,%s\n", list[i].id, list[i].username, list[i].category, list[i].amount, list[i].currency, list[i].description, list[i].period);
            fclose(fw);
            log_history(currentUser, "recurring_add", cat);
            char resp[64]; snprintf(resp,sizeof(resp),"{\"ok\":1,\"id\":%d}", r.id);
            send_http(fd, "200 OK", "application/json", resp); return;
        }

        if(handle_recurring_del){
            char idStr[16]=""; if(query) get_param(query,"id",idStr,sizeof(idStr));
            int ok=0; int id=0; if(idStr[0]) ok=sscanf(idStr,"%d",&id)==1; if(!ok){ send_http(fd, "400 Bad Request", "application/json", "{\"ok\":0}"); return; }
            int found=-1; for(int i=0;i<n;i++){ if(list[i].id==id && strcmp(list[i].username,currentUser)==0){ found=i; break; } }
            if(found<0){ send_http(fd, "404 Not Found", "application/json", "{\"ok\":0}"); return; }
            for(int i=found;i<n-1;i++) list[i]=list[i+1]; n--;
            FILE *fw=fopen(RECURRING_FILE,"w"); if(!fw){ send_http(fd, "500 Internal Server Error", "application/json", "{\"ok\":0}"); return; }
            for(int i=0;i<n;i++) fprintf(fw, "%d,%s,%s,%.2f,%s,%s,%s\n", list[i].id, list[i].username, list[i].category, list[i].amount, list[i].currency, list[i].description, list[i].period);
            fclose(fw);
            log_history(currentUser, "recurring_delete", idStr);
            send_http(fd, "200 OK", "application/json", "{\"ok\":1}"); return;
        }
    }

    /* Not found */
    send_http(fd, "404 Not Found", "application/json", "{\"error\":\"Not found\"}");
}

/* ======================== Server ========================
   Accept one connection at a time and handle it.
*/
static void handle_client(int fd){
    char req[16384];
    int n=recv(fd, req, sizeof(req)-1, 0);
    if(n<=0){ close(fd); return; }
    req[n]='\0';

    char method[8]="", url[1024]="", proto[16]="";
    sscanf(req, "%7s %1023s %15s", method, url, proto);

    char *p=url; char *qmark=strchr(p,'?');
    char path[1024]=""; char query[1024]="";
    if(qmark){ strncpy(path,p,(size_t)(qmark-p)); path[qmark-p]='\0'; strncpy(query,qmark+1,sizeof(query)-1);} else { strncpy(path,p,sizeof(path)-1);} 

    char *body=NULL; char *bodyStart=strstr(req,"\r\n\r\n");
    int content_len=0; char *clp=strstr(req,"Content-Length:");
    if(clp){ clp+=strlen("Content-Length:"); while(*clp==' '||*clp=='\t') clp++; content_len=atoi(clp); if(content_len<0) content_len=0; }

    char *body_alloc=NULL;
    if(bodyStart){
        body=bodyStart+4;
        int already=n-(int)(body-req);
        if(strcmp(method,"POST")==0 && content_len>0 && already<content_len){
            body_alloc=(char*)malloc(content_len+1);
            if(already>0) memcpy(body_alloc, body, already);
            int got=already;
            while(got<content_len){ int m=recv(fd, body_alloc+got, content_len-got, 0); if(m<=0) break; got+=m; }
            body_alloc[got]='\0'; body=body_alloc;
        } else if(strcmp(method,"POST")==0 && content_len>0){
            body_alloc=(char*)malloc(content_len+1);
            memcpy(body_alloc, body, content_len);
            body_alloc[content_len]='\0'; body=body_alloc;
        }
    }

    /* Static files */
    if(strcmp(path,"/")==0){ serve_file(fd, "index.html"); close(fd); if(body_alloc) free(body_alloc); return; }
    if(strcmp(path,"/styles.css")==0){ serve_file(fd, "styles.css"); close(fd); if(body_alloc) free(body_alloc); return; }
    if(strcmp(path,"/app.js")==0){ serve_file(fd, "app.js"); close(fd); if(body_alloc) free(body_alloc); return; }

    /* API */
    if(strncmp(path,"/api/",5)==0){ handle_api(fd, method, path, query[0]?query:NULL, body); close(fd); if(body_alloc) free(body_alloc); return; }

    /* Default */
    send_http(fd, "404 Not Found", "text/plain", "Not found");
    close(fd);
    if(body_alloc) free(body_alloc);
}

/* Start the server loop on 127.0.0.1:port. */
static int start_server(int port){
    int sock=socket(AF_INET, SOCK_STREAM, 0); if(sock<0){ perror("socket"); return 1; }
    int yes=1; setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr; memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET; addr.sin_port=htons(port);
    addr.sin_addr.s_addr=inet_addr("127.0.0.1");
    if(bind(sock,(struct sockaddr*)&addr,sizeof(addr))<0){ perror("bind"); close(sock); return 1; }
    if(listen(sock, 16)<0){ perror("listen"); close(sock); return 1; }
    printf("Server listening at http://127.0.0.1:%d/\n", port);
    while(1){ int client=accept(sock,NULL,NULL); if(client<0){ perror("accept"); continue; } handle_client(client); }
    close(sock); return 0;
}

/* Program entry point. */
int main(void){
    ensure_files_exist();
    load_transactions();
    last_income_mtime=get_mtime(INCOME_FILE);
    last_expenses_mtime=get_mtime(EXPENSES_FILE);
    isLoggedIn=0; currentUser[0]='\0';
    int port=8000;
    start_server(port);
    save_all_transactions();
    return 0;
}