/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef _GNU_SOURECE
#define _GNU_SOURCE
char *strcasestr (const char *haystack, const char *needle);
#undef _GNU_SOURCE
#endif
#include "common.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#ifndef WIN32
    #include <arpa/inet.h>
#endif

#include <openssl/sha.h>
#include <searpc-utils.h>

#include "seafile-session.h"
#include "seafile-error.h"
#include "fs-mgr.h"
#include "block-mgr.h"
#include "utils.h"
#include "seaf-utils.h"
#define DEBUG_FLAG SEAFILE_DEBUG_OTHER
#include "log.h"
#include "../common/seafile-crypt.h"

#ifndef SEAFILE_SERVER
#include "../daemon/vc-utils.h"
#include "vc-common.h"
#endif  /* SEAFILE_SERVER */

#include "db.h"

#define SEAF_TMP_EXT "~"

struct _SeafFSManagerPriv { // 私有域
    /* GHashTable      *seafile_cache; */
    GHashTable      *bl_cache; // 块表缓存
};

typedef struct SeafileOndisk { // Seafile字节流内容（版本0下的seafile对象存储）
    guint32          type;
    guint64          file_size;
    unsigned char    block_ids[0];
} __attribute__((gcc_struct, __packed__)) SeafileOndisk;

typedef struct DirentOndisk { // Seafdirent字节流内容
    guint32 mode;
    char    id[40];
    guint32 name_len;
    char    name[0];
} __attribute__((gcc_struct, __packed__)) DirentOndisk;

typedef struct SeafdirOndisk { // Seadir字节流内容
    guint32 type;
    char    dirents[0];
} __attribute__((gcc_struct, __packed__)) SeafdirOndisk;

#ifndef SEAFILE_SERVER
uint32_t // 计算分块大小
calculate_chunk_size (uint64_t total_size);
static int // 写seafile
write_seafile (SeafFSManager *fs_mgr,
               const char *repo_id, int version, // 仓库id及版本
               CDCFileDescriptor *cdc, // 文件分块信息
               unsigned char *obj_sha1); // seafile对象的SHA1
#endif  /* SEAFILE_SERVER */

SeafFSManager * // 创建新的文件系统管理器
seaf_fs_manager_new (SeafileSession *seaf,
                     const char *seaf_dir)
{
    SeafFSManager *mgr = g_new0 (SeafFSManager, 1);

    mgr->seaf = seaf; // 会话

    mgr->obj_store = seaf_obj_store_new (seaf, "fs"); // 对象存储
    if (!mgr->obj_store) { // 出错
        g_free (mgr);
        return NULL;
    }

    mgr->priv = g_new0(SeafFSManagerPriv, 1); // 私有域

    return mgr;
}

int // 初始化管理器
seaf_fs_manager_init (SeafFSManager *mgr)
{
    if (seaf_obj_store_init (mgr->obj_store) < 0) { // 初始化对象存储
        seaf_warning ("[fs mgr] Failed to init fs object store.\n");
        return -1;
    }

    return 0;
}

#ifndef SEAFILE_SERVER
static int // 查看块（打开块，解密块，然后写入到临时文件wfd中）
checkout_block (const char *repo_id,
                int version,
                const char *block_id, // 块id
                int wfd, // 临时文件句柄
                SeafileCrypt *crypt)
{
    SeafBlockManager *block_mgr = seaf->block_mgr;
    BlockHandle *handle;
    BlockMetadata *bmd;
    char *dec_out = NULL;
    int dec_out_len = -1;
    char *blk_content = NULL;

    handle = seaf_block_manager_open_block (block_mgr,
                                            repo_id, version,
                                            block_id, BLOCK_READ); // 打开块，只读
    if (!handle) {
        seaf_warning ("Failed to open block %s\n", block_id);
        return -1;
    }

    /* first stat the block to get its size */
    bmd = seaf_block_manager_stat_block_by_handle (block_mgr, handle); // 获取统计信息
    if (!bmd) {
        seaf_warning ("can't stat block %s.\n", block_id);
        goto checkout_blk_error;
    }

    /* empty file, skip it */
    if (bmd->size == 0) { // 跳过空
        seaf_block_manager_close_block (block_mgr, handle);
        seaf_block_manager_block_handle_free (block_mgr, handle);
        return 0;
    }

    blk_content = (char *)malloc (bmd->size * sizeof(char));

    /* read the block to prepare decryption */
    if (seaf_block_manager_read_block (block_mgr, handle,
                                       blk_content, bmd->size) != bmd->size) { // 读块
        seaf_warning ("Error when reading from block %s.\n", block_id);
        goto checkout_blk_error;
    }

    if (crypt != NULL) { // 存在加密

        /* An encrypted block size must be a multiple of
           ENCRYPT_BLK_SIZE
        */
        if (bmd->size % ENCRYPT_BLK_SIZE != 0) {
            seaf_warning ("Error: An invalid encrypted block, %s \n", block_id);
            goto checkout_blk_error;
        }

        /* decrypt the block */ // 对块内容进行解密
        int ret = seafile_decrypt (&dec_out,
                                   &dec_out_len,
                                   blk_content,
                                   bmd->size,
                                   crypt);

        if (ret != 0) { // 无法解密
            seaf_warning ("Decryt block %s failed. \n", block_id);
            goto checkout_blk_error;
        }

        /* write the decrypted content */
        ret = writen (wfd, dec_out, dec_out_len); // 写入解密内容


        if (ret !=  dec_out_len) {
            seaf_warning ("Failed to write the decryted block %s.\n",
                       block_id);
            goto checkout_blk_error;
        }

        g_free (blk_content);
        g_free (dec_out);

    } else {
        /* not an encrypted block */
        if (writen(wfd, blk_content, bmd->size) != bmd->size) { // 直接写入
            seaf_warning ("Failed to write the decryted block %s.\n",
                       block_id);
            goto checkout_blk_error;
        }
        g_free (blk_content);
    }

    g_free (bmd);
    seaf_block_manager_close_block (block_mgr, handle); // 关闭块
    seaf_block_manager_block_handle_free (block_mgr, handle); // 关闭句柄
    return 0;

checkout_blk_error:

    if (blk_content)
        free (blk_content);
    if (dec_out)
        g_free (dec_out);
    if (bmd)
        g_free (bmd);

    seaf_block_manager_close_block (block_mgr, handle);
    seaf_block_manager_block_handle_free (block_mgr, handle);
    return -1;
}

int // 查看seafile内容（解密文件至file_path）
seaf_fs_manager_checkout_file (SeafFSManager *mgr,
                               const char *repo_id,
                               int version,
                               const char *file_id,
                               const char *file_path,
                               guint32 mode,
                               guint64 mtime, // 最后修改时间
                               SeafileCrypt *crypt, // 加密上下文
                               const char *in_repo_path, // 仓库内路径
                               const char *conflict_head_id, // 冲突头
                               gboolean force_conflict, // 强制冲突
                               gboolean *conflicted, //是否冲突
                               const char *email) // 提供邮箱（用于生成冲突路径）
{
    Seafile *seafile; // seafile文件对象
    char *blk_id;
    int wfd;
    int i;
    char *tmp_path;
    char *conflict_path; // 冲突路径

    *conflicted = FALSE;

    seafile = seaf_fs_manager_get_seafile (mgr, repo_id, version, file_id); // 获取seafile对象
    if (!seafile) {
        seaf_warning ("File %s does not exist.\n", file_id);
        return -1;
    }

    tmp_path = g_strconcat (file_path, SEAF_TMP_EXT, NULL); // 合成临时文件路径

    mode_t rmode = mode & 0100 ? 0777 : 0666;
    wfd = seaf_util_create (tmp_path, O_WRONLY | O_TRUNC | O_CREAT | O_BINARY,
                            rmode & ~S_IFMT); // 创建临时文件，只写
    if (wfd < 0) {
        seaf_warning ("Failed to open file %s for checkout: %s.\n",
                   tmp_path, strerror(errno));
        goto bad;
    }

    for (i = 0; i < seafile->n_blocks; ++i) { // 遍历所有块
        blk_id = seafile->blk_sha1s[i];
        if (checkout_block (repo_id, version, blk_id, wfd, crypt) < 0) // 检查每个块(解密这些块至临时文件)
            goto bad;
    }

    close (wfd); // 关临时文件
    wfd = -1;

    if (force_conflict || seaf_util_rename (tmp_path, file_path) < 0) { // 如果强制冲突，或重命名失败（说明file_path已存在）
        *conflicted = TRUE;

        /* XXX
         * In new syncing protocol and http sync, files are checked out before
         * the repo is created. So we can't get user email from repo at this point.
         * So a email parameter is needed.
         * For old syncing protocol, repo always exists when files are checked out.
         * This is a quick and dirty hack. A cleaner solution should modifiy the
         * code of old syncing protocol to pass in email too. But I don't want to
         * spend more time on the nearly obsoleted code.
         * 
         * 在新的同步协议和HTTP同步中，文件在仓库创建前被检查。所以这个阶段我们还不能获取用户的邮箱，因此需要提供邮箱。
         * 旧的协议中，检查文件时仓库总存在。这是一个快但脏的方法。一个更简洁的方法也需要邮箱，但作者不想花太多时间在已弃用的代码上。
         */
        const char *suffix = NULL;
        if (email) {
            suffix = email; // 后缀为邮箱
        } else {
            SeafRepo *repo = seaf_repo_manager_get_repo (seaf->repo_mgr, repo_id);
            if (!repo)
                goto bad;
            suffix = email; // 否则根据仓库获取邮箱
        }

        conflict_path = gen_conflict_path (file_path, suffix, (gint64)time(NULL)); // 生成一个冲突路径

        seaf_warning ("Cannot update %s, creating conflict file %s.\n",
                      file_path, conflict_path);

        /* First try to rename the local version to a conflict file,
         * this will preserve the version from the server.
         * If this fails, fall back to checking out the server version
         * to the conflict file.
         */
        if (seaf_util_rename (file_path, conflict_path) == 0) { // 暂存至冲突路径
            if (seaf_util_rename (tmp_path, file_path) < 0) {
                g_free (conflict_path);
                goto bad;
            }
        } else { // 继续冲突
            g_free (conflict_path);
            conflict_path = gen_conflict_path_wrapper (repo_id, version,
                                                       conflict_head_id, in_repo_path,
                                                       file_path);
            if (!conflict_path)
                goto bad;

            if (seaf_util_rename (tmp_path, conflict_path) < 0) {
                g_free (conflict_path);
                goto bad;
            }
        }

        g_free (conflict_path);
    }

    if (mtime > 0) { // 如果有最后修改时间
        /* 
         * Set the checked out file mtime to what it has to be.
         */
        if (seaf_set_file_time (file_path, mtime) < 0) { // 设置最后修改时间
            seaf_warning ("Failed to set mtime for %s.\n", file_path);
        }
    }

    g_free (tmp_path);
    seafile_unref (seafile);
    return 0;

bad:
    if (wfd >= 0)
        close (wfd);
    /* Remove the tmp file if it still exists, in case that rename fails. */
    seaf_util_unlink (tmp_path); // 移除临时文件
    g_free (tmp_path);
    seafile_unref (seafile);
    return -1;
}

#endif /* SEAFILE_SERVER */

static void * // 获取seafile字节流；版本0，结构体字节流
create_seafile_v0 (CDCFileDescriptor *cdc, int *ondisk_size, char *seafile_id)
{
    SeafileOndisk *ondisk;

    rawdata_to_hex (cdc->file_sum, seafile_id, 20); // 直接使用文件内容sha1作为文件id

    *ondisk_size = sizeof(SeafileOndisk) + cdc->block_nr * 20; // 估算大小
    ondisk = (SeafileOndisk *)g_new0 (char, *ondisk_size);

    ondisk->type = htonl(SEAF_METADATA_TYPE_FILE); // 转字节序
    ondisk->file_size = hton64 (cdc->file_size);
    memcpy (ondisk->block_ids, cdc->blk_sha1s, cdc->block_nr * 20); // 复制块索引

    return ondisk; // 返回字节流
}

