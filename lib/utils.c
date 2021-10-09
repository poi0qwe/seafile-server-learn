/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* 实用封装 (对系统和依赖库的接口做了本地化处理) */

#include <config.h>

#include "common.h"

#ifdef WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x500
#endif
#endif

#include "utils.h"

#ifdef WIN32

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Rpc.h>
#include <shlobj.h>
#include <psapi.h>

#else
#include <arpa/inet.h>
#endif

#ifndef WIN32
#include <pwd.h>
#include <uuid/uuid.h>
#endif

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>

#include <string.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <searpc-utils.h>

#include <jansson.h>

#include <utime.h>

#include <zlib.h>

extern int inet_pton(int af, const char *src, void *dst); // 点分IP转字节序


struct timeval
timeval_from_msec (uint64_t milliseconds) // ms转timeval
{
    struct timeval ret; // timeval { sec, usec }
    const uint64_t microseconds = milliseconds * 1000; // 先得到微秒
    ret.tv_sec  = microseconds / 1000000; // 然后转秒
    ret.tv_usec = microseconds % 1000000; // 余数的微秒数
    return ret;
}

void
rawdata_to_hex (const unsigned char *rawdata, char *hex_str, int n_bytes) // 将原始字符串转化为HEX字符串（8-bit -> 4-bit）
{
    static const char hex[] = "0123456789abcdef";
    int i;

    for (i = 0; i < n_bytes; i++) {
        unsigned int val = *rawdata++; // rawdata中是8位char
        *hex_str++ = hex[val >> 4]; // 得到高四位，并移动指针
        *hex_str++ = hex[val & 0xf]; // 得到低四位，并移动指针
    }
    *hex_str = '\0';
}

static unsigned hexval(char c) // 获取HEX的整数值
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return ~0; // 11111111，表示无效
}

int
hex_to_rawdata (const char *hex_str, unsigned char *rawdata, int n_bytes) // 将HEX字符串转原始字符串（4-bit -> 8-bit）
{
    int i;
    for (i = 0; i < n_bytes; i++) {
        unsigned int val = (hexval(hex_str[0]) << 4) | hexval(hex_str[1]); // 将两个4四位连接成八位
        if (val & ~0xff) // 结果高于八位，则无效（因为unsigned int是32位的）
            return -1;
        *rawdata++ = val;
        hex_str += 2; // 移动指针
    }
    return 0;
}

size_t
ccnet_strlcpy (char *dest, const char *src, size_t size) // 复制字符串，仅复制前min(size, len)个字符
{
    size_t ret = strlen(src); // 获取长度

    if (size) { // 若长度大于零
        size_t len = (ret >= size) ? size - 1 : ret;
        memcpy(dest, src, len); // 复制内存，不包括最后的'\0'
        dest[len] = '\0'; // 手动写最后的'\0'
    }
    return ret;
}


int
checkdir (const char *dir) // 判断seafile目录是否存在，不存在或不是目录则返回-1，否则返回0
{
    SeafStat st;

#ifdef WIN32
    /* remove trailing '\\' */
    char *path = g_strdup(dir);
    char *p = (char *)path + strlen(path) - 1;
    while (*p == '\\' || *p == '/') *p-- = '\0';
    if ((seaf_stat(dir, &st) < 0) || !S_ISDIR(st.st_mode)) {
        g_free (path);
        return -1;
    }
    g_free (path);
    return 0;
#else
    if ((seaf_stat(dir, &st) < 0) || !S_ISDIR(st.st_mode))
    // 首先根据路径查找seafile的状态，查不到则是-1；反之继续检查该seafile的状态，看它是不是目录
        return -1;
    return 0;
#endif
}

int
checkdir_with_mkdir (const char *dir) // 检查是不是目录，若不是则创建
{
#ifdef WIN32
    int ret;
    char *path = g_strdup(dir);
    char *p = (char *)path + strlen(path) - 1;
    while (*p == '\\' || *p == '/') *p-- = '\0';
    ret = g_mkdir_with_parents(path, 0755);
    g_free (path);
    return ret;
#else
    return g_mkdir_with_parents(dir, 0755); // 创建包括父目录在内的所有目录
#endif
}

int
objstore_mkdir (const char *base) // 创建ccnet对象存储目录
{
/**
 * 创建子目录从 '00' 到 'ff'.
 * 若 `base` 和 subdir 不存在则创建. 
 */
    int ret;
    int i, j, len;
    static const char hex[] = "0123456789abcdef";
    char subdir[SEAF_PATH_MAX];

    if ( (ret = checkdir_with_mkdir(base)) < 0) // 不存在则创建base
        return ret;

    len = strlen(base);
    memcpy(subdir, base, len);
    subdir[len] = G_DIR_SEPARATOR;
    subdir[len+3] = '\0'; // 组合一个新的目录路径

    for (i = 0; i < 16; i++) {
        subdir[len+1] = hex[i];
        for (j = 0; j < 16; j++) {
            subdir[len+2] = hex[j]; // 从'aa'到'ff'
            if ( (ret = checkdir_with_mkdir(subdir)) < 0)
                return ret;
        }
    }
    return 0;
}

void
objstore_get_path (char *path, const char *base, const char *obj_id) // 给定ccnet对象存储目录base，以及ccnet对象的id（'aa'+`id`），获取它的存储路径至path
{
    int len;

    len = strlen(base);
    memcpy(path, base, len); // 首先把存储目录复制到path
    path[len] = G_DIR_SEPARATOR;
    path[len+1] = obj_id[0]; // 然后把'aa'复制到path，并加上分隔符。'aa'是一个子目录
    path[len+2] = obj_id[1];
    path[len+3] = G_DIR_SEPARATOR;
    strcpy(path+len+4, obj_id+2); // 最后把对象名后几位直接接到末尾。后几位代表一个文件
}

#ifdef WIN32

/* UNIX epoch expressed in Windows time, the unit is 100 nanoseconds.
 * See http://msdn.microsoft.com/en-us/library/ms724228
 */
#define UNIX_EPOCH 116444736000000000ULL

__time64_t
file_time_to_unix_time (FILETIME *ftime)
{
    guint64 win_time, unix_time;

    win_time = (guint64)ftime->dwLowDateTime + (((guint64)ftime->dwHighDateTime)<<32);
    unix_time = (win_time - UNIX_EPOCH)/10000000;

    return (__time64_t)unix_time;
}

static int
get_utc_file_time_fd (int fd, __time64_t *mtime, __time64_t *ctime)
{
    HANDLE handle;
    FILETIME write_time, create_time;

    handle = (HANDLE)_get_osfhandle (fd);
    if (handle == INVALID_HANDLE_VALUE) {
        g_warning ("Failed to get handle from fd: %lu.\n", GetLastError());
        return -1;
    }

    if (!GetFileTime (handle, &create_time, NULL, &write_time)) {
        g_warning ("Failed to get file time: %lu.\n", GetLastError());
        return -1;
    }

    *mtime = file_time_to_unix_time (&write_time);
    *ctime = file_time_to_unix_time (&create_time);

    return 0;
}

#define EPOCH_DIFF 11644473600ULL

inline static void
unix_time_to_file_time (guint64 unix_time, FILETIME *ftime)
{
    guint64 win_time;

    win_time = (unix_time + EPOCH_DIFF) * 10000000;
    ftime->dwLowDateTime = win_time & 0xFFFFFFFF;
    ftime->dwHighDateTime = (win_time >> 32) & 0xFFFFFFFF;
}

static int
set_utc_file_time (const char *path, const wchar_t *wpath, guint64 mtime)
{
    HANDLE handle;
    FILETIME write_time;

    handle = CreateFileW (wpath,
                          GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          NULL,
                          OPEN_EXISTING,
                          FILE_FLAG_BACKUP_SEMANTICS,
                          NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        g_warning ("Failed to open %s: %lu.\n", path, GetLastError());
        return -1;
    }

    unix_time_to_file_time (mtime, &write_time);

    if (!SetFileTime (handle, NULL, NULL, &write_time)) {
        g_warning ("Failed to set file time for %s: %lu.\n", path, GetLastError());
        CloseHandle (handle);
        return -1;
    }
    CloseHandle (handle);

    return 0;
}

wchar_t *
win32_long_path (const char *path)
{
    char *long_path, *p;
    wchar_t *long_path_w;

    if (strncmp(path, "//", 2) == 0)
        long_path = g_strconcat ("\\\\?\\UNC\\", path + 2, NULL);
    else
        long_path = g_strconcat ("\\\\?\\", path, NULL);
    for (p = long_path; *p != 0; ++p)
        if (*p == '/')
            *p = '\\';

    long_path_w = g_utf8_to_utf16 (long_path, -1, NULL, NULL, NULL);

    g_free (long_path);
    return long_path_w;
}

