/* 使用文件系统实现seafile对象后台 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x500
#endif

#include "common.h"
#include "utils.h"
#include "obj-backend.h"

#ifndef WIN32
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#ifdef WIN32
#include <windows.h>
#include <io.h>
#endif

#define DEBUG_FLAG SEAFILE_DEBUG_OTHER
#include "log.h"

typedef struct FsPriv { // 文件系统私有域
    char *obj_dir; // seafile对象目录
    int   dir_len; // 目录长度
} FsPriv;

static void // 将对象id转为目录
id_to_path (FsPriv *priv, const char *obj_id, char path[], // 结果存储在path中
            const char *repo_id, int version)
// seafile对象路径的完整构成：
// [obj_dir]/
// [repo_id]/
// [obj_id前2位]/
// [obj_id后38位]
{
    char *pos = path;
    int n;

#if defined MIGRATION || defined SEAFILE_CLIENT
    if (version > 0) {
        n = snprintf (path, SEAF_PATH_MAX, "%s/%s/", priv->obj_dir, repo_id);
        pos += n;
    }
#else
    n = snprintf (path, SEAF_PATH_MAX, "%s/%s/", priv->obj_dir, repo_id);
    pos += n;
#endif

    memcpy (pos, obj_id, 2);
    pos[2] = '/';
    pos += 3;

    memcpy (pos, obj_id + 2, 41 - 2);
}

static int // 读文件
obj_backend_fs_read (ObjBackend *bend, // 后台结构体
                     const char *repo_id, // 仓库id
                     int version, // 版本
                     const char *obj_id, // 对象id
                     void **data, // 获得的对象数据
                     int *len) // 获得的对象数据的长度
{
    char path[SEAF_PATH_MAX];
    gsize tmp_len;
    GError *error = NULL;

    id_to_path (bend->priv, obj_id, path, repo_id, version); // 得到路径

    /* seaf_debug ("object path: %s\n", path); */

    g_file_get_contents (path, (gchar**)data, &tmp_len, &error); // 获取文件内容
    if (error) {
#ifdef MIGRATION
        g_clear_error (&error);
        id_to_path (bend->priv, obj_id, path, repo_id, 1);
        g_file_get_contents (path, (gchar**)data, &tmp_len, &error);
        if (error) {
            seaf_debug ("[obj backend] Failed to read object %s: %s.\n",
                        obj_id, error->message);
            g_clear_error (&error);
            return -1;
        }
#else
        seaf_debug ("[obj backend] Failed to read object %s: %s.\n",
                    obj_id, error->message);
        g_clear_error (&error);
        return -1;
#endif
    }

    *len = (int)tmp_len;
    return 0;
}

/*
 * Flush operating system and disk caches for @fd.
 */
static int
fsync_obj_contents (int fd) // 重置系统和硬盘对fd的缓存
{
#ifdef __linux__
    /* Some file systems may not support fsync().
     * In this case, just skip the error.
     */
    if (fsync (fd) < 0) {
        if (errno == EINVAL)
            return 0;
        else {
            seaf_warning ("Failed to fsync: %s.\n", strerror(errno));
            return -1;
        }
    }
    return 0;
#endif

#ifdef __APPLE__
    /* OS X: fcntl() is required to flush disk cache, fsync() only
     * flushes operating system cache.
     */
    if (fcntl (fd, F_FULLFSYNC, NULL) < 0) {
        seaf_warning ("Failed to fsync: %s.\n", strerror(errno));
        return -1;
    }
    return 0;
#endif

#ifdef WIN32
    HANDLE handle;

    handle = (HANDLE)_get_osfhandle (fd);
    if (handle == INVALID_HANDLE_VALUE) {
        seaf_warning ("Failed to get handle from fd.\n");
        return -1;
    }

    if (!FlushFileBuffers (handle)) {
        seaf_warning ("FlushFileBuffer() failed: %lu.\n", GetLastError());
        return -1;
    }

    return 0;
#endif
}

/*
 * Rename file from @tmp_path to @obj_path.
 * This also makes sure the changes to @obj_path's parent folder
 * is flushed to disk.
 */
