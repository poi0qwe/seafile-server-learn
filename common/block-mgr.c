/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* 块管理器 */

#include "common.h"

#include "seafile-session.h"
#include "utils.h"
#include "seaf-utils.h"
#include "block-mgr.h"
#include "log.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <glib/gstdio.h>

#include "block-backend.h"

#define SEAF_BLOCK_DIR "blocks"


extern BlockBackend * // 创建新的后台，基于文件系统；延后实现
block_backend_fs_new (const char *block_dir, const char *tmp_dir);


SeafBlockManager * // 创建新的块管理器
seaf_block_manager_new (struct _SeafileSession *seaf,
                        const char *seaf_dir)
{
    SeafBlockManager *mgr;

    mgr = g_new0 (SeafBlockManager, 1); // 申请内存
    mgr->seaf = seaf; // 会话

    mgr->backend = block_backend_fs_new (seaf_dir, seaf->tmp_file_dir); // 新的块操作后台
    if (!mgr->backend) {
        seaf_warning ("[Block mgr] Failed to load backend.\n");
        goto onerror;
    }

    return mgr;

onerror:
    g_free (mgr);

    return NULL;
}

int
seaf_block_manager_init (SeafBlockManager *mgr)
{
    return 0;
}


BlockHandle * // 打开块
seaf_block_manager_open_block (SeafBlockManager *mgr,
                               const char *store_id, // 仓库id
                               int version, // 版本
                               const char *block_id, // 块id
                               int rw_type) // 读写类型
{
    if (!store_id || !is_uuid_valid(store_id) ||
        !block_id || !is_object_id_valid(block_id)) // 非法id
        return NULL;

    return mgr->backend->open_block (mgr->backend,
                                     store_id, version,
                                     block_id, rw_type); // 转发
}

int // 读
seaf_block_manager_read_block (SeafBlockManager *mgr,
                               BlockHandle *handle,
                               void *buf, int len)
{
    return mgr->backend->read_block (mgr->backend, handle, buf, len); // 转发
}

int // 写
seaf_block_manager_write_block (SeafBlockManager *mgr,
                                BlockHandle *handle,
                                const void *buf, int len)
{
    return mgr->backend->write_block (mgr->backend, handle, buf, len); // 转发
}

int // 关闭
seaf_block_manager_close_block (SeafBlockManager *mgr,
                                BlockHandle *handle)
{
    return mgr->backend->close_block (mgr->backend, handle); // 转发
}

void // 释放句柄
seaf_block_manager_block_handle_free (SeafBlockManager *mgr,
                                      BlockHandle *handle)
{
    return mgr->backend->block_handle_free (mgr->backend, handle); // 转发
}

int // 提交
seaf_block_manager_commit_block (SeafBlockManager *mgr,
                                 BlockHandle *handle)
{
    return mgr->backend->commit_block (mgr->backend, handle); // 转发
}
// 检测块是否存在
gboolean seaf_block_manager_block_exists (SeafBlockManager *mgr,
                                          const char *store_id,
                                          int version,
                                          const char *block_id)
{
    if (!store_id || !is_uuid_valid(store_id) ||
        !block_id || !is_object_id_valid(block_id)) // 非法id
        return FALSE;

    return mgr->backend->exists (mgr->backend, store_id, version, block_id); // 转发
}

int // 移除块
seaf_block_manager_remove_block (SeafBlockManager *mgr,
                                 const char *store_id,
                                 int version,
                                 const char *block_id)
{
    if (!store_id || !is_uuid_valid(store_id) ||
        !block_id || !is_object_id_valid(block_id)) // 非法id
        return -1;

    return mgr->backend->remove_block (mgr->backend, store_id, version, block_id); // 转发
}

BlockMetadata * // 获取块元数据
seaf_block_manager_stat_block (SeafBlockManager *mgr,
                               const char *store_id,
                               int version,
                               const char *block_id)
{
    if (!store_id || !is_uuid_valid(store_id) ||
        !block_id || !is_object_id_valid(block_id)) // 非法id
        return NULL;

    return mgr->backend->stat_block (mgr->backend, store_id, version, block_id); // 转发
}

