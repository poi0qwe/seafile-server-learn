# GC服务

## GC 垃圾回收

完成此项工作的相关源码：[gc-core](https://github.com/poi0qwe/seafile-server-learn/blob/main/server/gc/gc-core.c)、[verify](https://github.com/poi0qwe/seafile-server-learn/blob/main/server/gc/verify.c)、[repo-mgr](https://github.com/poi0qwe/seafile-server-learn/blob/main/server/gc/repo-mgr.c)

### 仓库的生效期、最大历史保留时间 

为了避免过多的历史版本以节省仓库的存储空间，允许用户设置仓库生效期和最大历史保留时间。

- 仓库生效期

    描述仓库从何时起考试生效。在该时间后的仓库提交为有效提交版本；在该时间前的版本将被自动删除。这个量用关系`RepoHistoryLimit`维护。

- 最大历史保留时间

    描述仓库内历史数据的最大保留时间。超过历史保留时间的版本将被自动删除。这个量用关系`RepoValidSince`维护。

- 维护截止时间

    这是一个导出量，由前两个量派生。其派生规则如下：

    |优先级|条件|维护截止时间|描述|
    |:-:|:-|:-|:-|
    |3|最大历史保留时间>0|=max(当前时间-最大历史保留时间, 仓库生效期)|主以最大历史保留时间，辅以仓库生效期|
    |2|仓库生效期>0|=仓库生效期|以仓库生效期维护|
    |1|=0|仅维护head分支|

    维护截止时间的大小也会影响维护对象的范畴，规则如下：

    |数值|描述|
    |:-|:-|
    |$>0$|仅维护截止时间后的版本|
    |$=0$|仅维护head分支|
    |$<0$|维护所有历史版本|

    不被维护的历史版本将被垃圾回收清除。

- repo-mgr实现

    详见：[repo-mgr.c](https://github.com/poi0qwe/seafile-server-learn/blob/main/server/gc/repo-mgr.c)。其中前面的几个部分与FUSE中的仓库管理大同小异，主要在后面增加了以下几个类的实现：

    ```cpp
    int
    seaf_repo_manager_set_repo_history_limit (SeafRepoManager *mgr,
                                            const char *repo_id,
                                            int days); // 设置最大历史保留时间
    int
    seaf_repo_manager_get_repo_history_limit (SeafRepoManager *mgr,
                                            const char *repo_id); // 获取最大历史保留时间
    int
    seaf_repo_manager_set_repo_valid_since (SeafRepoManager *mgr,
                                            const char *repo_id,
                                            gint64 timestamp); // 设置生效日期
    gint64
    seaf_repo_manager_get_repo_valid_since (SeafRepoManager *mgr,
                                            const char *repo_id); // 获取生效日期
    gint64
    seaf_repo_manager_get_repo_truncate_time (SeafRepoManager *mgr,
                                            const char *repo_id); // 计算截止时间
    GList *
    seaf_repo_manager_get_virtual_repo_ids_by_origin (SeafRepoManager *mgr,
                                                    const char *origin_repo); // 根据原始仓库，获取所有虚拟仓库
    GList *
    seaf_repo_manager_list_garbage_repos (SeafRepoManager *mgr); // 列出所有垃圾仓库
    void
    seaf_repo_manager_remove_garbage_repo (SeafRepoManager *mgr, const char *repo_id); // 移除垃圾仓库
    ```

    垃圾仓库即不被维护的仓库。

### 仓库垃圾回收

分为两个步骤：

```cpp
int gc_core_run (GList *repo_id_list, int dry_run, int verbose); // 垃圾仓库回收
void delete_garbaged_repos (int dry_run); // 移除垃圾仓库
```

1. 给定一个仓库列表repo_id_list（仅原始仓库），然后调用gc_core_run，统计垃圾仓库信息

    - dry_run：表示仅获取垃圾仓库信息。dry_run为false时，将调用gc_core_run。
    - verbose：通过seaf_message输出这些信息。

2. 根据统计的垃圾仓库信息，进行垃圾回收

    - dry_run：表示仅获取垃圾仓库信息，不进行删除。

上述两个部分是静态单例的，相关信息通过数据库持久化。

- 具体操作

    - gc_core_run
        
        1. 遍历仓库列表
        2. 遍历仓库和虚拟仓库
        3. 遍历各个分支
        4. 从头分支开始遍历，加入垃圾仓库信息表
        5. 对于每个提交，如果已处理过头分支，则停止（防止重复删除）
        6. 遍历文件系统对象和块对象，加入对应的垃圾信息列表中

    - delete_garbaged_repos

        1. 获取垃圾仓库信息表
        2. 删除仓库存储


### 仓库完整性检查

只有一个接口`verify_repos`用于验证仓库。其主要工作是：

1. 验证块的存在性（不进行校验）
2. 验证文件系统对象的存在性
3. 验证提交和分支的存在性
4. 验证仓库的存在性

## Seafile-GC服务

一个命令行子进程，是对上述几个功能的总封装，构建一个可用的gc服务。代码详见：[seafserv-gc.c](https://github.com/poi0qwe/seafile-server-learn/blob/main/server/gc/seafserv-gc.c)。

参数如下：

|参数|描述|
|:-|:-|
|-h|帮助|
|-v|获取版本|
|-c|指定的ccnet目录|
|-d|指定的seafile目录|
|-F|指定的配置文件目录|
|-V|是否verbose|
|-D|是否dry run|
|-r|是否删除垃圾仓库|

该服务在获取后相应的执行gc_core_run，然后退出。

# FSCK 文件系统检查与修复

完成此项工作的相关源码：[fsck](https://github.com/poi0qwe/seafile-server-learn/blob/main/server/gc/fsck.c)。

这个子系统将递归仓库列表下各仓库的头分支下的文件系统，检查并列出其中丢失或损坏的文件。

```cpp
int
seaf_fsck (GList *repo_id_list, gboolean repair, int max_thread_num);
```

这个过程是允许并发的，以提高效率。若repair为真，则在对应位置创建空文件、空目录，并不是自动修复内容。

整个子系统还有导出功能，用于将文件系统进行备份。

```cpp
void export_file (GList *repo_id_list, const char *seafile_dir, char *export_path);
```

### Seafile-FSCK服务

一个命令行子进程，是对上述几个功能的总封装，构建一个可用的fsck服务。代码详见：[seaf-fsck.c](https://github.com/poi0qwe/seafile-server-learn/blob/main/server/gc/seaf-fsck.c)。

参数如下：

|参数|描述|
|:-|:-|
|-h|帮助|
|-v|获取版本|
|-c|指定的ccnet目录|
|-d|指定的seafile目录|
|-F|指定的配置文件目录|
|-f|强制运行，即使seafile-server并非当前用户运行|
|-t|指定最大线程数，默认0|
|-r|是否修复|
|-E|进行备份，而非检查与修复；参数为备份路径|