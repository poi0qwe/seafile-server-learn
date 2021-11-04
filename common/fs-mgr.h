/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* 文件系统管理 */

#ifndef SEAF_FILE_MGR_H
#define SEAF_FILE_MGR_H

#include <glib.h>

#include "seafile-object.h"

#include "obj-store.h"

#include "cdc/cdc.h"
#include "../common/seafile-crypt.h"

#define CURRENT_DIR_OBJ_VERSION 1
#define CURRENT_SEAFILE_OBJ_VERSION 1

typedef struct _SeafFSManager SeafFSManager; // 文件系统管理器
typedef struct _SeafFSObject SeafFSObject; // 文件系统对象（文件和目录的基类）
typedef struct _Seafile Seafile; // 文件
typedef struct _SeafDir SeafDir; // 目录
typedef struct _SeafDirent SeafDirent; // 目录项

typedef enum { // 元数据类型
    SEAF_METADATA_TYPE_INVALID, // 无效
    SEAF_METADATA_TYPE_FILE, // 文件
    SEAF_METADATA_TYPE_LINK, // 链接
    SEAF_METADATA_TYPE_DIR, // 目录
} SeafMetadataType;

/* Common to seafile and seafdir objects. */
struct _SeafFSObject { // 文件系统对象
    int type;
};

struct _Seafile { // 文件
    SeafFSObject object; // 基类
    int         version; // seafile版本
    char        file_id[41]; // 文件id
    guint64     file_size; // 文件大小
    guint32     n_blocks; // 块数
    char        **blk_sha1s; // 块索引
    int         ref_count; // 引用次数
};

typedef struct SearchResult { // 搜索结果
    char *path; // 路径
    gint64 size; // 大小
    gint64 mtime; // 最后修改时间
    gboolean is_dir;
} SearchResult;

void // 引用一个文件（用于GC）
seafile_ref (Seafile *seafile);

void // 解除引用
seafile_unref (Seafile *seafile);

int // 保存文件
seafile_save (SeafFSManager *fs_mgr,
              const char *repo_id, // 仓库id
              int version,
              Seafile *file); // 文件

#define SEAF_DIR_NAME_LEN 256 // 目录名长度(最大值)

struct _SeafDirent { // 目录项（指向目录或者文件）
    int        version; // seafile版本
    guint32    mode; // 模式
    char       id[41]; // 指向id
    guint32    name_len;
    char       *name; // 名称（目录名或文件名）

    /* attributes for version > 0 */ // 版本1以上
    gint64     mtime; // 修改时间
    char       *modifier;       /* for files only */ // 修改者（仅对文件）
    gint64     size;            /* for files only */ // 大小（仅对文件）
};

struct _SeafDir { // 目录
    SeafFSObject object; // 基类
    int    version; // 版本
    char   dir_id[41]; // 目录id
    GList *entries; // 目录项列表

    /* data in on-disk format. */ // 硬盘存储数据
    void  *ondisk; // 目录字节流
    int    ondisk_size; // 字节流大小
};

SeafDir * // 新建目录对象
seaf_dir_new (const char *id, GList *entries, int version);

void  // 释放目录对象
seaf_dir_free (SeafDir *dir);

SeafDir * // 字节流转目录对象
seaf_dir_from_data (const char *dir_id, uint8_t *data, int len,
                    gboolean is_json);

void * // 目录对象转字节流
seaf_dir_to_data (SeafDir *dir, int *len);

int // 保存目录对象
seaf_dir_save (SeafFSManager *fs_mgr,
               const char *repo_id,
               int version,
               SeafDir *dir);

SeafDirent * // 新建目录项对象
seaf_dirent_new(int version, const char *sha1, int mode, const char *name,
                gint64 mtime, const char *modifier, gint64 size);

void // 释放目录项对象
seaf_dirent_free (SeafDirent *dent);

SeafDirent * // 复制目录项对象
seaf_dirent_dup (SeafDirent *dent);

int // 字节流转对象类型（即元数据）
seaf_metadata_type_from_data (const char *obj_id,
                              uint8_t *data, int len, gboolean is_json);

/* Parse an fs object without knowing its type. */ // 未知类型时，获取基类
SeafFSObject * // 字节流转文件系统对象
seaf_fs_object_from_data (const char *obj_id,
                          uint8_t *data, int len,
                          gboolean is_json);

void // 释放文件系统对象
seaf_fs_object_free (SeafFSObject *obj);

typedef struct { // 块表
    /* TODO: GHashTable may be inefficient when we have large number of IDs. */ // id多的情况下，哈希可能效率比较低
    GHashTable  *block_hash; // 块哈希表
    GPtrArray   *block_ids; // 块id表
    uint32_t     n_blocks; // 块数
    uint32_t     n_valid_blocks; // 有效块数
} BlockList;

BlockList * // 创建新的块表
block_list_new ();

void // 释放块表
block_list_free (BlockList *bl);

