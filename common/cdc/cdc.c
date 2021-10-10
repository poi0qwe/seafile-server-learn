/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
    Content-Defined Chunking
    基于内容可变长度分块
    
    https://www.usenix.org/legacy/event/fast10/tech/full_papers/kruus.pdf
    CDC使修改前后的文件在分块后有尽可能多的块相同
*/

#include "common.h"

#include "log.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <glib/gstdio.h>

#include "utils.h"

#include "cdc.h"
#include "../seafile-crypt.h"

#include "rabin-checksum.h"
#define finger rabin_checksum
#define rolling_finger rabin_rolling_checksum

#define BLOCK_SZ        (1024*1024*1)
#define BLOCK_MIN_SZ    (1024*256)
#define BLOCK_MAX_SZ    (1024*1024*4)
#define BLOCK_WIN_SZ    48

#define NAME_MAX_SZ     4096

#define BREAK_VALUE     0x0013    ///0x0513

#define READ_SIZE 1024 * 4

#define BYTE_TO_HEX(b)  (((b)>=10)?('a'+b-10):('0'+b))

static int default_write_chunk (CDCDescriptor *chunk_descr) // 默认写块文件的方法
{
    char filename[NAME_MAX_SZ];
    char chksum_str[CHECKSUM_LENGTH *2 + 1];
    int fd_chunk, ret;

    memset(chksum_str, 0, sizeof(chksum_str));
    rawdata_to_hex (chunk_descr->checksum, chksum_str, CHECKSUM_LENGTH); // 将checksum转为HEX串
    snprintf (filename, NAME_MAX_SZ, "./%s", chksum_str); // 设置文件名为checksum的HEX串，并限定其最大长度为`NAME_MAX_SZ-1`
    fd_chunk = g_open (filename, O_RDWR | O_CREAT | O_BINARY, 0644); // 创建文件，并写文件
    if (fd_chunk < 0) // 打开文件失败
        return -1;    
    
    ret = writen (fd_chunk, chunk_descr->block_buf, chunk_descr->len); // 将缓冲写入文件，写n个
    close (fd_chunk); // 关闭文件
    return ret; // 返回写的结果
}

// 给定文件，初始化它的文件分块信息（只用到了文件大小）
static int init_cdc_file_descriptor (int fd,
                                     uint64_t file_size,
                                     CDCFileDescriptor *file_descr)
{
    int max_block_nr = 0;
    int block_min_sz = 0;

    file_descr->block_nr = 0; // 实际块数
    // 若为空，则设置默认值
    if (file_descr->block_min_sz <= 0)
        file_descr->block_min_sz = BLOCK_MIN_SZ;
    if (file_descr->block_max_sz <= 0)
        file_descr->block_max_sz = BLOCK_MAX_SZ;
    if (file_descr->block_sz <= 0)
        file_descr->block_sz = BLOCK_SZ;

    if (file_descr->write_block == NULL)
        file_descr->write_block = (WriteblockFunc)default_write_chunk; // 默认写块文件的方法

    block_min_sz = file_descr->block_min_sz; // 块的最小大小
    max_block_nr = ((file_size + block_min_sz - 1) / block_min_sz); // 计算最大块数（极值）
    file_descr->blk_sha1s = (uint8_t *)calloc (sizeof(uint8_t),
                                               max_block_nr * CHECKSUM_LENGTH); // 按照最大块数申请空间
    file_descr->max_block_nr = max_block_nr;

    return 0;
}

