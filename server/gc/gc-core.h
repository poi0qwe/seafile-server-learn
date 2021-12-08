#ifndef GC_CORE_H
#define GC_CORE_H

int gc_core_run (GList *repo_id_list, int dry_run, int verbose); // 垃圾仓库回收

void
delete_garbaged_repos (int dry_run); // 移除垃圾仓库

#endif