static void * // 获取seafile字节流；json字节流
create_seafile_json (int repo_version,
                     CDCFileDescriptor *cdc,
                     int *ondisk_size,
                     char *seafile_id)
{
    json_t *object, *block_id_array;

    object = json_object ();

    json_object_set_int_member (object, "type", SEAF_METADATA_TYPE_FILE);
    json_object_set_int_member (object, "version",
                                seafile_version_from_repo_version(repo_version));

    json_object_set_int_member (object, "size", cdc->file_size);

    block_id_array = json_array ();
    int i;
    uint8_t *ptr = cdc->blk_sha1s;
    char block_id[41];
    for (i = 0; i < cdc->block_nr; ++i) {
        rawdata_to_hex (ptr, block_id, 20);
        json_array_append_new (block_id_array, json_string(block_id));
        ptr += 20;
    }
    json_object_set_new (object, "block_ids", block_id_array);

    char *data = json_dumps (object, JSON_SORT_KEYS);
    *ondisk_size = strlen(data);

    /* The seafile object id is sha1 hash of the json object. */
    unsigned char sha1[20];
    calculate_sha1 (sha1, data, *ondisk_size); // 字节流数据 -> sha1
    rawdata_to_hex (sha1, seafile_id, 20); // 转HEX串

    json_decref (object);
    return data;
}

void // 获取seafile对象SHA1；json字节流
seaf_fs_manager_calculate_seafile_id_json (int repo_version,
                                           CDCFileDescriptor *cdc,
                                           guint8 *file_id_sha1)
{
    json_t *object, *block_id_array;

    object = json_object ();

    json_object_set_int_member (object, "type", SEAF_METADATA_TYPE_FILE);
    json_object_set_int_member (object, "version",
                                seafile_version_from_repo_version(repo_version));

    json_object_set_int_member (object, "size", cdc->file_size);

    block_id_array = json_array ();
    int i;
    uint8_t *ptr = cdc->blk_sha1s;
    char block_id[41];
    for (i = 0; i < cdc->block_nr; ++i) {
        rawdata_to_hex (ptr, block_id, 20);
        json_array_append_new (block_id_array, json_string(block_id));
        ptr += 20;
    }
    json_object_set_new (object, "block_ids", block_id_array);

    char *data = json_dumps (object, JSON_SORT_KEYS);
    int ondisk_size = strlen(data);

    /* The seafile object id is sha1 hash of the json object. */
    calculate_sha1 (file_id_sha1, data, ondisk_size);

    json_decref (object);
    free (data);
}

static int // 写seafile对象
write_seafile (SeafFSManager *fs_mgr, // 管理器
               const char *repo_id, // 仓库id
               int version, // 版本
               CDCFileDescriptor *cdc, // 文件分块信息
               unsigned char *obj_sha1) // 对象SHA1（对象SHA1 <-> 对象id）
{
    int ret = 0;
    char seafile_id[41];
    void *ondisk;
    int ondisk_size;

    if (version > 0) { // 版本大于0
        ondisk = create_seafile_json (version, cdc, &ondisk_size, seafile_id); // 获取json字节流

        guint8 *compressed;
        int outlen;

        if (seaf_obj_store_obj_exists (fs_mgr->obj_store, repo_id, version, seafile_id)) { // 判断对象是否已存在
            ret = 0;
            free (ondisk);
            goto out;
        }

        if (seaf_compress (ondisk, ondisk_size, &compressed, &outlen) < 0) { // 将字节流压缩
            seaf_warning ("Failed to compress seafile obj %s:%s.\n",
                          repo_id, seafile_id);
            ret = -1;
            free (ondisk);
            goto out;
        }

        if (seaf_obj_store_write_obj (fs_mgr->obj_store, repo_id, version, seafile_id,
                                      compressed, outlen, FALSE) < 0) // 写对象
            ret = -1;
        g_free (compressed);
        free (ondisk);
    } else { // 版本0
        ondisk = create_seafile_v0 (cdc, &ondisk_size, seafile_id); // 字节流
        if (seaf_obj_store_obj_exists (fs_mgr->obj_store, repo_id, version, seafile_id)) { // 判断对象是否已存在
            ret = 0;
            g_free (ondisk);
            goto out;
        }

        if (seaf_obj_store_write_obj (fs_mgr->obj_store, repo_id, version, seafile_id,
                                      ondisk, ondisk_size, FALSE) < 0) // 写
            ret = -1;
        g_free (ondisk);
    }

out:
    if (ret == 0)
        hex_to_rawdata (seafile_id, obj_sha1, 20);

    return ret;
}

uint32_t // 计算分块大小（未被使用）
calculate_chunk_size (uint64_t total_size)
{
    const uint64_t GiB = 1073741824;
    const uint64_t MiB = 1048576;

    if (total_size >= (8 * GiB)) return 8 * MiB;
    if (total_size >= (4 * GiB)) return 4 * MiB;
    if (total_size >= (2 * GiB)) return 2 * MiB;

    return 1 * MiB;
}

static int // 实际的写块（利用块管理器进行硬链接）
do_write_chunk (const char *repo_id, int version, // 仓库、版本
                uint8_t *checksum, const char *buf, int len) // checksum(HEX形式作为块id)，块内容及长度
{
    SeafBlockManager *blk_mgr = seaf->block_mgr;
    char chksum_str[41];
    BlockHandle *handle;
    int n;

    rawdata_to_hex (checksum, chksum_str, 20); // 作为块id

    /* Don't write if the block already exists. */
    if (seaf_block_manager_block_exists (seaf->block_mgr,
                                         repo_id, version,
                                         chksum_str)) // 块已存在
        return 0;

    handle = seaf_block_manager_open_block (blk_mgr,
                                            repo_id, version,
                                            chksum_str, BLOCK_WRITE); // 写模式打开块
    if (!handle) {
        seaf_warning ("Failed to open block %s.\n", chksum_str);
        return -1;
    }

    n = seaf_block_manager_write_block (blk_mgr, handle, buf, len); // 写
    if (n < 0) {
        seaf_warning ("Failed to write chunk %s.\n", chksum_str);
        seaf_block_manager_close_block (blk_mgr, handle);
        seaf_block_manager_block_handle_free (blk_mgr, handle);
        return -1;
    }

    if (seaf_block_manager_close_block (blk_mgr, handle) < 0) { // 关
        seaf_warning ("failed to close block %s.\n", chksum_str);
        seaf_block_manager_block_handle_free (blk_mgr, handle);
        return -1;
    }

    if (seaf_block_manager_commit_block (blk_mgr, handle) < 0) { // 提交
        seaf_warning ("failed to commit chunk %s.\n", chksum_str);
        seaf_block_manager_block_handle_free (blk_mgr, handle);
        return -1;
    }

    seaf_block_manager_block_handle_free (blk_mgr, handle);
    return 0;
}

/* write the chunk and store its checksum */
int // Seafile文件系统写块文件方法（替换默认写块文件方法）
seafile_write_chunk (const char *repo_id,
                     int version,
                     CDCDescriptor *chunk,
                     SeafileCrypt *crypt,
                     uint8_t *checksum,
                     gboolean write_data)
{
    SHA_CTX ctx;
    int ret = 0;

    /* Encrypt before write to disk if needed, and we don't encrypt
     * empty files. */
    if (crypt != NULL && chunk->len) { // 进行加密，要求非空文件
        char *encrypted_buf = NULL;         /* encrypted output */
        int enc_len = -1;                /* encrypted length */

        ret = seafile_encrypt (&encrypted_buf, /* output */
                               &enc_len,      /* output len */
                               chunk->block_buf, /* input */
                               chunk->len,       /* input len */
                               crypt); // 进行加密
        if (ret != 0) {
            seaf_warning ("Error: failed to encrypt block\n");
            return -1;
        }

        SHA1_Init (&ctx);
        SHA1_Update (&ctx, encrypted_buf, enc_len);
        SHA1_Final (checksum, &ctx); // 计算出checksum

        if (write_data)
            ret = do_write_chunk (repo_id, version, checksum, encrypted_buf, enc_len);
        g_free (encrypted_buf);
    } else { // 不进行加密
        /* not a encrypted repo, go ahead */
        SHA1_Init (&ctx);
        SHA1_Update (&ctx, chunk->block_buf, chunk->len);
        SHA1_Final (checksum, &ctx);

        if (write_data)
            ret = do_write_chunk (repo_id, version, checksum, chunk->block_buf, chunk->len);
    }

    return ret;
}

static void // 对空文件创建文件分块信息
create_cdc_for_empty_file (CDCFileDescriptor *cdc)
{
    memset (cdc, 0, sizeof(CDCFileDescriptor));
}

#if defined SEAFILE_SERVER && defined FULL_FEATURE // 全部功能（多线程分块）

#define FIXED_BLOCK_SIZE (1<<20)

typedef struct ChunkingData {
    const char *repo_id;
    int version;
    const char *file_path;
    SeafileCrypt *crypt;
    guint8 *blk_sha1s;
    GAsyncQueue *finished_tasks;
} ChunkingData;

static void
chunking_worker (gpointer vdata, gpointer user_data)
{
    ChunkingData *data = user_data;
    CDCDescriptor *chunk = vdata;
    int fd = -1;
    ssize_t n;
    int idx;

    chunk->block_buf = g_new0 (char, chunk->len);
    if (!chunk->block_buf) {
        seaf_warning ("Failed to allow chunk buffer\n");
        goto out;
    }

    fd = seaf_util_open (data->file_path, O_RDONLY | O_BINARY);
    if (fd < 0) {
        seaf_warning ("Failed to open %s: %s\n", data->file_path, strerror(errno));
        chunk->result = -1;
        goto out;
    }

    if (seaf_util_lseek (fd, chunk->offset, SEEK_SET) == (gint64)-1) {
        seaf_warning ("Failed to lseek %s: %s\n", data->file_path, strerror(errno));
        chunk->result = -1;
        goto out;
    }

    n = readn (fd, chunk->block_buf, chunk->len);
    if (n < 0) {
        seaf_warning ("Failed to read chunk from %s: %s\n",
                      data->file_path, strerror(errno));
        chunk->result = -1;
        goto out;
    }

    chunk->result = seafile_write_chunk (data->repo_id, data->version,
                                         chunk, data->crypt,
                                         chunk->checksum, 1);
    if (chunk->result < 0)
        goto out;

    idx = chunk->offset / seaf->http_server->fixed_block_size;
    memcpy (data->blk_sha1s + idx * CHECKSUM_LENGTH, chunk->checksum, CHECKSUM_LENGTH);

out:
    g_free (chunk->block_buf);
    close (fd);
    g_async_queue_push (data->finished_tasks, chunk);
}

static int
split_file_to_block (const char *repo_id,
                     int version,
                     const char *file_path,
                     gint64 file_size,
                     SeafileCrypt *crypt,
                     CDCFileDescriptor *cdc,
                     gboolean write_data,
                     gint64 *indexed)
{
    int n_blocks;
    uint8_t *block_sha1s = NULL;
    GThreadPool *tpool = NULL;
    GAsyncQueue *finished_tasks = NULL;
    GList *pending_tasks = NULL;
    int n_pending = 0;
    CDCDescriptor *chunk;
    int ret = 0;

    n_blocks = (file_size + seaf->http_server->fixed_block_size - 1) / seaf->http_server->fixed_block_size;
    block_sha1s = g_new0 (uint8_t, n_blocks * CHECKSUM_LENGTH);
    if (!block_sha1s) {
        seaf_warning ("Failed to allocate block_sha1s.\n");
        ret = -1;
        goto out;
    }

    finished_tasks = g_async_queue_new ();

    ChunkingData data;
    memset (&data, 0, sizeof(data));
    data.repo_id = repo_id;
    data.version = version;
    data.file_path = file_path;
    data.crypt = crypt;
    data.blk_sha1s = block_sha1s;
    data.finished_tasks = finished_tasks;

    tpool = g_thread_pool_new (chunking_worker, &data,
                               seaf->http_server->max_indexing_threads, FALSE, NULL);
    if (!tpool) {
        seaf_warning ("Failed to allocate thread pool\n");
        ret = -1;
        goto out;
    }

    guint64 offset = 0;
    guint64 len;
    guint64 left = (guint64)file_size;
    while (left > 0) {
        len = ((left >= seaf->http_server->fixed_block_size) ? seaf->http_server->fixed_block_size : left);

        chunk = g_new0 (CDCDescriptor, 1);
        chunk->offset = offset;
        chunk->len = (guint32)len;

        g_thread_pool_push (tpool, chunk, NULL);
        pending_tasks = g_list_prepend (pending_tasks, chunk);
        n_pending++;

        left -= len;
        offset += len;
    }

    while ((chunk = g_async_queue_pop (finished_tasks)) != NULL) {
        if (chunk->result < 0) {
            ret = -1;
            goto out;
        }
        if (indexed)
            *indexed += seaf->http_server->fixed_block_size;

        if ((--n_pending) <= 0) {
            if (indexed)
                *indexed = (guint64)file_size;
            break;
        }
    }

    cdc->block_nr = n_blocks;
    cdc->blk_sha1s = block_sha1s;

out:
    if (tpool)
        g_thread_pool_free (tpool, TRUE, TRUE);
    if (finished_tasks)
        g_async_queue_unref (finished_tasks);
    g_list_free_full (pending_tasks, g_free);
    if (ret < 0)
        g_free (block_sha1s);

    return ret;
}

