/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
    Content-Defined Chunking
    基于内容可变长度分块

    https://www.usenix.org/legacy/event/fast10/tech/full_papers/kruus.pdf
    CDC使修改前后的文件在分块后有尽可能多的块相同
    
    另外一个作者的解析：https://github.com/baixiangcpp/FileCDC
*/
#ifndef _CDC_H
#define _CDC_H

#include <glib.h>
#include <stdint.h>

#ifdef HAVE_MD5
#include "md5.h"
#define get_checksum md5
#define CHECKSUM_LENGTH 16
#else
#include <openssl/sha.h>
#define get_checksum sha1
#define CHECKSUM_LENGTH 20
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

struct _CDCFileDescriptor;
struct _CDCDescriptor;
struct SeafileCrypt;

// 规定写块文件的方法参数列表如下
typedef int (*WriteblockFunc)(const char *repo_id, // 仓库id
                              int version, // 版本
                              struct _CDCDescriptor *chunk_descr, // 区块
                              struct SeafileCrypt *crypt, // seafile加密
                              uint8_t *checksum, // 区块校验和
                              gboolean write_data); // 是否写数据

/* define chunk file header and block entry */
// 描述分块的过程和块文件头
typedef struct _CDCFileDescriptor { // 文件分块信息
    uint32_t block_min_sz; // 块的最小大小
    uint32_t block_max_sz; // 块的最大大小
    uint32_t block_sz;     // 块的(实际/平均)大小
    uint64_t file_size;    // 整个文件的大小

    uint32_t block_nr;     // 实际的块数
    uint8_t *blk_sha1s;    // 各个块的sha1值（若两个块的sha1值相等，则可认为它们相同）
    int max_block_nr;      // 最大的块数
    uint8_t  file_sum[CHECKSUM_LENGTH]; // 文件的校验和（若两个文件的校验和相等，则可认为它们相同）

    WriteblockFunc write_block; // 写块文件的方法

    char repo_id[37];      // 文件所属仓库的id
    int version;           // 文件的版本
} CDCFileDescriptor;

typedef struct _CDCDescriptor { // 分块过程信息
    uint64_t offset;            // 偏移（字节指针）
    uint32_t len;               // 数据缓冲长度
    uint8_t checksum[CHECKSUM_LENGTH]; // 当前校验和
    char *block_buf;                   // 数据缓冲
    int result;                        // 结果
} CDCDescriptor;

/* 输入: 一个seafile文件(其文件描述符为fd_src)、文件分块信息(CDCFileDescriptor, 空则新建)、seafile加密信息(SeafileCrypt)、是否写数据、块索引(indexed)。
 * 功能: 若CDCFileDescriptor为空，则对seafile文件进行分块，并将分块信息保存在CDCFileDescriptor中；
 *       若CDCFileDescriptor不为空，则重新对seafile文件进行分块，并更新CDCFileDescriptor。
 *       indexed记录已处理的长度
 * 输出: 0成功, -1失败。
 */
int file_chunk_cdc(int fd_src,                    // seafile文件描述符
                   CDCFileDescriptor *file_descr, // 文件分块信息
                   struct SeafileCrypt *crypt,    // seafile加密信息
                   gboolean write_data,           // 是否写入硬盘
                   gint64 *indexed);              // 已处理的长度

/* 输入: 一个seafile文件(其文件路径为filename)、其余同上
 * 功能: 同上
 * 输出: 0成功, -1失败。
 */
int filename_chunk_cdc(const char *filename,          // seafile文件路径
                       CDCFileDescriptor *file_descr, // 文件分块信息
                       struct SeafileCrypt *crypt,    // seafile加密信息
                       gboolean write_data,           // 是否写入硬盘
                       gint64 *indexed);              // 已处理的长度

void cdc_init (); // 初始化

#endif
