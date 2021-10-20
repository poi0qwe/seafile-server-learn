/* seafile对象的存储 */

#include "common.h"

#include "log.h"

#include "seafile-session.h"

#include "utils.h"

#include "obj-backend.h"
#include "obj-store.h"

struct SeafObjStore { // 实际上封装了一个后台
    ObjBackend   *bend;
};
typedef struct SeafObjStore SeafObjStore;

extern ObjBackend * // 创建新的seafile对象后台
obj_backend_fs_new (const char *seaf_dir, const char *obj_type);

struct SeafObjStore * // 创建
seaf_obj_store_new (SeafileSession *seaf, const char *obj_type)
{
    SeafObjStore *store = g_new0 (SeafObjStore, 1);

    if (!store)
        return NULL;

    store->bend = obj_backend_fs_new (seaf->seaf_dir, obj_type); // 创建一个指定seafile目录、指定对象类型的后台
    if (!store->bend) {
        seaf_warning ("[Object store] Failed to load backend.\n");
        g_free (store);
        return NULL;
    }

    return store;
}

int // 暂时无功能
seaf_obj_store_init (SeafObjStore *obj_store)
{
    return 0;
}

int // 读
seaf_obj_store_read_obj (struct SeafObjStore *obj_store,
                         const char *repo_id,
                         int version,
                         const char *obj_id,
                         void **data,
                         int *len)
{
    ObjBackend *bend = obj_store->bend;

    if (!repo_id || !is_uuid_valid(repo_id) ||
        !obj_id || !is_object_id_valid(obj_id)) // 判断仓库和对象是否都有效
        return -1;

    return bend->read (bend, repo_id, version, obj_id, data, len); // 转发给后台
}

int // 写，同上
seaf_obj_store_write_obj (struct SeafObjStore *obj_store,
                          const char *repo_id,
                          int version,
                          const char *obj_id,
                          void *data,
                          int len,
                          gboolean need_sync)
{
    ObjBackend *bend = obj_store->bend;

    if (!repo_id || !is_uuid_valid(repo_id) ||
        !obj_id || !is_object_id_valid(obj_id))
        return -1;

    return bend->write (bend, repo_id, version, obj_id, data, len, need_sync);
}

gboolean // 存在，同上
seaf_obj_store_obj_exists (struct SeafObjStore *obj_store,
                           const char *repo_id,
                           int version,
                           const char *obj_id)
{
    ObjBackend *bend = obj_store->bend;

    if (!repo_id || !is_uuid_valid(repo_id) ||
        !obj_id || !is_object_id_valid(obj_id))
        return FALSE;

    return bend->exists (bend, repo_id, version, obj_id);
}

void // 删除，同上
seaf_obj_store_delete_obj (struct SeafObjStore *obj_store,
                           const char *repo_id,
                           int version,
                           const char *obj_id)
{
    ObjBackend *bend = obj_store->bend;

    if (!repo_id || !is_uuid_valid(repo_id) ||
        !obj_id || !is_object_id_valid(obj_id))
        return;

    return bend->delete (bend, repo_id, version, obj_id);
}

int // 遍历，同样转发给后台
seaf_obj_store_foreach_obj (struct SeafObjStore *obj_store,
                            const char *repo_id,
                            int version,
                            SeafObjFunc process,
                            void *user_data)
{
    ObjBackend *bend = obj_store->bend;

    return bend->foreach_obj (bend, repo_id, version, process, user_data);
}

int // 复制，同上
seaf_obj_store_copy_obj (struct SeafObjStore *obj_store,
                         const char *src_repo_id,
                         int src_version,
                         const char *dst_repo_id,
                         int dst_version,
                         const char *obj_id)
{
    ObjBackend *bend = obj_store->bend;

    if (strcmp (obj_id, EMPTY_SHA1) == 0)
        return 0;

    return bend->copy (bend, src_repo_id, src_version, dst_repo_id, dst_version, obj_id);
}

int // 移除，同上
seaf_obj_store_remove_store (struct SeafObjStore *obj_store,
                             const char *store_id)
{
    ObjBackend *bend = obj_store->bend;

    return bend->remove_store (bend, store_id);
}
