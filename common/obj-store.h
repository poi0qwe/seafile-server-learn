/* seafile对象的存储 */

#ifndef OBJ_STORE_H
#define OBJ_STORE_H

#include <glib.h>
#include <sys/types.h>

struct _SeafileSession;
struct SeafObjStore;

struct SeafObjStore * // 创建新的对象存储结构体
seaf_obj_store_new (struct _SeafileSession *seaf, const char *obj_type);

int // 初始化对象存储结构体
seaf_obj_store_init (struct SeafObjStore *obj_store);

/* Synchronous I/O interface. */

int // 读对象
seaf_obj_store_read_obj (struct SeafObjStore *obj_store, // 结构体
                         const char *repo_id, // 仓库id
                         int version, // 版本
                         const char *obj_id, // 对象id
                         void **data, // 数据缓冲
                         int *len); // 长度

int // 写对象
seaf_obj_store_write_obj (struct SeafObjStore *obj_store,
                          const char *repo_id,
                          int version,
                          const char *obj_id,
                          void *data,
                          int len,
                          gboolean need_sync); // 需要同步

gboolean // 判断是否存在
seaf_obj_store_obj_exists (struct SeafObjStore *obj_store,
                           const char *repo_id,
                           int version,
                           const char *obj_id);

void // 删除
seaf_obj_store_delete_obj (struct SeafObjStore *obj_store,
                           const char *repo_id,
                           int version,
                           const char *obj_id);

typedef gboolean (*SeafObjFunc) (const char *repo_id,
                                 int version,
                                 const char *obj_id,
                                 void *user_data); // seafile对象用户函数

int // 遍历，并调用用户函数
seaf_obj_store_foreach_obj (struct SeafObjStore *obj_store,
                            const char *repo_id,
                            int version,
                            SeafObjFunc process,
                            void *user_data);

int // 复制
seaf_obj_store_copy_obj (struct SeafObjStore *obj_store,
                         const char *src_store_id,
                         int src_version,
                         const char *dst_store_id,
                         int dst_version,
                         const char *obj_id);

int // 移除整个存储
seaf_obj_store_remove_store (struct SeafObjStore *obj_store,
                             const char *store_id);

#endif
