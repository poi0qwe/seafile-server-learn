#ifndef SEAF_FSCK_H
#define SEAF_FSCK_H

int
seaf_fsck (GList *repo_id_list, gboolean repair, int max_thread_num); // 检查与修复

void export_file (GList *repo_id_list, const char *seafile_dir, char *export_path); // 导出

#endif