#endif  /* SEAFILE_SERVER */

#define CDC_AVERAGE_BLOCK_SIZE (1 << 23) /* 8MB */
#define CDC_MIN_BLOCK_SIZE (6 * (1 << 20)) /* 6MB */
#define CDC_MAX_BLOCK_SIZE (10 * (1 << 20)) /* 10MB */

int // 对文件分块并索引
seaf_fs_manager_index_blocks (SeafFSManager *mgr,
                              const char *repo_id,
                              int version,
                              const char *file_path, // 文件的路径
                              unsigned char sha1[], // 文件总SHA1
                              gint64 *size,
                              SeafileCrypt *crypt,
                              gboolean write_data,
                              gboolean use_cdc,
                              gint64 *indexed)
{
    SeafStat sb;
    CDCFileDescriptor cdc;

    if (seaf_stat (file_path, &sb) < 0) {
        seaf_warning ("Bad file %s: %s.\n", file_path, strerror(errno));
        return -1;
    }

    g_return_val_if_fail (S_ISREG(sb.st_mode), -1);

    if (sb.st_size == 0) {
        /* handle empty file. */
        memset (sha1, 0, 20);
        create_cdc_for_empty_file (&cdc);
    } else {
        memset (&cdc, 0, sizeof(cdc));
#if defined SEAFILE_SERVER && defined FULL_FEATURE
        if (use_cdc || version == 0) { // CDC
            cdc.block_sz = CDC_AVERAGE_BLOCK_SIZE;
            cdc.block_min_sz = CDC_MIN_BLOCK_SIZE;
            cdc.block_max_sz = CDC_MAX_BLOCK_SIZE;
            cdc.write_block = seafile_write_chunk;
            memcpy (cdc.repo_id, repo_id, 36);
            cdc.version = version;
            if (filename_chunk_cdc (file_path, &cdc, crypt, write_data, indexed) < 0) {
                seaf_warning ("Failed to chunk file with CDC.\n");
                return -1;
            }
        } else { // 定长分块
            memcpy (cdc.repo_id, repo_id, 36);
            cdc.version = version;
            cdc.file_size = sb.st_size;
            if (split_file_to_block (repo_id, version, file_path, sb.st_size,
                                     crypt, &cdc, write_data, indexed) < 0) {
                return -1;
            }
        }
#else
        cdc.block_sz = CDC_AVERAGE_BLOCK_SIZE;
        cdc.block_min_sz = CDC_MIN_BLOCK_SIZE;
        cdc.block_max_sz = CDC_MAX_BLOCK_SIZE;
        cdc.write_block = seafile_write_chunk;
        memcpy (cdc.repo_id, repo_id, 36);
        cdc.version = version;
        if (filename_chunk_cdc (file_path, &cdc, crypt, write_data, indexed) < 0) { // 根据路径进行分块
            seaf_warning ("Failed to chunk file with CDC.\n");
            return -1;
        }
#endif

        if (write_data && write_seafile (mgr, repo_id, version, &cdc, sha1) < 0) { // 若写数据，顺便写seafile对象
            g_free (cdc.blk_sha1s);
            seaf_warning ("Failed to write seafile for %s.\n", file_path);
            return -1;
        }
    }

    *size = (gint64)sb.st_size;

    if (cdc.blk_sha1s)
        free (cdc.blk_sha1s);

    return 0;
}

static int // 检查并对块硬链接
check_and_write_block (const char *repo_id, int version,
                       const char *path, unsigned char *sha1, const char *block_id)
{
    char *content;
    gsize len;
    GError *error = NULL;
    int ret = 0;

    if (!g_file_get_contents (path, &content, &len, &error)) { // 获取路径中的内容
        if (error) {
            seaf_warning ("Failed to read %s: %s.\n", path, error->message);
            g_clear_error (&error);
            return -1;
        }
    }

    SHA_CTX block_ctx;
    unsigned char checksum[20];

    SHA1_Init (&block_ctx);
    SHA1_Update (&block_ctx, content, len);
    SHA1_Final (checksum, &block_ctx); // 计算块文件SHA1

    if (memcmp (checksum, sha1, 20) != 0) { // 不匹配
        seaf_warning ("Block id %s:%s doesn't match content.\n", repo_id, block_id);
        ret = -1;
        goto out;
    }

    if (do_write_chunk (repo_id, version, sha1, content, len) < 0) { // 开始硬连接
        ret = -1;
        goto out;
    }

out:
    g_free (content);
    return ret;
}

static int // 给定一个块表，检查并硬链接；同时记录这些块的SHA1表和总SHA1至文件分块信息
check_and_write_file_blocks (CDCFileDescriptor *cdc, GList *paths, GList *blockids)
{
    GList *ptr, *q;
    SHA_CTX file_ctx;
    int ret = 0;

    SHA1_Init (&file_ctx);
    for (ptr = paths, q = blockids; ptr; ptr = ptr->next, q = q->next) { // 遍历各块的路径和id
        char *path = ptr->data;
        char *blk_id = q->data;
        unsigned char sha1[20];

        hex_to_rawdata (blk_id, sha1, 20); // SHA1和块id的关系：前者是8bit（字节串），后者是4bit（HEX串）
        ret = check_and_write_block (cdc->repo_id, cdc->version, path, sha1, blk_id); // 进行硬连接
        if (ret < 0)
            goto out;

        memcpy (cdc->blk_sha1s + cdc->block_nr * CHECKSUM_LENGTH,
                sha1, CHECKSUM_LENGTH); // 将算得的SHA1填充到文件分块信息的blk_sha1s表中
        cdc->block_nr++;

        SHA1_Update (&file_ctx, sha1, 20); // 更新文件总SHA1
    }

    SHA1_Final (cdc->file_sum, &file_ctx); // 总SHA1

out:
    return ret;
}

static int // 校验已存在的块；同时记录这些块的SHA1表和总SHA1至文件分块信息
check_existed_file_blocks (CDCFileDescriptor *cdc, GList *blockids)
{
    GList *q;
    SHA_CTX file_ctx;
    int ret = 0;

    SHA1_Init (&file_ctx);
    for (q = blockids; q; q = q->next) {
        char *blk_id = q->data;
        unsigned char sha1[20];

        if (!seaf_block_manager_block_exists ( // 块已存在
                seaf->block_mgr, cdc->repo_id, cdc->version, blk_id)) {
            ret = -1;
            goto out;
        }

        hex_to_rawdata (blk_id, sha1, 20);
        memcpy (cdc->blk_sha1s + cdc->block_nr * CHECKSUM_LENGTH,
                sha1, CHECKSUM_LENGTH); // 更新文件分块信息中的SHA1
        cdc->block_nr++;

        SHA1_Update (&file_ctx, sha1, 20); // 更新文件总SHA1
    }

    SHA1_Final (cdc->file_sum, &file_ctx);

out:
    return ret;
}

static int // 初始化文件分块信息
init_file_cdc (CDCFileDescriptor *cdc,
               const char *repo_id, int version,
               int block_nr, gint64 file_size)
{
    memset (cdc, 0, sizeof(CDCFileDescriptor));

    cdc->file_size = file_size;

    cdc->blk_sha1s =  (uint8_t *)calloc (sizeof(uint8_t), block_nr * CHECKSUM_LENGTH);
    if (!cdc->blk_sha1s) {
        seaf_warning ("Failed to alloc block sha1 array.\n");
        return -1;
    }

    memcpy (cdc->repo_id, repo_id, 36);
    cdc->version = version;

    return 0;
}

int // 根据块的路径进行索引（检查并硬链接，然后生成seafile对象）
seaf_fs_manager_index_file_blocks (SeafFSManager *mgr,
                                   const char *repo_id,
                                   int version,
                                   GList *paths,
                                   GList *blockids,
                                   unsigned char sha1[],
                                   gint64 file_size)
{
    int ret = 0;
    CDCFileDescriptor cdc;

    if (!paths) { // 空文件
        /* handle empty file. */
        memset (sha1, 0, 20);
        create_cdc_for_empty_file (&cdc);
    } else {
        int block_nr = g_list_length (paths);

        if (init_file_cdc (&cdc, repo_id, version, block_nr, file_size) < 0) { // 初始化分块信息
            ret = -1;
            goto out;
        }

        if (check_and_write_file_blocks (&cdc, paths, blockids) < 0) { // 硬链接，并更新SHA1表及总SHA1
            seaf_warning ("Failed to check and write file blocks.\n");
            ret = -1;
            goto out;
        }

        if (write_seafile (mgr, repo_id, version, &cdc, sha1) < 0) { // 生成seafile对象并存入
            seaf_warning ("Failed to write seafile.\n");
            ret = -1;
            goto out;
        }
    }

out:
    if (cdc.blk_sha1s)
        free (cdc.blk_sha1s);

    return ret;
}

int // 根据块的路径进行索引（只检查并硬链接）
seaf_fs_manager_index_raw_blocks (SeafFSManager *mgr,
                                  const char *repo_id,
                                  int version,
                                  GList *paths,
                                  GList *blockids)
{
    int ret = 0;
    GList *ptr, *q;

    if (!paths)
        return -1;

    for (ptr = paths, q = blockids; ptr; ptr = ptr->next, q = q->next) {
        char *path = ptr->data;
        char *blk_id = q->data;
        unsigned char sha1[20];

        hex_to_rawdata (blk_id, sha1, 20);
        ret = check_and_write_block (repo_id, version, path, sha1, blk_id); // 仅硬连接
        if (ret < 0)
            break;

    }

    return ret;
}

int // 重新进行索引（只检查）（若块不存在，则报错）
seaf_fs_manager_index_existed_file_blocks (SeafFSManager *mgr,
                                           const char *repo_id,
                                           int version,
                                           GList *blockids,
                                           unsigned char sha1[],
                                           gint64 file_size)
{
    int ret = 0;
    CDCFileDescriptor cdc;

    int block_nr = g_list_length (blockids);
    if (block_nr == 0) {
        /* handle empty file. */
        memset (sha1, 0, 20);
        create_cdc_for_empty_file (&cdc);
    } else {
        if (init_file_cdc (&cdc, repo_id, version, block_nr, file_size) < 0) { // 初始化CDC
            ret = -1;
            goto out;
        }

        if (check_existed_file_blocks (&cdc, blockids) < 0) { // 检查
            seaf_warning ("Failed to check and write file blocks.\n");
            ret = -1;
            goto out;
        }

        if (write_seafile (mgr, repo_id, version, &cdc, sha1) < 0) { // 写seafile
            seaf_warning ("Failed to write seafile.\n");
            ret = -1;
            goto out;
        }
    }

out:
    if (cdc.blk_sha1s)
        free (cdc.blk_sha1s);

    return ret;
}

void //
seafile_ref (Seafile *seafile)
{
    ++seafile->ref_count;
}

static void 增加引用
seafile_free (Seafile *seafile)
{
    int i;

    if (seafile->blk_sha1s) {
        for (i = 0; i < seafile->n_blocks; ++i)
            g_free (seafile->blk_sha1s[i]);
        g_free (seafile->blk_sha1s);
    }

    g_free (seafile);
}

void // 移除引用
seafile_unref (Seafile *seafile)
{
    if (!seafile)
        return;

    if (--seafile->ref_count <= 0)
        seafile_free (seafile);
}