BlockMetadata * // 获取块元数据，依靠句柄
seaf_block_manager_stat_block_by_handle (SeafBlockManager *mgr,
                                         BlockHandle *handle)
{
    return mgr->backend->stat_block_by_handle (mgr->backend, handle); // 转发
}

int // 遍历
seaf_block_manager_foreach_block (SeafBlockManager *mgr,
                                  const char *store_id,
                                  int version,
                                  SeafBlockFunc process,
                                  void *user_data)
{
    return mgr->backend->foreach_block (mgr->backend,
                                        store_id, version,
                                        process, user_data); // 转发
}

int // 复制
seaf_block_manager_copy_block (SeafBlockManager *mgr,
                               const char *src_store_id,
                               int src_version,
                               const char *dst_store_id,
                               int dst_version,
                               const char *block_id)
{
    if (strcmp (block_id, EMPTY_SHA1) == 0) // 非法id
        return 0;
    if (seaf_block_manager_block_exists (mgr, dst_store_id, dst_version, block_id)) { // 不存在
        return 0;
    }

    return mgr->backend->copy (mgr->backend,
                               src_store_id,
                               src_version,
                               dst_store_id,
                               dst_version,
                               block_id); // 转发
}

static gboolean // 块操作函数，用于获取总块数
get_block_number (const char *store_id,
                  int version,
                  const char *block_id,
                  void *data)
{
    guint64 *n_blocks = data;

    ++(*n_blocks); // 自增

    return TRUE;
}

guint64 // 获取总块数
seaf_block_manager_get_block_number (SeafBlockManager *mgr,
                                     const char *store_id,
                                     int version)
{
    guint64 n_blocks = 0;

    seaf_block_manager_foreach_block (mgr, store_id, version,
                                      get_block_number, &n_blocks); // 对每个块执行get_block_number；每执行一次，n_blocks++

    return n_blocks;
}

gboolean // 验证某块
seaf_block_manager_verify_block (SeafBlockManager *mgr,
                                 const char *store_id,
                                 int version,
                                 const char *block_id,
                                 gboolean *io_error)
{
    BlockHandle *h;
    char buf[10240];
    int n;
    SHA_CTX ctx;
    guint8 sha1[20];
    char check_id[41];

    h = seaf_block_manager_open_block (mgr,
                                       store_id, version,
                                       block_id, BLOCK_READ); // 打开块
    if (!h) { // 打开失败
        seaf_warning ("Failed to open block %s:%.8s.\n", store_id, block_id);
        *io_error = TRUE;
        return FALSE;
    }

    SHA1_Init (&ctx); // 初始化SHA1
    while (1) { // 计算块中数据的SHA1
        n = seaf_block_manager_read_block (mgr, h, buf, sizeof(buf)); // 读至buf
        if (n < 0) {
            seaf_warning ("Failed to read block %s:%.8s.\n", store_id, block_id);
            *io_error = TRUE;
            return FALSE;
        }
        if (n == 0) // 直到全部读完
            break;

        SHA1_Update (&ctx, buf, n); // 更新SHA1值
    }

    seaf_block_manager_close_block (mgr, h); // 关闭块
    seaf_block_manager_block_handle_free (mgr, h); // 关闭句柄

    SHA1_Final (sha1, &ctx); // 结束
    rawdata_to_hex (sha1, check_id, 20); // 将SHA1的前20个字符转化为16进制串

    // 在CDC的default_write_chunk函数中，默认将块文件命名为SHA1的前20个字符的HEX串形式，也就是块的id
    // 对比块id（文件名）和计算所得是否相等；相等则认为验证成功
    if (strcmp (check_id, block_id) == 0)
        return TRUE; // 成功
    else
        return FALSE; // 失败
}

int // 移除仓库中的所有块
seaf_block_manager_remove_store (SeafBlockManager *mgr,
                                 const char *store_id)
{
    return mgr->backend->remove_store (mgr->backend, store_id); // 转发
}
