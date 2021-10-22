/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* 日志 */

#include "common.h"

#include <stdio.h>
#include <glib/gstdio.h>

#ifndef WIN32
#ifdef SEAFILE_SERVER
#include <sys/syslog.h>
#endif
#endif

#include "log.h"
#include "utils.h"

/* message with greater log levels will be ignored */
static int ccnet_log_level; // ccnet日志等级
static int seafile_log_level; // seafile日志等级
static char *logfile;
static FILE *logfp; // 日志文件

#ifndef WIN32
#ifdef SEAFILE_SERVER
static gboolean enable_syslog;
#endif
#endif

#ifndef WIN32
#ifdef SEAFILE_SERVER
static int
get_syslog_level (GLogLevelFlags level)
{
    switch (level) {
        case G_LOG_LEVEL_DEBUG:
            return LOG_DEBUG;
        case G_LOG_LEVEL_INFO:
            return LOG_INFO;
        case G_LOG_LEVEL_WARNING:
            return LOG_WARNING;
        case G_LOG_LEVEL_ERROR:
            return LOG_ERR;
        default:
            return LOG_DEBUG;
    }
}
#endif
#endif

static void // seafile日志
seafile_log (const gchar *log_domain, GLogLevelFlags log_level, // 日志域、日志等级
             const gchar *message,    gpointer user_data) // 消息、用户数据
{
    time_t t;
    struct tm *tm;
    char buf[1024];
    int len;

    if (log_level > seafile_log_level) // 日志等级大于seafile日志等级，返回
        return;

    t = time(NULL); // 当前时间戳
    tm = localtime(&t);
    len = strftime (buf, 1024, "%Y-%m-%d %H:%M:%S ", tm); // 时间字符串
    g_return_if_fail (len < 1024);
    if (logfp) {    // 日志文件
        fputs (buf, logfp); // 输入时间
        fputs (message, logfp); // 输入消息
        fflush (logfp); // 清空缓存写入硬盘
    }

#ifndef WIN32
#ifdef SEAFILE_SERVER
    if (enable_syslog)
        syslog (get_syslog_level (log_level), "%s", message);
#endif
#endif
}

static void // ccnet日志
ccnet_log (const gchar *log_domain, GLogLevelFlags log_level,
             const gchar *message,    gpointer user_data)
{
    time_t t;
    struct tm *tm;
    char buf[1024];
    int len;

    if (log_level > ccnet_log_level) // 日志等级大于ccnet日志等级，返回
        return;

    t = time(NULL);
    tm = localtime(&t);
    len = strftime (buf, 1024, "[%x %X] ", tm);
    g_return_if_fail (len < 1024);
    if (logfp) {
        fputs (buf, logfp);
        fputs (message, logfp);
        fflush (logfp);
    }

#ifndef WIN32
#ifdef SEAFILE_SERVER
    if (enable_syslog)
        syslog (get_syslog_level (log_level), "%s", message);
#endif
#endif
}

static int // 根据串，返回debug等级
get_debug_level(const char *str, int default_level)
{
    if (strcmp(str, "debug") == 0)
        return G_LOG_LEVEL_DEBUG;
    if (strcmp(str, "info") == 0)
        return G_LOG_LEVEL_INFO;
    if (strcmp(str, "warning") == 0)
        return G_LOG_LEVEL_WARNING;
    return default_level;
}

int // 初始化日志
seafile_log_init (const char *_logfile, const char *ccnet_debug_level_str,
                  const char *seafile_debug_level_str)
{ // 使用glib输出日志：每次调用g_log时，用参数生成字符串并转发给句柄中的函数（此处是seafile_log和ccnet_log）
    g_log_set_handler (NULL, G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
                       | G_LOG_FLAG_RECURSION, seafile_log, NULL);
    g_log_set_handler ("Ccnet", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
                       | G_LOG_FLAG_RECURSION, ccnet_log, NULL);

    /* record all log message */ // 设置日志等级
    ccnet_log_level = get_debug_level(ccnet_debug_level_str, G_LOG_LEVEL_INFO);
    seafile_log_level = get_debug_level(seafile_debug_level_str, G_LOG_LEVEL_DEBUG);

    if (strcmp(_logfile, "-") == 0) { // 标准输出
        logfp = stdout;
        logfile = g_strdup (_logfile); // 设置文件句柄
    }
    else { // 保存为ccnet文件
        logfile = ccnet_expand_path(_logfile); // 设置文件句柄
        if ((logfp = g_fopen (logfile, "a+")) == NULL) {
            seaf_message ("Failed to open file %s\n", logfile);
            return -1;
        }
    }

    return 0;
}

int // 重新打开日志文件
seafile_log_reopen ()
{
    FILE *fp, *oldfp;

    if (strcmp(logfile, "-") == 0) // 没有日志文件
        return 0;

    if ((fp = g_fopen (logfile, "a+")) == NULL) {
        seaf_message ("Failed to open file %s\n", logfile);
        return -1;
    }

    //TODO: check file's health

    oldfp = logfp;
    logfp = fp;
    if (fclose(oldfp) < 0) { // 关闭旧日志文件
        seaf_message ("Failed to close file %s\n", logfile);
        return -1;
    }

    return 0;
}

static SeafileDebugFlags debug_flags = 0;

static GDebugKey debug_keys[] = { // debug键
  { "Transfer", SEAFILE_DEBUG_TRANSFER },
  { "Sync", SEAFILE_DEBUG_SYNC },
  { "Watch", SEAFILE_DEBUG_WATCH },
  { "Http", SEAFILE_DEBUG_HTTP },
  { "Merge", SEAFILE_DEBUG_MERGE },
  { "Other", SEAFILE_DEBUG_OTHER },
};

gboolean // 判断标志位flag是否在debug_flags中已经被设置了
seafile_debug_flag_is_set (SeafileDebugFlags flag)
{
    return (debug_flags & flag) != 0; // 判断flag和debug_flag中有没有比特位同为1
}

void // 设置标志位
seafile_debug_set_flags (SeafileDebugFlags flags)
{
    g_message ("Set debug flags %#x\n", flags);
    debug_flags |= flags;
}

void // 根据字符串数组设置标志位
seafile_debug_set_flags_string (const gchar *flags_string)
{
    guint nkeys = G_N_ELEMENTS (debug_keys);

    if (flags_string)
        seafile_debug_set_flags (
            g_parse_debug_string (flags_string, debug_keys, nkeys));
}

void // 实现Debug输出
seafile_debug_impl (SeafileDebugFlags flag, const gchar *format, ...)
{
    if (flag & debug_flags) { // 如果允许输出该Debug
        va_list args;
        va_start (args, format); // 读取可变参数（即错误或异常信息）
        g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args); // 参数转发
        va_end (args); // 结束读取
    }
}

#ifndef WIN32
#ifdef SEAFILE_SERVER
void
set_syslog_config (GKeyFile *config) // 系统日志
{
    enable_syslog = g_key_file_get_boolean (config,
                                            "general", "enable_syslog",
                                            NULL); // 判断是否开启系统日志
    if (enable_syslog)
        openlog (NULL, LOG_NDELAY | LOG_PID, LOG_USER); // 打开系统日志
}
#endif
#endif