void // 块表插入新的块id
block_list_insert (BlockList *bl, const char *block_id);

/* Return a blocklist containing block ids which are in @bl1 but
 * not in @bl2.
 */
BlockList * // 块表1-块表2 （集合差运算）
block_list_difference (BlockList *bl1, BlockList *bl2);

struct _SeafileSession; // 声明seafile会话

typedef struct _SeafFSManagerPriv SeafFSManagerPriv; // 声明文件系统管理器私有域

struct _SeafFSManager { // 文件系统管理器
    struct _SeafileSession *seaf;

    struct SeafObjStore *obj_store; // 对象存储

    SeafFSManagerPriv *priv; // 私有域
};

SeafFSManager * // 新建文件系统管理器
seaf_fs_manager_new (struct _SeafileSession *seaf,
                     const char *seaf_dir);

int // 初始化文件系统管理器
seaf_fs_manager_init (SeafFSManager *mgr);

#ifndef SEAFILE_SERVER

int // 客户端需要检查文件是否冲突，并作出相应处理
seaf_fs_manager_checkout_file (SeafFSManager *mgr, 
                               const char *repo_id,
                               int version,
                               const char *file_id, 
                               const char *file_path,
                               guint32 mode,
                               guint64 mtime,
                               struct SeafileCrypt *crypt,
                               const char *in_repo_path,
                               const char *conflict_head_id,
                               gboolean force_conflict,
                               gboolean *conflicted,
                               const char *email);

#endif  /* not SEAFILE_SERVER */

/**
 * Check in blocks and create seafile/symlink object.
 * Returns sha1 id for the seafile/symlink object in @sha1 parameter.
 */
int // 根据块的路径进行索引（检查并硬链接，然后生成seafile对象）
seaf_fs_manager_index_file_blocks (SeafFSManager *mgr,
                                   const char *repo_id,
                                   int version,
                                   GList *paths,
                                   GList *blockids,
                                   unsigned char sha1[],
                                   gint64 file_size);

int // 根据块的路径进行索引（只检查并硬链接）
seaf_fs_manager_index_raw_blocks (SeafFSManager *mgr,
                                  const char *repo_id,
                                  int version,
                                  GList *paths,
                                  GList *blockids);

int // 重新进行索引（只检查，然后生成seafile对象）（若块不存在，则报错）
seaf_fs_manager_index_existed_file_blocks (SeafFSManager *mgr,
                                           const char *repo_id,
                                           int version,
                                           GList *blockids,
                                           unsigned char sha1[],
                                           gint64 file_size);
int // 对文件分块并索引
seaf_fs_manager_index_blocks (SeafFSManager *mgr,
                              const char *repo_id,
                              int version,
                              const char *file_path,
                              unsigned char sha1[],
                              gint64 *size,
                              SeafileCrypt *crypt,
                              gboolean write_data,
                              gboolean use_cdc,
                              gint64 *indexed);

Seafile * // 获取seafile对象
seaf_fs_manager_get_seafile (SeafFSManager *mgr,
                             const char *repo_id,
                             int version,
                             const char *file_id);

SeafDir * // 获取seadir对象
seaf_fs_manager_get_seafdir (SeafFSManager *mgr,
                             const char *repo_id,
                             int version,
                             const char *dir_id);

/* Make sure entries in the returned dir is sorted in descending order.
 */
SeafDir * // 获取seafdir，且seafdirent(根据名称)降序
seaf_fs_manager_get_seafdir_sorted (SeafFSManager *mgr,
                                    const char *repo_id,
                                    int version,
                                    const char *dir_id);

SeafDir * // 根据相对路径获取seafdir，且seafdirent降序
seaf_fs_manager_get_seafdir_sorted_by_path (SeafFSManager *mgr,
                                            const char *repo_id,
                                            int version,
                                            const char *root_id, // seafdir的id（** root_id表示是commit->root_id **）
                                            const char *path);

int // 记录目录下的所有块
seaf_fs_manager_populate_blocklist (SeafFSManager *mgr,
                                    const char *repo_id,
                                    int version,
                                    const char *root_id,
                                    BlockList *bl);

/*
 * For dir object, set *stop to TRUE to stop traversing the subtree.
 */ // 遍历文件树回调函数；当stop为真则停止
typedef gboolean (*TraverseFSTreeCallback) (SeafFSManager *mgr,
                                            const char *repo_id,
                                            int version,
                                            const char *obj_id,
                                            int type,
                                            void *user_data,
                                            gboolean *stop);

int // 遍历文件树
seaf_fs_manager_traverse_tree (SeafFSManager *mgr,
                               const char *repo_id,
                               int version,
                               const char *root_id,
                               TraverseFSTreeCallback callback,
                               void *user_data,
                               gboolean skip_errors);

