#ifndef SEAF_BRANCH_MGR_H
#define SEAF_BRANCH_MGR_H
/* 分支管理（分支信息存储在数据库中，分支管理就是从数据库读写分支信息） */

#include "commit-mgr.h"
#define NO_BRANCH "-"

typedef struct _SeafBranch SeafBranch;

struct _SeafBranch { // 分支结构
    int   ref; // 引用次数
    char *name; // 分支名
    char  repo_id[37]; // 分支id
    char  commit_id[41]; // 提交id
};

// 创建新分支
SeafBranch *seaf_branch_new (const char *name, // 分支名
                             const char *repo_id, // 仓库名
                             const char *commit_id); // 提交id
void seaf_branch_free (SeafBranch *branch); // 删除分支
void seaf_branch_set_commit (SeafBranch *branch, const char *commit_id); // 设置提交id

void seaf_branch_ref (SeafBranch *branch); // 增加一个引用
void seaf_branch_unref (SeafBranch *branch); // 解除一个引用


typedef struct _SeafBranchManager SeafBranchManager;
typedef struct _SeafBranchManagerPriv SeafBranchManagerPriv;

struct _SeafileSession;
struct _SeafBranchManager { // 分支管理器（用于管理和读写数据库中的分支信息）
    struct _SeafileSession *seaf; // 会话

    SeafBranchManagerPriv *priv; // 私有域
};

SeafBranchManager *seaf_branch_manager_new (struct _SeafileSession *seaf); // 创建新的分支管理器
int seaf_branch_manager_init (SeafBranchManager *mgr); // 初始化分支管理器

int
seaf_branch_manager_add_branch (SeafBranchManager *mgr, SeafBranch *branch); // 通过分支管理器增加新的分支

int
seaf_branch_manager_del_branch (SeafBranchManager *mgr, // 通过分支管理器移除分支
                                const char *repo_id,
                                const char *name);

void
seaf_branch_list_free (GList *blist); // 释放分支列表

int
seaf_branch_manager_update_branch (SeafBranchManager *mgr,
                                   SeafBranch *branch); // 通过分支管理器更新分支

#ifdef SEAFILE_SERVER
/**
 * Atomically test whether the current head commit id on @branch
 * is the same as @old_commit_id and update branch in db.
 */
// 自动检测现在的commit_id是否和旧的一样
int
seaf_branch_manager_test_and_update_branch (SeafBranchManager *mgr,
                                            SeafBranch *branch,
                                            const char *old_commit_id);
#endif

SeafBranch *
seaf_branch_manager_get_branch (SeafBranchManager *mgr,
                                const char *repo_id,
                                const char *name); // 获取分支


gboolean
seaf_branch_manager_branch_exists (SeafBranchManager *mgr,
                                   const char *repo_id,
                                   const char *name); // 是否存在分支

GList *
seaf_branch_manager_get_branch_list (SeafBranchManager *mgr,
                                     const char *repo_id); // 获取某仓库的分支列表

gint64
seaf_branch_manager_calculate_branch_size (SeafBranchManager *mgr,
                                           const char *repo_id, 
                                           const char *commit_id); // 获取分支大小
#endif /* SEAF_BRANCH_MGR_H */
