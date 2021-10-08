/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* sqlite数据库操作封装 */

#ifndef DB_UTILS_H
#define DB_UTILS_H

#include <sqlite3.h>

int sqlite_open_db (const char *db_path, sqlite3 **db); // 打开数据库

int sqlite_close_db (sqlite3 *db); // 关闭数据库

sqlite3_stmt *sqlite_query_prepare (sqlite3 *db, const char *sql); // 通过sql得到statement

int sqlite_query_exec (sqlite3 *db, const char *sql); // 执行查询
int sqlite_begin_transaction (sqlite3 *db); // 开始事务
int sqlite_end_transaction (sqlite3 *db); // 结束事务

gboolean sqlite_check_for_existence (sqlite3 *db, const char *sql); // 判断是否存在

typedef gboolean (*SqliteRowFunc) (sqlite3_stmt *stmt, void *data); // 行回调函数，返回bool

int
sqlite_foreach_selected_row (sqlite3 *db, const char *sql, 
                             SqliteRowFunc callback, void *data); // 遍历每行，执行上面的回调函数；回调函数返回0则提前终止

int sqlite_get_int (sqlite3 *db, const char *sql); // 单值查询，获取int值

gint64 sqlite_get_int64 (sqlite3 *db, const char *sql); // 单值查询，获取int64值

char *sqlite_get_string (sqlite3 *db, const char *sql); // 单值查询，获取字符串值


#endif
