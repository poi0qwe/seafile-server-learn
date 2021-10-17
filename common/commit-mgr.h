/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef SEAF_COMMIT_MGR_H
#define SEAF_COMMIT_MGR_H

struct _SeafCommitManager;
typedef struct _SeafCommit SeafCommit;

#include <glib/gstdio.h>
#include "db.h"

#include "obj-store.h"

struct _SeafCommit { // seafile提交
    struct _SeafCommitManager *manager; // 提交管理器

    int         ref; // 引用次数

    char        commit_id[41]; // 提交id
    char        repo_id[37]; // 仓库id
    char        root_id[41];    /* the fs root */ // 根id
    char       *desc;
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

char * // 提交给数据
seaf_commit_to_data (SeafCommit *commit, gsize *len);

SeafCommit * // 从数据提交
seaf_commit_from_data (const char *id, char *data, gsize len);

void // 增加引用
seaf_commit_ref (SeafCommit *commit);

void // 移除引用
seaf_commit_unref (SeafCommit *commit);

/* Set stop to TRUE if you want to stop traversing a branch in the history graph. 
   Note, if currently there are multi branches, this function will be called again. 
   So, set stop to TRUE not always stop traversing the history graph.
*/
typedef gboolean (*CommitTraverseFunc) (SeafCommit *commit, void *data, gboolean *stop);

struct _SeafileSession;

typedef struct _SeafCommitManager SeafCommitManager;
typedef struct _SeafCommitManagerPriv SeafCommitManagerPriv;

struct _SeafCommitManager {
    struct _SeafileSession *seaf;

    sqlite3    *db;
    struct SeafObjStore *obj_store;

    SeafCommitManagerPriv *priv;
};

SeafCommitManager *
seaf_commit_manager_new (struct _SeafileSession *seaf);

int
seaf_commit_manager_init (SeafCommitManager *mgr);

/**
 * Add a commit to commit manager and persist it to disk.
 * Any new commit should be added to commit manager before used.
 * This function increments ref count of the commit object.
 * Not MT safe.
 */
int
seaf_commit_manager_add_commit (SeafCommitManager *mgr, SeafCommit *commit);

/**
 * Delete a commit from commit manager and permanently remove it from disk.
 * A commit object to be deleted should have ref cournt <= 1.
 * Not MT safe.
 */
void
seaf_commit_manager_del_commit (SeafCommitManager *mgr,
                                const char *repo_id,
                                int version,
                                const char *id);

/**
 * Find a commit object.
 * This function increments ref count of returned object.
 * Not MT safe.
 */
SeafCommit* 
seaf_commit_manager_get_commit (SeafCommitManager *mgr,
                                const char *repo_id,
                                int version,
                                const char *id);

/**
 * Get a commit object, with compatibility between version 0 and version 1.
 * It will first try to get commit with version 1 layout; if fails, will
 * try version 0 layout for compatibility.
 * This is useful for loading a repo. In that case, we don't know the version
 * of the repo before loading its head commit.
 */
SeafCommit *
seaf_commit_manager_get_commit_compatible (SeafCommitManager *mgr,
                                           const char *repo_id,
                                           const char *id);

/**
 * Traverse the commits DAG start from head in topological order.
 * The ordering is based on commit time.
 * return FALSE if some commits is missing, TRUE otherwise.
 */
gboolean
seaf_commit_manager_traverse_commit_tree (SeafCommitManager *mgr,
                                          const char *repo_id,
                                          int version,
                                          const char *head,
                                          CommitTraverseFunc func,
                                          void *data,
                                          gboolean skip_errors);

/*
 * The same as the above function, but stops traverse down if parent commit
 * doesn't exists, instead of returning error.
 */
gboolean
seaf_commit_manager_traverse_commit_tree_truncated (SeafCommitManager *mgr,
                                                    const char *repo_id,
                                                    int version,
                                                    const char *head,
                                                    CommitTraverseFunc func,
                                                    void *data,
                                                    gboolean skip_errors);

/**
 * Works the same as seaf_commit_manager_traverse_commit_tree, but stops
 * traversing when a total number of _limit_ commits is reached. If
 * limit <= 0, there is no limit
 */
gboolean
seaf_commit_manager_traverse_commit_tree_with_limit (SeafCommitManager *mgr,
                                                     const char *repo_id,
                                                     int version,
                                                     const char *head,
                                                     CommitTraverseFunc func,
                                                     int limit,
                                                     void *data,
                                                     char **next_start_commit,
                                                     gboolean skip_errors);

gboolean
seaf_commit_manager_commit_exists (SeafCommitManager *mgr,
                                   const char *repo_id,
                                   int version,
                                   const char *id);

int
seaf_commit_manager_remove_store (SeafCommitManager *mgr,
                                  const char *store_id);

#endif