/* Convert a (possible) 8.3 format path to long path */
wchar_t *
win32_83_path_to_long_path (const char *worktree, const wchar_t *path, int path_len)
{
    wchar_t *worktree_w = g_utf8_to_utf16 (worktree, -1, NULL, NULL, NULL);
    int wt_len;
    wchar_t *p;
    wchar_t *fullpath_w = NULL;
    wchar_t *fullpath_long = NULL;
    wchar_t *ret = NULL;
    char *fullpath;

    for (p = worktree_w; *p != L'\0'; ++p)
        if (*p == L'/')
            *p = L'\\';

    wt_len = wcslen(worktree_w);

    fullpath_w = g_new0 (wchar_t, wt_len + path_len + 6);
    wcscpy (fullpath_w, L"\\\\?\\");
    wcscat (fullpath_w, worktree_w);
    wcscat (fullpath_w, L"\\");
    wcsncat (fullpath_w, path, path_len);

    fullpath_long = g_new0 (wchar_t, SEAF_PATH_MAX);

    DWORD n = GetLongPathNameW (fullpath_w, fullpath_long, SEAF_PATH_MAX);
    if (n == 0) {
        /* Failed. */
        fullpath = g_utf16_to_utf8 (fullpath_w, -1, NULL, NULL, NULL);
        g_free (fullpath);

        goto out;
    } else if (n > SEAF_PATH_MAX) {
        /* In this case n is the necessary length for the buf. */
        g_free (fullpath_long);
        fullpath_long = g_new0 (wchar_t, n);

        if (GetLongPathNameW (fullpath_w, fullpath_long, n) != (n - 1)) {
            fullpath = g_utf16_to_utf8 (fullpath_w, -1, NULL, NULL, NULL);
            g_free (fullpath);

            goto out;
        }
    }

    /* Remove "\\?\worktree\" from the beginning. */
    ret = wcsdup (fullpath_long + wt_len + 5);

out:
    g_free (worktree_w);
    g_free (fullpath_w);
    g_free (fullpath_long);

    return ret;
}

static int
windows_error_to_errno (DWORD error)
{
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return ENOENT;
    case ERROR_ALREADY_EXISTS:
        return EEXIST;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
        return EACCES;
    case ERROR_DIR_NOT_EMPTY:
        return ENOTEMPTY;
    default:
        return 0;
    }
}

#endif

