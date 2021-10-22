/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include "common.h"

#include "object-list.h"


ObjectList * // 创建列表
object_list_new ()
{
    ObjectList *ol = g_new0 (ObjectList, 1);

    ol->obj_hash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL); // 创建哈希表
    ol->obj_ids = g_ptr_array_new_with_free_func (g_free); // 创建指针数组

    return ol;
}

void // 释放
object_list_free (ObjectList *ol)
{
    if (ol->obj_hash)
        g_hash_table_destroy (ol->obj_hash);
    g_ptr_array_free (ol->obj_ids, TRUE);
    g_free (ol);
}

void // 序列化
object_list_serialize (ObjectList *ol, uint8_t **buffer, uint32_t *len)
{
    uint32_t i;
    uint32_t offset = 0;
    uint8_t *buf;
    int ollen = object_list_length(ol);

    buf = g_new (uint8_t, 41 * ollen); // 创建缓冲
    for (i = 0; i < ollen; ++i) {
        memcpy (&buf[offset], g_ptr_array_index(ol->obj_ids, i), 41); // 用id填充缓冲
        offset += 41;
    }

    *buffer = buf; // 返回值
    *len = 41 * ollen;
}

gboolean // 插入
object_list_insert (ObjectList *ol, const char *object_id)
{
    if (g_hash_table_lookup (ol->obj_hash, object_id)) // 存在则返回FALSE
        return FALSE;
    char *id = g_strdup(object_id);
    g_hash_table_replace (ol->obj_hash, id, id); // hash表插入
    g_ptr_array_add (ol->obj_ids, id); // 数组增加
    return TRUE;
}
