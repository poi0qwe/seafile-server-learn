
# seafile仓库管理

仓库是一个文件系统的管理单位。之前介绍的分支、提交、文件、目录等对象，都是以仓库集中进行管理。

此处介绍的只是FUSE中实现的最基本的seafile仓库管理，更完整的内容是在server核心中实现的。

## 对象格式

```cpp
struct _SeafRepo { // 仓库对象
    struct _SeafRepoManager *manager; // 仓库管理器

    gchar       id[37];             // 仓库id
    gchar      *name;               // 仓库名
    gchar      *desc;               // 排序
    gchar      *category;           // 分类（未实现）
    gboolean    encrypted;          // 是否加密
    int         enc_version;        // 加密版本
    gchar       magic[33];          // 用于密码校验
    gboolean    no_local_history;   // 是否无本地历史记录

    SeafBranch *head;               // 头指针，指向头分支(当前分支)

    gboolean    is_corrupted;       // 是否损坏
    gboolean    delete_pending;     // 是否等待删除
    int         ref_cnt;            // 引用次数，用于GC

    int version;                    // seafile版本
    gchar       store_id[37];       // 虚拟仓库使用
};
```

前面几个域都是仓库的基本信息。通过仓库id来唯一识别一个仓库；加密部分具体看此前介绍的seaf-encrypt。

通过仓库访问当前文件系统的途经就是head指针，正如此前的提交与分支中介绍的，它是仓库的头指针，指向了当前分支。（而分支又指向了一个提交，提交又包含了一个根目录对象，所以借此我们就能访问当前版本下的所有文件）

接着是几个标签，分别用于判断仓库的损坏、滞后删除、以及仓库对象本身的垃圾回收。

最后是seafile版本和虚拟仓库的存储id。seafile版本是为了消除不同仓库间的版本差异，提供差异化的操作；而存储id是为虚拟仓库设计的，具体看下文。

## 虚拟仓库

虚拟仓库是对仓库补充。其关系如下：