int
seaf_stat (const char *path, SeafStat *st) // 根据seafile路径获取seafile状态至st
{
#ifdef WIN32
    wchar_t *wpath = win32_long_path (path);
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    int ret = 0;

    if (!GetFileAttributesExW (wpath, GetFileExInfoStandard, &attrs)) {
        ret = -1;
        errno = windows_error_to_errno (GetLastError());
        goto out;
    }

    memset (st, 0, sizeof(SeafStat));

    if (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        st->st_mode = (S_IFDIR | S_IRWXU);
    else
        st->st_mode = (S_IFREG | S_IRUSR | S_IWUSR);

    st->st_atime = file_time_to_unix_time (&attrs.ftLastAccessTime);
    st->st_ctime = file_time_to_unix_time (&attrs.ftCreationTime);
    st->st_mtime = file_time_to_unix_time (&attrs.ftLastWriteTime);

    st->st_size = ((((__int64)attrs.nFileSizeHigh)<<32) + attrs.nFileSizeLow);

out:
    g_free (wpath);

    return ret;
#else
    return stat (path, st); // 转发到stat
#endif
}

int
seaf_fstat (int fd, SeafStat *st) // 根据seafile的id获取状态
{
#ifdef WIN32
    if (_fstat64 (fd, st) < 0)
        return -1;

    if (get_utc_file_time_fd (fd, &st->st_mtime, &st->st_ctime) < 0)
        return -1;

    return 0;
#else
    return fstat (fd, st); // 转发到fstat
#endif
}

#ifdef WIN32

void
seaf_stat_from_find_data (WIN32_FIND_DATAW *fdata, SeafStat *st)
{
    memset (st, 0, sizeof(SeafStat));

    if (fdata->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        st->st_mode = (S_IFDIR | S_IRWXU);
    else
        st->st_mode = (S_IFREG | S_IRUSR | S_IWUSR);

    st->st_atime = file_time_to_unix_time (&fdata->ftLastAccessTime);
    st->st_ctime = file_time_to_unix_time (&fdata->ftCreationTime);
    st->st_mtime = file_time_to_unix_time (&fdata->ftLastWriteTime);

    st->st_size = ((((__int64)fdata->nFileSizeHigh)<<32) + fdata->nFileSizeLow);
}

#endif

int
seaf_set_file_time (const char *path, guint64 mtime) // 设置seafile的文件时间
{
#ifndef WIN32
    struct stat st; // 文件状态
    struct utimbuf times;

    if (stat (path, &st) < 0) {
        g_warning ("Failed to stat %s: %s.\n", path, strerror(errno));
        return -1;
    }

    times.actime = st.st_atime; // 访问时间
    times.modtime = (time_t)mtime; // 修改时间

    return utime (path, &times); // API，把时间应用到文件
#else
    wchar_t *wpath = win32_long_path (path);
    int ret = 0;

    if (set_utc_file_time (path, wpath, mtime) < 0)
        ret = -1;

    g_free (wpath);
    return ret;
#endif
}

int
seaf_util_unlink (const char *path) // 删除seafile文件
{
#ifdef WIN32
    wchar_t *wpath = win32_long_path (path);
    int ret = 0;

    if (!DeleteFileW (wpath)) {
        ret = -1;
        errno = windows_error_to_errno (GetLastError());
    }

    g_free (wpath);
    return ret;
#else
    return unlink (path); // API，删除文件
#endif
}

int
seaf_util_rmdir (const char *path) // 删除seafile目录
{
#ifdef WIN32
    wchar_t *wpath = win32_long_path (path);
    int ret = 0;

    if (!RemoveDirectoryW (wpath)) {
        ret = -1;
        errno = windows_error_to_errno (GetLastError());
    }

    g_free (wpath);
    return ret;
#else
    return rmdir (path); // API，删除目录
#endif
}

int
seaf_util_mkdir (const char *path, mode_t mode) // 创建seafile目录
{
#ifdef WIN32
    wchar_t *wpath = win32_long_path (path);
    int ret = 0;

    if (!CreateDirectoryW (wpath, NULL)) {
        ret = -1;
        errno = windows_error_to_errno (GetLastError());
    }

    g_free (wpath);
    return ret;
#else
    return mkdir (path, mode); // API，创建目录
#endif
}

int
seaf_util_open (const char *path, int flags) // 打开seafile文件，返回文件描述符
{
#ifdef WIN32
    wchar_t *wpath;
    DWORD access = 0;
    HANDLE handle;
    int fd;

    access |= GENERIC_READ;
    if (flags & (O_WRONLY | O_RDWR))
        access |= GENERIC_WRITE;

    wpath = win32_long_path (path);

    handle = CreateFileW (wpath,
                          access,
                          FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                          NULL,
                          OPEN_EXISTING,
                          0,
                          NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        errno = windows_error_to_errno (GetLastError());
        g_free (wpath);
        return -1;
    }

    fd = _open_osfhandle ((intptr_t)handle, 0);

    g_free (wpath);
    return fd;
#else
    return open (path, flags); // API，打开文件，返回文件描述符
#endif
}

int
seaf_util_create (const char *path, int flags, mode_t mode) // 创建seafile文件
{
#ifdef WIN32
    wchar_t *wpath;
    DWORD access = 0;
    HANDLE handle;
    int fd;

    access |= GENERIC_READ;
    if (flags & (O_WRONLY | O_RDWR))
        access |= GENERIC_WRITE;

    wpath = win32_long_path (path);

    handle = CreateFileW (wpath,
                          access,
                          FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                          NULL,
                          CREATE_ALWAYS,
                          0,
                          NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        errno = windows_error_to_errno (GetLastError());
        g_free (wpath);
        return -1;
    }

    fd = _open_osfhandle ((intptr_t)handle, 0);

    g_free (wpath);
    return fd;
#else
    return open (path, flags, mode); // API，创建文件（指定权限），返回文件描述符
#endif
}

int
seaf_util_rename (const char *oldpath, const char *newpath) // 重命名
{
#ifdef WIN32
    wchar_t *oldpathw = win32_long_path (oldpath);
    wchar_t *newpathw = win32_long_path (newpath);
    int ret = 0;

    if (!MoveFileExW (oldpathw, newpathw, MOVEFILE_REPLACE_EXISTING)) {
        ret = -1;
        errno = windows_error_to_errno (GetLastError());
    }

    g_free (oldpathw);
    g_free (newpathw);
    return ret;
#else
    return rename (oldpath, newpath);
#endif
}

gboolean
seaf_util_exists (const char *path) // 是否存在
{
#ifdef WIN32
    wchar_t *wpath = win32_long_path (path);
    DWORD attrs;
    gboolean ret;

    attrs = GetFileAttributesW (wpath);
    ret = (attrs != INVALID_FILE_ATTRIBUTES);

    g_free (wpath);
    return ret;
#else
    return (access (path, F_OK) == 0);
#endif
}

gint64
seaf_util_lseek (int fd, gint64 offset, int whence) // 文件指针
{
#ifdef WIN32
    return _lseeki64 (fd, offset, whence);
#else
    return lseek (fd, offset, whence);
#endif
}

#ifdef WIN32

int
traverse_directory_win32 (wchar_t *path_w,
                          DirentCallback callback,
                          void *user_data)
{
    WIN32_FIND_DATAW fdata;
    HANDLE handle;
    wchar_t *pattern;
    char *path;
    int path_len_w;
    DWORD error;
    gboolean stop;
    int ret = 0;

    path = g_utf16_to_utf8 (path_w, -1, NULL, NULL, NULL);

    path_len_w = wcslen(path_w);

    pattern = g_new0 (wchar_t, (path_len_w + 3));
    wcscpy (pattern, path_w);
    wcscat (pattern, L"\\*");

    handle = FindFirstFileW (pattern, &fdata);
    if (handle == INVALID_HANDLE_VALUE) {
        g_warning ("FindFirstFile failed %s: %lu.\n",
                   path, GetLastError());
        ret = -1;
        goto out;
    }

    do {
        if (wcscmp (fdata.cFileName, L".") == 0 ||
            wcscmp (fdata.cFileName, L"..") == 0)
            continue;

        ++ret;

        stop = FALSE;
        if (callback (path_w, &fdata, user_data, &stop) < 0) {
            ret = -1;
            FindClose (handle);
            goto out;
        }
        if (stop) {
            FindClose (handle);
            goto out;
        }
    } while (FindNextFileW (handle, &fdata) != 0);

    error = GetLastError();
    if (error != ERROR_NO_MORE_FILES) {
        g_warning ("FindNextFile failed %s: %lu.\n",
                   path, error);
        ret = -1;
    }

    FindClose (handle);

out:
    g_free (path);
    g_free (pattern);
    return ret;
}

#endif

ssize_t
readn (int fd, void *buf, size_t n) // 读n个字节
{
	size_t	n_left;
	ssize_t	n_read;
	char	*ptr;

	ptr = buf;
	n_left = n;
	while (n_left > 0) {
        n_read = read(fd, ptr, n_left); // 根据文件句柄，尝试读n_left个字节，返回实际读的字节数
		if (n_read < 0) {
			if (errno == EINTR)
				n_read = 0;
			else
				return -1;
		} else if (n_read == 0)
			break;

        n_left -= n_read; // 减去读了的几个字节
        ptr += n_read;
	}
	return (n - n_left);
}

ssize_t
writen (int fd, const void *buf, size_t n) // 写，流程同上
{
	size_t		n_left;
	ssize_t		n_written;
	const char	*ptr;

	ptr = buf;
	n_left = n;
	while (n_left > 0) {
        n_written = write(fd, ptr, n_left);
		if (n_written <= 0) {
			if (n_written < 0 && errno == EINTR)
				n_written = 0;
			else
				return -1;
		}

		n_left -= n_written;
		ptr += n_written;
	}
	return n;
}


ssize_t
recvn (evutil_socket_t fd, void *buf, size_t n) // 从socket读（socket其实就是文件，也使用unix文件描述符）
{
	size_t	n_left;
	ssize_t	n_read;
	char	*ptr;

	ptr = buf;
	n_left = n;
	while (n_left > 0) {
#ifndef WIN32
        if ((n_read = read(fd, ptr, n_left)) < 0)
#else
        if ((n_read = recv(fd, ptr, n_left, 0)) < 0)
#endif
        {
			if (errno == EINTR)
				n_read = 0;
			else
				return -1;
		} else if (n_read == 0)
			break;

		n_left -= n_read;
		ptr   += n_read;
	}
	return (n - n_left);
}

ssize_t
sendn (evutil_socket_t fd, const void *buf, size_t n) // 向socket写
{
	size_t		n_left;
	ssize_t		n_written;
	const char	*ptr;

	ptr = buf;
	n_left = n;
	while (n_left > 0) {
#ifndef WIN32
        if ( (n_written = write(fd, ptr, n_left)) <= 0)
#else
        if ( (n_written = send(fd, ptr, n_left, 0)) <= 0)
#endif
        {
			if (n_written < 0 && errno == EINTR)
				n_written = 0;
			else
				return -1;
		}

		n_left -= n_written;
		ptr   += n_written;
	}
	return n;
}

int copy_fd (int ifd, int ofd) // 根据文件描述符复制文件（从ifd到ofd）
{
    while (1) {
        char buffer[8192]; // 8kb的缓冲
        ssize_t len = readn (ifd, buffer, sizeof(buffer)); // 读8kb到缓冲，获取读的长度len
        if (!len) // 没读到，结束
            break;
        if (len < 0) { // 出错
            close (ifd);
            return -1;
        }
        if (writen (ofd, buffer, len) < 0) { // 将buffer写入到ofd
            close (ofd);
            return -1;
        }
    }
    close(ifd);
    return 0;
}

int copy_file(const char *dst, const char *src, int mode) // 根据路径复制文件（若dest存在则不操作）
{
    int fdi, fdo, status; // 文件描述符、状态

    if ((fdi = g_open (src, O_RDONLY | O_BINARY, 0)) < 0) // 只读以二进制打开
        return fdi;

    fdo = g_open (dst, O_WRONLY | O_CREAT | O_EXCL | O_BINARY, mode); // 创建新文件，只写，以二进制打开
    if (fdo < 0 && errno == EEXIST) { // 已存在
        close (fdi); // 直接关
        return 0;
    } else if (fdo < 0){ // 错误
        close (fdi);
        return -1;
    }

    status = copy_fd (fdi, fdo); // 转发copy_fd
    if (close (fdo) != 0)
        return -1;

    return status; // 返回copy_fd的状态
}

char*
ccnet_expand_path (const char *src) // ccnet路径规范化，输入一个目录路径src，返回规范化后的绝对路径
{
#ifdef WIN32
    char new_path[SEAF_PATH_MAX + 1];
    char *p = new_path;
    const char *q = src;

    memset(new_path, 0, sizeof(new_path));
    if (*src == '~') {
        const char *home = g_get_home_dir();
        memcpy(new_path, home, strlen(home));
        p += strlen(new_path);
        q++;
    }
    memcpy(p, q, strlen(q));

    /* delete the charactor '\' or '/' at the end of the path
     * because the function stat faied to deal with directory names
     * with '\' or '/' in the end */
    p = new_path + strlen(new_path) - 1;
    while(*p == '\\' || *p == '/') *p-- = '\0';

    return strdup (new_path);
#else
    const char *next_in, *ntoken; // next_in是src的指针；ntoken是另一个src的指针用于定位斜杠
    char new_path[SEAF_PATH_MAX + 1]; // 规范化的结果
    char *next_out; // new_path的指针
    int len;

   /* special cases */
    if (!src || *src == '\0')
        return NULL;
    if (strlen(src) > SEAF_PATH_MAX)
        return NULL;

    next_in = src;
    next_out = new_path;
    *next_out = '\0';

    // 此步骤将src开头的~或~<user>转化为工作目录；或者对不合法路径直接返回当前工作目录
    // 结果存储到new_path中
    if (*src == '~') { // 如果源目录在主目录下
        /* handle src start with '~' or '~<user>' like '~plt' */
        struct passwd *pw = NULL;

        for ( ; *next_in != '/' && *next_in != '\0'; next_in++) ;
        
        len = next_in - src;
        if (len == 1) { // 执行者的主目录
            pw = getpwuid (geteuid()); // 获取当前用户的passwd
        } else {
            /* copy '~<user>' to new_path */
            memcpy (new_path, src, len);
            new_path[len] = '\0';
            pw = getpwnam (new_path + 1); // 获取<user>的passwd
        }
        if (pw == NULL)
            return NULL;
       
        len = strlen (pw->pw_dir);
        memcpy (new_path, pw->pw_dir, len);
        next_out = new_path + len;
        *next_out = '\0';

        if (*next_in == '\0') // 就是主目录，则直接返回
            return strdup (new_path);
    } else if (*src != '/') { // 如果不是以'/'开头，说明错误
        getcwd (new_path, SEAF_PATH_MAX); // 则设为当前用户工作目录
        for ( ; *next_out; next_out++) ; /* to '\0' */
    }

    // 此步骤对余下部分进行规范化（去除'.'和'..'，并统一在最后加上斜杠表示目录）
    while (*next_in != '\0') {
        /* move ntoken to the next not '/' char  */ // 找到下一个非斜杠
        for (ntoken = next_in; *ntoken == '/'; ntoken++) ;

        for (next_in = ntoken; *next_in != '/' 
                 && *next_in != '\0'; next_in++) ; // 找到下一个斜杠
 
        len = next_in - ntoken; // 中间就是目录名

        if (len == 0) {
            /* the path ends with '/', keep it */
            *next_out++ = '/';
            *next_out = '\0';
            break; // 路径以斜杠结尾，保留这个斜杠
        }

        if (len == 2 && ntoken[0] == '.' && ntoken[1] == '.') // 含'..'的情况
        {
            /* '..' */
            for (; next_out > new_path && *next_out != '/'; next_out--)
                ; // next_out回到父目录
            *next_out = '\0';
        } else if (ntoken[0] != '.' || len != 1) { // 有目录名的情况
            /* not '.' */
            *next_out++ = '/';
            memcpy (next_out, ntoken, len); // 复制目录名
            next_out += len;
            *next_out = '\0';
        }
        // 含'.'的情况直接跳过
    }

    /* the final special case */
    if (new_path[0] == '\0') { // 目录为空，则设为根目录
        new_path[0] = '/';
        new_path[1] = '\0';
    }
    return strdup (new_path); // 拷贝到堆并返回，避免局部变量回收
#endif
}


int
calculate_sha1 (unsigned char *sha1, const char *msg, int len) // 计算msg的SHA1
{ //（openssl） 返回20位SHA1字符串
    SHA_CTX c;

    if (len < 0)
        len = strlen(msg);

    SHA1_Init(&c);
    SHA1_Update(&c, msg, len);    
	SHA1_Final(sha1, &c);
    return 0;
}

uint32_t
ccnet_sha1_hash (const void *v) // 对SHA1字符串进行31-bit哈希
{
    /* 31 bit hash function */
    const unsigned char *p = v;
    uint32_t h = 0;
    int i;

    for (i = 0; i < 20; i++)
        h = (h << 5) - h + p[i];

    return h;
}

int
ccnet_sha1_equal (const void *v1,
                  const void *v2) // 比较两个SHA1字符串是否相同
{
    const unsigned char *p1 = v1;
    const unsigned char *p2 = v2;
    int i;

    for (i = 0; i < 20; i++)
        if (p1[i] != p2[i])
            return 0;
    
    return 1;
}

#ifndef WIN32
char* gen_uuid () // 生成UUID
{ // (libuuid)
    char *uuid_str = g_malloc (37);
    uuid_t uuid;

    uuid_generate (uuid);
    uuid_unparse_lower (uuid, uuid_str);

    return uuid_str; // 返回
}

void gen_uuid_inplace (char *buf) // 直接在buf处生成UUID
{
    uuid_t uuid;

    uuid_generate (uuid);
    uuid_unparse_lower (uuid, buf);
}

gboolean
is_uuid_valid (const char *uuid_str) // 判断uuid是否有效
{
    uuid_t uuid;

    if (!uuid_str) // 是NULL
        return FALSE;

    if (uuid_parse (uuid_str, uuid) < 0) // 把uuid_str转换为uuid失败
        return FALSE;
    return TRUE;
}

#else
char* gen_uuid ()
{
    char *uuid_str = g_malloc (37);
    unsigned char *str = NULL;
    UUID uuid;

    UuidCreate(&uuid);
    UuidToString(&uuid, &str);
    memcpy(uuid_str, str, 37);
    RpcStringFree(&str);
    return uuid_str;
}

void gen_uuid_inplace (char *buf)
{
    unsigned char *str = NULL;
    UUID uuid;

    UuidCreate(&uuid);
    UuidToString(&uuid, &str);
    memcpy(buf, str, 37);
    RpcStringFree(&str);
}

gboolean
is_uuid_valid (const char *uuid_str)
{
    if (!uuid_str)
        return FALSE;

    UUID uuid;
    if (UuidFromString((unsigned char *)uuid_str, &uuid) != RPC_S_OK)
        return FALSE;
    return TRUE;
}

#endif

gboolean
is_object_id_valid (const char *obj_id) // 判断ccnet对象id是否有效
{
    if (!obj_id)
        return FALSE;

    int len = strlen(obj_id);
    int i;
    char c;

    if (len != 40) // 要求40位
        return FALSE;

    for (i = 0; i < len; ++i) { // 要求HEX
        c = obj_id[i];
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
            continue;
        return FALSE;
    }

    return TRUE;
}

char* strjoin_n (const char *seperator, int argc, char **argv) // 将n个字符串连接
{
    GString *buf;
    int i;
    char *str;
    // argc表示数目，argv表示字符串列表
    if (argc == 0)
        return NULL;
    
    buf = g_string_new (argv[0]);
    for (i = 1; i < argc; ++i) {
        g_string_append (buf, seperator);
        g_string_append (buf, argv[i]);
    }

    str = buf->str;
    g_string_free (buf, FALSE);
    return str;
}


gboolean is_ipaddr_valid (const char *ip) // 检查ip是否有效
{
    unsigned char buf[sizeof(struct in6_addr)];

    if (evutil_inet_pton(AF_INET, ip, buf) == 1) // 是IPV4
        return TRUE;

    if (evutil_inet_pton(AF_INET6, ip, buf) == 1) // 是IPV6
        return TRUE;
    
    return FALSE;
}

void parse_key_value_pairs (char *string, KeyValueFunc func, void *data)
{
    char *line = string, *next, *space;
    char *key, *value;

    while (*line) {
        /* handle empty line */
        if (*line == '\n') { // 空行跳过
            ++line;
            continue;
        }

        for (next = line; *next != '\n' && *next; ++next) ; // 获取'\n'
        *next = '\0';
        
        for (space = line; space < next && *space != ' '; ++space) ; // 获取' '
        if (*space != ' ') {
            g_warning ("Bad key value format: %s\n", line); // 格式错误
            return;
        }
        *space = '\0';
        key = line; // 键
        value = space + 1; // 值
        
        func (data, key, value);

        line = next + 1; // 下一行
    } // string只能是临时变量，因为string的'\n'都被修改为了'\0'，且未还原，所以不能复用
}

void parse_key_value_pairs2 (char *string, KeyValueFunc2 func, void *data) // 同上，但只读，且可提前终止
{
    char *line = string, *next, *space;
    char *key, *value;

    while (*line) {
        /* handle empty line */
        if (*line == '\n') {
            ++line;
            continue;
        }

        for (next = line; *next != '\n' && *next; ++next) ;
        *next = '\0';
        
        for (space = line; space < next && *space != ' '; ++space) ;
        if (*space != ' ') {
            g_warning ("Bad key value format: %s\n", line);
            return;
        }
        *space = '\0';
        key = line;
        value = space + 1;
        
        if (func(data, key, value) == FALSE) // 返回0则直接终止
            break;

        line = next + 1;
    }
}

/**
 * string_list_is_exists:
 * @str_list: 
 * @string: a C string or %NULL
 *
 * Check whether @string is in @str_list.
 *
 * returns: %TRUE if @string is in str_list, %FALSE otherwise
 */
gboolean
string_list_is_exists (GList *str_list, const char *string) // 判断字符串是否在列表中
{
    GList *ptr;
    for (ptr = str_list; ptr; ptr = ptr->next) { // 遍历每个字符串，逐一比对
        if (g_strcmp0(string, ptr->data) == 0) // 存在
            return TRUE;
    }
    return FALSE;
}

/**
 * string_list_append:
 * @str_list: 
 * @string: a C string (can't be %NULL
 *
 * Append @string to @str_list if it is in the list.
 *
 * returns: the new start of the list
 */
GList*
string_list_append (GList *str_list, const char *string) // 增加一个字符串
{
    g_return_val_if_fail (string != NULL, str_list); // 字符串空，直接返回

    if (string_list_is_exists(str_list, string)) // 如果字符串已在集合内，直接返回
        return str_list;

    str_list = g_list_append (str_list, g_strdup(string)); // 堆中复制字符串，增加该指针
    return str_list;
}

GList *
string_list_append_sorted (GList *str_list, const char *string) // 字符串列表排序
{
    g_return_val_if_fail (string != NULL, str_list);

    if (string_list_is_exists(str_list, string))
        return str_list;

    str_list = g_list_insert_sorted_with_data (str_list, g_strdup(string),
                                 (GCompareDataFunc)g_strcmp0, NULL); // 排序
    return str_list;
}


GList *
string_list_remove (GList *str_list, const char *string) // 移除
{
    g_return_val_if_fail (string != NULL, str_list);

    GList *ptr;

    for (ptr = str_list; ptr; ptr = ptr->next) {
        if (strcmp((char *)ptr->data, string) == 0) { // 比对
            g_free (ptr->data); // 释放空间
            return g_list_delete_link (str_list, ptr); // 删去该指针
        }
    }
    return str_list;
}


void
string_list_free (GList *str_list) // 释放字符串列表
{
    GList *ptr = str_list;

    while (ptr) { // 释放所有字符串
        g_free (ptr->data);
        ptr = ptr->next;
    }

    g_list_free (str_list); // 释放此数据结构
}


void
string_list_join (GList *str_list, GString *str, const char *seperator) // 将字符串列表以seperator分隔符连接成字符串，存到str里
{
    GList *ptr;
    if (!str_list) // 列表为空，直接返回
        return;

    ptr = str_list;
    g_string_append (str, ptr->data); // 加入第一个字符串

    for (ptr = ptr->next; ptr; ptr = ptr->next) { // 遍历列表中每个字符串
        g_string_append (str, seperator); // 先加入分隔符
        g_string_append (str, (char *)ptr->data); // 再加入字符串
    }
}

GList *
string_list_parse (const char *list_in_str, const char *seperator) // 将字符串以分割符切割成字符串列表
{
    if (!list_in_str)
        return NULL;

    GList *list = NULL;
    char **array = g_strsplit (list_in_str, seperator, 0); // 切割成字符串数组
    char **ptr;

    for (ptr = array; *ptr; ptr++) {
        list = g_list_prepend (list, g_strdup(*ptr)); // 复制然后加入到列表内
    }
    list = g_list_reverse (list);
    
    g_strfreev (array); // 释放字符串数组
    return list;
}

GList *
string_list_parse_sorted (const char *list_in_str, const char *seperator) // 切割+排序
{
    GList *list = string_list_parse (list_in_str, seperator);

    return g_list_sort (list, (GCompareFunc)g_strcmp0);
}

gboolean
string_list_sorted_is_equal (GList *list1, GList *list2) // 判断两个字符串列表是否相同
{
    GList *ptr1 = list1, *ptr2 = list2;

    while (ptr1 && ptr2) { // 分别遍历每个字符串，一一比对
        if (g_strcmp0(ptr1->data, ptr2->data) != 0)
            break;

        ptr1 = ptr1->next;
        ptr2 = ptr2->next;
    }

    if (!ptr1 && !ptr2)
        return TRUE;
    return FALSE;
}

char **
ncopy_string_array (char **orig, int n) // 复制orig字符串数组中的前n个字符串并加入到新的数组内，返回新的数组
{
    char **ret = g_malloc (sizeof(char *) * n);
    int i = 0;

    for (; i < n; i++)
        ret[i] = g_strdup(orig[i]);
    return ret;
}

void
nfree_string_array (char **array, int n) // 释放array字符串数组的前n个字符串
{
    int i = 0;

    for (; i < n; i++)
        g_free (array[i]);
    g_free (array);
}

gint64
get_current_time() // 获取当前系统时间
{
    GTimeVal tv;
    gint64 t;

    g_get_current_time (&tv);
    t = tv.tv_sec * (gint64)1000000 + tv.tv_usec; // GTimeval -> timestamp
    return t;
}

#ifdef WIN32
static SOCKET pg_serv_sock = INVALID_SOCKET;
static struct sockaddr_in pg_serv_addr;

/* pgpipe() should only be called in the main loop,
 * since it accesses the static global socket.
 */
int
pgpipe (ccnet_pipe_t handles[2])
{
    int len = sizeof( pg_serv_addr );

    handles[0] = handles[1] = INVALID_SOCKET;

    if (pg_serv_sock == INVALID_SOCKET) {
        if ((pg_serv_sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
            g_warning("pgpipe failed to create socket: %d\n", WSAGetLastError());
            return -1;
        }

        memset(&pg_serv_addr, 0, sizeof(pg_serv_addr));
        pg_serv_addr.sin_family = AF_INET;
        pg_serv_addr.sin_port = htons(0);
        pg_serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (bind(pg_serv_sock, (SOCKADDR *)&pg_serv_addr, len) == SOCKET_ERROR) {
            g_warning("pgpipe failed to bind: %d\n", WSAGetLastError());
            closesocket(pg_serv_sock);
            pg_serv_sock = INVALID_SOCKET;
            return -1;
        }

        if (listen(pg_serv_sock, SOMAXCONN) == SOCKET_ERROR) {
            g_warning("pgpipe failed to listen: %d\n", WSAGetLastError());
            closesocket(pg_serv_sock);
            pg_serv_sock = INVALID_SOCKET;
            return -1;
        }

        struct sockaddr_in tmp_addr;
        int tmp_len = sizeof(tmp_addr);
        if (getsockname(pg_serv_sock, (SOCKADDR *)&tmp_addr, &tmp_len) == SOCKET_ERROR) {
            g_warning("pgpipe failed to getsockname: %d\n", WSAGetLastError());
            closesocket(pg_serv_sock);
            pg_serv_sock = INVALID_SOCKET;
            return -1;
        }
        pg_serv_addr.sin_port = tmp_addr.sin_port;
    }

    if ((handles[1] = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        g_warning("pgpipe failed to create socket 2: %d\n", WSAGetLastError());
        closesocket(pg_serv_sock);
        pg_serv_sock = INVALID_SOCKET;
        return -1;
    }

    if (connect(handles[1], (SOCKADDR *)&pg_serv_addr, len) == SOCKET_ERROR)
    {
        g_warning("pgpipe failed to connect socket: %d\n", WSAGetLastError());
        closesocket(handles[1]);
        handles[1] = INVALID_SOCKET;
        closesocket(pg_serv_sock);
        pg_serv_sock = INVALID_SOCKET;
        return -1;
    }

    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);
    if ((handles[0] = accept(pg_serv_sock, (SOCKADDR *)&client_addr, &client_len)) == INVALID_SOCKET)
    {
        g_warning("pgpipe failed to accept socket: %d\n", WSAGetLastError());
        closesocket(handles[1]);
        handles[1] = INVALID_SOCKET;
        closesocket(pg_serv_sock);
        pg_serv_sock = INVALID_SOCKET;
        return -1;
    }

    return 0;
}
#endif

/*
  The EVP_EncryptXXX and EVP_DecryptXXX series of functions have a
  weird choice of returned value.
*/
#define ENC_SUCCESS 1
#define ENC_FAILURE 0
#define DEC_SUCCESS 1
#define DEC_FAILURE 0


#include <openssl/aes.h>
#include <openssl/evp.h>

/* Block size, in bytes. For AES it can only be 16 bytes. */
#define BLK_SIZE 16
#define ENCRYPT_BLK_SIZE BLK_SIZE


int  // ccnet加密，-1失败，0成功
ccnet_encrypt (char **data_out, // 输出数据
               int *out_len, // 输出的个数
               const char *data_in, // 输入数据
               const int in_len, // 输入的个数
               const char *code, // 密文
               const int code_len) // 密码长度
{
    *data_out = NULL;
    *out_len = -1;

    /* check validation */
    if ( data_in == NULL || in_len <= 0 ||
         code == NULL || code_len <= 0) { // 非法输入

        g_warning ("Invalid params.\n");
        return -1;
    }

    EVP_CIPHER_CTX *ctx;
    int ret, key_len;
    unsigned char key[16], iv[16];
    int blks;                   

    
    /* Generate the derived key. We use AES 128 bits key,
       Electroic-Code-Book cipher mode, and SHA1 as the message digest
       when generating the key. IV is not used in ecb mode,
       actually. */
    // 根据密文生成密钥，使用AES 128bit密钥、ECB模式、SHA1摘要。IV在ECB模式中没用到。
    key_len  = EVP_BytesToKey (EVP_aes_128_ecb(), /* cipher mode */ // 对称算法类型
                               EVP_sha1(),        /* message digest */ // 摘要
                               NULL,              /* salt */ // 盐
                               (unsigned char*)code, /* passwd */ // 密文
                               code_len, // 密文长度
                               3,   /* iteration times */ // 迭代次数
                               key, /* the derived key */ // 生成的密钥
                               iv); /* IV, initial vector */ // 初始向量

    /* The key should be 16 bytes long for our 128 bit key. */
    if (key_len != 16) { // 如果返回的密钥长度不是128bit，即16B
        g_warning ("failed to init EVP_CIPHER_CTX.\n");
        return -1;
    }

    /* Prepare CTX for encryption. */
    ctx = EVP_CIPHER_CTX_new (); // 对称算法上下文

    ret = EVP_EncryptInit_ex (ctx,
                              EVP_aes_128_ecb(), /* cipher mode */ // 对称算法
                              NULL, /* engine, NULL for default */ // 引擎
                              key,  /* derived key */ // 密钥
                              iv);  /* initial vector */ // 初始向量

    if (ret == ENC_FAILURE){ // 结果为失败
        EVP_CIPHER_CTX_free (ctx); // 释放上下文
        return -1;
    }
    /* Allocating output buffer. */
    
    /*
      For EVP symmetric encryption, padding is always used __even if__
      data size is a multiple of block size, in which case the padding
      length is the block size. so we have the following:
    */ // EVP对称加密中一定会进行填充，即便数据长度是BLK_SIZE的整数倍
    
    blks = (in_len / BLK_SIZE) + 1; // 组数

    *data_out = (char *)g_malloc (blks * BLK_SIZE); // 为数据输出申请一个是BLK_SIZE整数倍的内存空间

    if (*data_out == NULL) { // 申请失败
        g_warning ("failed to allocate the output buffer.\n");
        goto enc_error;
    }                

    int update_len, final_len;

    /* Do the encryption. */ // 进行加密
    ret = EVP_EncryptUpdate (ctx,
                             (unsigned char*)*data_out,
                             &update_len, // 返回已处理的长度
                             (unsigned char*)data_in,
                             in_len);

    if (ret == ENC_FAILURE) // 加密失败
        goto enc_error;
    
    /* Finish the possible partial block. */ // 处理余下的部分
    ret = EVP_EncryptFinal_ex (ctx,
                               (unsigned char*)*data_out + update_len,
                               &final_len); // 返回已处理的长度

    *out_len = update_len + final_len; // 总长度等于两次处理的长度之和

    /* out_len should be equal to the allocated buffer size. */ // 检查长度
    if (ret == ENC_FAILURE || *out_len != (blks * BLK_SIZE))
        goto enc_error;
    
    EVP_CIPHER_CTX_free (ctx); // 释放上下文

    return 0;

enc_error: // 失败，则回收申请的内存空间

    EVP_CIPHER_CTX_free (ctx);

    *out_len = -1;

    if (*data_out != NULL)
        g_free (*data_out);

    *data_out = NULL;

    return -1;   
}

int  // ccnet加密，-1失败，0成功；上面的逆过程
ccnet_decrypt (char **data_out,
               int *out_len,
               const char *data_in,
               const int in_len,
               const char *code,
               const int code_len)
{
    *data_out = NULL;
    *out_len = -1;

    /* Check validation. Because padding is always used, in_len must
     * be a multiple of BLK_SIZE */
    if ( data_in == NULL || in_len <= 0 || in_len % BLK_SIZE != 0 ||
         code == NULL || code_len <= 0) {

        g_warning ("Invalid param(s).\n");
        return -1;
    }

    EVP_CIPHER_CTX *ctx;
    int ret, key_len;
    unsigned char key[16], iv[16];

   
    /* Generate the derived key. We use AES 128 bits key,
       Electroic-Code-Book cipher mode, and SHA1 as the message digest
       when generating the key. IV is not used in ecb mode,
       actually. */ // 根据密文生成密钥
    key_len  = EVP_BytesToKey (EVP_aes_128_ecb(), /* cipher mode */
                               EVP_sha1(),        /* message digest */
                               NULL,              /* salt */
                               (unsigned char*)code, /* passwd */
                               code_len,
                               3,   /* iteration times */
                               key, /* the derived key */
                               iv); /* IV, initial vector */

    /* The key should be 16 bytes long for our 128 bit key. */
    if (key_len != 16) {
        g_warning ("failed to init EVP_CIPHER_CTX.\n");
        return -1;
    }


    /* Prepare CTX for decryption. */
    ctx = EVP_CIPHER_CTX_new ();

    ret = EVP_DecryptInit_ex (ctx,
                              EVP_aes_128_ecb(), /* cipher mode */
                              NULL, /* engine, NULL for default */
                              key,  /* derived key */
                              iv);  /* initial vector */

    if (ret == DEC_FAILURE)
        return -1;

    /* Allocating output buffer. */
    
    *data_out = (char *)g_malloc (in_len); // 为数据输出申请内存空间

    if (*data_out == NULL) { // 申请失败
        g_warning ("failed to allocate the output buffer.\n");
        goto dec_error;
    }                

    int update_len, final_len;

    /* Do the decryption. */ // 进行解密
    ret = EVP_DecryptUpdate (ctx,
                             (unsigned char*)*data_out,
                             &update_len,
                             (unsigned char*)data_in,
                             in_len);

    if (ret == DEC_FAILURE) // 解密失败
        goto dec_error;


    /* Finish the possible partial block. */ // 处理余下部分
    ret = EVP_DecryptFinal_ex (ctx,
                               (unsigned char*)*data_out + update_len,
                               &final_len);

    *out_len = update_len + final_len;

    /* out_len should be smaller than in_len. */ // 检查长度
    if (ret == DEC_FAILURE || *out_len > in_len)
        goto dec_error;

    EVP_CIPHER_CTX_free (ctx);
    
    return 0;

dec_error: // 失败，则回收申请的内存空间

    EVP_CIPHER_CTX_free (ctx);

    *out_len = -1;
    if (*data_out != NULL)
        g_free (*data_out);

    *data_out = NULL;

    return -1;
    
}

/* convert locale specific input to utf8 encoded string  */
char *ccnet_locale_to_utf8 (const gchar *src) // 本地文字编码转utf-8
{
    if (!src)
        return NULL;

    gsize bytes_read = 0;
    gsize bytes_written = 0;
    GError *error = NULL;
    gchar *dst = NULL;

    dst = g_locale_to_utf8 // 通过g_locale_to_utf8转换
        (src,                   /* locale specific string */
         strlen(src),           /* len of src */
         &bytes_read,           /* length processed */
         &bytes_written,        /* output length */
         &error);

    if (error) {
        return NULL;
    }

    return dst;
}

/* convert utf8 input to locale specific string  */
char *ccnet_locale_from_utf8 (const gchar *src) // 上面的逆变换
{
    if (!src)
        return NULL;

    gsize bytes_read = 0;
    gsize bytes_written = 0;
    GError *error = NULL;
    gchar *dst = NULL;

    dst = g_locale_from_utf8
        (src,                   /* locale specific string */
         strlen(src),           /* len of src */
         &bytes_read,           /* length processed */
         &bytes_written,        /* output length */
         &error);

    if (error) {
        return NULL;
    }

    return dst;
}

#ifdef WIN32

static HANDLE
get_process_handle (const char *process_name_in)
{
    char name[256];
    if (strstr(process_name_in, ".exe")) {
        snprintf (name, sizeof(name), "%s", process_name_in);
    } else {
        snprintf (name, sizeof(name), "%s.exe", process_name_in);
    }

    DWORD aProcesses[1024], cbNeeded, cProcesses;

    if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
        return NULL;

    /* Calculate how many process identifiers were returned. */
    cProcesses = cbNeeded / sizeof(DWORD);

    HANDLE hProcess;
    HMODULE hMod;
    char process_name[SEAF_PATH_MAX];
    unsigned int i;

    for (i = 0; i < cProcesses; i++) {
        if(aProcesses[i] == 0)
            continue;
        hProcess = OpenProcess (PROCESS_ALL_ACCESS, FALSE, aProcesses[i]);
        if (!hProcess)
            continue;
            
        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
            GetModuleBaseName(hProcess, hMod, process_name, 
                              sizeof(process_name)/sizeof(char));
        }

        if (strcasecmp(process_name, name) == 0)
            return hProcess;
        else {
            CloseHandle(hProcess);
        }
    }
    /* Not found */
    return NULL;
}

int count_process (const char *process_name_in)
{
    char name[SEAF_PATH_MAX];
    char process_name[SEAF_PATH_MAX];
    DWORD aProcesses[1024], cbNeeded, cProcesses;
    HANDLE hProcess;
    HMODULE hMods[1024];
    int count = 0;
    int i, j;
    
    if (strstr(process_name_in, ".exe")) {
        snprintf (name, sizeof(name), "%s", process_name_in);
    } else {
        snprintf (name, sizeof(name), "%s.exe", process_name_in);
    }

    if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded)) {
        return 0;
    }

    /* Calculate how many process identifiers were returned. */
    cProcesses = cbNeeded / sizeof(DWORD);

    for (i = 0; i < cProcesses; i++) {
        if(aProcesses[i] == 0)
            continue;
        hProcess = OpenProcess (PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, aProcesses[i]);
        if (!hProcess) {
            continue;
        }
            
        if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
            for (j = 0; j < cbNeeded / sizeof(HMODULE); j++) {
                if (GetModuleBaseName(hProcess, hMods[j], process_name,
                                      sizeof(process_name))) {
                    if (strcasecmp(process_name, name) == 0)
                        count++;
                }
            } 
        }

        CloseHandle(hProcess);
    }
    
    return count;
}

