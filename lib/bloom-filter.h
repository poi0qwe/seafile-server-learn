/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* 布隆过滤器 （包含Counting Bloom Filter） */

#ifndef __BLOOM_H__
#define __BLOOM_H__

#include <stdlib.h>

typedef struct {
    size_t          asize; // bitvec(比特向量)大小
    unsigned char  *a; // bitvec
    size_t          csize; // reg-16(16位计数器组)大小，csize=4*asize
    unsigned char  *counters; // reg-16
    int             k;
    char            counting:1;
} Bloom;

Bloom *bloom_create (size_t size, int k, int counting); // 创建 (k表示哈希个数(0<k<=4)、counting表示是否是计数布隆过滤器)
int bloom_destroy (Bloom *bloom); // 销毁
int bloom_add (Bloom *bloom, const char *s); // 增加key
int bloom_remove (Bloom *bloom, const char *s); // 移除key
int bloom_test (Bloom *bloom, const char *s); // 检测key，存在则返回1，否则返回0

#endif
