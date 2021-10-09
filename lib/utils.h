/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* 实用封装 (对系统和依赖库的接口做了本地化处理) */

#ifndef CCNET_UTILS_H
#define CCNET_UTILS_H

#ifdef WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x500
#endif
#include <windows.h>
#endif

#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <sys/stat.h>

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <event2/util.h>
#else
#include <evutil.h>
#endif

#ifdef __linux__
#include <endian.h>
#endif

#ifdef __OpenBSD__
#include <machine/endian.h>
#endif

#ifdef WIN32 // Windows
#include <errno.h>
#include <glib/gstdio.h>

#ifndef WEXITSTATUS
#define WEXITSTATUS(status) (((status) & 0xff00) >> 8)
#endif

/* Borrowed from libevent */
#define ccnet_pipe_t intptr_t

int pgpipe (ccnet_pipe_t handles[2]);
/* Should only be called in main loop. */
#define ccnet_pipe(a) pgpipe((a))
#define piperead(a,b,c) recv((a),(b),(c),0)
#define pipewrite(a,b,c) send((a),(b),(c),0)
#define pipeclose(a) closesocket((a))

#define SeafStat struct __stat64

#else // Linux

#define ccnet_pipe_t int // 定义ccnet管道的id类型 （实际为int）

#define ccnet_pipe(a) pipe((a)) // 定义ccnet管道函数 （实际为pipe）
#define piperead(a,b,c) read((a),(b),(c)) // 定义管道读函数 （实际为read）
#define pipewrite(a,b,c) write((a),(b),(c)) // 定义管道写函数 （实际为write）
#define pipeclose(a) close((a)) // 定义管道关闭函数 （实际为close）

#define SeafStat struct stat // 定义seafile状态状态结构体 （实际为stat）

#endif

#define pipereadn(a,b,c) recvn((a),(b),(c)) // 定义管道读n字节函数 （实际为recvn）
#define pipewriten(a,b,c) sendn((a),(b),(c)) // 定义管道写n字节函数 （实际为sendn）

int seaf_stat (const char *path, SeafStat *st); // 根据seafile的路径获取状态
int seaf_fstat(int fd, SeafStat *st);           // 根据seafile的文件描述符获取状态

#ifdef WIN32 // Windows
void
seaf_stat_from_find_data (WIN32_FIND_DATAW *fdata, SeafStat *st);
#endif

int seaf_set_file_time (const char *path, guint64 mtime); // 设置seafile文件时间

#ifdef WIN32 // Windows
wchar_t *
win32_long_path (const char *path);

/* Convert a (possible) 8.3 format path to long path */
wchar_t *
win32_83_path_to_long_path (const char *worktree, const wchar_t *path, int path_len);

__time64_t
file_time_to_unix_time (FILETIME *ftime);
#endif

int
seaf_util_unlink (const char *path); // 删除seafile文件

int
seaf_util_rmdir (const char *path); // 删除seafile目录

int
seaf_util_mkdir (const char *path, mode_t mode); // 创建seafile目录

int
seaf_util_open (const char *path, int flags); // 打开seafile文件

int
seaf_util_create (const char *path, int flags, mode_t mode); // 创建seafile文件

int
seaf_util_rename (const char *oldpath, const char *newpath); // 重命名seafile文件

gboolean
seaf_util_exists (const char *path); // 判断seafile文件是否存在

gint64
seaf_util_lseek (int fd, gint64 offset, int whence); // 改变seafile文件读写指针位置

#ifdef WIN32 // Windows

typedef int (*DirentCallback) (wchar_t *parent,
                               WIN32_FIND_DATAW *fdata,
                               void *user_data,
                               gboolean *stop);

int
traverse_directory_win32 (wchar_t *path_w,
                          DirentCallback callback,
                          void *user_data);
#endif

#ifndef O_BINARY // 定义二进制0
#define O_BINARY 0
#endif

/* for debug */
#ifndef ccnet_warning // 定义ccnet警告输出
#define ccnet_warning(fmt, ...) g_warning("%s(%d): " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#ifndef ccnet_error // 定义ccnet错误输出
#define ccnet_error(fmt, ...)   g_error("%s(%d): " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#ifndef ccnet_message // 定义ccnet消息输出
#define ccnet_message(fmt, ...) g_message("%s(%d): " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#define CCNET_DOMAIN g_quark_from_string("ccnet") // 定义一个值，该值等于字符串"ccnet"对应的唯一quark值（字符串哈希）


struct timeval timeval_from_msec (uint64_t milliseconds); // 声明ms转timeval函数


size_t ccnet_strlcpy (char *dst, const char *src, size_t size); // 进行字符串复制

void rawdata_to_hex (const unsigned char *rawdata, char *hex_str, int n_bytes); // 字符串转十六进制串
int hex_to_rawdata (const char *hex_str, unsigned char *rawdata, int n_bytes); // 十六进制串转字符串