static Seafile * // 获取seafile对象；版本0
seafile_from_v0_data (const char *id, const void *data, int len)
{
    const SeafileOndisk *ondisk = data;
    Seafile *seafile;
    int id_list_len, n_blocks;

    if (len < sizeof(SeafileOndisk)) {
        seaf_warning ("[fs mgr] Corrupt seafile object %s.\n", id);
        return NULL;
    }

    if (ntohl(ondisk->type) != SEAF_METADATA_TYPE_FILE) {
        seaf_warning ("[fd mgr] %s is not a file.\n", id);
        return NULL;
    }

    id_list_len = len - sizeof(SeafileOndisk);
    if (id_list_len % 20 != 0) {
        seaf_warning ("[fs mgr] Corrupt seafile object %s.\n", id);
        return NULL;
    }
    n_blocks = id_list_len / 20;

    seafile = g_new0 (Seafile, 1);

    seafile->object.type = SEAF_METADATA_TYPE_FILE;
    seafile->version = 0;
    memcpy (seafile->file_id, id, 41);
    seafile->file_size = ntoh64 (ondisk->file_size);
    seafile->n_blocks = n_blocks;

    seafile->blk_sha1s = g_new0 (char*, seafile->n_blocks);
    const unsigned char *blk_sha1_ptr = ondisk->block_ids;
    int i;
    for (i = 0; i < seafile->n_blocks; ++i) {
        char *blk_sha1 = g_new0 (char, 41);
        seafile->blk_sha1s[i] = blk_sha1;
        rawdata_to_hex (blk_sha1_ptr, blk_sha1, 20);
        blk_sha1_ptr += 20;
    }

    seafile->ref_count = 1;
    return seafile;
}

static Seafile * // 通过json对象获取seafile对象
seafile_from_json_object (const char *id, json_t *object)
{
    json_t *block_id_array = NULL;
    int type;
    int version;
    guint64 file_size;
    Seafile *seafile = NULL;

    /* Sanity checks. */
    type = json_object_get_int_member (object, "type");
    if (type != SEAF_METADATA_TYPE_FILE) {
        seaf_debug ("Object %s is not a file.\n", id);
        return NULL;
    }

    version = (int) json_object_get_int_member (object, "version");
    if (version < 1) {
        seaf_debug ("Seafile object %s version should be > 0, version is %d.\n",
                    id, version);
        return NULL;
    }

    file_size = (guint64) json_object_get_int_member (object, "size");

    block_id_array = json_object_get (object, "block_ids");
    if (!block_id_array) {
        seaf_debug ("No block id array in seafile object %s.\n", id);
        return NULL;
    }

    seafile = g_new0 (Seafile, 1);

    seafile->object.type = SEAF_METADATA_TYPE_FILE;

    memcpy (seafile->file_id, id, 40);
    seafile->version = version;
    seafile->file_size = file_size;
    seafile->n_blocks = json_array_size (block_id_array);
    seafile->blk_sha1s = g_new0 (char *, seafile->n_blocks);

    int i;
    json_t *block_id_obj;
    const char *block_id;
    for (i = 0; i < seafile->n_blocks; ++i) {
        block_id_obj = json_array_get (block_id_array, i);
        block_id = json_string_value (block_id_obj);
        if (!block_id || !is_object_id_valid(block_id)) {
            seafile_free (seafile);
            return NULL;
        }
        seafile->blk_sha1s[i] = g_strdup(block_id);
    }

    seafile->ref_count = 1;

    return seafile;
}

static Seafile * // 通过json串获取seafile对象
seafile_from_json (const char *id, void *data, int len)
{
    guint8 *decompressed;
    int outlen;
    json_t *object = NULL;
    json_error_t error;
    Seafile *seafile;

    if (seaf_decompress (data, len, &decompressed, &outlen) < 0) { // 解压缩
        seaf_warning ("Failed to decompress seafile object %s.\n", id);
        return NULL;
    }

    object = json_loadb ((const char *)decompressed, outlen, 0, &error);
    g_free (decompressed);
    if (!object) {
        if (error.text)
            seaf_warning ("Failed to load seafile json object: %s.\n", error.text);
        else
            seaf_warning ("Failed to load seafile json object.\n");
        return NULL;
    }

    seafile = seafile_from_json_object (id, object);

    json_decref (object);
    return seafile;
}

static Seafile * // 字节流转对象
seafile_from_data (const char *id, void *data, int len, gboolean is_json)
{
    if (is_json)
        return seafile_from_json (id, data, len);
    else
        return seafile_from_v0_data (id, data, len);
}

Seafile * // 获取seafile对象
seaf_fs_manager_get_seafile (SeafFSManager *mgr,
                             const char *repo_id,
                             int version,
                             const char *file_id)
{
    void *data;
    int len;
    Seafile *seafile;

#if 0
    seafile = g_hash_table_lookup (mgr->priv->seafile_cache, file_id);
    if (seafile) {
        seafile_ref (seafile);
        return seafile;
    }
#endif

    if (memcmp (file_id, EMPTY_SHA1, 40) == 0) {
        seafile = g_new0 (Seafile, 1);
        memset (seafile->file_id, '0', 40);
        seafile->ref_count = 1;
        return seafile;
    }

    if (seaf_obj_store_read_obj (mgr->obj_store, repo_id, version,
                                 file_id, &data, &len) < 0) {
        seaf_warning ("[fs mgr] Failed to read file %s.\n", file_id);
        return NULL;
    }

    seafile = seafile_from_data (file_id, data, len, (version > 0));
    g_free (data);

#if 0
    /*
     * Add to cache. Also increase ref count.
     */
    seafile_ref (seafile);
    g_hash_table_insert (mgr->priv->seafile_cache, g_strdup(file_id), seafile);
#endif

    return seafile;
}

static guint8 * // 对象转字节流；版本0
seafile_to_v0_data (Seafile *file, int *len)
{
    SeafileOndisk *ondisk;

    *len = sizeof(SeafileOndisk) + file->n_blocks * 20;
    ondisk = (SeafileOndisk *)g_new0 (char, *len);

    ondisk->type = htonl(SEAF_METADATA_TYPE_FILE);
    ondisk->file_size = hton64 (file->file_size);

    guint8 *ptr = ondisk->block_ids;
    int i;
    for (i = 0; i < file->n_blocks; ++i) {
        hex_to_rawdata (file->blk_sha1s[i], ptr, 20);
        ptr += 20;
    }

    return (guint8 *)ondisk;
}

static guint8 * // 对象转json字节流
seafile_to_json (Seafile *file, int *len)
{
    json_t *object, *block_id_array;

    object = json_object ();

    json_object_set_int_member (object, "type", SEAF_METADATA_TYPE_FILE);
    json_object_set_int_member (object, "version", file->version);

    json_object_set_int_member (object, "size", file->file_size);

    block_id_array = json_array ();
    int i;
    for (i = 0; i < file->n_blocks; ++i) {
        json_array_append_new (block_id_array, json_string(file->blk_sha1s[i]));
    }
    json_object_set_new (object, "block_ids", block_id_array);

    char *data = json_dumps (object, JSON_SORT_KEYS);
    *len = strlen(data);

    unsigned char sha1[20];
    calculate_sha1 (sha1, data, *len);
    rawdata_to_hex (sha1, file->file_id, 20);

    json_decref (object);
    return (guint8 *)data;
}

static guint8 * // 对象转字节流
seafile_to_data (Seafile *file, int *len)
{
    if (file->version > 0) {
        guint8 *data;
        int orig_len;
        guint8 *compressed;

        data = seafile_to_json (file, &orig_len); // 获取字节流
        if (!data)
            return NULL;

        if (seaf_compress (data, orig_len, &compressed, len) < 0) { // 压缩
            seaf_warning ("Failed to compress file object %s.\n", file->file_id);
            g_free (data);
            return NULL;
        }
        g_free (data);
        return compressed;
    } else
        return seafile_to_v0_data (file, len);
}

int // 保存seafile对象
seafile_save (SeafFSManager *fs_mgr,
              const char *repo_id,
              int version,
              Seafile *file)
{
    guint8 *data;
    int len;
    int ret = 0;

    if (seaf_obj_store_obj_exists (fs_mgr->obj_store, repo_id, version, file->file_id))
        return 0;

    data = seafile_to_data (file, &len);
    if (!data)
        return -1;

    if (seaf_obj_store_write_obj (fs_mgr->obj_store, repo_id, version, file->file_id,
                                  data, len, FALSE) < 0)
        ret = -1;

    g_free (data);
    return ret;
}

static void compute_dir_id_v0 (SeafDir *dir, GList *entries) // 计算目录id；版本0
{
    SHA_CTX ctx;
    GList *p;
    uint8_t sha1[20];
    SeafDirent *dent;
    guint32 mode_le;

    /* ID for empty dirs is EMPTY_SHA1. */
    if (entries == NULL) {
        memset (dir->dir_id, '0', 40);
        return;
    }

    SHA1_Init (&ctx);
    for (p = entries; p; p = p->next) { // 通过目录下的目录项，生辰id
        dent = (SeafDirent *)p->data;
        SHA1_Update (&ctx, dent->id, 40); // 目录项id
        SHA1_Update (&ctx, dent->name, dent->name_len); // 目录项名
        /* Convert mode to little endian before compute. */
        if (G_BYTE_ORDER == G_BIG_ENDIAN)
            mode_le = GUINT32_SWAP_LE_BE (dent->mode);
        else
            mode_le = dent->mode;
        SHA1_Update (&ctx, &mode_le, sizeof(mode_le));
    }
    SHA1_Final (sha1, &ctx);

    rawdata_to_hex (sha1, dir->dir_id, 20);
}

SeafDir * // 创建新的seafdir
seaf_dir_new (const char *id, GList *entries, int version) // 给定id、目录项、版本
{
    SeafDir *dir;

    dir = g_new0(SeafDir, 1);

    dir->version = version;
    if (id != NULL) { // 指定的id
        memcpy(dir->dir_id, id, 40);
        dir->dir_id[40] = '\0';
    } else if (version == 0) { // 生成id
        compute_dir_id_v0 (dir, entries);
    }
    dir->entries = entries;

    if (dir->entries != NULL) // 项目非空
        dir->ondisk = seaf_dir_to_data (dir, &dir->ondisk_size); // 对象转字节流
    else
        memcpy (dir->dir_id, EMPTY_SHA1, 40); // 设置id为空

    return dir;
}

void // 释放seafdir
seaf_dir_free (SeafDir *dir)
{
    if (dir == NULL)
        return;

    GList *ptr = dir->entries;
    while (ptr) {
        seaf_dirent_free ((SeafDirent *)ptr->data);
        ptr = ptr->next;
    }

    g_list_free (dir->entries);
    g_free (dir->ondisk);
    g_free(dir);
}

SeafDirent * // 创建新的seafdirent
seaf_dirent_new (int version, const char *sha1, int mode, const char *name,
                 gint64 mtime, const char *modifier, gint64 size)
{
    SeafDirent *dent;

    dent = g_new0 (SeafDirent, 1);
    dent->version = version;
    memcpy(dent->id, sha1, 40);
    dent->id[40] = '\0';
    /* Mode for files must have 0644 set. To prevent the caller from forgetting,
     * we set the bits here.
     */
    if (S_ISREG(mode))
        dent->mode = (mode | 0644);
    else
        dent->mode = mode;
    dent->name = g_strdup(name);
    dent->name_len = strlen(name);

    if (version > 0) {
        dent->mtime = mtime;
        if (S_ISREG(mode)) {
            dent->modifier = g_strdup(modifier);
            dent->size = size;
        }
    }

    return dent;
}

void // 释放seafdirent
seaf_dirent_free (SeafDirent *dent)
{
    if (!dent)
        return;
    g_free (dent->name);
    g_free (dent->modifier);
    g_free (dent);
}

SeafDirent * // 复制seafdirent
seaf_dirent_dup (SeafDirent *dent)
{
    SeafDirent *new_dent;

    new_dent = g_memdup (dent, sizeof(SeafDirent));
    new_dent->name = g_strdup(dent->name);
    new_dent->modifier = g_strdup(dent->modifier);

    return new_dent;
}

