// 仓库管理
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef SEAF_REPO_MGR_H
#define SEAF_REPO_MGR_H

#include <pthread.h>

#include "seafile-object.h"
#include "commit-mgr.h"
#include "branch-mgr.h"

struct _SeafRepoManager;
typedef struct _SeafRepo SeafRepo;

struct _SeafRepo { // 仓库对象
    struct _SeafRepoManager *manager;

    gchar       id[37]; // 仓库id
    gchar      *name; // 仓库名
    gchar      *desc; // 排序
    gchar      *category;       /* not used yet */ // 分类（未实现）
    gboolean    encrypted; // 是否加密
    int         enc_version; // 加密版本
    gchar       magic[33];       /* hash(repo_id + passwd), key stretched. */ // 用于密码校验
    gboolean    no_local_history; // 是否无本地历史记录

    SeafBranch *head; // 头指针，指向头分支(当前分支)

    gboolean    is_corrupted; // 是否损坏
    gboolean    delete_pending; // 是否等待删除
    int         ref_cnt; // 引用次数，用于GC

    int version; // seafile版本
    /* Used to access fs and block sotre.
     * This id is different from repo_id when this repo is virtual.
     * Virtual repos share fs and block store with its origin repo.
     * However, commit store for each repo is always independent.
     * So always use repo_id to access commit store.
     */
    gchar       store_id[37]; // 虚拟仓库使用
};

gboolean is_repo_id_valid (const char *id); // 判断仓库是否有效

SeafRepo* 
seaf_repo_new (const char *id, const char *name, const char *desc); // 创建新的对象

void
seaf_repo_free (SeafRepo *repo); // 释放

void
seaf_repo_ref (SeafRepo *repo); // 增加引用

void
seaf_repo_unref (SeafRepo *repo); // 解除引用

typedef struct _SeafRepoManager SeafRepoManager;
typedef struct _SeafRepoManagerPriv SeafRepoManagerPriv;

struct _SeafRepoManager { // 仓库管理器
    struct _SeafileSession *seaf;

    SeafRepoManagerPriv *priv;
};

SeafRepoManager* 
seaf_repo_manager_new (struct _SeafileSession *seaf); // 新建仓库管理器

int
seaf_repo_manager_init (SeafRepoManager *mgr); // 初始化仓库管理器

int
seaf_repo_manager_start (SeafRepoManager *mgr); // 启动仓库管理器

int
seaf_repo_manager_add_repo (SeafRepoManager *mgr, SeafRepo *repo); // 增加仓库

int
seaf_repo_manager_del_repo (SeafRepoManager *mgr, SeafRepo *repo); // 删除仓库

SeafRepo* 
seaf_repo_manager_get_repo (SeafRepoManager *manager, const gchar *id); // 获取仓库

gboolean
seaf_repo_manager_repo_exists (SeafRepoManager *manager, const gchar *id); // 判断仓库是否存在

GList* 
seaf_repo_manager_get_repo_list (SeafRepoManager *mgr, int start, int limit); // 获取仓库列表

GList *
seaf_repo_manager_get_repo_id_list (SeafRepoManager *mgr); // 仓库id列表

GList *
seaf_repo_manager_get_repos_by_owner (SeafRepoManager *mgr,
                                      const char *email); // 根据拥有者获取仓库表

gboolean
seaf_repo_manager_is_virtual_repo (SeafRepoManager *mgr, const char *repo_id); // 判断是否是虚拟仓库 

#endif