#define sha1_to_hex(sha1, hex) rawdata_to_hex((sha1), (hex), 20) // 定义sha1转十六进制函数，其中sha1取20位
#define hex_to_sha1(hex, sha1) hex_to_rawdata((hex), (sha1), 20) // 定义十六进制转sha1函数，其中十六进制取20位

/* If msg is NULL-terminated, set len to -1 */
int calculate_sha1 (unsigned char *sha1, const char *msg, int len); // 字符串转sha1
int ccnet_sha1_equal (const void *v1, const void *v2); // 进行sha1对比
unsigned int ccnet_sha1_hash (const void *v); //sha1哈希（sha1串映射为int64）

char* gen_uuid (); // 生成uuid
void gen_uuid_inplace (char *buf); // 生成uuid并赋值给buf
gboolean is_uuid_valid (const char *uuid_str); // 判断uuid是否有效

gboolean
is_object_id_valid (const char *obj_id); // 判断ccent对象是否有效

/* dir operations */ // 目录操作
int checkdir (const char *dir); // 检查目录是否存在
int checkdir_with_mkdir (const char *path); // 检查目录是否存在，若不存在则创建
char* ccnet_expand_path (const char *src); // ccnet路径规范化

/**
 * Make directory with 256 sub-directories from '00' to 'ff'.
 * `base` and subdir will be created if they are not existing. 
 */
int objstore_mkdir (const char *base); // 创建ccnet对象存储目录
void objstore_get_path (char *path, const char *base, const char *obj_id); // 给定ccnet对象路径base，以及ccnet对象的id（'aa'+`id`），得到它的存储路径至path

/* Read "n" bytes from a descriptor. */ // 从seafile中操作n个字节
ssize_t	readn(int fd, void *vptr, size_t n);
ssize_t writen(int fd, const void *vptr, size_t n);

/* Read "n" bytes from a socket. */ // 从socket中操作n个字节
ssize_t	recvn(evutil_socket_t fd, void *vptr, size_t n);
ssize_t sendn(evutil_socket_t fd, const void *vptr, size_t n);

int copy_fd (int ifd, int ofd); // 复制seafile，根据文件描述符
int copy_file (const char *dst, const char *src, int mode); // 复制seafile，根据路径


/* string utilities */ // 字符串相关
char** strsplit_by_char (char *string, int *length, char c); // 根据字符分割字符串

char * strjoin_n (const char *seperator, int argc, char **argv); // 连接字符串

int is_ipaddr_valid (const char *ip); // 检查ip地址是否有效

// 键值对相关
typedef void (*KeyValueFunc) (void *data, const char *key, char *value); // 定义键值对操作函数
void parse_key_value_pairs (char *string, KeyValueFunc func, void *data); // 从string提取key和value，然后传递到上述函数（data作为基址存放结果）
// string的格式为：每一行代表一个键值对，键值之间以空格作为分隔符。例如"a b\nc d"即"{a:b, c:d}"
// string只能是临时变量，无法进行复用

typedef gboolean (*KeyValueFunc2) (void *data, const char *key,
                                   const char *value); // 同上，区别在于此处值只读；并且若func返回0则直接终止
void parse_key_value_pairs2 (char *string, KeyValueFunc2 func, void *data);

// 字符串列表相关
GList *string_list_append (GList *str_list, const char *string); // 字符串列表增加一个字符串
GList *string_list_append_sorted (GList *str_list, const char *string); // 字符串列表排序
GList *string_list_remove (GList *str_list, const char *string); // 字符串列表删除一个
void string_list_free (GList *str_list); // 清空字符串列表
gboolean string_list_is_exists (GList *str_list, const char *string); // 判断字符串是否在列表中
void string_list_join (GList *str_list, GString *strbuf, const char *seperator); // 字符串列表连接成新字符串（相当于join(s[])）
GList *string_list_parse (const char *list_in_str, const char *seperator); // 根据分隔符切割然后生成字符串列表（相当于split(s)）
GList *string_list_parse_sorted (const char *list_in_str, const char *seperator); // 同上并排序（相当于sorted(split(s))）
gboolean string_list_sorted_is_equal (GList *list1, GList *list2); // 判断两个列表是否相同

char** ncopy_string_array (char **orig, int n); // 字符串数组复制前n个
void nfree_string_array (char **array, int n); // 字符串数组释放前n个

/* 64bit time */ // 时间相关
gint64 get_current_time();


// ccnet加密和解密
int
ccnet_encrypt (char **data_out,
               int *out_len,
               const char *data_in,
               const int in_len,
               const char *code,
               const int code_len);


int
ccnet_decrypt (char **data_out,
               int *out_len,
               const char *data_in,
               const int in_len,
               const char *code,
               const int code_len);


/*
 * Utility functions for converting data to/from network byte order.
 */ // 网络相关，字节变换