#define WRITE_CDC_BLOCK(block_sz, write_data)                \ // 写一个块（block_sz是块的大小，write_data表示是否写入硬盘）
do {                                                         \
    int _block_sz = (block_sz);                              \
    chunk_descr.len = _block_sz;                             \ // 设置缓冲长度等于块的大小
    chunk_descr.offset = offset;                             \ // 设置偏移
    ret = file_descr->write_block (file_descr->repo_id,      \ // 调用写块文件的方法（默认是default_write_chunk）
                                   file_descr->version,      \
                                   &chunk_descr,             \
            crypt, chunk_descr.checksum,                     \
                                   (write_data));            \
    if (ret < 0) {                                           \ // 写失败
        free (buf);                                          \
        g_warning ("CDC: failed to write chunk.\n");         \
        return -1;                                           \
    }                                                        \
    memcpy (file_descr->blk_sha1s +                          \
            file_descr->block_nr * CHECKSUM_LENGTH,          \
            chunk_descr.checksum, CHECKSUM_LENGTH);          \ // 将checksum作为此块的sha1值，加入到blk_sha1s的末尾
    SHA1_Update (&file_ctx, chunk_descr.checksum, 20);       \ // 更新SHA1至checksum
    file_descr->block_nr++;                                  \ // 块数加一
    offset += _block_sz;                                     \ // 偏移加上块的大小
                                                             \
    memmove (buf, buf + _block_sz, tail - _block_sz);        \ // 更新buf，把已处理过的移除（通过将未处理的拷贝到开头）
    tail = tail - _block_sz;                                 \
    cur = 0;                                                 \ // tail，cur也随之进行相对移动
}while(0); // 表示执行一次