static SeafDir * // 字节流转seafdir；版本0
seaf_dir_from_v0_data (const char *dir_id, const uint8_t *data, int len)
{
    SeafDir *root;
    SeafDirent *dent;
    const uint8_t *ptr;
    int remain;
    int dirent_base_size;
    guint32 meta_type;
    guint32 name_len;

    ptr = data;
    remain = len;

    meta_type = get32bit (&ptr);
    remain -= 4;
    if (meta_type != SEAF_METADATA_TYPE_DIR) {
        seaf_warning ("Data does not contain a directory.\n");
        return NULL;
    }

    root = g_new0(SeafDir, 1);
    root->object.type = SEAF_METADATA_TYPE_DIR;
    root->version = 0;
    memcpy(root->dir_id, dir_id, 40);
    root->dir_id[40] = '\0';

    dirent_base_size = 2 * sizeof(guint32) + 40;
    while (remain > dirent_base_size) { // 读seafdirent
        dent = g_new0(SeafDirent, 1);

        dent->version = 0;
        dent->mode = get32bit (&ptr);
        memcpy (dent->id, ptr, 40);
        dent->id[40] = '\0';
        ptr += 40;
        name_len = get32bit (&ptr);
        remain -= dirent_base_size;
        if (remain >= name_len) {
            dent->name_len = MIN (name_len, SEAF_DIR_NAME_LEN - 1);
            dent->name = g_strndup((const char *)ptr, dent->name_len);
            ptr += dent->name_len;
            remain -= dent->name_len;
        } else {
            seaf_warning ("Bad data format for dir objcet %s.\n", dir_id);
            g_free (dent);
            goto bad;
        }

        root->entries = g_list_prepend (root->entries, dent);
    }

    root->entries = g_list_reverse (root->entries);

    return root;

bad:
    seaf_dir_free (root);
    return NULL;
}

static SeafDirent * // json对象转seafdirent
parse_dirent (const char *dir_id, int version, json_t *object)
{
    guint32 mode;
    const char *id;
    const char *name;
    gint64 mtime;
    const char *modifier;
    gint64 size;

    mode = (guint32) json_object_get_int_member (object, "mode");

    id = json_object_get_string_member (object, "id");
    if (!id) {
        seaf_debug ("Dirent id not set for dir object %s.\n", dir_id);
        return NULL;
    }
    if (!is_object_id_valid (id)) {
        seaf_debug ("Dirent id is invalid for dir object %s.\n", dir_id);
        return NULL;
    }

    name = json_object_get_string_member (object, "name");
    if (!name) {
        seaf_debug ("Dirent name not set for dir object %s.\n", dir_id);
        return NULL;
    }

    mtime = json_object_get_int_member (object, "mtime");
    if (S_ISREG(mode)) {
        modifier = json_object_get_string_member (object, "modifier");
        if (!modifier) {
            seaf_debug ("Dirent modifier not set for dir object %s.\n", dir_id);
            return NULL;
        }
        size = json_object_get_int_member (object, "size");
    }

    SeafDirent *dirent = g_new0 (SeafDirent, 1);
    dirent->version = version;
    dirent->mode = mode;
    memcpy (dirent->id, id, 40);
    dirent->name_len = strlen(name);
    dirent->name = g_strdup(name);
    dirent->mtime = mtime;
    if (S_ISREG(mode)) {
        dirent->modifier = g_strdup(modifier);
        dirent->size = size;
    }

    return dirent;
}

static SeafDir * // json对象转seafdir
seaf_dir_from_json_object (const char *dir_id, json_t *object)
{
    json_t *dirent_array = NULL;
    int type;
    int version;
    SeafDir *dir = NULL;

    /* Sanity checks. */
    type = json_object_get_int_member (object, "type");
    if (type != SEAF_METADATA_TYPE_DIR) {
        seaf_debug ("Object %s is not a dir.\n", dir_id);
        return NULL;
    }

    version = (int) json_object_get_int_member (object, "version");
    if (version < 1) {
        seaf_debug ("Dir object %s version should be > 0, version is %d.\n",
                    dir_id, version);
        return NULL;
    }

    dirent_array = json_object_get (object, "dirents");
    if (!dirent_array) {
        seaf_debug ("No dirents in dir object %s.\n", dir_id);
        return NULL;
    }

    dir = g_new0 (SeafDir, 1);

    dir->object.type = SEAF_METADATA_TYPE_DIR;

    memcpy (dir->dir_id, dir_id, 40);
    dir->version = version;

    size_t n_dirents = json_array_size (dirent_array);
    int i;
    json_t *dirent_obj;
    SeafDirent *dirent;
    for (i = 0; i < n_dirents; ++i) { // 将json对象转seafdirent
        dirent_obj = json_array_get (dirent_array, i);
        dirent = parse_dirent (dir_id, version, dirent_obj);
        if (!dirent) {
            seaf_dir_free (dir);
            return NULL;
        }
        dir->entries = g_list_prepend (dir->entries, dirent);
    }
    dir->entries = g_list_reverse (dir->entries);

    return dir;
}

static SeafDir * // json串转seafdir
seaf_dir_from_json (const char *dir_id, uint8_t *data, int len)
{
    guint8 *decompressed;
    int outlen;
    json_t *object = NULL;
    json_error_t error;
    SeafDir *dir;

    if (seaf_decompress (data, len, &decompressed, &outlen) < 0) { // 解压
        seaf_warning ("Failed to decompress dir object %s.\n", dir_id);
        return NULL;
    }

    object = json_loadb ((const char *)decompressed, outlen, 0, &error);
    g_free (decompressed);
    if (!object) {
        if (error.text)
            seaf_warning ("Failed to load seafdir json object: %s.\n", error.text);
        else
            seaf_warning ("Failed to load seafdir json object.\n");
        return NULL;
    }

    dir = seaf_dir_from_json_object (dir_id, object);

    json_decref (object);
    return dir;
}

SeafDir * // 字节流转seafdir
seaf_dir_from_data (const char *dir_id, uint8_t *data, int len,
                    gboolean is_json)
{
    if (is_json)
        return seaf_dir_from_json (dir_id, data, len);
    else
        return seaf_dir_from_v0_data (dir_id, data, len);
}

inline static int // 获取dirent的字节流大小
ondisk_dirent_size (SeafDirent *dirent)
{
    return sizeof(DirentOndisk) + dirent->name_len;
}

static void * // seafdir转字节流；版本0
seaf_dir_to_v0_data (SeafDir *dir, int *len)
{
    SeafdirOndisk *ondisk;
    int dir_ondisk_size = sizeof(SeafdirOndisk);
    GList *dirents = dir->entries;
    GList *ptr;
    SeafDirent *de;
    char *p;
    DirentOndisk *de_ondisk;

    for (ptr = dirents; ptr; ptr = ptr->next) {
        de = ptr->data;
        dir_ondisk_size += ondisk_dirent_size (de);
    }

    *len = dir_ondisk_size;
    ondisk = (SeafdirOndisk *) g_new0 (char, dir_ondisk_size);

    ondisk->type = htonl (SEAF_METADATA_TYPE_DIR);
    p = ondisk->dirents;
    for (ptr = dirents; ptr; ptr = ptr->next) { // 遍历seafdirent
        de = ptr->data;
        de_ondisk = (DirentOndisk *) p;

        de_ondisk->mode = htonl(de->mode);
        memcpy (de_ondisk->id, de->id, 40);
        de_ondisk->name_len = htonl (de->name_len);
        memcpy (de_ondisk->name, de->name, de->name_len);

        p += ondisk_dirent_size (de);
    }

    return (void *)ondisk;
}

static void // 将seafdirent对象添加到json对象数组中
add_to_dirent_array (json_t *array, SeafDirent *dirent)
{
    json_t *object;

    object = json_object ();
    json_object_set_int_member (object, "mode", dirent->mode);
    json_object_set_string_member (object, "id", dirent->id);
    json_object_set_string_member (object, "name", dirent->name);
    json_object_set_int_member (object, "mtime", dirent->mtime);
    if (S_ISREG(dirent->mode)) {
        json_object_set_string_member (object, "modifier", dirent->modifier);
        json_object_set_int_member (object, "size", dirent->size);
    }

    json_array_append_new (array, object);
}

static void * // seafdir转json
seaf_dir_to_json (SeafDir *dir, int *len)
{
    json_t *object, *dirent_array;
    GList *ptr;
    SeafDirent *dirent;

    object = json_object ();

    json_object_set_int_member (object, "type", SEAF_METADATA_TYPE_DIR);
    json_object_set_int_member (object, "version", dir->version);

    dirent_array = json_array ();
    for (ptr = dir->entries; ptr; ptr = ptr->next) {
        dirent = ptr->data;
        add_to_dirent_array (dirent_array, dirent);
    }
    json_object_set_new (object, "dirents", dirent_array);

    char *data = json_dumps (object, JSON_SORT_KEYS);
    *len = strlen(data);

    /* The dir object id is sha1 hash of the json object. */
    unsigned char sha1[20];
    calculate_sha1 (sha1, data, *len);
    rawdata_to_hex (sha1, dir->dir_id, 20);

    json_decref (object);
    return data;
}

void * // seafdir转字节流
seaf_dir_to_data (SeafDir *dir, int *len)
{
    if (dir->version > 0) {
        guint8 *data;
        int orig_len;
        guint8 *compressed;

        data = seaf_dir_to_json (dir, &orig_len);
        if (!data)
            return NULL;

        if (seaf_compress (data, orig_len, &compressed, len) < 0) { // 压缩
            seaf_warning ("Failed to compress dir object %s.\n", dir->dir_id);
            g_free (data);
            return NULL;
        }

        g_free (data);
        return compressed;
    } else
        return seaf_dir_to_v0_data (dir, len);
}

int // 保存seafdir对象
seaf_dir_save (SeafFSManager *fs_mgr,
               const char *repo_id,
               int version,
               SeafDir *dir)
{
    int ret = 0;

    /* Don't need to save empty dir on disk. */
    if (memcmp (dir->dir_id, EMPTY_SHA1, 40) == 0)
        return 0;

    if (seaf_obj_store_obj_exists (fs_mgr->obj_store, repo_id, version, dir->dir_id))
        return 0;

    if (seaf_obj_store_write_obj (fs_mgr->obj_store, repo_id, version, dir->dir_id,
                                  dir->ondisk, dir->ondisk_size, FALSE) < 0)
        ret = -1;

    return ret;
}

SeafDir * // 获取seadir对象
seaf_fs_manager_get_seafdir(SeafFSManager *mgr,
                            const char *repo_id,
                            int version,
                            const char *dir_id)
{
    void *data;
    int len;
    SeafDir *dir;

    /* TODO: add hash cache */

    if (memcmp (dir_id, EMPTY_SHA1, 40) == 0) { // 特判空目录
        dir = g_new0 (SeafDir, 1);
        dir->version = version;
        memset (dir->dir_id, '0', 40);
        return dir;
    }

    if (seaf_obj_store_read_obj (mgr->obj_store, repo_id, version,
                                 dir_id, &data, &len) < 0) { // 字节流
        seaf_warning ("[fs mgr] Failed to read dir %s.\n", dir_id);
        return NULL;
    }

    dir = seaf_dir_from_data (dir_id, data, len, (version > 0)); // 字节流转对象
    g_free (data);

    return dir;
}

static gint // seafdirent比较函数
compare_dirents (gconstpointer a, gconstpointer b)
{
    const SeafDirent *denta = a, *dentb = b;

    return strcmp (dentb->name, denta->name);
}

static gboolean // 判断dirent列表是否已排序
is_dirents_sorted (GList *dirents)
{
    GList *ptr;
    SeafDirent *dent, *dent_n;
    gboolean ret = TRUE;

    for (ptr = dirents; ptr != NULL; ptr = ptr->next) {
        dent = ptr->data;
        if (!ptr->next)
            break;
        dent_n = ptr->next->data;

        /* If dirents are not sorted in descending order, return FALSE. */
        if (strcmp (dent->name, dent_n->name) < 0) {
            ret = FALSE;
            break;
        }
    }

    return ret;
}

SeafDir * // 获取seafdir，同时排序seafdirent
seaf_fs_manager_get_seafdir_sorted (SeafFSManager *mgr,
                                    const char *repo_id,
                                    int version,
                                    const char *dir_id)
{
    SeafDir *dir = seaf_fs_manager_get_seafdir(mgr, repo_id, version, dir_id);

    if (!dir)
        return NULL;

    /* Only some very old dir objects are not sorted. */
    if (version > 0)
        return dir;

    if (!is_dirents_sorted (dir->entries)) // 排序
        dir->entries = g_list_sort (dir->entries, compare_dirents);

    return dir;
}

SeafDir * // 根据相对路径获取seafdir，并排序seafdirent
seaf_fs_manager_get_seafdir_sorted_by_path (SeafFSManager *mgr,
                                            const char *repo_id,
                                            int version,
                                            const char *root_id,
                                            const char *path)
{
    SeafDir *dir = seaf_fs_manager_get_seafdir_by_path (mgr, repo_id,
                                                        version, root_id,
                                                        path, NULL);

    if (!dir)
        return NULL;

    /* Only some very old dir objects are not sorted. */
    if (version > 0)
        return dir;

    if (!is_dirents_sorted (dir->entries)) // 排序
        dir->entries = g_list_sort (dir->entries, compare_dirents);

    return dir;
}