gboolean
process_is_running (const char *process_name)
{
    HANDLE proc_handle = get_process_handle(process_name);

    if (proc_handle) {
        CloseHandle(proc_handle);
        return TRUE;
    } else {
        return FALSE;
    }
}

int
win32_kill_process (const char *process_name)
{
    HANDLE proc_handle = get_process_handle(process_name);

    if (proc_handle) {
        TerminateProcess(proc_handle, 0);
        CloseHandle(proc_handle);
        return 0;
    } else {
        return -1;
    }
}

int
win32_spawn_process (char *cmdline_in, char *working_directory_in)
{
    if (!cmdline_in)
        return -1;

    wchar_t *cmdline_w = NULL;
    wchar_t *working_directory_w = NULL;

    cmdline_w = wchar_from_utf8 (cmdline_in);
    if (!cmdline_in) {
        g_warning ("failed to convert cmdline_in");
        return -1;
    }
    
    if (working_directory_in) {
        working_directory_w = wchar_from_utf8 (working_directory_in);
        if (!working_directory_w) {
            g_warning ("failed to convert working_directory_in");
            return -1;
        }
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    unsigned flags;
    BOOL success;

    /* we want to execute seafile without crreating a console window */
    flags = CREATE_NO_WINDOW;

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_FORCEOFFFEEDBACK;
    si.hStdInput = (HANDLE) _get_osfhandle(0);
    si.hStdOutput = (HANDLE) _get_osfhandle(1);
    si.hStdError = (HANDLE) _get_osfhandle(2);
    
    memset(&pi, 0, sizeof(pi));

    success = CreateProcessW (NULL, cmdline_w, NULL, NULL, TRUE, flags,
                              NULL, working_directory_w, &si, &pi);
    free (cmdline_w);
    if (working_directory_w) free (working_directory_w);
    
    if (!success) {
        g_warning ("failed to fork_process: GLE=%lu\n", GetLastError());
        return -1;
    }

    /* close the handle of thread so that the process object can be freed by
     * system
     */
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}

char *
wchar_to_utf8 (const wchar_t *wch)
{
    if (wch == NULL) {
        return NULL;
    }

    char *utf8 = NULL;
    int bufsize, len;

    bufsize = WideCharToMultiByte
        (CP_UTF8,               /* multibyte code page */
         0,                     /* flags */
         wch,                   /* src */
         -1,                    /* src len, -1 for all includes \0 */
         utf8,                  /* dst */
         0,                     /* dst buf len */
         NULL,                  /* default char */
         NULL);                 /* BOOL flag indicates default char is used */

    if (bufsize <= 0) {
        g_warning ("failed to convert a string from wchar to utf8 0");
        return NULL;
    }

    utf8 = g_malloc(bufsize);
    len = WideCharToMultiByte
        (CP_UTF8,               /* multibyte code page */
         0,                     /* flags */
         wch,                   /* src */
         -1,                    /* src len, -1 for all includes \0 */
         utf8,                  /* dst */
         bufsize,               /* dst buf len */
         NULL,                  /* default char */
         NULL);                 /* BOOL flag indicates default char is used */

    if (len != bufsize) {
        g_free (utf8);
        g_warning ("failed to convert a string from wchar to utf8");
        return NULL;
    }

    return utf8;
}

wchar_t *
wchar_from_utf8 (const char *utf8)
{
    if (utf8 == NULL) {
        return NULL;
    }

    wchar_t *wch = NULL;
    int bufsize, len;

    bufsize = MultiByteToWideChar
        (CP_UTF8,               /* multibyte code page */
         0,                     /* flags */
         utf8,                  /* src */
         -1,                    /* src len, -1 for all includes \0 */
         wch,                   /* dst */
         0);                    /* dst buf len */

    if (bufsize <= 0) {
        g_warning ("failed to convert a string from wchar to utf8 0");
        return NULL;
    }

    wch = g_malloc (bufsize * sizeof(wchar_t));
    len = MultiByteToWideChar
        (CP_UTF8,               /* multibyte code page */
         0,                     /* flags */
         utf8,                  /* src */
         -1,                    /* src len, -1 for all includes \0 */
         wch,                   /* dst */
         bufsize);              /* dst buf len */

    if (len != bufsize) {
        g_free (wch);
        g_warning ("failed to convert a string from utf8 to wchar");
        return NULL;
    }

    return wch;
}

#endif  /* ifdef WIN32 */

#ifdef __linux__
/* read the link of /proc/123/exe and compare with `process_name' */
static int
find_process_in_dirent(struct dirent *dir, const char *process_name)
{
    char path[512];
    /* fisrst construct a path like /proc/123/exe */
    if (sprintf (path, "/proc/%s/exe", dir->d_name) < 0) {
        return -1;
    }

    char buf[SEAF_PATH_MAX];
    /* get the full path of exe */
    ssize_t l = readlink(path, buf, SEAF_PATH_MAX);

    if (l < 0)
        return -1;
    buf[l] = '\0';

    /* get the base name of exe */
    char *base = g_path_get_basename(buf);
    int ret = strcmp(base, process_name);
    g_free(base);

    if (ret == 0)
        return atoi(dir->d_name);
    else
        return -1;
}

/* read the /proc fs to determine whether some process is running */
gboolean process_is_running (const char *process_name)
{
    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) {
        fprintf (stderr, "failed to open /proc/ dir\n");
        return FALSE;
    }

    struct dirent *subdir = NULL;
    while ((subdir = readdir(proc_dir))) {
        char first = subdir->d_name[0];
        /* /proc/[1-9][0-9]* */
        if (first > '9' || first < '1')
            continue;
        int pid = find_process_in_dirent(subdir, process_name);
        if (pid > 0) {
            closedir(proc_dir);
            return TRUE;
        }
    }

    closedir(proc_dir);
    return FALSE;
}

