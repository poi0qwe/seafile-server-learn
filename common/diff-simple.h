/* 提交差异相关（包括提交的比对、提交的合并） */

#ifndef DIFF_SIMPLE_H
#define DIFF_SIMPLE_H

#include <glib.h>
#include "seafile-session.h"

// 差异类型
#define DIFF_TYPE_WORKTREE              'W' /* diff from index to worktree */ // 工作树的索引差异
#define DIFF_TYPE_INDEX                 'I' /* diff from commit to index */ // 索引中的提交差异
#define DIFF_TYPE_COMMITS               'C' /* diff between two commits*/ // 两个提交间的差异

// 差异状态
#define DIFF_STATUS_ADDED               'A' // 被添加
#define DIFF_STATUS_DELETED             'D' // 被删除
#define DIFF_STATUS_MODIFIED	        'M' // 被修改
#define DIFF_STATUS_RENAMED             'R' // 被重命名
#define DIFF_STATUS_UNMERGED		    'U' // 未合并
#define DIFF_STATUS_DIR_ADDED           'B' // 目录被添加
#define DIFF_STATUS_DIR_DELETED         'C' // 目录被删除
#define DIFF_STATUS_DIR_RENAMED         'E' // 目录被重命名

// 未合并状态
enum {
    STATUS_UNMERGED_NONE, // 未合并
    /* I and others modified the same file differently. */
    STATUS_UNMERGED_BOTH_CHANGED, // 两者都被修改：相同文件被修改为不同结果
    /* I and others created the same file with different contents. */
    STATUS_UNMERGED_BOTH_ADDED, // 两者都被添加：两个合并都添加了相同的文件
    /* I removed a file while others modified it. */
    STATUS_UNMERGED_I_REMOVED, // 一个移除了文件，一个修改了文件
    /* Others removed a file while I modified it. */
    STATUS_UNMERGED_OTHERS_REMOVED, // 反之
    /* I replace a directory with a file while others modified files under the directory. */
    STATUS_UNMERGED_DFC_I_ADDED_FILE, // 一个将目录替换为了文件，另一个修改了目录中的文件
    /* Others replace a directory with a file while I modified files under the directory. */
    STATUS_UNMERGED_DFC_OTHERS_ADDED_FILE, // 反之
};

typedef struct DiffEntry { // 差异项
    char type; // 差异类型
    char status; // 差异状态
    int unmerge_state; // 未合并状态
    unsigned char sha1[20];     /* used for resolve rename */ // 用于解决重命名问题
    char *name; // 名称
    char *new_name;             /* only used in rename. */ // 新名称，仅被用于重命名情形
    gint64 size; // 大小
    gint64 origin_size;         /* only used in modified */ // 原始大小，仅被用于修改情形
} DiffEntry;

DiffEntry * // 创建新的差异项
diff_entry_new (char type, char status, unsigned char *sha1, const char *name);

void // 释放差异项结构体
diff_entry_free (DiffEntry *de);

/*
 * @fold_dir_diff: if TRUE, only the top level directory will be included
 *                 in the diff result if a directory with files is added or removed.
 *                 Otherwise all the files in the direcotory will be recursively
 *                 included in the diff result.
 * // 如果此项为真，只考虑顶级目录的文件增删；否则，递归整个目录的所有文件
 */
int // 比对两个提交的差异
diff_commits (SeafCommit *commit1, SeafCommit *commit2, GList **results, // 结果记录在results列表
              gboolean fold_dir_diff);

int // 比对两个提交的差异；给定根目录
diff_commit_roots (const char *store_id, int version,
                   const char *root1, const char *root2, GList **results,
                   gboolean fold_dir_diff);

int // 比对合并前后的差异（与两个父提交对比）
diff_merge (SeafCommit *merge, GList **results, gboolean fold_dir_diff);

int // 比对合并前后的差异；给定根目录
diff_merge_roots (const char *store_id, int version,
                  const char *merged_root, const char *p1_root, const char *p2_root,
                  GList **results, gboolean fold_dir_diff);

void // 解决重命名
diff_resolve_renames (GList **diff_entries);

void // 解决空目录
diff_resolve_empty_dirs (GList **diff_entries);

int // 未合并状态
diff_unmerged_state(int mask);

char * // 将差异结果格式化
format_diff_results(GList *results);

char * // 将差异结果转字符串
diff_results_to_description (GList *results);

// 比对文件用户方法
typedef int (*DiffFileCB) (int n, // 列表长度
                           const char *basedir, // 基目录
                           SeafDirent *files[], // 文件列表
                           void *data); // 用户参数
// 比对目录用户方法
typedef int (*DiffDirCB) (int n, // 列表长度
                          const char *basedir, // 基目录
                          SeafDirent *dirs[], // 目录列表
                          void *data, // 用户参数
                          gboolean *recurse); // 是否递归

typedef struct DiffOptions { // 比对选项
    char store_id[37]; // 存储id
    int version; // 版本
    // 两个回调
    DiffFileCB file_cb; 
    DiffDirCB dir_cb;
    void *data; // 用户参数
} DiffOptions;

int
diff_trees (int n, const char *roots[], DiffOptions *opt); // 比对工作树

#endif
