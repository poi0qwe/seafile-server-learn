#ifndef SEAF_FUSE_H
#define SEAF_FUSE_H

#include "seafile-session.h"

int parse_fuse_path (const char *path,
                     int *n_parts, char **user, char **repo_id, char **repo_path); // 根据级别，获取fuse路径下user、repo_id、repo_path

SeafDirent *
fuse_get_dirent_by_path (SeafFSManager *mgr,
                         const char *repo_id,
                         int version,
                         const char *root_id,
                         const char *path);

/* file.c */
int read_file(SeafileSession *seaf, const char *store_id, int version,
              Seafile *file, char *buf, size_t size,
              off_t offset, struct fuse_file_info *info); // 读文件

/* getattr.c */
int do_getattr(SeafileSession *seaf, const char *path, struct stat *stbuf); // 获取状态

/* readdir.c */
int do_readdir(SeafileSession *seaf, const char *path, void *buf,
               fuse_fill_dir_t filler, off_t offset,
               struct fuse_file_info *info); // 读目录

#endif /* SEAF_FUSE_H */
