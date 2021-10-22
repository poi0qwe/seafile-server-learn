/* seafile对象后台
 
seafile对象的本质就是键值对：repo_id + version + obj_id -> obj
*/
#ifndef OBJ_BACKEND_H
#define OBJ_BACKEND_H

#include <glib.h>
#include "obj-store.h"

typedef struct ObjBackend ObjBackend;

struct ObjBackend { // 后台结构体，封装了需要的函数
    int         (*read) (ObjBackend *bend,
                         const char *repo_id,
                         int version,
                         const char *obj_id,
                         void **data,
                         int *len); // 读

    int         (*write) (ObjBackend *bend,
                          const char *repo_id,
                          int version,
                          const char *obj_id,
                          void *data,
                          int len,
                          gboolean need_sync); // 写

    gboolean    (*exists) (ObjBackend *bend,
                           const char *repo_id,
                           int version,
                           const char *obj_id); // 存在

    void        (*delete) (ObjBackend *bend,
                           const char *repo_id,
                           int version,
                           const char *obj_id); // 删除

    int         (*foreach_obj) (ObjBackend *bend,
                                const char *repo_id,
                                int version,
                                SeafObjFunc process,
                                void *user_data); // 遍历

    int         (*copy) (ObjBackend *bend,
                         const char *src_repo_id,
                         int src_version,
                         const char *dst_repo_id,
                         int dst_version,
                         const char *obj_id); // 复制

    int        (*remove_store) (ObjBackend *bend,
                                const char *store_id); // 移除全部

    void *priv;
};

#endif