static int // 获取类型；版本0
parse_metadata_type_v0 (const uint8_t *data, int len)
{
    const uint8_t *ptr = data;

    if (len < sizeof(guint32))
        return SEAF_METADATA_TYPE_INVALID;

    return (int)(get32bit(&ptr));
}

static int // 从json获取类型
parse_metadata_type_json (const char *obj_id, uint8_t *data, int len)
{
    guint8 *decompressed;
    int outlen;
    json_t *object;
    json_error_t error;
    int type;

    if (seaf_decompress (data, len, &decompressed, &outlen) < 0) {
        seaf_warning ("Failed to decompress fs object %s.\n", obj_id);
        return SEAF_METADATA_TYPE_INVALID;
    }

    object = json_loadb ((const char *)decompressed, outlen, 0, &error);
    g_free (decompressed);
    if (!object) {
        if (error.text)
            seaf_warning ("Failed to load fs json object: %s.\n", error.text);
        else
            seaf_warning ("Failed to load fs json object.\n");
        return SEAF_METADATA_TYPE_INVALID;
    }

    type = json_object_get_int_member (object, "type");

    json_decref (object);
    return type;
}

int // seaf对象获取类型
seaf_metadata_type_from_data (const char *obj_id,
                              uint8_t *data, int len, gboolean is_json)
{
    if (is_json)
        return parse_metadata_type_json (obj_id, data, len);
    else
        return parse_metadata_type_v0 (data, len);
}

SeafFSObject * // 获取文件系统对象；版本0
fs_object_from_v0_data (const char *obj_id, const uint8_t *data, int len)
{
    int type = parse_metadata_type_v0 (data, len);

    if (type == SEAF_METADATA_TYPE_FILE)
        return (SeafFSObject *)seafile_from_v0_data (obj_id, data, len);
    else if (type == SEAF_METADATA_TYPE_DIR)
        return (SeafFSObject *)seaf_dir_from_v0_data (obj_id, data, len);
    else {
        seaf_warning ("Invalid object type %d.\n", type);
        return NULL;
    }
}

SeafFSObject * // json转文件系统对象
fs_object_from_json (const char *obj_id, uint8_t *data, int len)
{
    guint8 *decompressed;
    int outlen;
    json_t *object;
    json_error_t error;
    int type;
    SeafFSObject *fs_obj;

    if (seaf_decompress (data, len, &decompressed, &outlen) < 0) {
        seaf_warning ("Failed to decompress fs object %s.\n", obj_id);
        return NULL;
    }

    object = json_loadb ((const char *)decompressed, outlen, 0, &error);
    g_free (decompressed);
    if (!object) {
        if (error.text)
            seaf_warning ("Failed to load fs json object: %s.\n", error.text);
        else
            seaf_warning ("Failed to load fs json object.\n");
        return NULL;
    }

    type = json_object_get_int_member (object, "type");

    if (type == SEAF_METADATA_TYPE_FILE)
        fs_obj = (SeafFSObject *)seafile_from_json_object (obj_id, object);
    else if (type == SEAF_METADATA_TYPE_DIR)
        fs_obj = (SeafFSObject *)seaf_dir_from_json_object (obj_id, object);
    else {
        seaf_warning ("Invalid fs type %d.\n", type);
        json_decref (object);
        return NULL;
    }

    json_decref (object);

    return fs_obj;
}

SeafFSObject * // 获取文件系统对象
seaf_fs_object_from_data (const char *obj_id,
                          uint8_t *data, int len,
                          gboolean is_json)
{
    if (is_json)
        return fs_object_from_json (obj_id, data, len);
    else
        return fs_object_from_v0_data (obj_id, data, len);
}

void // 释放文件系统对象
seaf_fs_object_free (SeafFSObject *obj)
{
    if (!obj)
        return;

    if (obj->type == SEAF_METADATA_TYPE_FILE)
        seafile_unref ((Seafile *)obj);
    else if (obj->type == SEAF_METADATA_TYPE_DIR)
        seaf_dir_free ((SeafDir *)obj);
}

BlockList * // 新建块表
block_list_new ()
{
    BlockList *bl = g_new0 (BlockList, 1);

    bl->block_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    bl->block_ids = g_ptr_array_new_with_free_func (g_free);

    return bl;
}

void // 释放块表
block_list_free (BlockList *bl)
{
    if (bl->block_hash)
        g_hash_table_destroy (bl->block_hash);
    g_ptr_array_free (bl->block_ids, TRUE);
    g_free (bl);
}

void // 块表插入块
block_list_insert (BlockList *bl, const char *block_id)
{
    if (g_hash_table_lookup (bl->block_hash, block_id))
        return;

    char *key = g_strdup(block_id);
    g_hash_table_replace (bl->block_hash, key, key);
    g_ptr_array_add (bl->block_ids, g_strdup(block_id));
    ++bl->n_blocks;
}

BlockList * // 块表差集
block_list_difference (BlockList *bl1, BlockList *bl2)
{
    BlockList *bl;
    int i;
    char *block_id;
    char *key;

    bl = block_list_new ();

    for (i = 0; i < bl1->block_ids->len; ++i) { // 遍历表一
        block_id = g_ptr_array_index (bl1->block_ids, i); // 向新表插入
        if (g_hash_table_lookup (bl2->block_hash, block_id) == NULL) { // 在表二找到，就从新表删去
            key = g_strdup(block_id);
            g_hash_table_replace (bl->block_hash, key, key);
            g_ptr_array_add (bl->block_ids, g_strdup(block_id));
            ++bl->n_blocks;
        }
    }

    return bl;
}

static int // 对文件执行遍历回调函数
traverse_file (SeafFSManager *mgr,
               const char *repo_id,
               int version,
               const char *id, // seafile的id
               TraverseFSTreeCallback callback, // 回调函数
               void *user_data, // 回调函数用户参数
               gboolean skip_errors) // 跳过异常
{
    gboolean stop = FALSE;

    if (memcmp (id, EMPTY_SHA1, 40) == 0)
        return 0;

    if (!callback (mgr, repo_id, version, id, SEAF_METADATA_TYPE_FILE, user_data, &stop) &&
        !skip_errors) // 转发回调函数
        return -1;

    return 0;
}

static int // 递归遍历目录下的所有目录和文件
traverse_dir (SeafFSManager *mgr,
              const char *repo_id,
              int version,
              const char *id, // seafdir的id
              TraverseFSTreeCallback callback,
              void *user_data,
              gboolean skip_errors)
{
    SeafDir *dir;
    GList *p;
    SeafDirent *seaf_dent;
    gboolean stop = FALSE;

    if (!callback (mgr, repo_id, version,
                   id, SEAF_METADATA_TYPE_DIR, user_data, &stop) &&
        !skip_errors)
        return -1;

    if (stop)
        return 0;

    dir = seaf_fs_manager_get_seafdir (mgr, repo_id, version, id); // 获取seafdir
    if (!dir) {
        seaf_warning ("[fs-mgr]get seafdir %s failed\n", id);
        if (skip_errors)
            return 0;
        return -1;
    }
    for (p = dir->entries; p; p = p->next) { // 遍历dirent
        seaf_dent = (SeafDirent *)p->data;

        if (S_ISREG(seaf_dent->mode)) { // 是常规文件
            if (traverse_file (mgr, repo_id, version, seaf_dent->id,
                               callback, user_data, skip_errors) < 0) { // 对文件执行遍历回调函数
                if (!skip_errors) {
                    seaf_dir_free (dir);
                    return -1;
                }
            }
        } else if (S_ISDIR(seaf_dent->mode)) { // 是目录
            if (traverse_dir (mgr, repo_id, version, seaf_dent->id,
                              callback, user_data, skip_errors) < 0) { // 递归
                if (!skip_errors) {
                    seaf_dir_free (dir);
                    return -1;
                }
            }
        }
    }

    seaf_dir_free (dir);
    return 0;
}

int // 遍历文件树
seaf_fs_manager_traverse_tree (SeafFSManager *mgr,
                               const char *repo_id,
                               int version,
                               const char *root_id, // seafdir的id
                               TraverseFSTreeCallback callback,
                               void *user_data,
                               gboolean skip_errors)
{
    if (strcmp (root_id, EMPTY_SHA1) == 0) {
        return 0;
    }
    return traverse_dir (mgr, repo_id, version, root_id, callback, user_data, skip_errors);
}

static int // 递归遍历，根据路径
traverse_dir_path (SeafFSManager *mgr,
                   const char *repo_id,
                   int version,
                   const char *dir_path,
                   SeafDirent *dent,
                   TraverseFSPathCallback callback,
                   void *user_data)
{
    SeafDir *dir;
    GList *p;
    SeafDirent *seaf_dent;
    gboolean stop = FALSE;
    char *sub_path;
    int ret = 0;

    if (!callback (mgr, dir_path, dent, user_data, &stop))
        return -1;

    if (stop)
        return 0;

    dir = seaf_fs_manager_get_seafdir (mgr, repo_id, version, dent->id); // 获取seafdir
    if (!dir) {
        seaf_warning ("get seafdir %s:%s failed\n", repo_id, dent->id);
        return -1;
    }

    for (p = dir->entries; p; p = p->next) { // 遍历dirent
        seaf_dent = (SeafDirent *)p->data;
        sub_path = g_strconcat (dir_path, "/", seaf_dent->name, NULL); // 获取下级目录/文件名

        if (S_ISREG(seaf_dent->mode)) { // 常规文件，回调
            if (!callback (mgr, sub_path, seaf_dent, user_data, &stop)) {
                g_free (sub_path);
                ret = -1;
                break;
            }
        } else if (S_ISDIR(seaf_dent->mode)) {
            if (traverse_dir_path (mgr, repo_id, version, sub_path, seaf_dent,
                                   callback, user_data) < 0) { // 目录，递归
                g_free (sub_path);
                ret = -1;
                break;
            }
        }
        g_free (sub_path);
    }

    seaf_dir_free (dir);
    return ret;
}

int // 遍历文件树，根据相对路径
seaf_fs_manager_traverse_path (SeafFSManager *mgr,
                               const char *repo_id,
                               int version,
                               const char *root_id, // seafdir的id
                               const char *dir_path, // 相对路径
                               TraverseFSPathCallback callback,
                               void *user_data)
{
    SeafDirent *dent;
    int ret = 0;

    dent = seaf_fs_manager_get_dirent_by_path (mgr, repo_id, version,
                                               root_id, dir_path, NULL); // 获取相对路径下的seafdir
    if (!dent) {
        seaf_warning ("Failed to get dirent for %.8s:%s.\n", repo_id, dir_path);
        return -1;
    }

    ret = traverse_dir_path (mgr, repo_id, version, dir_path, dent,
                             callback, user_data); // 递归遍历，根据路径

    seaf_dirent_free (dent);
    return ret;
}

static gboolean // 填充块表
fill_blocklist (SeafFSManager *mgr,
                const char *repo_id, int version,
                const char *obj_id, int type,
                void *user_data, gboolean *stop)
{
    BlockList *bl = user_data;
    Seafile *seafile;
    int i;

    if (type == SEAF_METADATA_TYPE_FILE) { // 要求是seafile
        seafile = seaf_fs_manager_get_seafile (mgr, repo_id, version, obj_id); // 获取seafile
        if (!seafile) {
            seaf_warning ("[fs mgr] Failed to find file %s.\n", obj_id);
            return FALSE;
        }

        for (i = 0; i < seafile->n_blocks; ++i) // 遍历块索引
            block_list_insert (bl, seafile->blk_sha1s[i]); // 加入块表 

        seafile_unref (seafile);
    }

    return TRUE;
}

int // 记录seafdir下的所有块
seaf_fs_manager_populate_blocklist (SeafFSManager *mgr,
                                    const char *repo_id,
                                    int version,
                                    const char *root_id,
                                    BlockList *bl)
{
    return seaf_fs_manager_traverse_tree (mgr, repo_id, version, root_id,
                                          fill_blocklist, // 填充块表
                                          bl, FALSE); // 遍历文件树
}

