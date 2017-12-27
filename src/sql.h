#ifndef _SQL_H
#define _SQL_H


#include <stdio.h>
#include <stdlib.h>

#include <mysql/mysql.h>
#define DBHOST      "hostname"
#define DBUSER      "username"
#define DBPASSWORD  "password"
#define DBDATABASE  "database"

typedef struct _sql_col {
    char *name;
    char *data;
    
    struct _sql_col *next;
} SQL_COL;

typedef struct _sql_row {
    SQL_COL *cols;
    
    struct _sql_row *next;
} SQL_ROW;

typedef struct _sql_ctx {
    MYSQL *db;
} SQL_CTX;

SQL_ROW *sql_backend_mysql(SQL_CTX *, char *);
SQL_ROW *runsql(SQL_CTX *, char *);
SQL_ROW *sqlfmt(SQL_CTX *, char *, size_t, char *, ...);
char *get_column(SQL_ROW *, char *);
void print_rows(SQL_ROW *);
void sqldb_free_rows(SQL_ROW *);

#endif

