/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef OBJECT_LIST_H
#define OBJECT_LIST_H

#include <glib.h>

typedef struct {
    GHashTable  *obj_hash; // 对象哈希表
    GPtrArray   *obj_ids; // 对象id表
} ObjectList;


ObjectList * // 创建新的对象列表
object_list_new ();

void // 释放列表
object_list_free (ObjectList *ol);

void // 进行序列化
object_list_serialize (ObjectList *ol, uint8_t **buffer, uint32_t *len);

/**
 * Add object to ObjectList.
 * Return FALSE if it is already in the list, TRUE otherwise. 
 */
gboolean // 列表插入，返回FALSE如果已存在
object_list_insert (ObjectList *ol, const char *object_id);

inline static gboolean // 列表存在
object_list_exists (ObjectList *ol, const char *object_id)
{
    return (g_hash_table_lookup(ol->obj_hash, object_id) != NULL); // 判断哈希表中存在
}

inline static int // 列表长度
object_list_length (ObjectList *ol)
{
    return ol->obj_ids->len; // 返回id表长度
}

#endif