gboolean // 判断对象是否存在
seaf_fs_manager_object_exists (SeafFSManager *mgr,
                               const char *repo_id,
                               int version,
                               const char *id)
{
    /* Empty file and dir always exists. */
    if (memcmp (id, EMPTY_SHA1, 40) == 0) // 空文件
        return TRUE;

    return seaf_obj_store_obj_exists (mgr->obj_store, repo_id, version, id); // 转发
}

void // 删除对象
seaf_fs_manager_delete_object (SeafFSManager *mgr,
                               const char *repo_id,
                               int version,
                               const char *id)
{
    seaf_obj_store_delete_obj (mgr->obj_store, repo_id, version, id); // 转发
}

gint64 // 获取文件大小
seaf_fs_manager_get_file_size (SeafFSManager *mgr,
                               const char *repo_id,
                               int version,
                               const char *file_id)
{
    Seafile *file;
    gint64 file_size;

    file = seaf_fs_manager_get_seafile (seaf->fs_mgr, repo_id, version, file_id); // 获取seafile
    if (!file) {
        seaf_warning ("Couldn't get file %s:%s\n", repo_id, file_id);
        return -1;
    }

    file_size = file->file_size;

    seafile_unref (file);
    return file_size; // 返回seafile中记录的文件大小
}

static gint64 // 获取目录大小
get_dir_size (SeafFSManager *mgr, const char *repo_id, int version, const char *id)
{
    SeafDir *dir;
    SeafDirent *seaf_dent;
    guint64 size = 0;
    gint64 result;
    GList *p;

    dir = seaf_fs_manager_get_seafdir (mgr, repo_id, version, id); // 获取seafdir
    if (!dir)
        return -1;

    for (p = dir->entries; p; p = p->next) {
        seaf_dent = (SeafDirent *)p->data;

        if (S_ISREG(seaf_dent->mode)) { // 常规文件，则计算文件大小
            if (dir->version > 0)
                result = seaf_dent->size;
            else {
                result = seaf_fs_manager_get_file_size (mgr,
                                                        repo_id,
                                                        version,
                                                        seaf_dent->id);
                if (result < 0) {
                    seaf_dir_free (dir);
                    return result;
                }
            }
            size += result;
        } else if (S_ISDIR(seaf_dent->mode)) { // 否则递归
            result = get_dir_size (mgr, repo_id, version, seaf_dent->id);
            if (result < 0) {
                seaf_dir_free (dir);
                return result;
            }
            size += result;
        }
    }

    seaf_dir_free (dir);
    return size;
}

gint64 // 获取文件系统大小
seaf_fs_manager_get_fs_size (SeafFSManager *mgr,
                             const char *repo_id,
                             int version,
                             const char *root_id)
{
     if (strcmp (root_id, EMPTY_SHA1) == 0)
        return 0;
     return get_dir_size (mgr, repo_id, version, root_id); // 就是获取目录大小
}

static int // 统计文件数目
count_dir_files (SeafFSManager *mgr, const char *repo_id, int version, const char *id)
{
    SeafDir *dir;
    SeafDirent *seaf_dent;
    int count = 0;
    int result;
    GList *p;

    dir = seaf_fs_manager_get_seafdir (mgr, repo_id, version, id); // 获取seafdir
    if (!dir)
        return -1;

    for (p = dir->entries; p; p = p->next) { // 遍历seafdirent
        seaf_dent = (SeafDirent *)p->data;

        if (S_ISREG(seaf_dent->mode)) { // 文件，加一
            count ++;
        } else if (S_ISDIR(seaf_dent->mode)) { // 否则递归
            result = count_dir_files (mgr, repo_id, version, seaf_dent->id);
            if (result < 0) {
                seaf_dir_free (dir);
                return result;
            }
            count += result;
        }
    }

    seaf_dir_free (dir);
    return count;
}

static int // 记录目录数、文件数
get_file_count_info (SeafFSManager *mgr,
                     const char *repo_id,
                     int version,
                     const char *id,
                     gint64 *dir_count,
                     gint64 *file_count,
                     gint64 *size)
{
    SeafDir *dir;
    SeafDirent *seaf_dent;
    GList *p;
    int ret = 0;

    dir = seaf_fs_manager_get_seafdir (mgr, repo_id, version, id);
    if (!dir)
        return -1;

    for (p = dir->entries; p; p = p->next) {
        seaf_dent = (SeafDirent *)p->data;

        if (S_ISREG(seaf_dent->mode)) {
            (*file_count)++;
            if (version > 0)
                (*size) += seaf_dent->size;
        } else if (S_ISDIR(seaf_dent->mode)) {
            (*dir_count)++;
            ret = get_file_count_info (mgr, repo_id, version, seaf_dent->id,
                                       dir_count, file_count, size);
        }
    }
    seaf_dir_free (dir);

    return ret;
}

int // 记录文件系统的文件
seaf_fs_manager_count_fs_files (SeafFSManager *mgr,
                                const char *repo_id,
                                int version,
                                const char *root_id)
{
     if (strcmp (root_id, EMPTY_SHA1) == 0)
        return 0;
     return count_dir_files (mgr, repo_id, version, root_id); // 就是获取目录中的文件数
}

SeafDir * // 根据相对路径获取seafdir
seaf_fs_manager_get_seafdir_by_path (SeafFSManager *mgr,
                                     const char *repo_id,
                                     int version,
                                     const char *root_id,
                                     const char *path,
                                     GError **error)
{
    SeafDir *dir;
    SeafDirent *dent;
    const char *dir_id = root_id;
    char *name, *saveptr;
    char *tmp_path = g_strdup(path);

    dir = seaf_fs_manager_get_seafdir (mgr, repo_id, version, dir_id); // 获取根
    if (!dir) {
        g_set_error (error, SEAFILE_DOMAIN, SEAF_ERR_DIR_MISSING, "directory is missing");
        g_free (tmp_path);
        return NULL;
    }

    name = strtok_r (tmp_path, "/", &saveptr);
    while (name != NULL) { // 遍历子目录
        GList *l;
        for (l = dir->entries; l != NULL; l = l->next) { // 在seafdirent中寻找同名
            dent = l->data;

            if (strcmp(dent->name, name) == 0 && S_ISDIR(dent->mode)) {
                dir_id = dent->id;
                break;
            }
        }

        if (!l) { // 出错
            g_set_error (error, SEAFILE_DOMAIN, SEAF_ERR_PATH_NO_EXIST,
                         "Path does not exists %s", path);
            seaf_dir_free (dir);
            dir = NULL;
            break;
        }

        SeafDir *prev = dir;
        dir = seaf_fs_manager_get_seafdir (mgr, repo_id, version, dir_id); // 获取下一级seafdir
        seaf_dir_free (prev);

        if (!dir) { // 出错
            g_set_error (error, SEAFILE_DOMAIN, SEAF_ERR_DIR_MISSING,
                         "directory is missing");
            break;
        }

        name = strtok_r (NULL, "/", &saveptr); // 下一级目录名
    }

    g_free (tmp_path);
    return dir;
}

char * // 相对路径获取对象id
seaf_fs_manager_path_to_obj_id (SeafFSManager *mgr,
                                const char *repo_id,
                                int version,
                                const char *root_id,
                                const char *path,
                                guint32 *mode,
                                GError **error)
{
    char *copy = g_strdup (path);
    int off = strlen(copy) - 1;
    char *slash, *name;
    SeafDir *base_dir = NULL;
    SeafDirent *dent;
    GList *p;
    char *obj_id = NULL;

    while (off >= 0 && copy[off] == '/') // 找到最后一个'/'，因此copy[0:off]代表一个目录
        copy[off--] = 0;

    if (strlen(copy) == 0) { // 就是该目录
        /* the path is root "/" */
        if (mode) {
            *mode = S_IFDIR;
        }
        obj_id = g_strdup(root_id);
        goto out;
    }

    slash = strrchr (copy, '/');
    if (!slash) { // 对象在该目录下
        base_dir = seaf_fs_manager_get_seafdir (mgr, repo_id, version, root_id);
        if (!base_dir) {
            seaf_warning ("Failed to find root dir %s.\n", root_id);
            g_set_error (error, SEAFILE_DOMAIN, SEAF_ERR_GENERAL, " ");
            goto out;
        }
        name = copy;
    } else { // 对象在相对路径下
        *slash = 0;
        name = slash + 1;
        GError *tmp_error = NULL;
        base_dir = seaf_fs_manager_get_seafdir_by_path (mgr,
                                                        repo_id,
                                                        version,
                                                        root_id,
                                                        copy,
                                                        &tmp_error); // 获取相对路径所在目录
        if (tmp_error &&
            !g_error_matches(tmp_error,
                             SEAFILE_DOMAIN,
                             SEAF_ERR_PATH_NO_EXIST)) {
            seaf_warning ("Failed to get dir for %s.\n", copy);
            g_propagate_error (error, tmp_error);
            goto out;
        }

        /* The path doesn't exist in this commit. */
        if (!base_dir) {
            g_propagate_error (error, tmp_error);
            goto out;
        }
    }

    for (p = base_dir->entries; p != NULL; p = p->next) { // 找到同名dirent
        dent = p->data;

        if (!is_object_id_valid (dent->id))
            continue;

        if (strcmp (dent->name, name) == 0) {
            obj_id = g_strdup (dent->id); // 目标id
            if (mode) {
                *mode = dent->mode;
            }
            break;
        }
    }

out:
    if (base_dir)
        seaf_dir_free (base_dir);
    g_free (copy);
    return obj_id;
}

char * // 根据相对路径获取seafile
seaf_fs_manager_get_seafile_id_by_path (SeafFSManager *mgr,
                                        const char *repo_id,
                                        int version,
                                        const char *root_id,
                                        const char *path,
                                        GError **error)
{
    guint32 mode;
    char *file_id;

    file_id = seaf_fs_manager_path_to_obj_id (mgr, repo_id, version,
                                              root_id, path, &mode, error);

    if (!file_id)
        return NULL;

    if (file_id && S_ISDIR(mode)) {
        g_free (file_id);
        return NULL;
    }

    return file_id;
}

char * // 根据相对路径获取seafile的id
seaf_fs_manager_get_seafdir_id_by_path (SeafFSManager *mgr,
                                        const char *repo_id,
                                        int version,
                                        const char *root_id,
                                        const char *path,
                                        GError **error)
{
    guint32 mode = 0;
    char *dir_id;

    dir_id = seaf_fs_manager_path_to_obj_id (mgr, repo_id, version,
                                             root_id, path, &mode, error);

    if (!dir_id)
        return NULL;

    if (dir_id && !S_ISDIR(mode)) {
        g_free (dir_id);
        return NULL;
    }

    return dir_id;
}

SeafDirent * // 根据路径获取seafdirent
seaf_fs_manager_get_dirent_by_path (SeafFSManager *mgr,
                                    const char *repo_id,
                                    int version,
                                    const char *root_id,
                                    const char *path,
                                    GError **error)
{
    SeafDirent *dent = NULL;
    SeafDir *dir = NULL;
    char *parent_dir = NULL;
    char *file_name = NULL;

    parent_dir  = g_path_get_dirname(path);
    file_name = g_path_get_basename(path);

    if (strcmp (parent_dir, ".") == 0) {
        dir = seaf_fs_manager_get_seafdir (mgr, repo_id, version, root_id);
        if (!dir) {
            g_set_error (error, SEAFILE_DOMAIN, SEAF_ERR_DIR_MISSING, "directory is missing");
        }
    } else
        dir = seaf_fs_manager_get_seafdir_by_path (mgr, repo_id, version,
                                                   root_id, parent_dir, error);

    if (!dir) {
        seaf_warning ("dir %s doesn't exist in repo %.8s.\n", parent_dir, repo_id);
        goto out;
    }

    GList *p;
    for (p = dir->entries; p; p = p->next) { // 遍历dirent，找到同名项
        SeafDirent *d = p->data;
        if (strcmp (d->name, file_name) == 0) {
            dent = seaf_dirent_dup(d);
            break;
        }
    }

out:
    if (dir)
        seaf_dir_free (dir);
    g_free (parent_dir);
    g_free (file_name);

    return dent;
}

