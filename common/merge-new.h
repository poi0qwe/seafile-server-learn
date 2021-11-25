/* 处理合并 */
// 项目中只涉及三路合并，二路合并只调用回调函数

#ifndef MERGE_NEW_H
#define MERGE_NEW_H

#include "common.h"

#include "fs-mgr.h"

struct MergeOptions;

typedef int (*MergeCallback) (const char *basedir,
                              SeafDirent *dirents[],
                              struct MergeOptions *opt);

typedef struct MergeOptions { // 合并选项
    int                 n_ways; /* only 2 and 3 way merges are supported. */ // n路(n=2,3)

    MergeCallback       callback; // 回调
    void *              data; // 用户参数

    /* options only used in 3-way merge. */ // 仅三路合并使用
    char                remote_repo_id[37]; // 远程仓库id
    char                remote_head[41]; // 远程仓库分支头
    gboolean            do_merge;    /* really merge the contents
                                      * and handle conflicts */ // 是否真合并且处理冲突
    char                merged_tree_root[41]; /* merge result */ // 合并后的根
    int                 visit_dirs; // 是否访问目录
    gboolean            conflict; // 是否冲突
} MergeOptions;

int // 开始合并（n路；二路：remote>-<head；三路：(remote>-<head)on(base)）
seaf_merge_trees (const char *store_id, int version,
                  int n, const char *roots[], MergeOptions *opt); // 根据多个根目录id

#endif
