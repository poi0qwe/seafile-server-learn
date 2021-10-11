/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* 块管理器 */

#ifndef SEAF_BLOCK_MGR_H
#define SEAF_BLOCK_MGR_H

#include <glib.h>
#include <glib-object.h>
#include <stdint.h>

#include "block.h"

struct _SeafileSession;

typedef struct _SeafBlockManager SeafBlockManager;

struct _SeafBlockManager { // 块管理器；整合了seafile会话与块操作后台
    struct _SeafileSession *seaf; // 会话

    struct BlockBackend *backend; // 块操作后台
};


SeafBlockManager * // 创建新的块管理器
seaf_block_manager_new (struct _SeafileSession *seaf, // seafile会话
                        const char *seaf_dir); // seafile目录

/*
 * Open a block for read or write. // 打开块
 *
 * @store_id: id for the block store
 * @version: data format version for the repo
 * @block_id: ID of block.
 * @rw_type: BLOCK_READ or BLOCK_WRITE.
 * Returns: A handle for the block.
 */
BlockHandle * // 打开块，返回句柄
seaf_block_manager_open_block (SeafBlockManager *mgr,
                               const char *store_id,
                               int version,
                               const char *block_id,
                               int rw_type); // 读还是写

/*
 * Read data from a block.
 * The semantics is similar to readn.
 *
 * @handle: Handle returned by seaf_block_manager_open_block().
 * @buf: Data wuold be copied into this buf.
 * @len: At most @len bytes would be read.
 *
 * Returns: the bytes read.
 */
int // 读
seaf_block_manager_read_block (SeafBlockManager *mgr,
                               BlockHandle *handle, // 块的句柄
                               void *buf, int len); // 读len个字节到buf中

/*
 * Write data to a block.
 * The semantics is similar to writen.
 *
 * @handle: Hanlde returned by seaf_block_manager_open_block().
 * @buf: Data to be written to the block.
 * @len: At most @len bytes would be written.
 *
 * Returns: the bytes written.
 */
int // 写
seaf_block_manager_write_block (SeafBlockManager *mgr,
                                BlockHandle *handle, // 块的句柄
                                const void *buf, int len); // 从buf中写len个字节

/*
 * Commit a block to storage.
 * The block must be opened for write.
 *
 * @handle: Hanlde returned by seaf_block_manager_open_block().
 *
 * Returns: 0 on success, -1 on error.
 */
int // 提交
seaf_block_manager_commit_block (SeafBlockManager *mgr,
                                 BlockHandle *handle); // 块的句柄

/*
 * Close an open block.
 *
 * @handle: Hanlde returned by seaf_block_manager_open_block().
 *
 * Returns: 0 on success, -1 on error.
 */
int // 关闭
seaf_block_manager_close_block (SeafBlockManager *mgr,
                                BlockHandle *handle); // 块的句柄

void // 释放块的句柄
seaf_block_manager_block_handle_free (SeafBlockManager *mgr,
                                      BlockHandle *handle);

gboolean 
seaf_block_manager_block_exists (SeafBlockManager *mgr,
                                 const char *store_id,
                                 int version,
                                 const char *block_id);

int // 移除块
seaf_block_manager_remove_block (SeafBlockManager *mgr,
                                 const char *store_id,
                                 int version,
                                 const char *block_id);

BlockMetadata * // 通过仓库id、版本、块id来获取块的状态（元数据）
seaf_block_manager_stat_block (SeafBlockManager *mgr,
                               const char *store_id,
                               int version,
                               const char *block_id);

BlockMetadata * // 通过句柄获取块的状态
seaf_block_manager_stat_block_by_handle (SeafBlockManager *mgr,
                                         BlockHandle *handle);

int // 给定仓库id和版本，遍历其中的每个块，对其执行块操作函数；若块操作函数返回零，直接结束循环
seaf_block_manager_foreach_block (SeafBlockManager *mgr,
                                  const char *store_id,
                                  int version,
                                  SeafBlockFunc process, // 块操作函数
                                  void *user_data); // 用户参数，传递给块操作函数

int // 复制块，从某个仓库的某个版本到另一个仓库的另一个版本
seaf_block_manager_copy_block (SeafBlockManager *mgr,
                               const char *src_store_id,
                               int src_version,
                               const char *dst_store_id,
                               int dst_version,
                               const char *block_id);

/* Remove all blocks for a repo. Only valid for version 1 repo. */
int // 移除仓库中的所有块，仅对版本为1的仓库有效；即清空仓库
seaf_block_manager_remove_store (SeafBlockManager *mgr,
                                 const char *store_id);

guint64 // 获取某版本的仓库中块的数量
seaf_block_manager_get_block_number (SeafBlockManager *mgr,
                                     const char *store_id,
                                     int version);

gboolean // 验证某个块
seaf_block_manager_verify_block (SeafBlockManager *mgr,
                                 const char *store_id,
                                 int version,
                                 const char *block_id,
                                 gboolean *io_error);

#endif
