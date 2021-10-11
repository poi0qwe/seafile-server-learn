/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* 块的通用头文件 */

#ifndef BLOCK_H
#define BLOCK_H

typedef struct _BMetadata BlockMetadata;
typedef struct _BMetadata BMetadata;

struct _BMetadata { // 块的元数据，即块的统计信息
    char        id[41]; // 块的id
    uint32_t    size; // 块的大小
};

/* Opaque block handle.
 */
typedef struct _BHandle BlockHandle; // 别名
typedef struct _BHandle BHandle;

enum { // 枚举
    BLOCK_READ,
    BLOCK_WRITE,
};

// 定义块操作函数的参数及返回值
typedef gboolean (*SeafBlockFunc) (const char *store_id, // 仓库id
                                   int version, // 版本
                                   const char *block_id, // 块id
                                   void *user_data); // 用户参数

#endif