/* content-defined chunking */
// 基于内容可变长度分块
int file_chunk_cdc(int fd_src, // 文件标识符
                   CDCFileDescriptor *file_descr, // 文件分块信息，新建或更新
                   SeafileCrypt *crypt, // 加密信息
                   gboolean write_data, // 是否写入硬盘
                   gint64 *indexed) // 块索引
{
    char *buf; // 缓冲
    uint32_t buf_sz; // 缓冲大小
    SHA_CTX file_ctx; // SHA1上下文
    CDCDescriptor chunk_descr; // 创建分块过程信息
    SHA1_Init (&file_ctx); // 初始化SHA1

    SeafStat sb; // seafile状态
    if (seaf_fstat (fd_src, &sb) < 0) {
        seaf_warning ("CDC: failed to stat: %s.\n", strerror(errno));
        return -1;
    }
    uint64_t expected_size = sb.st_size; // 设置期望大小为文件大小

    init_cdc_file_descriptor (fd_src, expected_size, file_descr); // 初始化文件分块信息
    uint32_t block_min_sz = file_descr->block_min_sz; // 块的最小大小
    uint32_t block_mask = file_descr->block_sz - 1; // 分块基准值

    int fingerprint = 0; // 指纹
    int offset = 0; // 偏移
    int ret = 0; // 返回值
    int tail, cur, rsize; // tail是buf指针，表示当前缓冲长度；cur也是buf指针，用于分块 (cur <= tail)；rsize表示从文件读多少个字节

    buf_sz = file_descr->block_max_sz;
    buf = chunk_descr.block_buf = malloc (buf_sz); // 令缓冲长度等于块的最大大小
    if (!buf)
        return -1;

    /* buf: a fix-sized buffer. // 固定大小的缓冲
     * cur: data behind (inclusive) this offset has been scanned.
     *      cur + 1 is the bytes that has been scanned. // cur后的数据都被扫描过
     * tail: length of data loaded into memory. buf[tail] is invalid. // 载入内存的数据长度
     */
    tail = cur = 0;
    while (1) {
        // 读文件至buf，尽可能使 tail >= block_min_sz+READ_SIZE
        if (tail < block_min_sz) { // 若小于块的最小大小
            rsize = block_min_sz - tail + READ_SIZE; // 则多读block_min_sz-tail个字节使其大于块的最小大小
        } else { // 反之最多读READ_SIZE个字节
            rsize = (buf_sz - tail < READ_SIZE) ? (buf_sz - tail) : READ_SIZE;
        }
        ret = readn (fd_src, buf + tail, rsize); // 读，返回读成功的字节数
        if (ret < 0) { // 失败
            seaf_warning ("CDC: failed to read: %s.\n", strerror(errno));
            free (buf);
            return -1;
        }
        tail += ret; // 指针后移
        file_descr->file_size += ret; // 已读取的数据大小增加

        if (file_descr->file_size > expected_size) { // 大于预期大小，说明处理过程中文件大小发生改变
            seaf_warning ("File size changed while chunking.\n");
            free (buf);
            return -1;
        }

        /* We've read all the data in this file. Output the block immediately
         * in two cases:
         * 1. The data left in the file is less than block_min_sz;
         * 2. We cannot find the break value until the end of this file.
         */
        // 冗余处理；即读完了文件但tail仍然小于块的最小大小
        if (tail < block_min_sz || cur >= tail) { // 直接将冗余作为一块
            if (tail > 0) {
                if (file_descr->block_nr == file_descr->max_block_nr) { // 块数不够，则意味着没找到文件尾
                    seaf_warning ("Block id array is not large enough, bail out.\n");
                    free (buf);
                    return -1;
                }
                gint64 idx_size = tail;
                WRITE_CDC_BLOCK (tail, write_data); // 写块
                if (indexed) // 记录已处理的长度
                    *indexed += idx_size;
            }
            break;
        }

        /* 
         * A block is at least of size block_min_sz.
         */
        if (cur < block_min_sz - 1)
            cur = block_min_sz - 1; // 最小分块大小（此时buf中的数据长度一定大于等于block_min_sz）

        while (cur < tail) { // 一直扫描直到达到tail
            fingerprint = (cur == block_min_sz - 1) ?  // 计算指纹
                finger(buf + cur - BLOCK_WIN_SZ + 1, BLOCK_WIN_SZ) : // 首次计算
                rolling_finger (fingerprint, BLOCK_WIN_SZ, 
                                *(buf+cur-BLOCK_WIN_SZ), *(buf + cur)); // 滚动计算
            // 1. 若两个字符串完全相同，则它们的rabin_finger一定相等
            // 2. 对给定的mask，rabin_finger的碰撞概率为2^(-len(mask))
            //    也就是说对每BLOCK_WIN_SZ个字节，它们是断点的概率=2^(-len(block_mask))=2^(-20)
            //    即，平均每2^20个字节出现一个断点，因此块的平均大小就是2^20也就是block_mask+1=block_sz

            /* get a chunk, write block info to chunk file */
            if (((fingerprint & block_mask) ==  ((BREAK_VALUE & block_mask))) // 找到断点，开始分块
                || cur + 1 >= file_descr->block_max_sz)
            {
                if (file_descr->block_nr == file_descr->max_block_nr) {
                    seaf_warning ("Block id array is not large enough, bail out.\n");
                    free (buf);
                    return -1;
                }
                gint64 idx_size = cur + 1;
                WRITE_CDC_BLOCK (cur + 1, write_data); // 将buf[0..cur]写入块
                if (indexed) // 记录已处理的长度
                    *indexed += idx_size;
                break;
            } else { // 继续寻找
                cur ++;
            }
        }
    }

    SHA1_Final (file_descr->file_sum, &file_ctx); // 结束SHA1

    free (buf); // 释放缓冲

    return 0;
}

// 同上，但根据文件路径定位文件
int filename_chunk_cdc(const char *filename,
                       CDCFileDescriptor *file_descr,
                       SeafileCrypt *crypt,
                       gboolean write_data,
                       gint64 *indexed)
{
    int fd_src = seaf_util_open (filename, O_RDONLY | O_BINARY); // 打开文件，只读
    if (fd_src < 0) {
        seaf_warning ("CDC: failed to open %s.\n", filename);
        return -1;
    }

    int ret = file_chunk_cdc (fd_src, file_descr, crypt, write_data, indexed); // 调用之前的分块函数
    close (fd_src);
    return ret;
}

void cdc_init ()
{
    rabin_init (BLOCK_WIN_SZ);
}