typedef gboolean (*TraverseFSPathCallback) (SeafFSManager *mgr,
                                            const char *path,
                                            SeafDirent *dent,
                                            void *user_data,
                                            gboolean *stop); // 遍历路径回调函数

int
seaf_fs_manager_traverse_path (SeafFSManager *mgr, // 根据相对路径遍历文件树
                               const char *repo_id,
                               int version,
                               const char *root_id,
                               const char *dir_path,
                               TraverseFSPathCallback callback,
                               void *user_data);

gboolean // 判断对象是否存在
seaf_fs_manager_object_exists (SeafFSManager *mgr,
                               const char *repo_id,
                               int version,
                               const char *id);

void // 删除对象
seaf_fs_manager_delete_object (SeafFSManager *mgr,
                               const char *repo_id,
                               int version,
                               const char *id);

gint64 // 获取文件大小
seaf_fs_manager_get_file_size (SeafFSManager *mgr,
                               const char *repo_id,
                               int version,
                               const char *file_id);

gint64 // 获取目录大小
seaf_fs_manager_get_fs_size (SeafFSManager *mgr,
                             const char *repo_id,
                             int version,
                             const char *root_id);

#ifndef SEAFILE_SERVER
int // Seafile文件系统写块文件方法（替换默认写块文件方法）
seafile_write_chunk (const char *repo_id,
                     int version,
                     CDCDescriptor *chunk,
                     SeafileCrypt *crypt,
                     uint8_t *checksum,
                     gboolean write_data);
int
seafile_check_write_chunk (CDCDescriptor *chunk,
                           uint8_t *sha1,
                           gboolean write_data); // 只声明，无实现
#endif /* SEAFILE_SERVER */

uint32_t // 计算块的大小（未被用到）
calculate_chunk_size (uint64_t total_size);

int // 统计目录中的文件数目
seaf_fs_manager_count_fs_files (SeafFSManager *mgr,
                                const char *repo_id,
                                int version,
                                const char *root_id);

SeafDir * // 根据相对路径获取seafdir
seaf_fs_manager_get_seafdir_by_path (SeafFSManager *mgr,
                                    const char *repo_id,
                                    int version,
                                    const char *root_id,
                                    const char *path,
                                    GError **error);
char * // 根据相对路径获取seafile的id
seaf_fs_manager_get_seafile_id_by_path (SeafFSManager *mgr,
                                        const char *repo_id,
                                        int version,
                                        const char *root_id,
                                        const char *path,
                                        GError **error);

char * // 根据相对路径获取对象id
seaf_fs_manager_path_to_obj_id (SeafFSManager *mgr,
                                const char *repo_id,
                                int version,
                                const char *root_id,
                                const char *path,
                                guint32 *mode,
                                GError **error);

char * // 根据相对路径获取seafdir的id
seaf_fs_manager_get_seafdir_id_by_path (SeafFSManager *mgr,
                                        const char *repo_id,
                                        int version,
                                        const char *root_id,
                                        const char *path,
                                        GError **error);

SeafDirent * // 根据相对路径获取seafdirent
seaf_fs_manager_get_dirent_by_path (SeafFSManager *mgr,
                                    const char *repo_id,
                                    int version,
                                    const char *root_id,
                                    const char *path,
                                    GError **error);

/* Check object integrity. */

gboolean // 检查seafdir
seaf_fs_manager_verify_seafdir (SeafFSManager *mgr,
                                const char *repo_id,
                                int version,
                                const char *dir_id,
                                gboolean verify_id,
                                gboolean *io_error);

gboolean // 检查seafile
seaf_fs_manager_verify_seafile (SeafFSManager *mgr,
                                const char *repo_id,
                                int version,
                                const char *file_id,
                                gboolean verify_id,
                                gboolean *io_error);

gboolean // 检查对象
seaf_fs_manager_verify_object (SeafFSManager *mgr,
                               const char *repo_id,
                               int version,
                               const char *obj_id,
                               gboolean verify_id,
                               gboolean *io_error);

int // 根据仓库的版本获取seafdir的版本
dir_version_from_repo_version (int repo_version);

int // 根据仓库的版本获取seafile的版本
seafile_version_from_repo_version (int repo_version);

struct _CDCFileDescriptor;
void // 获取seafile对象SHA1
seaf_fs_manager_calculate_seafile_id_json (int repo_version,
                                           struct _CDCFileDescriptor *cdc,
                                           guint8 *file_id_sha1);

int // 移除存储
seaf_fs_manager_remove_store (SeafFSManager *mgr,
                              const char *store_id);

GObject * // 根据相对路径获取文件数量
seaf_fs_manager_get_file_count_info_by_path (SeafFSManager *mgr,
                                             const char *repo_id,
                                             int version,
                                             const char *root_id,
                                             const char *path,
                                             GError **error);

GList * // 搜索文件（按文件名），返回结果列表
seaf_fs_manager_search_files (SeafFSManager *mgr,
                              const char *repo_id,
                              const char *str);

#endif
