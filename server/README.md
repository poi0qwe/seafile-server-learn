# Seafile Server Core

SeafileSeverCore是seafile-server中的核心部分，提供了基于common各种子系统操作下的各种高级服务。这些服务在[server](https://github.com/poi0qwe/seafile-server-learn/blob/main/server/)下被定义与实现，最终被集成到了[seaf-server](https://github.com/poi0qwe/seafile-server-learn/blob/main/server/seaf-server.c)和[http-server](https://github.com/poi0qwe/seafile-server-learn/blob/main/server/http-server.h)中。前者是基于RPC协议进行服务，后者是基于Http协议进行服务。

## Seaf-Server (RPC)

Seaf-Server是一个基于RPC的服务器服务，是一个后台运行的进程。这里RPC是基于另外一个库[libsearpc](https://github.com/dreamsxin/libsearpc)，通过引用[libsearpc/lib](https://github.com/dreamsxin/libsearpc/tree/master/lib)中的库内容实现的。这个库的主要内容就是实现一个RPC协议，包括注册服务、类型转化、传输等。这部分内容具体请看[山大智云源码分析 by lzh](https://blog.csdn.net/Minori_wen/article/details/120810955?ops_request_misc=&request_id=&biz_id=102&utm_term=%E5%B1%B1%E5%A4%A7%E6%99%BA%E4%BA%91&utm_medium=distribute.pc_search_result.none-task-blog-2~all~sobaiduweb~default-4-120810955.first_rank_v2_pc_rank_v29&spm=1018.2226.3001.4187)及其相关系列。

RPC的服务内容基本涉及了Seafile服务端所涉及的所有方面。而Http服务除了协议本身和并发服务外，其余只是这些服务的一个子集：

![](https://img-blog.csdnimg.cn/e9b5b2bcbc2947f3abc3d353b9a3089e.png?x-oss-process=image/watermark,type_d3F5LXplbmhlaQ,shadow_50,text_Q1NETiBAY3lrMDYyMA==,size_20,color_FFFFFF,t_70,g_se,x_16)


### 服务注册
```cpp
static void start_rpc_service (const char *seafile_dir, const char *rpc_pipe_path) {
    SearpcNamedPipeServer *rpc_server = NULL;
    char *pipe_path = NULL;

    searpc_server_init (register_marshals);

    searpc_create_service ("seafserv-threaded-rpcserver");

    /* threaded services */

    /* repo manipulation */
    searpc_server_register_function ("seafserv-threaded-rpcserver",
                                     seafile_get_repo,
                                     "seafile_get_repo",
                                     searpc_signature_object__string());
    //...
    if (rpc_pipe_path) {
        pipe_path = g_build_path ("/", rpc_pipe_path, SEAFILE_RPC_PIPE_NAME, NULL);
    } else {
        pipe_path = g_build_path ("/", seafile_dir, SEAFILE_RPC_PIPE_NAME, NULL);
    }
    rpc_server = searpc_create_named_pipe_server_with_threadpool (pipe_path, NAMED_PIPE_SERVER_THREAD_POOL_SIZE);

    g_free(pipe_path);
    if (!rpc_server) {
        seaf_warning ("Failed to create rpc server.\n");
        exit (1);
    }

    if (searpc_named_pipe_server_start(rpc_server) < 0) {
        seaf_warning ("Failed to start rpc server.\n");
        exit (1);
    }
}
```

首先通过libsearpc创建新的RPC服务，名为“seafserv-threaded-rpcserver”，然后开始注册服务（一共有211个服务）。然后获取RPC管道路径。管道即RPC向外界通信的媒介。接着以此生成一个新的带线程池的RPC服务器。最后启动RPC服务器。

服务注册方法如下：

```cpp
searpc_server_register_function("seafserv-threaded-rpcserver",      // 服务器名
                                seafile_get_repo,                   // 服务函数
                                "seafile_get_repo",                 // 服务名
                                searpc_signature_object__string()); // 服务参数类型列表
```

### 服务内容

RPC中提供了两百多种服务，主要涉及如下数个方面：

1. 基本仓库操作
2. 用户仓库分享
3. 组内仓库分享
4. 分支与提交
5. 虚拟仓库
6. 清理垃圾
7. 令牌
8. 仓库复制
9. 密码管理
10. 配额管理
11. 仓库权限管理
12. 事件
13. 设置仓库历史保留
14. 设置系统默认仓库
15. 垃圾仓库管理
16. 配置管理
17. 用户管理
18. 组管理
19. 集群管理

这些服务既涵盖了common下的子系统中的基本操作，也有server-core下实现的更高级的操作。

### 参数列表

|参数|含义|
|:-:|:-|
|`h`|获取帮助|
|`v`|获取版本|
|`c`|设置ccnet目录|
|`d`|设置seafile目录|
|`F`|设置中央配置文件目录|
|`f`|是否是守护进程|
|`l`|设置日志文件路径|
|`D`|设置debug标志位|
|`P`|设置pid文件路径，与并发相关|
|`p`|设置管道路径|
|默认|获取帮助|

其中守护进程是利用`deamon`函数在后台运行此后的步骤。它独立于控制终端并且周期性地执行某种任务或等待处理某些发生的事件。

pid文件被用于seaf-controller和seafhub等其他关联进程中，用于定位到seaf-server进程。

其他文件在common下均有介绍，被各个子系统所依赖。

### 服务启动步骤

1. 是否deamon

    ```cpp
    #ifndef WIN32
        if (daemon_mode) {
    #ifndef __APPLE__
            daemon (1, 0);
    #else   /* __APPLE */
            /* daemon is deprecated under APPLE
            * use fork() instead
            * */
            switch (fork ()) {
            case -1:
                seaf_warning ("Failed to daemonize");
                exit (-1);
                break;
            case 0:
                /* all good*/
                break;
            default:
                /* kill origin process */
                exit (0);
            }
    #endif  /* __APPLE */
        }
    #endif /* !WIN32 */
    ```

    deamon在不同的操作系统中有不同的启动方式。主要是在Linux和苹果系统中存在守护进程，Windows则没有。Linux的实现方式最为直接，而苹果系统则是直接fork。

2. cdc初始化

    ```cpp
    cdc_init ();
    ```
    块子系统的初始化。其中主要就是对拉宾指纹算法的预处理。

3. 日志初始化

    ```cpp
    if (!debug_str)
        debug_str = g_getenv("SEAFILE_DEBUG");
    seafile_debug_set_flags_string (debug_str);
    ```
    首先是设置Debug标志位过滤Debug内容，具体内容在log.c中。

    ```cpp
    if (seafile_dir == NULL)
        seafile_dir = g_build_filename (ccnet_dir, "seafile", NULL);
    if (logfile == NULL)
        logfile = g_build_filename (seafile_dir, "seafile.log", NULL);

    if (seafile_log_init (logfile, "info", "debug") < 0) {
        seaf_warning ("Failed to init log.\n");
        exit (1);
    }
    ```

    然后获取默认日志文件路径，最后对日志系统进行初始化。

4. 事件初始化

    ```cpp
    event_init ();
    ```
    libevent库中事件初始化。libevent的事件是timer、job-mgr等与时间相关的组件的基础。

5. 开始RPC服务

    ```cpp
    start_rpc_service (seafile_dir, rpc_pipe_path);
    ```

    新建RPC服务，注册服务，然后启动。

6. seafile会话

    ```cpp
    seaf = seafile_session_new (central_config_dir, seafile_dir, ccnet_dir);
    if (!seaf) {
        seaf_warning ("Failed to create seafile session.\n");
        exit (1);
    }
    ```

    创建新的seafile会话。seafile会话在FUSE中详细介绍过，其中包括了对各个子系统管理器的初始化，以及一些关键句柄。

7. pidfile

    ```cpp
    static int
    write_pidfile (const char *pidfile_path)
    {
        if (!pidfile_path)
            return -1;

        pid_t pid = getpid();

        FILE *pidfile = g_fopen(pidfile_path, "w");
        if (!pidfile) {
            seaf_warning ("Failed to fopen() pidfile %s: %s\n",
                        pidfile_path, strerror(errno));
            return -1;
        }

        char buf[32];
        snprintf (buf, sizeof(buf), "%d\n", pid);
        if (fputs(buf, pidfile) < 0) {
            seaf_warning ("Failed to write pidfile %s: %s\n",
                        pidfile_path, strerror(errno));
            fclose (pidfile);
            return -1;
        }

        fflush (pidfile);
        fclose (pidfile);
        return 0;
    }

    if (pidfile) {
        if (write_pidfile (pidfile) < 0) {
            ccnet_message ("Failed to write pidfile\n");
            return -1;
        }
    }
    ```

    将当前进程的pid写入到pid文件。方便其它进程定位自己。
    
    到这个地方时存在一个隐患。首先seaf-controller必须通过pidfile得知server已启动，但是前一步Seafile会话的创建中，存在数据库连接。如果是远程数据库连接，则可能存在超时，这时会导致controller误认为seaf-server未启动，从而重启。

8. 默认系统目录

    ```cpp
    schedule_create_system_default_repo (seaf);
    ```
    
9. 事件分配

    ```cpp
    event_dispatch ();
    ```

    开始分配事件。通过libevent内部实现可知：

    ```cpp
    int event_dispatch(void) {
        return (event_loop(0));
    }
    ```

    实际上就是开启主循环进行事件响应。

10. 退出

    ```cpp
    atexit (on_seaf_server_exit);
    ```

    相当于设置回调函数。`on_seaf_server_exit`中的主要内容就是移除之前的pid文件。

## Http-Server (Http)

基于libevhtp实现的Http服务，是一个接口，通过HttpServerStruct在SeafileSession中被间接使用。

它与RPC服务、Seafile会话的关系如下：
![](https://img-blog.csdnimg.cn/299de845b0e34e86b02329d8027a9199.png?x-oss-process=image/watermark,type_d3F5LXplbmhlaQ,shadow_50,text_Q1NETiBAY3lrMDYyMA==,size_14,color_FFFFFF,t_70,g_se,x_16)
Http服务既能被外界直接使用，也能与内部通过一些通信技术（如后文涉及的消息队列）相互通信。

（Http服务中的一部分内容不被开源，故只能推测）

### 服务内容

|服务名|方法|请求数据|操作|返回数据类型|url|
|:-:|:-:|:-|:-|:-:|:-|
|GET_PROTO_PATH|GET|-|返回协议版本|状态码|`/protocol-version`|
|OP_PERM_CHECK_REGEX|GET|仓库id|检查仓库对客户端的许可|状态码|`/repo/[repo_id]/permission-check/.*`|
|HEAD_COMMIT_OPER|GET/PUT|仓库id|获取仓库的配额|整型|`/repo/[repo_id]/quota-check/.*`|
|GET_CHECK_QUOTA|GET|仓库id、提交id|GET则返回head提交内容、PUT则发送head提交内容|字节流|`/repo/[repo_id]/commit/HEAD`|
|GET_HEAD_COMMITS_MULTI|GET|仓库id表、提交id|获取这些仓库对应的head提交id|json列表|`/repo/head-commits-multi`|
|COMMIT_OPER|GET/PUT|仓库id、提交id|GET则返回提交内容、PUT则发送提交内容|字节流|`/repo/[repo_id]/commit/[commid_id]`|
|PUT_COMMIT_INFO|PUT|仓库id、提交id|上传提交的json对象，表示提交|json对象|`/repo/[repo_id]/commit/[commid_id]`|
|GET_FS_OBJ_ID_ID|PUT|仓库id、用户token|返回文件系统列表，前提是双端无差异|json列表|`/repo/[repo_id]/fs-id-list/.*`|
|START_FS_OBJ_ID|PUT|仓库id|生成新的用户token，开启文件系统服务|json对象|`/repo/[repo_id]/start-fs-id-list/.*`|
|QUERY_FS_OBJ_ID_REGEX|PUT|仓库id、用户token|返回是否用户token是否存在|json对象|`/repo/[repo_id]/query-fs-id-list/.*`|
|RETRIEVE_FS_OBJ_ID_REGEX|PUT|仓库id、用户token|返回token对应的文件对象id|json列表|`/repo/[repo_id]/retrieve-fs-id-list/*`|
|BLOCK_OPER|GET/PUT|仓库id、块id|GET则返回块内容、PUT则发送块内容|字节流|`/repo/[repo_id]/block-map/[block_id]`|
|POST_CHECK_FS_REGEX|POST|仓库id、文件对象id表|返回文件对象存在性真值表|json列表|`/repo/[repo_id]/check-fs`|
|POST_CHECK_BLOCK_REGEX|POST|仓库id、块id表|返回块存在性真值表|json列表|`/repo/[repo_id]/check-blocks`|
|POST_RECV_FS_REGEX|POST|仓库id|上传文件系统对象内容|字节流|`/repo/[repo_id]/recv-fs`|
|POST_PACK_FS_REGEX|POST|仓库id、文件系统对象id表单|返回文件系统对象内容|字节流|`/repo/[repo_id]/pack-fs`|
|GET_BLOCK_MAP|GET|仓库id、seafile文件id|返回各个块的大小|json列表|`/repo/[repo_id]/block-map/[file_id]`|

### 路由

对每个服务定义了一个路由，路由中包含一些参数，主要就是各种id。

回顾一下id的生成方式，seafile中统一规定：将SHA1的前20个字符转化为16进制串最为id。每个字符范围是0~255，故一个字符以两个HEX表示，因此总共是40个十六进制字符。

仓库id较为特殊，在[repo-mgr](https://github.com/poi0qwe/seafile-server-learn/blob/main/server/repo-mgr.c)中规定为uuid，所以是32位十六进制字符。

1. 仓库id

```cpp
"^/repo/[\\da-z]{8}-[\\da-z]{4}-[\\da-z]{4}-[\\da-z]{4}-[\\da-z]{12}/permission-check/.*"
```

其中的`[\\da-z]{8}-[\\da-z]{4}-[\\da-z]{4}-[\\da-z]{4}-[\\da-z]{12}`即仓库id，为32位uuid，只可能出现数字和小写字母（十六进制），并以8-4-4-4-12的形式被`-`分割。

2. 文件id、块id、提交id

```cpp
"^/repo/[\\da-z]{8}-[\\da-z]{4}-[\\da-z]{4}-[\\da-z]{4}-[\\da-z]{12}/commit/[\\da-z]{40}"
```

其中的`[\\da-z]{40}`即这些id，都是40位HEX形式下的SHA1摘要，且只可能出现数字和小写字母（十六进制）。

### 服务注册

服务注册集中在一个初始化方法中：

```cpp
static void
http_request_init (HttpServerStruct *server)
{
    HttpServer *priv = server->priv;

    evhtp_set_cb (priv->evhtp,
                  GET_PROTO_PATH, get_protocol_cb,
                  NULL);

    evhtp_set_regex_cb (priv->evhtp,
                        GET_CHECK_QUOTA_REGEX, get_check_quota_cb,
                        priv);

    //...
}
```

该部分使用libevthp来定义一个url路径，转发到相应的函数。


### Http服务器

Http服务是内置到Seafile会话中的一个接口，伴随会话的起止而开启与停止。

```cpp
struct _HttpServerStruct {
    struct _SeafileSession *seaf_session;

    struct _HttpServer *priv;

    char *bind_addr;                    // 绑定地址
    int bind_port;                      // 绑定端口
    char *http_temp_dir;                // 临时目录
    char *windows_encoding;             // ZIP编码
    gint64 fixed_block_size;            // 分块大小，默认8MB
    int web_token_expire_time;          // 令牌过期时间
    int max_indexing_threads;           // 最大索引线程数
    int worker_threads;                 // 工作线程数
    int max_index_processing_threads;   // 最大索引处理线程数
    int cluster_shared_temp_file_mode;  // 集群共享临时文件模式
};
```

向外提供的服务有以下几个：

```cpp
HttpServerStruct *seaf_http_server_new (struct _SeafileSession *session);
int seaf_http_server_start (HttpServerStruct *htp_server);
int seaf_http_server_invalidate_tokens (HttpServerStruct *htp_server, const GList *tokens);
void send_statistic_msg (const char *repo_id, char *user, char *operation, guint64 bytes);
```

它们分别是：新建Http服务器、开启服务器、清除令牌、发送静态消息。

### 高并发

Http服务器存在一个主要的问题，就是高并发。已知在libevthp中，每产生一个请求都会生成一个线程，因此请求是并发的。为了防止线程过多而拥塞，`worker_threads`限制了最大线程数。

基于多线程，seafile中通过以下几种策略来支持高并发：

1. 令牌缓存查询：哈希表+锁

    Http服务实现中有一个很常见的部分，那就是利用局部性原理进行缓存。所有的缓存及其锁都定义在一个全局结构体中，它就是HttpSeverStruct的priv私有域：

    ```cpp
    struct _HttpServer {
        evbase_t *evbase;
        evhtp_t *evhtp;
        pthread_t thread_id;

        GHashTable *token_cache; // 令牌表
        pthread_mutex_t token_cache_lock; /* token -> username */

        GHashTable *perm_cache; // 许可表
        pthread_mutex_t perm_cache_lock; /* repo_id:username -> permission */

        GHashTable *vir_repo_info_cache; // 虚拟仓库信息表
        pthread_mutex_t vir_repo_info_cache_lock;

        // ...

        GHashTable *fs_obj_ids; // 文件系统id表
        pthread_mutex_t fs_obj_ids_lock;
    };
    ```
    
    以使用最频繁的文件系统查询为例，其中的一个核心代码：

    ```cpp
    pthread_mutex_lock (&htp_server->fs_obj_ids_lock); // 线程锁
    result = g_hash_table_lookup (htp_server->fs_obj_ids, token);
    if (!result) {
        pthread_mutex_unlock (&htp_server->fs_obj_ids_lock);
        evhtp_send_reply (req, EVHTP_RES_NOTFOUND);
        goto out;
    } else {
        if (!result->done) {
            json_object_set_new (obj, "success", json_false());
        } else {
            json_object_set_new (obj, "success", json_true());
        }
    }
    pthread_mutex_unlock (&htp_server->fs_obj_ids_lock);
    ```

    通过哈希表`fs_obj_ids`能直接查询到文件的存在性。而锁的时间显然比递归子树快得多，因此可通过此方式提速。

    其他的频繁操作也用到了这种方法，例如虚拟仓库信息缓存、许可缓存、令牌缓存。

2. 计算线程池

    ```cpp
    event_t *reap_timer;
    GThreadPool *compute_fs_obj_id_pool;
    ```

    _HttpServer中还包含了一个线程池，用于处理计算密集型任务，例如维护所有文件系统id。`START_FS_OBJ_ID`其中的一个子任务就是创建这样一个线程池，为的是持久同步各种文件哈希表。

3. 借助数据库

    对于那些非频繁检查的项，可以使用数据库；并且数据库自带锁，所以支持并发。

    这些操作例如：列举仓库id、列举提交id、首次验证许可等。

4. 转发任务：消息队列

    （以下内容为推测，因为存在这样的特殊使用，但相关具体代码没有开源）

    有些服务与SeafileServer的RPC服务相重叠，为了避免耦合，所以转发至RPC服务。显然并发环境下不能直接转发，所以需要使用消息队列。

    这样的转发有两个类型：

    ```cpp
    static void
    publish_repo_event (RepoEventData *rdata) // 仓库事件
    {
        GString *buf = g_string_new (NULL);
        g_string_printf (buf, "%s\t%s\t%s\t%s\t%s\t%s",
                        rdata->etype, rdata->user, rdata->ip,
                        rdata->client_name ? rdata->client_name : "",
                        rdata->repo_id, rdata->path ? rdata->path : "/");

        seaf_mq_manager_publish_event (seaf->mq_mgr, SEAFILE_SERVER_CHANNEL_EVENT, buf->str);

        g_string_free (buf, TRUE);
    }

    static void
    publish_stats_event (StatsEventData *rdata) // 状态事件
    {
        GString *buf = g_string_new (NULL);
        g_string_printf (buf, "%s\t%s\t%s\t%"G_GUINT64_FORMAT,
                        rdata->etype, rdata->user,
                        rdata->repo_id, rdata->bytes);

        seaf_mq_manager_publish_event (seaf->mq_mgr, SEAFILE_SERVER_CHANNEL_STATS, buf->str);

        g_string_free (buf, TRUE);
    }
    ```

    第一个是声明一个仓库事件，第二个是声明一个状态事件。声明后，事件被以字符串形式传递给相关通道，通道即消息队列。最后事件被另一端RPC服务通过如下方法弹出：

    ```cpp
    json_t *
    seafile_pop_event(const char *channel, GError **error)
    {
        if (!channel) {
            g_set_error (error, SEAFILE_DOMAIN, SEAF_ERR_BAD_ARGS, "Argument should not be null");
            return NULL;
        }
        return seaf_mq_manager_pop_event (seaf->mq_mgr, channel);
    }
    ```

    返回的json串最后经由对应的方法进行处理。