int count_process(const char *process_name)
{
    int count = 0;
    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) {
        g_warning ("failed to open /proc/ :%s\n", strerror(errno));
        return FALSE;
    }

    struct dirent *subdir = NULL;
    while ((subdir = readdir(proc_dir))) {
        char first = subdir->d_name[0];
        /* /proc/[1-9][0-9]* */
        if (first > '9' || first < '1')
            continue;
        if (find_process_in_dirent(subdir, process_name) > 0) {
            count++;
        }
    }

    closedir (proc_dir);
    return count;
}

#endif

#ifdef __APPLE__
gboolean process_is_running (const char *process_name)
{
    //TODO
    return FALSE;
}
#endif

char*
ccnet_object_type_from_id (const char *object_id)
{
    char *ptr;

    if ( !(ptr = strchr(object_id, '/')) ) // 查找第一个'/'
        return NULL;

    return g_strndup(object_id, ptr - object_id); // 第一个'/'之前的字符串即ccnet的id
}


#ifdef WIN32
/**
 * In Win32 we need to use _stat64 for files larger than 2GB. _stat64 needs
 * the `path' argument in gbk encoding.
 */
    #define STAT_STRUCT struct __stat64
    #define STAT_FUNC win_stat64_utf8

static inline int
win_stat64_utf8 (char *path_utf8, STAT_STRUCT *sb)
{
    wchar_t *path_w = wchar_from_utf8 (path_utf8);
    int result = _wstat64 (path_w, sb);
    free (path_w);
    return result;
}

