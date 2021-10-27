#ifndef SEAF_DB_H
#define SEAF_DB_H

enum {
    SEAF_DB_TYPE_SQLITE,
    SEAF_DB_TYPE_MYSQL,
    SEAF_DB_TYPE_PGSQL,
};
// 派生出Seafile数据库和Ccnet数据库，两者本质为一个事物
typedef struct SeafDB SeafDB; // 数据库
typedef struct SeafDB CcnetDB;
typedef struct SeafDBRow SeafDBRow; // 行
typedef struct SeafDBRow CcnetDBRow;
typedef struct SeafDBTrans SeafDBTrans; // 事务
typedef struct SeafDBTrans CcnetDBTrans;
// 行操作函数
typedef gboolean (*SeafDBRowFunc) (SeafDBRow *, void *);
typedef gboolean (*CcnetDBRowFunc) (CcnetDBRow *, void *);

SeafDB * // (全局)使用mysql数据库
seaf_db_new_mysql (const char *host,
                   int port,
                   const char *user, 
                   const char *passwd,
                   const char *db,
                   const char *unix_socket,
                   gboolean use_ssl,
                   const char *charset,
                   int max_connections);

#if 0
SeafDB *
seaf_db_new_pgsql (const char *host,
                   unsigned int port,
                   const char *user,
                   const char *passwd,
                   const char *db_name,
                   const char *unix_socket,
                   int max_connections);
#endif

SeafDB * // (全局)使用sqlite数据库
seaf_db_new_sqlite (const char *db_path, int max_connections);

int // 查看数据库类型
seaf_db_type (SeafDB *db);

int // sql查询
seaf_db_query (SeafDB *db, const char *sql);

gboolean // 检查存在
seaf_db_check_for_existence (SeafDB *db, const char *sql, gboolean *db_err);

int // 遍历每行
seaf_db_foreach_selected_row (SeafDB *db, const char *sql, 
                              SeafDBRowFunc callback, void *data);

const char * // 获取字符串属性
seaf_db_row_get_column_text (SeafDBRow *row, guint32 idx);

int // 获取int属性
seaf_db_row_get_column_int (SeafDBRow *row, guint32 idx);

gint64 // 获取int64属性
seaf_db_row_get_column_int64 (SeafDBRow *row, guint32 idx);

int // 获取第一行int
seaf_db_get_int (SeafDB *db, const char *sql);

gint64 // 获取第一行int64
seaf_db_get_int64 (SeafDB *db, const char *sql);

char * // 获取第一行字符串
seaf_db_get_string (SeafDB *db, const char *sql);

/* Transaction related */

SeafDBTrans * // 开始事务
seaf_db_begin_transaction (SeafDB *db);

void // 关闭事务
seaf_db_trans_close (SeafDBTrans *trans);

int // 提交事务
seaf_db_commit (SeafDBTrans *trans);

int // 回滚事务
seaf_db_rollback (SeafDBTrans *trans);

int // 事务查询
seaf_db_trans_query (SeafDBTrans *trans, const char *sql, int n, ...);

gboolean // 事务检查存在
seaf_db_trans_check_for_existence (SeafDBTrans *trans,
                                   const char *sql,
                                   gboolean *db_err,
                                   int n, ...);

int // 事务遍历行
seaf_db_trans_foreach_selected_row (SeafDBTrans *trans, const char *sql,
                                    SeafDBRowFunc callback, void *data,
                                    int n, ...);

int // 获取列数
seaf_db_row_get_column_count (SeafDBRow *row);

/* Prepared Statements */
// 预编译Sql
int
seaf_db_statement_query (SeafDB *db, const char *sql, int n, ...);

gboolean
seaf_db_statement_exists (SeafDB *db, const char *sql, gboolean *db_err, int n, ...);

int
seaf_db_statement_foreach_row (SeafDB *db, const char *sql,
                                SeafDBRowFunc callback, void *data,
                                int n, ...);

int
seaf_db_statement_get_int (SeafDB *db, const char *sql, int n, ...);

gint64
seaf_db_statement_get_int64 (SeafDB *db, const char *sql, int n, ...);

char *
seaf_db_statement_get_string (SeafDB *db, const char *sql, int n, ...);

#endif