![](https://img-blog.csdnimg.cn/169d2a5e405e4b8687364e1ea4fa0c92.png?x-oss-process=image/watermark,type_d3F5LXplbmhlaQ,shadow_50,text_Q1NETiBAY3lrMDYyMA==,size_11,color_FFFFFF,t_70,g_se,x_16)


每个仓库在虚拟仓库关系中都有一个指针，指向自己的原始仓库。指向空的仓库为原始仓库。

虚拟仓库与原始仓库共享文件系统与块存储系统，这意味着虚拟仓库寄存在原始仓库下并能以最低代价切换版本。

试想一下，假如我们需要拷贝一个仓库，然后进行仓库更名、更新等操作，最朴素的想法是拷贝整个文件系统；但其实我们可以只开辟一个虚拟的指针，让它指向原始的文件系统，这样的开销极低。当然，除了效率因素，虚拟仓库的引入还能方便管理，因为这些仓库共享了文件系统，所以能进行相互切换。最后，虚拟化仓库可以很方便的进行不同分支下的平行更新，因为用户不需要频繁切换head指针，这类似于github里的fork。

## 管理操作

使用仓库管理器进行管理操作，这里只列举FUSE中实现的几个操作：

```cpp
gboolean
seaf_repo_manager_repo_exists (SeafRepoManager *manager, const gchar *id); // 判断仓库是否存在
GList* 
seaf_repo_manager_get_repo_list (SeafRepoManager *mgr, int start, int limit); // 获取仓库列表
GList *
seaf_repo_manager_get_repo_id_list (SeafRepoManager *mgr); // 仓库id列表
GList *
seaf_repo_manager_get_repos_by_owner (SeafRepoManager *mgr,
                                      const char *email); // 根据拥有者获取仓库表
gboolean
seaf_repo_manager_is_virtual_repo (SeafRepoManager *mgr, const char *repo_id); // 判断是否是虚拟仓库 
```

这些操作都与数据库相关，所以被剥离至此处实现。（两个表：`Repo`表示仓库，`VirtualRepo`表示虚拟仓库关系）

其他管理操作见server核心。

# seafile会话

这是一个贯穿整个seafile-server的对象，因为它是对所有此前分析的基本子系统的总封装。

seafile会话对象的结构如下：

```cpp
struct _SeafileSession {
    char                *seaf_dir;          // seafile目录
    char                *ccnet_dir;         // ccnet目录
    char                *tmp_file_dir;      // 临时文件目录
    /* Config that's only loaded on start */
    GKeyFile            *config;            // seafile配置
    GKeyFile            *ccnet_config;      // ccnet配置
    SeafDB              *db;                // seafile数据库
    SeafDB              *ccnet_db;          // ccnet数据库

    SeafBlockManager    *block_mgr;         // 块管理器
    SeafFSManager       *fs_mgr;            // 文件系统管理器
    SeafBranchManager   *branch_mgr;        // 分支管理器
    SeafCommitManager   *commit_mgr;        // 提交管理器
    SeafRepoManager     *repo_mgr;          // 仓库管理器
    CcnetUserManager    *user_mgr;          // 用户管理器
    CcnetGroupManager   *group_mgr;         // 群组管理器
    CcnetOrgManager     *org_mgr;           // 组织管理器

    GHashTable          *excluded_users;    // 排除用户表

    gboolean create_tables;                 // seafile数据库是否创建表
    gboolean ccnet_create_tables;           // ccnet数据库是否创建表
};
```

而seafile会话所提供的接口也就是两方面：创建和初始化。（`seafile_session_start`恒返回0，我们可以忽略）


```cpp
SeafileSession *
seafile_session_new(const char *central_config_dir,
                    const char *seafile_dir,
                    const char *ccnet_dir)
```

创建就是根据给定的三个目录，获取配置并创建各种管理器，在从配置中获取排除用户表。

```cpp
int
seafile_session_init (SeafileSession *session);
```

初始化则是对各个管理器的初始化。（数据库的初始化需要手动进行：借助`common/seaf-utils`中实现的`seaf_init_xxxx_database`来完成对数据库的初始化）

# 用户空间文件系统

用户空间文件系统(FUSE)是在此前的seafile文件系统的基础上引入了用户。

用户空间文件系统下的文件路径：

```
/[user]/[repo_id]/...
```

FUSE分为三个层次：根目录、用户目录、仓库。所以对FUSE的操作也是基于这三个部分完成的。

## seaf-fuse

提供了FUSE操作(只读)通用的接口，包括：获取文件内容、获取(文件或目录的)状态、获取目录信息。

- 通用操作

    1. 转化fuse路径

        将fuse路径转化为“用户”、“仓库id”、“仓库内路径”三部分。通过一个`n_parts`参数控制转化的级别，级别和转化内容表如下：

        |级别|用户|仓库id|仓库内路径|
        |:-|:-:|:-:|:-:|
        |0||||
        |1|√|||
        |2|√|√||
        |3|√|√|√|

        核心代码：

        ```cpp
        tokens = g_strsplit (path, "/", 3);
        n = g_strv_length (tokens);
        *n_parts = n;

        switch (n) {
        case 0: // 级别0：无内容
            break;
        case 1: // 级别1：用户
            *user = g_strdup(tokens[0]);
            break;
        case 2: // 级别2：用户、仓库id
            *repo_id = parse_repo_id(tokens[1]);
            if (*repo_id == NULL) {
                ret = -1;
                break;
            }
            *user = g_strdup(tokens[0]);
            *repo_path = g_strdup("/");
            break;
        case 3: // 级别3：用户、仓库id、仓库内路径
            *repo_id = parse_repo_id(tokens[1]);
            if (*repo_id == NULL) {
                ret = -1;
                break;
            }
            *user = g_strdup(tokens[0]);
            *repo_path = g_strdup(tokens[2]);
            break;
        }
        ```

        就是通过分支结构进行枚举。

    2. 打开fuse路径

        假装打开一个fuse路径。如果是级别1，直接警告；如果是级别2，则是打开仓库根目录；如果是级别3，则是打开仓库下的具体文件或目录。

        所谓假装打开，实际上就是获取seaf对象id，用以检查目标是否存在。这个操作是相对于仓库头分支，也就是当前分支进行的。

        核心代码：

        ```cpp
        if (parse_fuse_path (path, &n_parts, &user, &repo_id, &repo_path) < 0) {
            seaf_warning ("Invalid input path %s.\n", path);
            return -ENOENT;
        } // 进行转化，获取用户、仓库id、仓库内路径

        // 级别1，直接警告；级别2则是打开仓库头分支的根目录；级别3则是打开仓库内路径下对应的文件或目录
        if (n_parts != 2 && n_parts != 3) {
            seaf_warning ("Invalid input path for open: %s.\n", path);
            ret = -EACCES;
            goto out;
        }
        // 获取仓库
        repo = seaf_repo_manager_get_repo(seaf->repo_mgr, repo_id);
        if (!repo) {
            seaf_warning ("Failed to get repo %s.\n", repo_id);
            ret = -ENOENT;
            goto out;
        }
        // 获取头分支，获取分支指向的提交
        branch = repo->head;
        commit = seaf_commit_manager_get_commit(seaf->commit_mgr,
                                                repo->id,
                                                repo->version,
                                                branch->commit_id);
        if (!commit) { // 未找到，报错
            seaf_warning ("Failed to get commit %s:%.8s.\n", repo->id, branch->commit_id);
            ret = -ENOENT;
            goto out;
        }
        // 获取仓库内路径下的文件系统对象id（对于级别2，此时repo_path为空，等价于仓库根目录）
        char *id = seaf_fs_manager_path_to_obj_id(seaf->fs_mgr,
                                                repo->store_id, repo->version,
                                                commit->root_id,
                                                repo_path, &mode, NULL);
        ```

### file

实现了文件内容的获取。这部分内容定义并实现在了[file.c](https://github.com/poi0qwe/seafile-server-learn/blob/main/fuse/file.c)中。

由于根目录、用户目录下不存在文件，所以只需要实现仓库内的seafile读取。具体代码略。

### getattr

实现了状态的获取，既可以是目录状态也可以是文件状态。这部分内容定义并实现在了[getattr.c](https://github.com/poi0qwe/seafile-server-learn/blob/main/fuse/getattr.c)中。

```cpp
if (parse_fuse_path (path, &n_parts, &user, &repo_id, &repo_path) < 0) {
    return -ENOENT;
} // 对fuse路径进行转化

switch (n_parts) {
case 0:
    ret = getattr_root(seaf, stbuf); // 获取FUSE根目录的状态
    break;
case 1:
    ret = getattr_user(seaf, user, stbuf); // 获取用户目录的状态
    break;
case 2: // 获取仓库根目录的状态；被下一种情况包含
case 3:
    ret = getattr_repo(seaf, user, repo_id, repo_path, stbuf); // 获取仓库内某对象的状态
    break;
}
```

如上，进行了分支转发。转发的各个方法中的具体代码略。

### readdir

实现了目录的读取（仅名称）。这部分内容定义并实现在了[readdir.c](https://github.com/poi0qwe/seafile-server-learn/blob/main/fuse/readdir.c)中。

```cpp
if (parse_fuse_path (path, &n_parts, &user, &repo_id, &repo_path) < 0) {
    return -ENOENT;
}

switch (n_parts) {
case 0:
    ret = readdir_root(seaf, buf, filler, offset, info);
    break;
case 1:
    ret = readdir_user(seaf, user, buf, filler, offset, info);
    break;
case 2:
case 3:
    ret = readdir_repo(seaf, user, repo_id, repo_path, buf, filler, offset, info);
    break;
}
```

与getattr中的内容基本一致。