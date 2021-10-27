/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* 提交管理（提交信息存储在硬盘中(json的格式，seafobj的形式)，提交管理就是从硬盘读写提交信息） */

#ifndef SEAF_COMMIT_MGR_H
#define SEAF_COMMIT_MGR_H

    struct _SeafCommitManager;
typedef struct _SeafCommit SeafCommit;

#include <glib/gstdio.h>
#include "db.h"

#include "obj-store.h"

struct _SeafCommit { // seafile提交对象
    struct _SeafCommitManager *manager; // 提交管理器

    int         ref; // 引用次数

    char        commit_id[41]; // 提交id
    char        repo_id[37]; // 仓库id
    char        root_id[41];    /* the fs root */ // 根id
    char       *desc; // 提交描述
    char       *creator_name; // 创建者名字
    char        creator_id[41]; // 创建者id
    guint64     ctime;          /* creation time */ // 创建时间
    char       *parent_id; // 父id
    char       *second_parent_id; // 祖父id
    char       *repo_name; // 仓库名
    char       *repo_desc;
    char       *repo_category; // 仓库分类
    char       *device_name; // 设备名
    char       *client_version; // 客户端版本

    gboolean    encrypted; // 是否已加密
    int         enc_version; // 加密版本
    char       *magic;
    char       *random_key; // 随机密钥
    char       *salt; // 盐
    gboolean    no_local_history; // 有无本地记录

    int         version; // 版本
    gboolean    new_merge; // 是否是新的合并
    gboolean    conflict; // 是否冲突
    gboolean    repaired; // 是否被修复
};


/**
 * @commit_id: if this is NULL, will create a new id.
 * @ctime: if this is 0, will use current time.
 * 
 * Any new commit should be added to commit manager before used.
 */
SeafCommit * // 新的提交
seaf_commit_new (const char *commit_id,
                 const char *repo_id,
                 const char *root_id,
                 const char *author_name,
                 const char *creator_id,
                 const char *desc,
                 guint64 ctime);

char * // 提交转json串
seaf_commit_to_data (SeafCommit *commit, gsize *len);

SeafCommit * // json串转提交
seaf_commit_from_data (const char *id, char *data, gsize len);

void // 增加引用
seaf_commit_ref (SeafCommit *commit);

void // 移除引用
seaf_commit_unref (SeafCommit *commit);

/* Set stop to TRUE if you want to stop traversing a branch in the history graph. 
   Note, if currently there are multi branches, this function will be called again. 
   So, set stop to TRUE not always stop traversing the history graph.
*/
// 历史图遍历函数（若一次提交对应多个分支，则每个分支都会执行一遍；设置stop为TRUE以终止遍历）
typedef gboolean (*CommitTraverseFunc) (SeafCommit *commit, void *data, gboolean *stop);

struct _SeafileSession;

typedef struct _SeafCommitManager SeafCommitManager;
typedef struct _SeafCommitManagerPriv SeafCommitManagerPriv;

struct _SeafCommitManager { // 提交管理器
    struct _SeafileSession *seaf; // seafile会话

    sqlite3    *db; // 数据库（未用到）
    struct SeafObjStore *obj_store; // 存储对象

    SeafCommitManagerPriv *priv; // 私有域
};

SeafCommitManager *
seaf_commit_manager_new (struct _SeafileSession *seaf); // 创建提交管理器

int
seaf_commit_manager_init (SeafCommitManager *mgr); // 初始化提交管理器

/**
 * Add a commit to commit manager and persist it to disk.
 * Any new commit should be added to commit manager before used.
 * This function increments ref count of the commit object.
 * Not MT safe.
 */
int
// 添加提交并将其在硬盘中持久化
// 使用新的提交前必须将其添加到提交管理器
// 增加提交对象的引用次数
// 线程不安全
seaf_commit_manager_add_commit (SeafCommitManager *mgr, SeafCommit *commit); // 提交管理器添加提交

/**
 * Delete a commit from commit manager and permanently remove it from disk.
 * A commit object to be deleted should have ref cournt <= 1.
 * Not MT safe.
 */
// 删除提交且从硬盘中移除
// 提交对象将被删除如果引用次数<=1
// 线程不安全
void
seaf_commit_manager_del_commit (SeafCommitManager *mgr,
                                const char *repo_id,
                                int version,
                                const char *id); // 提交管理器删除提交

/**
 * Find a commit object.
 * This function increments ref count of returned object.
 * Not MT safe.
 */
// 寻找一个提交对象
// 增加引用并返回对象
// 线程不安全
SeafCommit* 
seaf_commit_manager_get_commit (SeafCommitManager *mgr,
                                const char *repo_id,
                                int version,
                                const char *id); // 提交管理器获取提交

/**
 * Get a commit object, with compatibility between version 0 and version 1.
 * It will first try to get commit with version 1 layout; if fails, will
 * try version 0 layout for compatibility.
 * This is useful for loading a repo. In that case, we don't know the version
 * of the repo before loading its head commit.
 */
// 获取提交对象，同时获取版本0和1之间的兼容性
// 先会寻找版本1，否则寻找兼容的版本0
// 用于加载仓库，因为在加载仓库的首次提交它的版本未知
SeafCommit *
seaf_commit_manager_get_commit_compatible (SeafCommitManager *mgr,
                                           const char *repo_id,
                                           const char *id); // 获取兼容的提交

/**
 * Traverse the commits DAG start from head in topological order.
 * The ordering is based on commit time.
 * return FALSE if some commits is missing, TRUE otherwise.
 */
// 拓扑序遍历提交图
// 基于提交时间
// 如果一些提交丢失了返回FALSE
gboolean
seaf_commit_manager_traverse_commit_tree (SeafCommitManager *mgr,
                                          const char *repo_id,
                                          int version,
                                          const char *head,
                                          CommitTraverseFunc func,
                                          void *data,
                                          gboolean skip_errors); // 遍历历史提交图

/*
 * The same as the above function, but stops traverse down if parent commit
 * doesn't exists, instead of returning error.
 */
// 同上，但只要父提交不存在，马上返回错误
gboolean
seaf_commit_manager_traverse_commit_tree_truncated (SeafCommitManager *mgr,
                                                    const char *repo_id,
                                                    int version,
                                                    const char *head,
                                                    CommitTraverseFunc func,
                                                    void *data,
                                                    gboolean skip_errors); // 遍历历史提交图，带终止

/**
 * Works the same as seaf_commit_manager_traverse_commit_tree, but stops
 * traversing when a total number of _limit_ commits is reached. If
 * limit <= 0, there is no limit
 */
// 同上，遍历了limit次就停止；若limit<=0则无限制
gboolean
seaf_commit_manager_traverse_commit_tree_with_limit (SeafCommitManager *mgr,
                                                     const char *repo_id,
                                                     int version,
                                                     const char *head,
                                                     CommitTraverseFunc func,
                                                     int limit,
                                                     void *data,
                                                     char **next_start_commit,
                                                     gboolean skip_errors); // 遍历历史提交图，有限制

gboolean
seaf_commit_manager_commit_exists (SeafCommitManager *mgr,
                                   const char *repo_id,
                                   int version,
                                   const char *id); // 判断提交存不存在

int
seaf_commit_manager_remove_store (SeafCommitManager *mgr,
                                  const char *store_id); // 释放提交管理器空间

#endif