static int // 重命名（由tmp_path变更为obj_path）并同步（将内容写入硬盘）
rename_and_sync (const char *tmp_path, const char *obj_path)
{
#ifdef __linux__
    char *parent_dir;
    int ret = 0;

    if (rename (tmp_path, obj_path) < 0) {
        seaf_warning ("Failed to rename from %s to %s: %s.\n",
                      tmp_path, obj_path, strerror(errno));
        return -1;
    }

    parent_dir = g_path_get_dirname (obj_path);
    int dir_fd = open (parent_dir, O_RDONLY);
    if (dir_fd < 0) {
        seaf_warning ("Failed to open dir %s: %s.\n", parent_dir, strerror(errno));
        goto out;
    }

    /* Some file systems don't support fsyncing a directory. Just ignore the error.
     */ // 有些操作系统不支持同步，直接忽略错误
    if (fsync (dir_fd) < 0) {
        if (errno != EINVAL) {
            seaf_warning ("Failed to fsync dir %s: %s.\n",
                          parent_dir, strerror(errno));
            ret = -1;
        }
        goto out;
    }

out:
    g_free (parent_dir);
    if (dir_fd >= 0)
        close (dir_fd);
    return ret;
#endif

#ifdef __APPLE__
    /*
     * OS X garantees an existence of obj_path always exists,
     * even when the system crashes.
     */
    if (rename (tmp_path, obj_path) < 0) {
        seaf_warning ("Failed to rename from %s to %s: %s.\n",
                      tmp_path, obj_path, strerror(errno));
        return -1;
    }
    return 0;
#endif

#ifdef WIN32
    wchar_t *w_tmp_path = g_utf8_to_utf16 (tmp_path, -1, NULL, NULL, NULL);
    wchar_t *w_obj_path = g_utf8_to_utf16 (obj_path, -1, NULL, NULL, NULL);
    int ret = 0;

    if (!MoveFileExW (w_tmp_path, w_obj_path,
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        seaf_warning ("MoveFilExW failed: %lu.\n", GetLastError());
        ret = -1;
        goto out;
    }

out:
    g_free (w_tmp_path);
    g_free (w_obj_path);
    return ret;
#endif
}

static int // 保存seafile对象的内容（先保存至临时文件，再更名）
save_obj_contents (const char *path, const void *data, int len, gboolean need_sync) // 路径、数据、是否同步（更新文件缓存至硬盘）
{ // 先保存再更名的与直接重写的区别在于，如果目标文件存在且正在被使用，则写数据完全会丢失；先保存至临时文件则可以将写数据暂时存储，即便发生冲突也可保存写数据
    char tmp_path[SEAF_PATH_MAX];
    int fd;

    snprintf (tmp_path, SEAF_PATH_MAX, "%s.XXXXXX", path); // 合成路径，表示临时文件
    fd = g_mkstemp (tmp_path);
    if (fd < 0) {
        seaf_warning ("[obj backend] Failed to open tmp file %s: %s.\n",
                      tmp_path, strerror(errno));
        return -1;
    }

    if (writen (fd, data, len) < 0) { // 写数据
        seaf_warning ("[obj backend] Failed to write obj %s: %s.\n",
                      tmp_path, strerror(errno));
        return -1;
    }

    if (need_sync && fsync_obj_contents (fd) < 0)
        return -1;

    /* Close may return error, especially in NFS. */
    if (close (fd) < 0) {
        seaf_warning ("[obj backend Failed close obj %s: %s.\n",
                      tmp_path, strerror(errno));
        return -1;
    }

    if (need_sync) { // 如果同步
        if (rename_and_sync (tmp_path, path) < 0) // 重命名并同步
            return -1;
    } else { // 无需同步
        if (g_rename (tmp_path, path) < 0) { // 直接重命名
            seaf_warning ("[obj backend] Failed to rename %s: %s.\n",
                          path, strerror(errno));
            return -1;
        }
    }

    return 0;
}

static int // 创建父目录（同utils）
create_parent_path (const char *path)
{
    char *dir = g_path_get_dirname (path);
    if (!dir)
        return -1;

    if (g_file_test (dir, G_FILE_TEST_EXISTS)) {
        g_free (dir);
        return 0;
    }

    if (g_mkdir_with_parents (dir, 0777) < 0) {
        seaf_warning ("Failed to create object parent path %s: %s.\n",
                      dir, strerror(errno));
        g_free (dir);
        return -1;
    }

    g_free (dir);
    return 0;
}

static int // 后台写文件
obj_backend_fs_write (ObjBackend *bend,
                      const char *repo_id,
                      int version,
                      const char *obj_id,
                      void *data,
                      int len,
                      gboolean need_sync)
{
    char path[SEAF_PATH_MAX];

    id_to_path (bend->priv, obj_id, path, repo_id, version); // 合成路径

    /* GTimeVal s, e; */

    /* g_get_current_time (&s); */

    if (create_parent_path (path) < 0) { // 创建之
        seaf_warning ("[obj backend] Failed to create path for obj %s:%s.\n",
                      repo_id, obj_id);
        return -1;
    }

    if (save_obj_contents (path, data, len, need_sync) < 0) { // 保存之
        seaf_warning ("[obj backend] Failed to write obj %s:%s.\n",
                      repo_id, obj_id);
        return -1;
    }

    /* g_get_current_time (&e); */

    /* seaf_message ("write obj time: %ldus.\n", */
    /*               ((e.tv_sec*1000000+e.tv_usec) - (s.tv_sec*1000000+s.tv_usec))); */

    return 0;
}

static gboolean // 判断文件是否存在
obj_backend_fs_exists (ObjBackend *bend,
                       const char *repo_id,
                       int version,
                       const char *obj_id)
{
    char path[SEAF_PATH_MAX];
    SeafStat st;

    id_to_path (bend->priv, obj_id, path, repo_id, version); // 合成路径

    if (seaf_stat (path, &st) == 0) // 判断是否存在
        return TRUE;

    return FALSE;
}

static void // 删除文件
obj_backend_fs_delete (ObjBackend *bend,
                       const char *repo_id,
                       int version,
                       const char *obj_id)
{
    char path[SEAF_PATH_MAX];

    id_to_path (bend->priv, obj_id, path, repo_id, version); // 合成路径
    g_unlink (path); // 删除
}

static int // 遍历
obj_backend_fs_foreach_obj (ObjBackend *bend,
                            const char *repo_id,
                            int version,
                            SeafObjFunc process, // 用户函数
                            void *user_data) // 用户参数
{
    FsPriv *priv = bend->priv;
    char *obj_dir = NULL;
    int dir_len;
    GDir *dir1 = NULL, *dir2;
    const char *dname1, *dname2;
    char obj_id[128];
    char path[SEAF_PATH_MAX], *pos;
    int ret = 0;

#if defined MIGRATION || defined SEAFILE_CLIENT
    if (version > 0)
        obj_dir = g_build_filename (priv->obj_dir, repo_id, NULL);
#else
    obj_dir = g_build_filename (priv->obj_dir, repo_id, NULL); // 获取仓库目录
#endif
    dir_len = strlen (obj_dir);

    dir1 = g_dir_open (obj_dir, 0, NULL); // 打开目录
    if (!dir1) {
        goto out;
    }

    memcpy (path, obj_dir, dir_len);
    pos = path + dir_len;

    while ((dname1 = g_dir_read_name(dir1)) != NULL) { // 遍历一级目录
        snprintf (pos, sizeof(path) - dir_len, "/%s", dname1);

        dir2 = g_dir_open (path, 0, NULL);
        if (!dir2) {
            seaf_warning ("Failed to open object dir %s.\n", path);
            continue;
        }

        while ((dname2 = g_dir_read_name(dir2)) != NULL) { // 遍历二级子目录（子目录名+文件名=obj_id）
            snprintf (obj_id, sizeof(obj_id), "%s%s", dname1, dname2);
            if (!process (repo_id, version, obj_id, user_data)) {
                g_dir_close (dir2);
                goto out;
            }
        }
        g_dir_close (dir2);
    }

out:
    if (dir1)
        g_dir_close (dir1);
    g_free (obj_dir);

    return ret;
}

static int // 复制
obj_backend_fs_copy (ObjBackend *bend,
                     const char *src_repo_id,
                     int src_version,
                     const char *dst_repo_id,
                     int dst_version,
                     const char *obj_id)
{
    char src_path[SEAF_PATH_MAX];
    char dst_path[SEAF_PATH_MAX];

    id_to_path (bend->priv, obj_id, src_path, src_repo_id, src_version); // 合成路径
    id_to_path (bend->priv, obj_id, dst_path, dst_repo_id, dst_version);

    if (g_file_test (dst_path, G_FILE_TEST_EXISTS))
        return 0;

    if (create_parent_path (dst_path) < 0) { // 创建父目录
        seaf_warning ("Failed to create dst path %s for obj %s.\n",
                      dst_path, obj_id);
        return -1;
    }

#ifdef WIN32
    if (!CreateHardLink (dst_path, src_path, NULL)) {
        seaf_warning ("Failed to link %s to %s: %lu.\n",
                      src_path, dst_path, GetLastError());
        return -1;
    }
    return 0;
#else
    int ret = link (src_path, dst_path); // 复制
    if (ret < 0 && errno != EEXIST) {
        seaf_warning ("Failed to link %s to %s: %s.\n",
                      src_path, dst_path, strerror(errno));
        return -1;
    }
    return ret;
#endif
}

static int // 移除存储
obj_backend_fs_remove_store (ObjBackend *bend, const char *store_id)
{
    FsPriv *priv = bend->priv;
    char *obj_dir = NULL;
    GDir *dir1, *dir2;
    const char *dname1, *dname2;
    char *path1, *path2;

    obj_dir = g_build_filename (priv->obj_dir, store_id, NULL); // 获取仓库路径

    dir1 = g_dir_open (obj_dir, 0, NULL);
    if (!dir1) {
        g_free (obj_dir);
        return 0;
    }

    while ((dname1 = g_dir_read_name(dir1)) != NULL) { // 遍历一级目录
        path1 = g_build_filename (obj_dir, dname1, NULL);

        dir2 = g_dir_open (path1, 0, NULL);
        if (!dir2) {
            seaf_warning ("Failed to open obj dir %s.\n", path1);
            g_dir_close (dir1);
            g_free (path1);
            g_free (obj_dir);
            return -1;
        }

        while ((dname2 = g_dir_read_name(dir2)) != NULL) { // 遍历二级子目录
            path2 = g_build_filename (path1, dname2, NULL);
            g_unlink (path2);
            g_free (path2);
        }
        g_dir_close (dir2);

        g_rmdir (path1);
        g_free (path1); // 删文件
    }

    g_dir_close (dir1);
    g_rmdir (obj_dir); // 删目录
    g_free (obj_dir);

    return 0;
}

ObjBackend * // 创建新的后台结构体（链接，表示实现方法）
obj_backend_fs_new (const char *seaf_dir, const char *obj_type)
{
    ObjBackend *bend;
    FsPriv *priv;

    bend = g_new0(ObjBackend, 1);
    priv = g_new0(FsPriv, 1);
    bend->priv = priv;

    priv->obj_dir = g_build_filename (seaf_dir, "storage", obj_type, NULL);
    priv->dir_len = strlen (priv->obj_dir);

    if (g_mkdir_with_parents (priv->obj_dir, 0777) < 0) {
        seaf_warning ("[Obj Backend] Objects dir %s does not exist and"
                   " is unable to create\n", priv->obj_dir);
        goto onerror;
    }

    bend->read = obj_backend_fs_read;
    bend->write = obj_backend_fs_write;
    bend->exists = obj_backend_fs_exists;
    bend->delete = obj_backend_fs_delete;
    bend->foreach_obj = obj_backend_fs_foreach_obj;
    bend->copy = obj_backend_fs_copy;
    bend->remove_store = obj_backend_fs_remove_store;

    return bend;

onerror:
    g_free (priv->obj_dir);
    g_free (priv);
    g_free (bend);

    return NULL;
}