static gboolean // 验证seafdir；版本0
verify_seafdir_v0 (const char *dir_id, const uint8_t *data, int len,
                   gboolean verify_id) // 是否验证SHA1
{
    guint32 meta_type;
    guint32 mode;
    char id[41];
    guint32 name_len;
    char name[SEAF_DIR_NAME_LEN];
    const uint8_t *ptr;
    int remain;
    int dirent_base_size;
    SHA_CTX ctx;
    uint8_t sha1[20];
    char check_id[41];

    if (len < sizeof(SeafdirOndisk)) {
        seaf_warning ("[fs mgr] Corrupt seafdir object %s.\n", dir_id);
        return FALSE;
    }

    ptr = data;
    remain = len;

    meta_type = get32bit (&ptr);
    remain -= 4;
    if (meta_type != SEAF_METADATA_TYPE_DIR) {
        seaf_warning ("Data does not contain a directory.\n");
        return FALSE;
    }

    if (verify_id)
        SHA1_Init (&ctx);

    dirent_base_size = 2 * sizeof(guint32) + 40;
    while (remain > dirent_base_size) { // 验证seafdirent
        mode = get32bit (&ptr);
        memcpy (id, ptr, 40);
        id[40] = '\0';
        ptr += 40;
        name_len = get32bit (&ptr);
        remain -= dirent_base_size;
        if (remain >= name_len) {
            name_len = MIN (name_len, SEAF_DIR_NAME_LEN - 1);
            memcpy (name, ptr, name_len);
            ptr += name_len;
            remain -= name_len;
        } else {
            seaf_warning ("Bad data format for dir objcet %s.\n", dir_id);
            return FALSE;
        }

        if (verify_id) {
            /* Convert mode to little endian before compute. */
            if (G_BYTE_ORDER == G_BIG_ENDIAN)
                mode = GUINT32_SWAP_LE_BE (mode);

            SHA1_Update (&ctx, id, 40);
            SHA1_Update (&ctx, name, name_len);
            SHA1_Update (&ctx, &mode, sizeof(mode));
        }
    }

    if (!verify_id)
        return TRUE;

    SHA1_Final (sha1, &ctx);
    rawdata_to_hex (sha1, check_id, 20);

    if (strcmp (check_id, dir_id) == 0)
        return TRUE;
    else
        return FALSE;
}

static gboolean // 验证json形式的seafile对象字节流
verify_fs_object_json (const char *obj_id, uint8_t *data, int len)
{
    guint8 *decompressed;
    int outlen;
    unsigned char sha1[20];
    char hex[41];

    if (seaf_decompress (data, len, &decompressed, &outlen) < 0) { // 解压
        seaf_warning ("Failed to decompress fs object %s.\n", obj_id);
        return FALSE;
    }

    calculate_sha1 (sha1, (const char *)decompressed, outlen); // 计算SHA1
    rawdata_to_hex (sha1, hex, 20);

    g_free (decompressed);
    return (strcmp(hex, obj_id) == 0); // 验证id
}

static gboolean // 给定id和字节流，验证seafdir
verify_seafdir (const char *dir_id, uint8_t *data, int len,
                gboolean verify_id, gboolean is_json)
{
    if (is_json)
        return verify_fs_object_json (dir_id, data, len);
    else
        return verify_seafdir_v0 (dir_id, data, len, verify_id);
}
                                        
gboolean // 验证seafdir
seaf_fs_manager_verify_seafdir (SeafFSManager *mgr,
                                const char *repo_id,
                                int version,
                                const char *dir_id,
                                gboolean verify_id,
                                gboolean *io_error)
{
    void *data;
    int len;

    if (memcmp (dir_id, EMPTY_SHA1, 40) == 0) {
        return TRUE;
    }

    if (seaf_obj_store_read_obj (mgr->obj_store, repo_id, version,
                                 dir_id, &data, &len) < 0) {
        seaf_warning ("[fs mgr] Failed to read dir %s:%s.\n", repo_id, dir_id);
        *io_error = TRUE;
        return FALSE;
    }

    gboolean ret = verify_seafdir (dir_id, data, len, verify_id, (version > 0));
    g_free (data);

    return ret;
}

static gboolean // 验证seafile；版本0
verify_seafile_v0 (const char *id, const void *data, int len, gboolean verify_id)
{
    const SeafileOndisk *ondisk = data;
    SHA_CTX ctx;
    uint8_t sha1[20];
    char check_id[41];

    if (len < sizeof(SeafileOndisk)) {
        seaf_warning ("[fs mgr] Corrupt seafile object %s.\n", id);
        return FALSE;
    }

    if (ntohl(ondisk->type) != SEAF_METADATA_TYPE_FILE) {
        seaf_warning ("[fd mgr] %s is not a file.\n", id);
        return FALSE;
    }

    int id_list_length = len - sizeof(SeafileOndisk);
    if (id_list_length % 20 != 0) {
        seaf_warning ("[fs mgr] Bad seafile id list length %d.\n", id_list_length);
        return FALSE;
    }

    if (!verify_id)
        return TRUE;

    SHA1_Init (&ctx);
    SHA1_Update (&ctx, ondisk->block_ids, len - sizeof(SeafileOndisk));
    SHA1_Final (sha1, &ctx);

    rawdata_to_hex (sha1, check_id, 20);

    if (strcmp (check_id, id) == 0)
        return TRUE;
    else
        return FALSE;
}

static gboolean // 根据id和字节流验证seafile
verify_seafile (const char *id, void *data, int len,
                gboolean verify_id, gboolean is_json)
{
    if (is_json)
        return verify_fs_object_json (id, data, len);
    else
        return verify_seafile_v0 (id, data, len, verify_id);
}

gboolean // 验证seafile
seaf_fs_manager_verify_seafile (SeafFSManager *mgr,
                                const char *repo_id,
                                int version,
                                const char *file_id,
                                gboolean verify_id,
                                gboolean *io_error)
{
    void *data;
    int len;

    if (memcmp (file_id, EMPTY_SHA1, 40) == 0) {
        return TRUE;
    }

    if (seaf_obj_store_read_obj (mgr->obj_store, repo_id, version,
                                 file_id, &data, &len) < 0) {
        seaf_warning ("[fs mgr] Failed to read file %s:%s.\n", repo_id, file_id);
        *io_error = TRUE;
        return FALSE;
    }

    gboolean ret = verify_seafile (file_id, data, len, verify_id, (version > 0));
    g_free (data);

    return ret;
}

static gboolean // 验证seafile对象；版本0
verify_fs_object_v0 (const char *obj_id,
                     uint8_t *data,
                     int len,
                     gboolean verify_id)
{
    gboolean ret = TRUE;

    int type = seaf_metadata_type_from_data (obj_id, data, len, FALSE);
    switch (type) {
    case SEAF_METADATA_TYPE_FILE:
        ret = verify_seafile_v0 (obj_id, data, len, verify_id);
        break;
    case SEAF_METADATA_TYPE_DIR:
        ret = verify_seafdir_v0 (obj_id, data, len, verify_id);
        break;
    default:
        seaf_warning ("Invalid meta data type: %d.\n", type);
        return FALSE;
    }

    return ret;
}

gboolean // 验证seafile对象（是验证seafdir、seafile的泛化）
seaf_fs_manager_verify_object (SeafFSManager *mgr,
                               const char *repo_id,
                               int version,
                               const char *obj_id,
                               gboolean verify_id,
                               gboolean *io_error)
{
    void *data;
    int len;
    gboolean ret = TRUE;

    if (memcmp (obj_id, EMPTY_SHA1, 40) == 0) {
        return TRUE;
    }

    if (seaf_obj_store_read_obj (mgr->obj_store, repo_id, version,
                                 obj_id, &data, &len) < 0) {
        seaf_warning ("[fs mgr] Failed to read object %s:%s.\n", repo_id, obj_id);
        *io_error = TRUE;
        return FALSE;
    }

    if (version == 0)
        ret = verify_fs_object_v0 (obj_id, data, len, verify_id);
    else
        ret = verify_fs_object_json (obj_id, data, len);

    g_free (data);
    return ret;
}

int // 获取seafdir版本（版本0返回0，其他版本返回头文件定义版本）
dir_version_from_repo_version (int repo_version)
{
    if (repo_version == 0)
        return 0;
    else
        return CURRENT_DIR_OBJ_VERSION;
}

int // 获取seafile版本
seafile_version_from_repo_version (int repo_version)
{
    if (repo_version == 0)
        return 0;
    else
        return CURRENT_SEAFILE_OBJ_VERSION;
}

int // 移除存储
seaf_fs_manager_remove_store (SeafFSManager *mgr,
                              const char *store_id)
{
    return seaf_obj_store_remove_store (mgr->obj_store, store_id);
}

GObject * // 根据相对路径，获取文件数量
seaf_fs_manager_get_file_count_info_by_path (SeafFSManager *mgr,
                                             const char *repo_id,
                                             int version,
                                             const char *root_id,
                                             const char *path,
                                             GError **error)
{
    char *dir_id = NULL;
    gint64 file_count = 0, dir_count = 0, size = 0;
    SeafileFileCountInfo *info = NULL;

    dir_id = seaf_fs_manager_get_seafdir_id_by_path (mgr,
                                                     repo_id,
                                                     version,
                                                     root_id,
                                                     path, NULL); // 根据相对路径获取seafdir
    if (!dir_id) {
        seaf_warning ("Path %s doesn't exist or is not a dir in repo %.10s.\n",
                      path, repo_id);
        g_set_error (error, SEAFILE_DOMAIN, SEAF_ERR_BAD_ARGS, "Bad path");
        goto out;
    }
    if (get_file_count_info (mgr, repo_id, version,
                             dir_id, &dir_count, &file_count, &size) < 0) { // 统计seafdir下文件数目
        seaf_warning ("Failed to get count info from path %s in repo %.10s.\n",
                      path, repo_id);
        goto out;
    }
    info = g_object_new (SEAFILE_TYPE_FILE_COUNT_INFO,
                         "file_count", file_count,
                         "dir_count", dir_count,
                         "size", size, NULL);
out:
    g_free (dir_id);

    return (GObject *)info;
}

static int // 递归查找文件
search_files_recursive (SeafFSManager *mgr,
                        const char *repo_id,
                        const char *path,
                        const char *id,
                        const char *str,
                        int version,
                        GList **file_list)
{
    SeafDir *dir;
    GList *p;
    SeafDirent *seaf_dent;
    int ret = 0;
    char *full_path = NULL;

    dir = seaf_fs_manager_get_seafdir (mgr, repo_id, version, id); // 获取seafdir
    if (!dir) {
        seaf_warning ("[fs-mgr]get seafdir %s failed\n", id);
        return -1;
    }

    for (p = dir->entries; p; p = p->next) {
        seaf_dent = (SeafDirent *)p->data;
        full_path = g_strconcat (path, "/", seaf_dent->name, NULL);

        if (seaf_dent->name && strcasestr (seaf_dent->name, str) != NULL) { // 不区分大小写，从文件名查找str
            SearchResult *sr = g_new0(SearchResult, 1);
            sr->path = g_strdup (full_path); // 记录文件路径（相对于根目录）
            sr->size = seaf_dent->size; // 记录文件大小
            sr->mtime = seaf_dent->mtime; // 记录文件最后修改时间
            *file_list = g_list_prepend (*file_list, sr); // 将查询结果添加到表中
            if (S_ISDIR(seaf_dent->mode)) {
                sr->is_dir = TRUE;
            }
        }

        if (S_ISDIR(seaf_dent->mode)) { // 递归查找
            if (search_files_recursive (mgr, repo_id, full_path,
                                        seaf_dent->id, str,
                                        version, file_list) < 0) {
                g_free (full_path);
                ret = -1;
                break;
            }
        }

        g_free (full_path);
    }

    seaf_dir_free (dir);
    return ret;
}

GList * // 搜索文件，返回结果列表
seaf_fs_manager_search_files (SeafFSManager *mgr,
                              const char *repo_id,
                              const char *str)
{
    GList *file_list = NULL;
    SeafCommit *head = NULL;

    SeafRepo *repo = seaf_repo_manager_get_repo (seaf->repo_mgr, repo_id); // 获取仓库
    if (!repo) {
        seaf_warning ("Failed to find repo %s\n", repo_id);
        goto out;
    }

    head = seaf_commit_manager_get_commit (seaf->commit_mgr,repo->id, repo->version, repo->head->commit_id); // 获取首次提交
    if (!head) {
        seaf_warning ("Failed to find commit %s\n", repo->head->commit_id);
        goto out;
    }

    search_files_recursive (mgr, repo_id, "", head->root_id, // 首次提交的seafdir
                            str, repo->version, &file_list);

out:
    seaf_repo_unref (repo);
    seaf_commit_unref (head);
    return file_list;
}
