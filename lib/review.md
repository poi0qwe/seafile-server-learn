# LIB 子文件夹源码总结

此部分代码涵盖了一些实用的功能及数据结构，涉及到数据库、线程管理、网络等常用功能，目的是对这些常用内容进行封装以降低耦合度。

以下是对c语言源码作出的简要分析，详细分析见。（vala部分的用途暂时未知；py文件给出了需要生成的RPC方法的参数与返回值类型，不可读）

## include

引用头文件、定义消息宏和Debug宏。

## bloom-filter

实现了布隆过滤器，包括计数布隆过滤器。

布隆过滤器通过比特向量和多次哈希的结果判断元素是否在集合中，而计数布隆过滤器只是将比特向量换成了计数器组。相较于哈希集合，布隆过滤器占用空间更小，而且不需要存储元素本身，更安全。

布隆过滤器的功能及原理详见：【[维基百科 - 布隆过滤器](https://zh.wikipedia.org/zh-cn/%E5%B8%83%E9%9A%86%E8%BF%87%E6%BB%A4%E5%99%A8)】。

对外提供的功能如下：

1. ### 创建

    ```c++
    Bloom *bloom_create (size_t size, int k, int counting);
    ```

    - size
        
        布隆过滤器单元数。对于非计数布隆过滤器，一个单元就是一个比特；对于计数布隆过滤器，一个单元就是一个计数器。

    - k

        这是一个常量，表示每个加入此过滤器的元素将影响多少个单元。（$0\lt k \le 4$）

    - counting

        一个布尔量，表示布隆过滤器是否是计数布隆过滤器。

    - @Return

        布隆过滤器指针。

    此函数将按照参数生成一个空的布隆过滤器，然后返回其指针。

2. ### 销毁

    ```c++
    int bloom_destroy (Bloom *bloom);
    ```

    - bloom

        布隆过滤器。

    - @Return

        恒为零。

    此函数将销毁布隆过滤器。

3. ### 增、删、检测

    ```c++
    int bloom_add (Bloom *bloom, const char *s);
    int bloom_remove (Bloom *bloom, const char *s);
    int bloom_test (Bloom *bloom, const char *s);
    ```

    - bloom

        布隆过滤器。

    - s

        字符串。

    - @Return

        增删中，-1表示出错，0表示成功；检测中，0表示不存在，1表示存在。

    这些函数实现了布隆过滤器的主要功能。其中哈希部分通过SHA256完成：先对$s$进行SHA256摘要，再截取前$k$个结果并选取对应的过滤器单元。增加元素就是把这些过滤器单元设为$1$（计数布隆过滤器则是加一）；删除元素则是设为$0$（计数布隆过滤器则是减一）；检测元素就是通过相同的过程选取$k$个过滤器单元，再判断这些单元是否都是$1$（计数布隆过滤器则是大于零）。

    值得一提的是在计数布隆过滤器中，若某个单元的数值大于等于$15$，则认为该单元已满，此后不再对该单元进行操作。


## job-mgr

实现了任务线程管理。其中有两个数据结构：任务、任务管理器。

任务中包括了所属的任务管理器指针、id、管道(用于发起完成事件)、用户任务函数、完成回调函数、用户数据(任务函数的参数)、运行结果(任务函数的返回值)。

任务管理器中包括了任务列表、线程池、历史任务数(用于生成任务id)。

1. ### 创建新的任务管理器

    ```c++
    CcnetJobManager *ccnet_job_manager_new (int max_threads);
    ```

    - max_threads

        最大线程数。

    - @Return

        任务管理器指针。

    创建任务列表（哈希表）、线程池。
    
    线程池的包装函数为job_thread_wrapper。线程池自动管理线程，每次在空闲线程上执行job_thread_wrapper，并安排线程池中的任务以参数的形式传递给它。

2. ### 增加新的任务

    ```c++
    typedef void* (*JobThreadFunc)(void *data);
    typedef void (*JobDoneCallback)(void *result);
    int
    ccnet_job_manager_schedule_job (CcnetJobManager *mgr,
                                JobThreadFunc func,
                                JobDoneCallback done_func,
                                void *data);
    ```

    - mgr

        任务管理器。

    - func

        用户任务函数。

    - done_func

        完成回调函数。

    - data

        用户数据。传递给用户任务函数。

    主要功能就是将任务加入到任务列表和线程池。最后使用libevent中的event_once去监听任务管道中是否有数据，表示完成。每个任务线程执行结束后，都将向任务管道写一个字节，表示发起完成事件。每次发起完成事件时，都将执行完成回调函数，并将任务的结果（即用户任务函数的返回值）传递给它。

3. ### 取消、等待任务

    取消任务尚未实现、等待任务只有实验性实现并未实际采用。

4. ### 销毁任务管理器

    ```c++
    void ccnet_job_manager_free (CcnetJobManager *mgr);
    ```

    - mgr

        任务管理器。

    释放任务管理器的任务列表、线程池、以及它本身所占用的空间。

## timer

基于libevent的evtimer，实现了定时器功能。功能等价于：

```c++
while (1) {
    if (func(user_data)) sleep(interval_milliseconds);
}
```

1. ## 创建新的定时器

    ```c++
    typedef int (*TimerCB) (void *data);
    CcnetTimer* ccnet_timer_new (TimerCB           func, 
                             void             *user_data,
                             uint64_t          timeout_milliseconds);
    ```

    - func

        用户函数。

    - user_data

        用户数据。作为用户函数的参数。

    - timeout_milliseconds

        间隔的毫秒数。

    创建定时器后，每`timeout_milliseconds`毫秒执行一次用户函数，直到用户函数返回$0$或该定时器被手动销毁。

    定时器是基于evtimer实现的，后者能在规定的时间间隔后发起事件。定时器通过evtimer实现了循环调用，使得用户函数能被反复以相同的时间间隔执行，且该过程非阻塞。

2. ## 销毁定时器

    ```c++
    void ccnet_timer_free (CcnetTimer **timer);
    ```

    - timer

        指向定时器指针的指针。

    此函数不仅释放定时器空间，还让定时器的指针指向NULL，用以停止定时器内的循环调用。

## db

对sqlite相关操作的封装。使用的是sqlite3的API。

1. ### 打开、关闭

    ```c++
    int sqlite_open_db (const char *db_path, sqlite3 **db); // 打开
    int sqlite_close_db (sqlite3 *db); // 关闭
    ```

    - db_path

        数据库链接。

    - db

        数据库。

2. ### SQL相关

    ```c++
    sqlite3_stmt *sqlite_query_prepare (sqlite3 *db, const char *sql); // 预编译sql
    int sqlite_query_exec (sqlite3 *db, const char *sql); // 执行sql
    int sqlite_begin_transaction (sqlite3 *db); // 开始事务
    int sqlite_end_transaction (sqlite3 *db); // 结束事务
    ```

    sqlite_query_prepare：预编译sql生成sqlite3_stmt预编译语句，优化sql语句并制定执行计划。

    sqlite_query_exec：直接执行sql，不进行预编译。执行结果被舍弃。

    sqlite_begin_transaction/sqlite_end_transaction：开始与结束事务。

3. ### 查询相关

    ```c++
    typedef gboolean (*SqliteRowFunc) (sqlite3_stmt *stmt, void *data);
    int
    sqlite_foreach_selected_row (sqlite3 *db, const char *sql, 
                                SqliteRowFunc callback, void *data); // 遍历每行
    ```

    - callback

        回调函数，对每行结果都执行一次。若返回零，则提前退出循环。

    - data

        用户数据，作为回调函数的参数。

    ```c++
    gboolean sqlite_check_for_existence (sqlite3 *db, const char *sql); // 检查sql结果集是否为空
    int sqlite_get_int (sqlite3 *db, const char *sql); // int单值查询
    gint64 sqlite_get_int64 (sqlite3 *db, const char *sql); // in64单值查询
    char *sqlite_get_string (sqlite3 *db, const char *sql); // 字符串单值查询
    ```

    sqlite_check_for_existence：检查sql结果集是否为空，为空则返回0，反之返回1.

    sqlite_get_int/sqlite_get_int64/sqlite_get_string：获取单值查询的结果。

## net

socket网络编程相关。

1. ### TCP

    ```c++
    evutil_socket_t ccnet_net_open_tcp (const struct sockaddr *sa, int nonblock); // 客户端打开tcp连接
    evutil_socket_t ccnet_net_bind_tcp (int port, int nonblock); // 服务端绑定端口
    evutil_socket_t ccnet_net_accept (evutil_socket_t b, 
                                  struct sockaddr_storage *cliaddr,
                                  socklen_t *len, int nonblock); // 服务端接收连接
    evutil_socket_t ccnet_net_bind_v4 (const char *ipaddr, int *port); // 客户端绑定'ipaddr:port'
    ```

    - sockaddr/sockaddr_in/sockaddr_in6/sockaddr_storage

        都存储了IP地址与端口信息，是两者的整合。

    上述操作都是以面向连接的方式创建socket。（SOCK_STREAM）

2. ### UDP

    ```c++
    evutil_socket_t udp_client (const char *host, const char *serv,
                struct sockaddr **saptr, socklen_t *lenp); // 客户端打开udp连接('host:serv')
    int mcast_set_loop(evutil_socket_t sockfd, int onoff); // 多播循环开关
    evutil_socket_t create_multicast_sock (struct sockaddr *sasend, socklen_t salen); // 多播客户端连接多播服务端
    ```

    上述操作都是以无连接的方式创建scoket。（SOCK_DGRAM）

3. ### 实用函数

    ```c++
    int ccnet_net_make_socket_blocking (evutil_socket_t fd); // 使socket称为阻塞式
    int ccnet_netSetTOS ( evutil_socket_t s, int tos ); // 设置TOS
    char *sock_ntop(const struct sockaddr *sa, socklen_t salen); // 从sockaddr中获取ip地址
    uint16_t sock_port (const struct sockaddr *sa); // 从sockaddr中获取端口
    int is_valid_ipaddr (const char *addr_str); // 判断ip是否有效
    int sock_pton (const char *addr_str, uint16_t port, 
               struct sockaddr_storage *sa); // 通过ip地址和端口得到sockaddr_storage
    ```

## utils

使用函数集。由于函数太多，只作笼统概述。

1. ### 管道、读写

    ```c++
    #define ccnet_pipe_t int // 定义ccnet管道的id类型
    #define ccnet_pipe(a) pipe((a)) // 定义ccnet管道
    #define piperead(a,b,c) read((a),(b),(c)) // 定义管道读函数
    #define pipewrite(a,b,c) write((a),(b),(c)) // 定义管道写函数
    #define pipeclose(a) close((a)) // 定义管道关闭函数
    #define pipereadn(a,b,c) recvn((a),(b),(c)) // 定义管道读n字节函数
    #define pipewriten(a,b,c) sendn((a),(b),(c)) // 定义管道写n字节函数

    // 从seafile中操作n个bit
    ssize_t	readn(int fd, void *vptr, size_t n);
    ssize_t writen(int fd, const void *vptr, size_t n);

    // 从socket中操作n个bit
    ssize_t	recvn(evutil_socket_t fd, void *vptr, size_t n);
    ssize_t sendn(evutil_socket_t fd, const void *vptr, size_t n);
    ```

2. ### Seafile

    ```c++
    #define SeafStat struct stat // 定义seafile状态结构体
    int seaf_stat (const char *path, SeafStat *st); // 根据seafile的路径获取状态
    int seaf_fstat(int fd, SeafStat *st);           // 根据seafile的文件描述符获取状态
    int seaf_set_file_time (const char *path, guint64 mtime); // 设置seafile文件最后修改时间
    
    int seaf_util_unlink (const char *path); // 删除seafile文件
    int seaf_util_rmdir (const char *path); // 删除seafile目录
    int seaf_util_mkdir (const char *path, mode_t mode); // 创建seafile目录
    int seaf_util_open (const char *path, int flags); // 打开seafile文件
    int seaf_util_create (const char *path, int flags, mode_t mode); // 创建seafile文件
    int seaf_util_rename (const char *oldpath, const char *newpath); // 重命名seafile文件
    gboolean seaf_util_exists (const char *path); // 判断seafile文件是否存在
    gint64 seaf_util_lseek (int fd, gint64 offset, int whence); // 改变seafile文件读写指针位置
    
    int copy_fd (int ifd, int ofd); // 复制seafile，根据文件描述符
    int copy_file (const char *dst, const char *src, int mode); // 复制seafile，根据路径
    ```

3. ### 字符串

    ```c++
    size_t ccnet_strlcpy (char *dst, const char *src, size_t size); // 进行字符串复制

    void rawdata_to_hex (const unsigned char *rawdata, char *hex_str, int n_bytes); // 字符串转十六进制串
    int hex_to_rawdata (const char *hex_str, unsigned char *rawdata, int n_bytes); // 十六进制串转字符串

    #define sha1_to_hex(sha1, hex) rawdata_to_hex((sha1), (hex), 20) // 定义sha1转十六进制函数，其中sha1取20位
    #define hex_to_sha1(hex, sha1) hex_to_rawdata((hex), (sha1), 20) // 定义十六进制转sha1函数，其中十六进制取20位

    int calculate_sha1 (unsigned char *sha1, const char *msg, int len); // 字符串转sha1
    int ccnet_sha1_equal (const void *v1, const void *v2); // 进行sha1对比
    unsigned int ccnet_sha1_hash (const void *v); //sha1哈希（sha1串映射为int64）

    char* gen_uuid (); // 生成uuid
    void gen_uuid_inplace (char *buf); // 生成uuid并赋值给buf
    gboolean is_uuid_valid (const char *uuid_str); // 判断uuid是否有效

    char** strsplit_by_char (char *string, int *length, char c); // 根据字符分割字符串
    char * strjoin_n (const char *seperator, int argc, char **argv); // 连接字符串
    int is_ipaddr_valid (const char *ip); // 检查ip地址是否有效

    gboolean is_empty_string (const char *str); // 字符串判空

    // 字符串键值对
    typedef void (*KeyValueFunc) (void *data, const char *key, char *value); // 定义键值对操作函数
    void parse_key_value_pairs (char *string, KeyValueFunc func, void *data); // 从string提取key和value，然后传递到上述函数，data用于存放结果
    typedef gboolean (*KeyValueFunc2) (void *data, const char *key,
                                   const char *value);
    void parse_key_value_pairs2 (char *string, KeyValueFunc2 func, void *data); // 同上，区别在于此处值只读；并且若func返回0则直接终止

    // 字符串列表
    GList *string_list_append (GList *str_list, const char *string); // 字符串列表增加一个字符串
    GList *string_list_append_sorted (GList *str_list, const char *string); // 字符串列表排序
    GList *string_list_remove (GList *str_list, const char *string); // 字符串列表删除一个
    void string_list_free (GList *str_list); // 清空字符串列表
    gboolean string_list_is_exists (GList *str_list, const char *string); // 判断字符串是否在列表中
    void string_list_join (GList *str_list, GString *strbuf, const char *seperator); // 字符串列表连接成新字符串
    GList *string_list_parse (const char *list_in_str, const char *seperator); // 根据分隔符切割然后生成字符串列表
    GList *string_list_parse_sorted (const char *list_in_str, const char *seperator); // 同上并排序
    gboolean string_list_sorted_is_equal (GList *list1, GList *list2); // 判断两个列表是否相同

    // 字符串数组
    char** ncopy_string_array (char **orig, int n); // 字符串数组复制前n个
    void nfree_string_array (char **array, int n); // 字符串数组释放前n个

    // 语言&编码
    char *ccnet_locale_from_utf8 (const gchar *src); // utf-8转本地语言编码
    char *ccnet_locale_to_utf8 (const gchar *src); // 本地语言编码转utf-8
    void clean_utf8_data (char *data, int len); // 将非utf-8字符转化为'?'
    char * normalize_utf8_path (const char *path); // 将路径字符串转为utf-8

    // 配置文件
    char * seaf_key_file_get_string (GKeyFile *key_file,
                                    const char *group,
                                    const char *key,
                                    GError **error); // 配置文件取值，并记录错误
    gchar* ccnet_key_file_get_string (GKeyFile *keyf,
                                  const char *category,
                                  const char *key); // 配置文件取值，并删除末尾的空格

    char* format_dir_path (const char *path); // 目录路径格式化

    gboolean is_permission_valid (const char *perm); // 判断权限是否有效（规定'r'和'rw'有效）
    ```

1. ### CCNET

    ```c++
    gboolean is_object_id_valid (const char *obj_id); // 判断ccent对象是否有效

    int checkdir (const char *dir); // 检查目录是否存在
    int checkdir_with_mkdir (const char *path); // 检查目录是否存在，若不存在则创建
    char* ccnet_expand_path (const char *src); // ccnet路径规范化

    int objstore_mkdir (const char *base); // 创建ccnet对象存储目录
    void objstore_get_path (char *path, const char *base, const char *obj_id); // 给定ccnet对象路径base，以及ccnet对象的id，得到它的存储路径并存储到path中

    char* ccnet_object_type_from_id (const char *object_id); // 根据id获取ccnet对象
    gint64 ccnet_calc_directory_size (const char *path, GError **error); // 获取ccnet对象目录的大小
    ```

2. ### 时间
    
    ```c++
    struct timeval timeval_from_msec (uint64_t milliseconds); // ms转timeval
    gint64 get_current_time(); // 获取系统时间
    ```

3. ### 加密解密

    ```c++
    int
    ccnet_encrypt (char **data_out,
                int *out_len,
                const char *data_in,
                const int in_len,
                const char *code,
                const int code_len);
    int
    ccnet_decrypt (char **data_out,
                int *out_len,
                const char *data_in,
                const int in_len,
                const char *code,
                const int code_len);
    ```

4. ### 网络

    ```c++
    static inline uint64_t bswap64 (uint64_t val); // 大小端互换
    static inline uint64_t hton64(uint64_t val); // 主机字节序转网络字节序
    static inline uint64_t ntoh64(uint64_t val); // 网络字节序转主机字节序
    static inline void putxbit(uint8_t **ptr, uintX_t val); // 写入Xbit
    static inline uintX_t getxbit(uint8_t **ptr); // 读出Xbit
    ```

5. ### 进程

    ```c++
    gboolean process_is_running(const char *name); // 根据进程名判断进程是否正在运行
    int count_process (const char *process_name_in); // 获取某进程正在运行的实例的数目
    ```

6. ### json

    ```c++
    const char * json_object_get_string_member (json_t *object, const char *key); // object[key] -> str
    gboolean json_object_has_member (json_t *object, const char *key); // object[key] != null
    gint64 json_object_get_int_member (json_t *object, const char *key); // object[key] -> int
    void json_object_set_string_member (json_t *object, const char *key, const char *value); // object[key] = (str)
    void json_object_set_int_member (json_t *object, const char *key, gint64 value); // object[key] = (int)
    ```

7.  ### 压缩解压缩

    ```c++
    int seaf_compress (guint8 *input, int inlen, guint8 **output, int *outlen); // 压缩seafile文件数据
    int seaf_decompress (guint8 *input, int inlen, guint8 **output, int *outlen); // 解压缩seafile文件数据
    ```

8.  ### 其他