#else
    #define STAT_STRUCT struct stat
    #define STAT_FUNC stat
#endif

static gint64
calc_recursively (const char *path, GError **calc_error) // 递归获取path下的所有文件的大小
{
    gint64 sum = 0;

    GError *error = NULL;
    GDir *folder = g_dir_open(path, 0, &error);
    if (!folder) { // 传入的路径一定是一个目录的路径，不是目录则报错
        g_set_error (calc_error, CCNET_DOMAIN, 0,
                     "g_open() dir %s failed:%s\n", path, error->message);
        return -1;
    }

    const char *name = NULL;
    while ((name = g_dir_read_name(folder)) != NULL) { // 遍历目录的文件
        STAT_STRUCT sb;
        char *full_path= g_build_filename (path, name, NULL);
        if (STAT_FUNC(full_path, &sb) < 0) { // 获取文件状态错误
            g_set_error (calc_error, CCNET_DOMAIN, 0, "failed to stat on %s: %s\n",
                         full_path, strerror(errno));
            g_free(full_path);
            g_dir_close(folder);
            return -1;
        }

        if (S_ISDIR(sb.st_mode)) { // 是目录，继续递归
            gint64 size = calc_recursively(full_path, calc_error);
            if (size < 0) { // 失败就递归报错，提前关闭folder
                g_free (full_path);
                g_dir_close (folder);
                return -1;
            }
            sum += size; // 加上该目录下的文件大小
            g_free(full_path); // 关闭full_path
        } else if (S_ISREG(sb.st_mode)) { // 是文件
            sum += sb.st_size; // 加上文件大小
            g_free(full_path); // 关闭full_path
        }
    }

    g_dir_close (folder);
    return sum;
}

