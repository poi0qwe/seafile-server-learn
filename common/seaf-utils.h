#ifndef SEAF_UTILS_H
#define SEAF_UTILS_H

#include <searpc-client.h>

struct _SeafileSession;

char * // 获取临时文件路径
seafile_session_get_tmp_file_path (struct _SeafileSession *session, // 会话
                                   const char *basename, // 基
                                   char path[]); // 路径

int // 加载数据库配置
load_database_config (struct _SeafileSession *session);

int // 加载ccnet数据库配置
load_ccnet_database_config (struct _SeafileSession *session);

#endif