static inline uint64_t
bswap64 (uint64_t val) // 字节高低位交换
{
    uint64_t ret;
    uint8_t *ptr = (uint8_t *)&ret;

    ptr[0]=((val)>>56)&0xFF;
    ptr[1]=((val)>>48)&0xFF;
    ptr[2]=((val)>>40)&0xFF;
    ptr[3]=((val)>>32)&0xFF;
    ptr[4]=((val)>>24)&0xFF;
    ptr[5]=((val)>>16)&0xFF;
    ptr[6]=((val)>>8)&0xFF;
    ptr[7]=(val)&0xFF;

    return ret;
}

static inline uint64_t
hton64(uint64_t val) // 主机字节序转变成网络字节序
{
#if __BYTE_ORDER == __LITTLE_ENDIAN || defined WIN32 || defined __APPLE__
    return bswap64 (val); // bswap
#else
    return val;
#endif
}

static inline uint64_t 
ntoh64(uint64_t val) // 上面的逆变换
{
#if __BYTE_ORDER == __LITTLE_ENDIAN || defined WIN32 || defined __APPLE__
    return bswap64 (val); // bswap
#else
    return val;
#endif
}

static inline void put64bit(uint8_t **ptr,uint64_t val) // 写入64位的数据
{
    uint64_t val_n = hton64 (val);
    *((uint64_t *)(*ptr)) = val_n;
    (*ptr)+=8;
}

static inline void put32bit(uint8_t **ptr,uint32_t val) // 写入32位的数据
{
    uint32_t val_n = htonl (val);
    *((uint32_t *)(*ptr)) = val_n;
    (*ptr)+=4;
}

static inline void put16bit(uint8_t **ptr,uint16_t val) // 写入16位的数据
{
    uint16_t val_n = htons (val);
    *((uint16_t *)(*ptr)) = val_n;
    (*ptr)+=2;
}

static inline uint64_t get64bit(const uint8_t **ptr) // 读下一个64位的数据
{
    uint64_t val_h = ntoh64 (*((uint64_t *)(*ptr)));
    (*ptr)+=8;
    return val_h;
}

static inline uint32_t get32bit(const uint8_t **ptr) // 读下一个32位的数据
{
    uint32_t val_h = ntohl (*((uint32_t *)(*ptr)));
    (*ptr)+=4;
    return val_h;
}

static inline uint16_t get16bit(const uint8_t **ptr) // 读下一个16位的数据
{
    uint16_t val_h = ntohs (*((uint16_t *)(*ptr)));
    (*ptr)+=2;
    return val_h;
}

/* Convert between local encoding and utf8. Returns the converted
 * string if success, otherwise return NULL
 */ // 系统语言转utf8
char *ccnet_locale_from_utf8 (const gchar *src);
char *ccnet_locale_to_utf8 (const gchar *src);

/* Detect whether a process with the given name is running right now. */
// 根据进程名判断进程是否正在运行
gboolean process_is_running(const char *name);

/* count how much instance of a program is running  */
// 获取某进程正在运行的实例的数目
int count_process (const char *process_name_in);

#ifdef WIN32 // Windows
int win32_kill_process (const char *process_name_in);
int win32_spawn_process (char *cmd, char *wd);
char *wchar_to_utf8 (const wchar_t *src);
wchar_t *wchar_from_utf8 (const char *src);
#endif

char* ccnet_object_type_from_id (const char *object_id); // 根据id获取ccnet对象

gint64 ccnet_calc_directory_size (const char *path, GError **error); // 获取ccnet对象目录的大小

#ifdef WIN32
char * strtok_r(char *s, const char *delim, char **save_ptr);
#endif

// Json相关
#include <jansson.h>

const char *
json_object_get_string_member (json_t *object, const char *key); // object[key] -> str

gboolean
json_object_has_member (json_t *object, const char *key); // object[key] != null

gint64
json_object_get_int_member (json_t *object, const char *key); // object[key] -> int

void
json_object_set_string_member (json_t *object, const char *key, const char *value); // object[key] = (str)

void
json_object_set_int_member (json_t *object, const char *key, gint64 value); // object[key] = (int)

/* Replace invalid UTF-8 bytes with '?' */
// 将非utf-8字符转化为'?'
void
clean_utf8_data (char *data, int len);

char *
normalize_utf8_path (const char *path); // 将路径字符串转为utf-8

/* zlib related functions. */
// Zlib文件压缩相关
int
seaf_compress (guint8 *input, int inlen, guint8 **output, int *outlen); // 压缩seafile文件数据

int
seaf_decompress (guint8 *input, int inlen, guint8 **output, int *outlen); // 解压缩seafile文件数据

// 其他
char*
format_dir_path (const char *path); // 目录路径格式化

gboolean
is_empty_string (const char *str); // 字符串判空

gboolean
is_permission_valid (const char *perm); // 判断权限是否有效（仅'r'和'rw'有效）

char *
seaf_key_file_get_string (GKeyFile *key_file,
                          const char *group,
                          const char *key,
                          GError **error); // 配置文件取值，key_file[group][key] -> str

gchar* ccnet_key_file_get_string (GKeyFile *keyf,
                                  const char *category,
                                  const char *key); // 配置文件取值，keyf[category][key] -> str，并删除末尾的空格

#endif
