/* 日志（全局一个日志） */
#ifndef LOG_H
#define LOG_H

#define SEAFILE_DOMAIN g_quark_from_string("seafile")

#ifndef seaf_warning
#define seaf_warning(fmt, ...) g_warning("%s(%d): " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#ifndef seaf_message
#define seaf_message(fmt, ...) g_message("%s(%d): " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#endif


int seafile_log_init (const char *logfile, const char *ccnet_debug_level_str,
                      const char *seafile_debug_level_str); // 初始化日志
int seafile_log_reopen (); // 重新打开日志

#ifndef WIN32
#ifdef SEAFILE_SERVER
void
set_syslog_config (GKeyFile *config);
#endif
#endif

void // 设置标志位
seafile_debug_set_flags_string (const gchar *flags_string);

typedef enum
{
    SEAFILE_DEBUG_TRANSFER = 1 << 1, // 传输debug
    SEAFILE_DEBUG_SYNC = 1 << 2, // 同步debug
    SEAFILE_DEBUG_WATCH = 1 << 3, /* wt-monitor */ // 监听debug
    SEAFILE_DEBUG_HTTP = 1 << 4,  /* http server */ // HTTP debug
    SEAFILE_DEBUG_MERGE = 1 << 5, // 合并debug
    SEAFILE_DEBUG_OTHER = 1 << 6, // 其他debug
} SeafileDebugFlags;

void seafile_debug_impl (SeafileDebugFlags flag, const gchar *format, ...); // 实现Debug输出

// 这个函数被封装为宏“seaf_debug”
#ifdef DEBUG_FLAG

#undef seaf_debug
#define seaf_debug(fmt, ...)  \
    seafile_debug_impl (DEBUG_FLAG, "%.10s(%d): " fmt, __FILE__, __LINE__, ##__VA_ARGS__)

#endif  /* DEBUG_FLAG */

#endif