gint64
ccnet_calc_directory_size (const char *path, GError **error) // 获取目录大小
{
    return calc_recursively (path, error); // 递归获取大小
}

#ifdef WIN32
/*
 * strtok_r code directly from glibc.git /string/strtok_r.c since windows
 * doesn't have it.
 */
char *
strtok_r(char *s, const char *delim, char **save_ptr)
{
    char *token;
    
    if(s == NULL)
        s = *save_ptr;
    
    /* Scan leading delimiters.  */
    s += strspn(s, delim);
    if(*s == '\0') {
        *save_ptr = s;
        return NULL;
    }
    
    /* Find the end of the token.  */
    token = s;
    s = strpbrk(token, delim);
    
    if(s == NULL) {
        /* This token finishes the string.  */
        *save_ptr = strchr(token, '\0');
    } else {
        /* Terminate the token and make *SAVE_PTR point past it.  */
        *s = '\0';
        *save_ptr = s + 1;
    }
    
    return token;
}
#endif

/* JSON related utils. For compatibility with json-glib. */

const char *
json_object_get_string_member (json_t *object, const char *key) // 根据键key，返回json对象的值，值为字符串
{
    json_t *string = json_object_get (object, key);
    if (!string)
        return NULL;
    return json_string_value (string);
}

gboolean
json_object_has_member (json_t *object, const char *key) // 判断json对象是否包含键key
{
    return (json_object_get (object, key) != NULL);
}

