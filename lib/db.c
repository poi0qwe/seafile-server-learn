/* sqlite数据库操作封装 */

#include <glib.h>
#include <unistd.h>

#include "db.h"

int
sqlite_open_db (const char *db_path, sqlite3 **db) // 打开数据库
{
    int result;
    const char *errmsg;

    result = sqlite3_open (db_path, db); // 打开
    if (result) { // 出错则关闭
        errmsg = sqlite3_errmsg (*db);
                                
        g_warning ("Couldn't open database:'%s', %s\n", 
                   db_path, errmsg ? errmsg : "no error given");

        sqlite3_close (*db);
        return -1;
    }

    return 0;
}

int sqlite_close_db (sqlite3 *db)
{
    return sqlite3_close (db);
}

sqlite3_stmt *
sqlite_query_prepare (sqlite3 *db, const char *sql) // 通过sql得到statement
{
    sqlite3_stmt *stmt;
    int result;

    result = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL); // 获取statment

    if (result != SQLITE_OK) { // 出错则返回空
        const gchar *str = sqlite3_errmsg (db);

        g_warning ("Couldn't prepare query, error:%d->'%s'\n\t%s\n", 
                   result, str ? str : "no error given", sql);

        return NULL;
    }

    return stmt;
}

int
sqlite_query_exec (sqlite3 *db, const char *sql) // 执行sql
{
    char *errmsg = NULL;
    int result;

    result = sqlite3_exec (db, sql, NULL, NULL, &errmsg); // 执行

    if (result != SQLITE_OK) { // 出错则输出errmsg然后释放
        if (errmsg != NULL) {
            g_warning ("SQL error: %d - %s\n:\t%s\n", result, errmsg, sql);
            sqlite3_free (errmsg);
        }
        return -1;
    }

    return 0;
}

int
sqlite_begin_transaction (sqlite3 *db) // 开始事务
{
    char *sql = "BEGIN TRANSACTION;";
    return sqlite_query_exec (db, sql);
}

int
sqlite_end_transaction (sqlite3 *db) // 结束事务
{
    char *sql = "END TRANSACTION;";
    return sqlite_query_exec (db, sql);
}


gboolean
sqlite_check_for_existence (sqlite3 *db, const char *sql) // 判断sql是否有结果
{
    sqlite3_stmt *stmt;
    int result;

    stmt = sqlite_query_prepare (db, sql); // 得到statement
    if (!stmt)
        return FALSE;

    result = sqlite3_step (stmt); // 获取下一行的结果
    if (result == SQLITE_ERROR) { // 出错
        const gchar *str = sqlite3_errmsg (db);

        g_warning ("Couldn't execute query, error: %d->'%s'\n", 
                   result, str ? str : "no error given");
        sqlite3_finalize (stmt); // 销毁statement
        return FALSE;
    }
    sqlite3_finalize (stmt); // 销毁statement

    if (result == SQLITE_ROW)
        return TRUE;
    return FALSE;
}

int
sqlite_foreach_selected_row (sqlite3 *db, const char *sql, 
                             SqliteRowFunc callback, void *data) // 遍历每行，执行回调函数callback
{
    sqlite3_stmt *stmt;
    int result;
    int n_rows = 0;

    stmt = sqlite_query_prepare (db, sql); // 得到statement
    if (!stmt) {
        return -1;
    }

    while (1) {
        result = sqlite3_step (stmt); // 获取下一行的结果
        if (result != SQLITE_ROW) // 遍历结束
            break;
        n_rows++;
        if (!callback (stmt, data)) // 调用callback；若返回0则提前终止
            break;
    }

    if (result == SQLITE_ERROR) { // 出错
        const gchar *s = sqlite3_errmsg (db);

        g_warning ("Couldn't execute query, error: %d->'%s'\n",
                   result, s ? s : "no error given");
        sqlite3_finalize (stmt);
        return -1;
    }

    sqlite3_finalize (stmt); // 销毁statement
    return n_rows;
}

int sqlite_get_int (sqlite3 *db, const char *sql) // 单值查询，获取int值
{
    int ret = -1;
    int result;
    sqlite3_stmt *stmt;

    if ( !(stmt = sqlite_query_prepare(db, sql)) ) // 得到statement
        return 0;

    result = sqlite3_step (stmt);
    if (result == SQLITE_ROW) {
        ret = sqlite3_column_int (stmt, 0); // 取结果
        sqlite3_finalize (stmt);
        return ret;
    }

    if (result == SQLITE_ERROR) { // 出错
        const gchar *str = sqlite3_errmsg (db);
        g_warning ("Couldn't execute query, error: %d->'%s'\n",
                   result, str ? str : "no error given");
        sqlite3_finalize (stmt);
        return -1;
    }

    sqlite3_finalize(stmt); // 销毁statement
    return ret;
}

gint64 sqlite_get_int64 (sqlite3 *db, const char *sql) // 单值查询，获取int64值
{
    gint64 ret = -1;
    int result;
    sqlite3_stmt *stmt;

    if ( !(stmt = sqlite_query_prepare(db, sql)) ) // 得到statement
        return 0;

    result = sqlite3_step (stmt);
    if (result == SQLITE_ROW) {
        ret = sqlite3_column_int64 (stmt, 0); // 取结果
        sqlite3_finalize (stmt);
        return ret;
    }

    if (result == SQLITE_ERROR) { // 出错
        const gchar *str = sqlite3_errmsg (db);
        g_warning ("Couldn't execute query, error: %d->'%s'\n",
                   result, str ? str : "no error given");
        sqlite3_finalize (stmt);
        return -1;
    }

    sqlite3_finalize(stmt); // 销毁statement
    return ret;
}

char *sqlite_get_string (sqlite3 *db, const char *sql) // 单值查询，获取字符串值
{
    const char *res = NULL;
    int result;
    sqlite3_stmt *stmt;
    char *ret;

    if ( !(stmt = sqlite_query_prepare(db, sql)) ) // 得到statement
        return NULL;

    result = sqlite3_step (stmt);
    if (result == SQLITE_ROW) {
        res = (const char *)sqlite3_column_text (stmt, 0); // 取结果
        ret = g_strdup(res); // 复制，避免局部变量回收
        sqlite3_finalize (stmt);
        return ret;
    }

    if (result == SQLITE_ERROR) { // 出错
        const gchar *str = sqlite3_errmsg (db);
        g_warning ("Couldn't execute query, error: %d->'%s'\n",
                   result, str ? str : "no error given");
        sqlite3_finalize (stmt);
        return NULL;
    }

    sqlite3_finalize(stmt); // 销毁statement
    return NULL;
}
