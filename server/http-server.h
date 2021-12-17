#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <glib.h>

struct _SeafileSession;

struct _HttpServer;

struct _HttpServerStruct {
    struct _SeafileSession *seaf_session;

    struct _HttpServer *priv;

    char *bind_addr; // 绑定地址
    int bind_port; // 绑定端口
    char *http_temp_dir;        /* temp dir for file upload */ // 临时目录
    char *windows_encoding; // ZIP编码
    gint64 fixed_block_size; // 分块大小，默认8MB
    int web_token_expire_time; // 令牌过期时间
    int max_indexing_threads; // 最大索引线程数
    int worker_threads; // 工作线程数
    int max_index_processing_threads; // 最大索引处理线程数
    int cluster_shared_temp_file_mode; // 集群共享临时文件模式
};

typedef struct _HttpServerStruct HttpServerStruct;

HttpServerStruct *
seaf_http_server_new (struct _SeafileSession *session);

int
seaf_http_server_start (HttpServerStruct *htp_server);

int
seaf_http_server_invalidate_tokens (HttpServerStruct *htp_server,
                                    const GList *tokens);

void
send_statistic_msg (const char *repo_id, char *user, char *operation, guint64 bytes);

#endif
