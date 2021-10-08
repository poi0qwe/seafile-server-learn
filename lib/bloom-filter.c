/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* 布隆过滤器 */
// https://zh.wikipedia.org/zh-cn/%E5%B8%83%E9%9A%86%E8%BF%87%E6%BB%A4%E5%99%A8

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <openssl/sha.h>
#include <assert.h>

#include "bloom-filter.h"

#define SETBIT(a, n) (a[n/CHAR_BIT] |= (1<<(n%CHAR_BIT))) // 将a的第n位设为1
#define CLEARBIT(a, n) (a[n/CHAR_BIT] &= ~(1<<(n%CHAR_BIT))) // 将a的第n位设为0
#define GETBIT(a, n) (a[n/CHAR_BIT] & (1<<(n%CHAR_BIT))) // 获取a的第n位

Bloom* bloom_create(size_t size, int k, int counting)
{
    Bloom *bloom;
    size_t csize = 0;

    if (k <=0 || k > 4) return NULL; // 限定0<k<=4，即
    
    if ( !(bloom = malloc(sizeof(Bloom))) ) return NULL; // 创建结构体
    if ( !(bloom->a = calloc((size+CHAR_BIT-1)/CHAR_BIT, sizeof(char))) ) // 申请size/CHAR_BIT的数组空间
    {
        free (bloom);
        return NULL; // 申请失败
    }
    if (counting) { // 如果是计数布隆过滤器
        csize = size*4;
        bloom->counters = calloc((csize+CHAR_BIT-1)/CHAR_BIT, sizeof(char)); // 申请csize/CHAR_BIT的数组空间
        if (!bloom->counters) { // 申请失败
            free (bloom);
            return NULL;
        }
    }
    // 将参数保存至结构体中
    bloom->asize = size;
    bloom->csize = csize;
    bloom->k = k;
    bloom->counting = counting;

    return bloom;
}

int bloom_destroy(Bloom *bloom) // 销毁
{ // 释放申请的空间
    free (bloom->a);
    if (bloom->counting) free (bloom->counters);
    free (bloom);

    return 0;
}

static void
incr_bit (Bloom *bf, unsigned int bit_idx) // 过滤器的第bit_idx位加一
{
    unsigned int char_idx, offset;
    unsigned char value;
    unsigned int high;
    unsigned int low;

    SETBIT (bf->a, bit_idx); // 将bitvec的第bit_idx位设为1

    if (!bf->counting) return;

    // 计数器是16位的，每半个字节(4bit)代表一个计数器
    // 要获取bit_idx所对应的计数器，先计算该计数器所在的字节，再判断它是字节的高四位还是低四位
    char_idx = bit_idx / 2; // 计数器所在的字节
    offset = bit_idx % 2; // 高低位判断

    value = bf->counters[char_idx]; // 取字节值
    low = value & 0xF; // 低四位值
    high = (value & 0xF0) >> 4; // 高四位值

    if (offset == 0) { // 若low是计数器数值
        if (low < 0xF) // 小于15
            low++;
    } else { // 若high是计数器数值
        if (high < 0xF) // 同上
            high++;
    }
    // 写回计数器组
    value = ((high << 4) | low); // 写回字节
    bf->counters[char_idx] = value; // 写回数组
}

static void
decr_bit (Bloom *bf, unsigned int bit_idx) // 过滤器的第bit_idx位减一
{
    unsigned int char_idx, offset;
    unsigned char value;
    unsigned int high;
    unsigned int low;

    if (!bf->counting) { // 如果不是计数布隆过滤器
        CLEARBIT (bf->a, bit_idx);  // 直接将bitvec的第bit_idx位设为0
        return;
    }

    char_idx = bit_idx / 2;
    offset = bit_idx % 2;

    value = bf->counters[char_idx];
    low = value & 0xF;
    high = (value & 0xF0) >> 4;

    /* decrement, but once we have reached the max, never go back! */
    // 减一，除非计数器数值达到最大(=15)
    if (offset == 0) {
        if ((low > 0) && (low < 0xF)) // 大于0，小于15
            low--;
        if (low == 0) { // 等于0
            CLEARBIT (bf->a, bit_idx); // 将bitvec的第bit_idx位设为0
        }
    } else {
        if ((high > 0) && (high < 0xF)) // 同上
            high--;
        if (high == 0) {
            CLEARBIT (bf->a, bit_idx);
        }
    }
    // 写回
    value = ((high << 4) | low);
    bf->counters[char_idx] = value;
}

int bloom_add(Bloom *bloom, const char *s) // 增加key
{
    int i;
    SHA256_CTX c;
    unsigned char sha256[SHA256_DIGEST_LENGTH];
    size_t *sha_int = (size_t *)&sha256;
    
    SHA256_Init(&c);
    SHA256_Update(&c, s, strlen(s)); // 对s进行SHA256摘要
    SHA256_Final (sha256, &c); // 将结果存储到变量sha256中
    
    for (i=0; i < bloom->k; ++i) // 取sha256的前k位作为哈希映射结果
        incr_bit (bloom, sha_int[i] % bloom->asize); // 过滤器加一

    return 0;
}

int bloom_remove(Bloom *bloom, const char *s) // 移除key
{
    int i;
    SHA256_CTX c;
    unsigned char sha256[SHA256_DIGEST_LENGTH];
    size_t *sha_int = (size_t *)&sha256;
    
    if (!bloom->counting)
        return -1;

    SHA256_Init(&c);
    SHA256_Update(&c, s, strlen(s));
    SHA256_Final (sha256, &c);
    
    for (i=0; i < bloom->k; ++i)
        decr_bit (bloom, sha_int[i] % bloom->asize); // 过滤器减一

    return 0;
}

int bloom_test(Bloom *bloom, const char *s) // 检测key，存在则返回1，否则返回0
{
    int i;
    SHA256_CTX c;
    unsigned char sha256[SHA256_DIGEST_LENGTH];
    size_t *sha_int = (size_t *)&sha256;
    
    SHA256_Init(&c);
    SHA256_Update(&c, s, strlen(s));
    SHA256_Final (sha256, &c);
    
    for (i=0; i < bloom->k; ++i) // 检测每个哈希是否都碰撞
        if(!(GETBIT(bloom->a, sha_int[i] % bloom->asize))) return 0;

    return 1;
}