gint64
json_object_get_int_member (json_t *object, const char *key) // 返回json值，值为int
{
    json_t *integer = json_object_get (object, key);
    return json_integer_value (integer);
}

void
json_object_set_string_member (json_t *object, const char *key, const char *value) // 设置值，值为字符串
{
    json_object_set_new (object, key, json_string (value));
}

void
json_object_set_int_member (json_t *object, const char *key, gint64 value) // 设置值，值为int
{
    json_object_set_new (object, key, json_integer (value));
}

void
clean_utf8_data (char *data, int len) // 将非utf-8字符转化为'?'
{
    const char *s, *e;
    char *p; // 其实s, p, e都是字符串data的指针，只是为了方便区分功能
    gboolean is_valid;

    s = data;
    p = data;

    while ((s - data) != len) {
        is_valid = g_utf8_validate (s, len - (s - data), &e); // 寻找下一个非utf8的字符，令e指向它
        if (is_valid) // 若有效，则直接退出
            break;

        if (s != e)
            p += (e - s); // 令p等于e
        *p = '?'; // p处替换为'?'
        ++p;
        s = e + 1; // 继续寻找
    }
}

char *
normalize_utf8_path (const char *path) // 将路径字符串转为utf-8
{
    if (!g_utf8_validate (path, -1, NULL)) // 存在非utf-8字符，返回空
        return NULL;
    return g_utf8_normalize (path, -1, G_NORMALIZE_NFC); // 进行标准化
}

/* zlib related wrapper functions. */

#define ZLIB_BUF_SIZE 16384

int
seaf_compress (guint8 *input, int inlen, guint8 **output, int *outlen) // seafile压缩，压缩input的inlen个字节到output，并将压缩后的长度存储到outlen
{
    int ret;
    unsigned have;
    z_stream strm; // 压缩流
    guint8 out[ZLIB_BUF_SIZE]; // 输出
    GByteArray *barray;

    if (inlen == 0)
        return -1;

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION); // 初始化
    if (ret != Z_OK) {
        g_warning ("deflateInit failed.\n");
        return -1;
    }

    strm.avail_in = inlen;
    strm.next_in = input;
    barray = g_byte_array_new ();

    do {
        strm.avail_out = ZLIB_BUF_SIZE;
        strm.next_out = out; // 令流的输出指向out
        ret = deflate(&strm, Z_FINISH);    /* no bad return value */ // 压缩
        have = ZLIB_BUF_SIZE - strm.avail_out;
        g_byte_array_append (barray, out, have); // 将out复制到barray中
    } while (ret != Z_STREAM_END); // 直到结束

    *outlen = barray->len; // 压缩后的长度
    *output = g_byte_array_free (barray, FALSE); // 将barray中的数据转移到output中，然后释放barray

    /* clean up and return */
    (void)deflateEnd(&strm); // 清空流
    return 0;
}

int
seaf_decompress (guint8 *input, int inlen, guint8 **output, int *outlen) // seafile解压，同上，逆过程
{
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char out[ZLIB_BUF_SIZE];
    GByteArray *barray;

    if (inlen == 0) {
        g_warning ("Empty input for zlib, invalid.\n");
        return -1;
    }

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK) {
        g_warning ("inflateInit failed.\n");
        return -1;
    }

    strm.avail_in = inlen;
    strm.next_in = input;
    barray = g_byte_array_new ();

    do {
        strm.avail_out = ZLIB_BUF_SIZE;
        strm.next_out = out;
        ret = inflate(&strm, Z_NO_FLUSH); // 解压
        if (ret < 0) {
            g_warning ("Failed to inflate.\n");
            goto out;
        }
        have = ZLIB_BUF_SIZE - strm.avail_out;
        g_byte_array_append (barray, out, have);
    } while (ret != Z_STREAM_END);

out:
    /* clean up and return */
    (void)inflateEnd(&strm);

    if (ret == Z_STREAM_END) { // 确认解压结束
        *outlen = barray->len;
        *output = g_byte_array_free (barray, FALSE);
        return 0;
    } else { // 解压未结束，返回错误
        g_byte_array_free (barray, TRUE);
        return -1;
    }
}

char*
format_dir_path (const char *path) // 格式化目录路径；如果传入的是文件路径，则返回它所在的目录的路径
{
    int path_len = strlen (path);
    char *rpath;
    if (path[0] != '/') { // 不以'/'开头的，以'/'开头
        rpath = g_strconcat ("/", path, NULL);
        path_len++;
    } else { // 否则直接复制
        rpath = g_strdup (path);
    }
    while (path_len > 1 && rpath[path_len-1] == '/') { // 针对path是文件路径的情况，从后往前找'/'
        rpath[path_len-1] = '\0'; // 将其替换为'\0'，直到找到'/'
        path_len--;
    }

    return rpath;
}

gboolean
is_empty_string (const char *str) // 字符串判空
{
    return !str || strcmp (str, "") == 0;
}

gboolean
is_permission_valid (const char *perm) // 判断权限是否有效（仅当权限为'r'或'rw'时有效）
{
    if (is_empty_string (perm)) {
        return FALSE;
    }

    return strcmp (perm, "r") == 0 || strcmp (perm, "rw") == 0;
}

char * // 根据配置文件获取值
seaf_key_file_get_string (GKeyFile *key_file, // 配置文件
                          const char *group, // 配置组(如：[xxxx])
                          const char *key, // 配置键(如：key=value)
                          GError **error) // 返回错误
{
    char *v;

    v = g_key_file_get_string (key_file, group, key, error); // 取值
    if (!v || v[0] == '\0') { // 如果是空字符串，就释放，然后返回空指针
        g_free (v);
        return NULL;
    }

    return g_strchomp(v);
}

gchar*
ccnet_key_file_get_string (GKeyFile *keyf,
                           const char *category,
                           const char *key) // 同上，并删除末尾的空格
{
    gchar *v;

    if (!g_key_file_has_key (keyf, category, key, NULL))
        return NULL;

    v = g_key_file_get_string (keyf, category, key, NULL);
    if (v != NULL && v[0] == '\0') {
        g_free(v);
        return NULL;
    }

    return g_strchomp(v); // 删除末尾的空格
}
