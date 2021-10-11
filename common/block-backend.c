/* 块操作后台；通用结构体，隐藏了实现细节 */

#include "common.h"

#include "log.h"

#include "block-backend.h"

extern BlockBackend *
block_backend_fs_new (const char *block_dir, const char *tmp_dir); // 创建新的后台，基于文件系统；延后实现

BlockBackend *
load_filesystem_block_backend(GKeyFile *config) // 根据配置文件加载后台，要求后台名为'filesystem'；该后台基于文件系统
{
    BlockBackend *bend;
    char *tmp_dir;
    char *block_dir;
    
    block_dir = g_key_file_get_string (config, "block_backend", "block_dir", NULL); // 块文件目录
    if (!block_dir) {
        seaf_warning ("Block dir not set in config.\n");
        return NULL;
    }

    tmp_dir = g_key_file_get_string (config, "block_backend", "tmp_dir", NULL); // 临时文件目录
    if (!tmp_dir) {
        seaf_warning ("Block tmp dir not set in config.\n");
        return NULL;
    }

    bend = block_backend_fs_new (block_dir, tmp_dir); // 创建新的后台

    g_free (block_dir);
    g_free (tmp_dir);
    return bend; // 返回
}

BlockBackend*
load_block_backend (GKeyFile *config) // 根据配置文件加载后台
{
    char *backend;
    BlockBackend *bend;

    backend = g_key_file_get_string (config, "block_backend", "name", NULL); // 获取后台名
    if (!backend) {
        return NULL;
    }

    if (strcmp(backend, "filesystem") == 0) { // 若后台名为'filesystem'，开始加载
        bend = load_filesystem_block_backend(config);
        g_free (backend);
        return bend;
    }

    seaf_warning ("Unknown backend\n"); // 其他输出警告
    return NULL;
}
