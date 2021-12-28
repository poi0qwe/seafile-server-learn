# Seafile对象

广义上是指所有用到的结构体，狭义上是指本项目中几个重要概念所对应的结构体（类似于Java Bean）。你可以在`lib/*.vala`文件里看到项目中所有Seafile对象的定义。

# 存储

一些Seafile对象需要持久化至硬盘。它们的存储方式有两种，一种是非关系型存储，另一种是关系型存储。前者要求对象必须含有id。

# 非关系型存储

**非关系型存储主要被Seafile文件管理系统所使用。因为Seafile文件管理系统是分布式的，而非关系型存储能为分布式存储提供良好的基础。**

相关代码的结构如下：

![](https://img-blog.csdnimg.cn/d23865ff67914a479a519823187ea492.png)

<small>与块系统类似，并且有多种实现方式。</small>

使用这种方式存储的的Seafile对象包括：commit、seafile、seafdir、seafdirent等。

- ### 对象的逻辑位置
    
    只考虑Seafile文件管理系统中的Seafile对象，我们可以通过以下信息定位一个Seafile对象：

    1. repo_id

        仓库存储id。

    2. version

        对象的版本。与仓库的seafile版本有关。

    3. obj_id

        对象id。是一个UUID。

- ### 对象序列化

    要将对象以非关系的形式存储到硬盘，则需要先进行序列化，将其转化为字节流。每个使用非关系存储的对象必须实现序列化与反序列化。实现方式有以下两种：

    - 结构体字节对齐

        这是seafile版本0中的方法。使用如下标识，告诉编译器将结构体字节对齐：

        ```cpp
        __attribute__ ((packed)) update_pack_t;
        ```

        然后用虚指针和sizeof运算符可以直接获取对象字节流。

    - json字符串

        这是seafile版本1中的方法。使用jansson库生成对象对应的json对象，然后转化为json字符串。

- ### 对象存储操作

    [obj-backend](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/obj-backend.h)：规定了对象进行非关系型存储所需要的抽象操作。

    |操作|说明|
    |:-|:-|
    |读|根据仓库id、版本、对象id，获取对象的字节流。|
    |写|根据仓库id、版本、对象id，写入对象的字节流。</br>支持备份与同步。|
    |存在|根据仓库id、版本、对象id，判断是否存在。|
    |删除|根据仓库id、版本、对象id，删除对象字节流。|
    |遍历仓库|根据仓库id、版本，遍历仓库中的所有对象字节流。|
    |复制|给定对象id，将其从某个版本的某个仓库复制到另一个仓库。|
    |清空|移除某个仓库内的所有对象字节流。|

## 对象存储器

[obj-store](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/obj-store.h)：对象存储器是对对象后台的进一步封装。

目前基于的是文件系统实现的后台。可由SeafileSession和对象类型生成对象存储器。

## 基于文件系统实现

项目中实现了一个基于文件系统的非关系型存储系统。

- ### 对象的物理位置

    对象的物理存储位置如下：

    ```s
    [obj_dir] / [repo_id] / [obj_id[:2]] / [obj_id[2:]]
    ```

    （`obj_id`一般是对象字节流的SHA1）

    其中`obj_dir`由对象存储器给定，格式如下：

    ```s
    [seaf_dir] / storage / [obj_type]
    ```

    1. seaf_dir：即配置文件中的`seafile_dir`；
    2. obj_type：Seafile对象类型。（例如`fs`表示文件系统相关对象）

- ### 对象存储操作的实现

    主要利用的是文件系统调用，略。

## 基于riak实现

Riak是一个分布式NoSQL数据存储系统，使用键值存储。

- ### 对象的物理位置

    ```s
    [host]:[port] / [bucket] / [obj_id]
    ```

    通过主机、端口、桶名和对象id定位对象。

- ### 对象存储操作的实现

    使用Riak的API实现，略。

# 关系型存储

部分Seafile对象采用的是关系型存储方式。相关代码的结构如下：

![](https://img-blog.csdnimg.cn/5f8fe2b769d24c79b85218b19b590e1f.png)


支持的数据库有：sqlite、mysql。编译时只能选择一个，默认为sqlite。（源码中还定义了pgsql，但未实现）

使用这种方式存储的的Seafile对象包括：branch、group等。

## 关系化对象

- ### 对象的数据库和表

    1. 数据库

        依照对象从属的系统，规定对象被存储的数据库。整个项目有两个数据库：seafile（用于seafile系统）、ccnet（用于ccnet系统）。

    2. 表

        依照对象类型，创建表。如SeafBranch对象对应了Branch表。

- ### 对象关系映射

    读写对象本质上就是读写数据库。相关对象中借助数据库操作的抽象封装，对自身的各个属性实现了双向数据流动。
    
    项目中整个部分是面向过程的，没有统一的ORM。结构体中通过一个私有指针指向数据库上下文(SeafDB)，每次读写都对应了一个SQL和一个数据库操作。

## 数据库操作

- ### 上下文

    - SeafDB：Seafile数据库上下文。
    - SeafRow：Seafile数据库行上下文。
    - SeafTrans：Seafile数据库事务上下文。
    - CcnetDB、CcnetRow、CcnetTrans：对应Ccnet数据库。

- ### 抽象操作

    |操作|说明|
    |:-|:-|
    |获取连接|（根据配置）连接数据库并获取SeafDB。|
    |释放连接|断开连接并释放SeafDB。|
    |非预编译执行SQL|直接执行SQL。|
    |执行SQL|输入SQL、参数列表，然后预编译并执行。|
    |遍历行|输入SQL、参数列表，然后预编译并执行；随后遍历各个行，传递给回调函数|
    |获取列数|获取一行中的列的数量|
    |获取字符串属性|给定行，列的索引，获取字符串属性|
    |获取整型属性|给定行，列的索引，获取整型属性|
    |获取长整型属性|给定行，列的索引，获取长整型属性|

- ### 操作封装

    [seaf-db](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/seaf-db.h)：对数据库抽象操作进行进一步的封装，以适配本项目的需求。

    [seaf-utils](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/seaf-utils.h)：提供了一些实用接口，包括根据会话配置创建数据库连接(SeafDB)。

## 基于Mysql实现

[seaf-db.c](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/seaf-db.c)中通过Mysql API实现了数据库抽象操作。

关于常规数据库操作的实现略，因为这些操作大都是对API进行的一定程度的封装。Mysql实现中主要关注的是多连接管理。因为当一个连建长时间无操作时，有可能断线，所以需要去反复发出ping信号，防止连接断开。项目中的实现方式是通过线程池进行线程调度。每个连接占有一个线程，这些线程通过一个互斥锁来争夺ping的资源。默认每30s进行一次ping。Mysql实现中不需要考虑数据库内的并发，因为Mysql已经实现了锁机制。

## 基于Sqlite实现

[seaf-db.c](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/seaf-db.c)中通过sqlite API实现了数据库抽象操作。

关于常规数据库操作的实现略，主要关注数据库并发处理。sqlite提供了一个解锁通知的接口`sqlite3_unlock_notify`，每次sqlite数据库解锁时，会调用用户提供的回调函数。因此并发机制主要靠用户实现。

项目中主要通过`wait_for_unlock_notify`函数进行互斥并发，每次执行SQL前将调用该方法等待sqlite解锁：

```cpp
typedef struct UnlockNotification { // 通知上下文
        int fired; // 是否已经通知
        pthread_cond_t cond; // 条件变量
        pthread_mutex_t mutex; // 条件变量的互斥锁
} UnlockNotification;

static void
unlock_notify_cb(void **ap_arg, int n_arg) // 回调函数
{
    int i;

    for (i = 0; i < n_arg; i++) { // 遍历用户参数
        UnlockNotification *p = (UnlockNotification *)ap_arg[i];
        pthread_mutex_lock (&p->mutex);
        p->fired = 1; // 表示已经通知
        pthread_cond_signal (&p->cond); // 条件变量发送信号
        pthread_mutex_unlock (&p->mutex);
    }
}

static int
wait_for_unlock_notify(sqlite3 *db)
{
    UnlockNotification un;
    un.fired = 0;
    pthread_mutex_init (&un.mutex, NULL);
    pthread_cond_init (&un.cond, NULL);

    int rc = sqlite3_unlock_notify(db, unlock_notify_cb, (void *)&un); // 并发通知

    if (rc == SQLITE_OK) {
        pthread_mutex_lock(&un.mutex);
        if (!un.fired) // 若已经通知，则跳过等待
            pthread_cond_wait (&un.cond, &un.mutex); // 否则等待条件变量发送信号
        pthread_mutex_unlock(&un.mutex);
    }

    pthread_cond_destroy (&un.cond);
    pthread_mutex_destroy (&un.mutex);

    return rc;
}
```

# Seafile文件系统

相关代码涉及[fs-mgr](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/fs-mgr.h)、[seaf-crypt](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/seaf-crypt.h)、[vc-common](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/vc-common.h)。在还没介绍提交、分支管理前，可以认为每个仓库都是独立的文件系统。

## 文件系统结构
- ### 抽象结构
	
	![请添加图片描述](https://img-blog.csdnimg.cn/2031439318c748d5a81560343db0a03d.png?x-oss-process=image/watermark,type_ZHJvaWRzYW5zZmFsbGJhY2s,shadow_50,text_Q1NETiBAY3lrMDYyMA==,size_11,color_FFFFFF,t_70,g_se,x_16)
	每个仓库都带有一个seafdir作为根目录，根目录通过一系列直接或间接的链接形成   了文件树。整个文件系统中主要分为三个内容：seafile、seafdirent、seafdir。中间这个seafdirent指的是seafdir对seafdir或seafdir对seafile的链接。
    - 链接实现形式
		
		根据非关系对象存储中的规定，每个对象都携带了一个id。在实际实现中，这个id都等于对象字节流的SHA1。链接就是通过这个id实现的。并且这样还为文件系统带来了一些好处，比如内容相同则无需创建新的拷贝，所以更节省空间。
	
- ### 物理结构	![](https://img-blog.csdnimg.cn/10353385895346a2b553907bdf0a56c9.png?x-oss-process=image/watermark,type_ZHJvaWRzYW5zZmFsbGJhY2s,shadow_50,text_Q1NETiBAY3lrMDYyMA==,size_20,color_FFFFFF,t_70,g_se,x_16)

    存储分为三层：目录、文件、块。（需要注意，seafdirent被包含在了seafdir中）

	第一层和第二层都由对象存储系统维护；第三层由块系统维护。第一层通过seafdirent链接第二层(或第一层)；第二层通过block_sha1s块表链接第三层。

    在Seafile CE中，也就是公开的源码中，这三层的底层都是操作系统的文件系统。
    
## 文件系统类
我们知道了seafile实现的文件系统主要是通过seafile对象来进行管理的。其中主要包括四个类：
![](https://img-blog.csdnimg.cn/c372d0a32da14ccd8b4b50c08e17f7e6.png?x-oss-process=image/watermark,type_ZHJvaWRzYW5zZmFsbGJhY2s,shadow_50,text_Q1NETiBAY3lrMDYyMA==,size_14,color_FFFFFF,t_70,g_se,x_16)
1. SeafFSObject
	seafile文件系统对象的基类。
2. Seafile
	seafile文件。其中包含了块的索引。
3. SeafDir
	seafile目录。其中包含了目录项目列表。
4. SeafDirent
	seafile目录项目。包含了文件名/目录名、最后修改时间等签名内容，以及一个指向文件id/目录id的指针。

## 文件系统操作
- ### 块加密

	为了确保异地存储的安全性，需要对文件内容进行加密。由于文件被分块存储，而且我们在读写时主要操作的是块，所以主要是对块的内容进行加密/解密。

    为了保证安全性，在仓库中随机生成了一个盐(salt)，用于防止彩虹表之类的攻击。用户可以给仓库设置一个密码，以协助进行加密和解密。为了避免密码被明文存储，它与仓库id、盐一起生成了一个magic，用于对密码进行核验。

    当然，仓库的加密和解密仍然不是通过用户密码生成的密钥进行的。仓库会随机生成一个密钥，它被用户密码加密后存储在远程服务器，用户可以提供密码解密并得到这个仓库密钥。仓库密钥才是真正参与块的加密和解密的密钥。

    之所以引入仓库密钥，是因为块存储服务器可能与seafile服务器分离。如果只用用户密码加密，则整个加密过程中seafile服务器与块服务器相互独立，seafile服务器没有对加密过程的绝对控制权，从而存在安全隐患。
![](https://img-blog.csdnimg.cn/4c8b0147d5e741119073828ee9330b77.png?x-oss-process=image/watermark,type_ZHJvaWRzYW5zZmFsbGJhY2s,shadow_50,text_Q1NETiBAY3lrMDYyMA==,size_20,color_FFFFFF,t_70,g_se,x_16)
- ### 文件系统操作

	Seafile对象和字节流的相互转换略。

	- #### 文件操作
		
		读：将块解密，然后合并为完整的明文信息；

		写：存在如下四种写的形式：

		1. 给出一个原始文件。分块，生成索引，硬链接块（写块），然后生成seafile对象。
		2. 给出一些块和一个索引。检查索引，然后硬链接块、生成seafile对象。
		3. 给出一些块和一个索引。检查索引，然后只硬链接块。
		4. 给出seafile对象id，一些块和一个索引。检查索引，要求每个块都已存在，然后重新生成seafile对象。
		
		第一个操作用于首次创建；第二和第三个用于更新；最后一个操作用于校验。

	- #### 目录操作
		
		主要都是围绕文件树进行。分为以下两类：

		1. 遍历文件树。包括统计子目录大小、统计文件数量、搜索文件等。
		2. 路径跳转。跳转到相对根目录的某个位置（以`/`表示根目录），该位置可能是seafile、seafdir或seafdirent。然后获取某些信息。

	- #### 目录项目操作
		
		伴随目录操作进行。略。


# 版本管理系统

seafile中，此部分的相关内容与git大体相同。

# 提交

## 版本管理理论前提

想要对仓库进行版本化管理，即持久化追踪仓库中的更新，则需要一个节点来存储每个版本下仓库内容的拷贝。这个节点就是提交。

每次提交后，仓库将重新组织新的seafile文件系统，生成新的seafobj，产生新的块，也就是一个拷贝。但显然，每次都重新拷贝一份实在是太浪费空间了。这时候，分块、SHA1命名的作用就体现出来了。每次更新只会影响被修改的块，所有被修改的内容只会影响被修改的块；SHA1命名基于操作系统文件系统，会替换同名的文件，而SHA1相同又代表内容相同。综上，在分块和SHA1命名的前提下，两个版本的交集实际上并没有被拷贝，拷贝的仅仅是新版本相对于老版本的增量内容。

在关于新版本的空间问题被解决后，剩下就是文件系统的重新组织问题，因为新版本与老版本的文件树很一定不一致。新版本实际上是仍然按照旧版本生成文件树，并保存seafile对象，且由于SHA1命名的关系，如果一个文件没被修改或一个目录没被修改，那么对象内部的链接仍然指向同一个对象，所以新版本相对旧版本只是根目录到某一个子目录的一个子树上做了更新。对于旧版本而言，文件系统的组织结构也是增量更新的。

上述两段论证了基于seafile文件系统（包括分块、文件组织）的版本管理，不论是从内容的角度还是从文件结构的角度，都是增量更新的。这大大提高了空间利用率，使得版本化管理更加高效。

## 提交的实质
提交的实质就是指向某个根目录的指针。

![](https://img-blog.csdnimg.cn/a1ddae70cb1d4b388198066820f693ca.png?x-oss-process=image/watermark,type_ZHJvaWRzYW5zZmFsbGJhY2s,shadow_50,text_Q1NETiBAY3lrMDYyMA==,size_20,color_FFFFFF,t_70,g_se,x_16)
每次提交后，重新对用户文件系统的原始目录进行组织，生成对应的新的seafile文件结构，然后新的提交就指向了新的根目录。根据版本管理增量更新的理论依据，旧的文件结构与内容都仍然存在，并且仍然可由旧的提交找到，所以我们可以进一步把提交泛化为一个抽象的対象，该对象就代表了一个版本的仓库，包括了该版本下的文件内容与文件结构。（这也是从用户的角度，去理解提交的概念）

## Seafile提交

seafile版本管理中的提交对象，拥有以下信息：
```cpp
struct _SeafCommit { 					// 提交对象
    struct _SeafCommitManager *manager; // 提交管理器

    int         ref; 			    	// 引用次数（GC）

    char        commit_id[41]; 			// 提交id
    char        repo_id[37]; 			// 仓库id
    char        root_id[41];  			// 根目录id
    char       *desc; 					// 提交描述
    char       *creator_name; 			// 创建者名字
    char        creator_id[41]; 		// 创建者id
    guint64     ctime;           		// 创建时间
    char       *parent_id; 				// 父提交id
    char       *second_parent_id; 		// 第二父提交id
    char       *repo_name; 				// 仓库名
    char       *repo_desc; 				// 仓库排列顺序
    char       *repo_category; 			// 仓库分类
    char       *device_name; 			// 设备名
    char       *client_version; 		// 客户端版本

    gboolean    encrypted; 				// 是否加密
    int         enc_version; 			// 加密版本
    char       *magic; 					// 仓库密码校验
    char       *random_key; 			// 随机密钥
    char       *salt; 					// 盐
    gboolean    no_local_history; 		// 有无本地记录

    int         version; 				// 版本
    gboolean    new_merge; 				// 是否是新的合并
    gboolean    conflict; 				// 是否冲突
    gboolean    repaired; 				// 是否被修复
};
```

一个提交对象包含的信息主要由以下几个方面。

1. ### 签名
	包括提交者、提交时间等。

2. ### 仓库
	依照从用户角度的理解，一个提交就对应了某版本下的整个仓库。实际上，提交中含有一个指针(repo_id)指向了其所在的仓库。同时我们还发现了许多与仓库对象耦合的字段，这些字段其实只是为了方便获取，实际上不包含在提交对象中。

3. ### 历史提交图

	每个提交可能有一或两个父提交，这是因为两个提交可以合并成一个新的提交（准确来说是分支的合并）。如果我们把提交间以父指针联系起来，则组成了一个DAG（有向无环图），每个节点代表一个提交，且节点入度小于等于2；并且该DAG只有一个根，称为初始提交（init）。
    和历史提交图有关的信息包括：父指针、合并相关信息。

## 提交管理操作
在[commit-mgr](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/commit-mgr.h)中定义了一些提交管理操作，仅限于对提交的管理。合并等不在其中。

这些操作中除了增删查改提交对象（键值对存储）外，还有拓扑遍历操作。拓扑遍历操作将从一个提交开始，向上遍历所有提交；如果某个提交有两个父提交，则会遍历各个分支（图分支）。

# 分支
我们知道一次提交是基于另一次提交之上、或者是两个提交的合并，那么如何指代另一个提交或者另两个提交？显然，如果细粒度地使用提交名，效率太低，可读性太差。

分支广义上是指得到某个提交的过程，那么得到不同的提交就对应了不同的分支。以小版本发布的开源软件为例，此时需要两个分支，一个是成品，另一个是开发过程中的半成品；前者指代得到某个版本的成品的过程，后者指代开发的过程；发布新版本时，我们不需要关心两个过程中各个提交间的具体联系，只需要将开发分支向成品分支合并，然后提交得到新的成品分支。

如果提交是对版本的细粒度管理，那么分支则是对版本的粗粒度管理。狭义上，用一个分支去指代一个提交，并且随着新提交的加入，分支向新提交移动。

我们还要区分一下历史提交图上的图分支和分支的区别。分支在版本管理中只是指代不同的提交，而图分支代表图上不相交的路径。两者是完全截然不同的概念，只在合并操作中会出现交集（合并既是分支的合并，也是图分支的合并）。

## 分支的实质
分支实质就是指向提交的指针。
![](https://img-blog.csdnimg.cn/fca8d81304884882935877bad00d43f6.png)
如图，有两个分支指向同一个提交。在提交了若干次后，分支和提交的关系可能如下：

![](https://img-blog.csdnimg.cn/2ca224e64b8c4e73b1ee39bdc74b6f30.png?x-oss-process=image/watermark,type_ZHJvaWRzYW5zZmFsbGJhY2s,shadow_50,text_Q1NETiBAY3lrMDYyMA==,size_11,color_FFFFFF,t_70,g_se,x_16)
两个不同的分支向两个不同的提交方向移动，最终指向两个不同的提交，代表两个不同的提交过程。

- Head指针
	
	指向分支的指针，代表该分支为当前分支。此后的提交、合并，都向head指针指向的分支进行。

## Seafile分支
```cpp
struct _SeafBranch {     // 分支对象
    int   ref; 			 // 引用次数
    char *name; 		 // 分支名
    char  repo_id[37];   // 仓库id
    char  commit_id[41]; // 提交id
};
```

分支的对象结构很简单，包含repo_id代表所在仓库；name表示分支名；commit_id表示所指向的提交。

## Seafile分支操作
分支可以很好地用关系去描述，因此被持久化至数据库。主要操作就是增删查改，略。

# 提交差异
为了方便用户检视，以及为合并做准备，需要设计一种算法，来统计两个提交间（分支）的差异。在思考算法的具体流程前，需要先总结差异的类型。

## 差异类型
若有两个提交Commit, Parent，假设我们考虑Commit相对于Parent的差异。即，Commit是我的提交，Parent是别人的提交，现在需要考虑我的提交相对于别人的提交的不同。
### 文件差异
|差异|说明|符号|
|:-:|:-|:-:|
|添加|Commit相对于Parent添加了该文件。|A|
|删除|Commit相对于Parent删除了该文件。|D|
|修改|Commit相对于Parent添加了该文件。|M|
|重命名|Commit相对于Parent重命名了该文件，且内容尚未被修改。|R|
### 目录差异
|差异|说明|符号|
|:-:|:-|:-:|
|添加|Commit相对于Parent添加了该目录。|B|
|删除|Commit相对于Parent删除了该目录。|C|
|重命名|Commit相对于Parent重命名了该目录，且链接尚未被修改。|E|
### 未合并差异
三路差异中的特殊情况。

|差异|说明|
|:-:|:-|
|STATUS_UNMERGED_NONE|未合并|
|STATUS_UNMERGED_BOTH_CHANGED|两者都被修改：相同文件被修改为不同结果|
|STATUS_UNMERGED_BOTH_ADDED|两者都被添加：两个合并都添加了相同的文件|
|STATUS_UNMERGED_I_REMOVED|一个移除了文件，一个修改了文件|
|STATUS_UNMERGED_OTHERS_REMOVED|一个修改了文件，一个移除了文件|
|STATUS_UNMERGED_DFC_I_ADDED_FILE|一个将目录替换为了文件，另一个修改了目录中的文件|
|STATUS_UNMERGED_DFC_OTHERS_ADDED_FILE|一个修改了目录中的文件，另一个将目录替换为了文件|

（实际使用中的目的未知，因为尚未找到相关实现代码）

## 算法
### 流程
- 同构递归

	目的是递归遍历各路文件树位置同构的部分。伪代码如下：
	```cpp
	文件树同构递归（Tree[s][n]：n路文件树上位置同构的目录）：
		Dirents[n]：n路文件树上位置同构的目录项
		While (1)：
			获取同构目录项至Dirents
			如果n路的Dirents完全相同：
		    	Return
			文件差异处理（Dirents）
			目录差异处理（Dirents）

	文件差异处理（Dirents[n]：n路文件树上位置同构的文件）：
		获取对应的Files，然后调用差异回调函数
	
	目录差异处理（Dirents[n]：n路文件树上位置同构的文件）：
		获取对应的Dir，然后调用差异回调函数
		文件树同构递归（Dirs）
	```
- 同构目录项

	同构递归之所以能做到多路位置同构，很重要的一点是需要获取同构目录项，而这一点需要某些基准。回到位置同构的本质，它实际上就是需要节点到根的路径完全相同。考虑搜索二/多叉树，因为存在权值，所以我们很容易找到多棵树中节点到根权值完全相同的路径。那么文件树中可以用什么作为权值呢？文件名/目录名。
	
	我们认为相同名称的目录项，在多路位置同构目录中也位置同构。基于这个假设，我们很快能设计出高效复杂度的遍历同构项的方法。考虑将目录项按名称排序（这也是获取Seafdir时的默认操作），然后我们就可以用类似归并排序中的方法，基于多指针来得到多路相同的目录项。

当然，仅仅是知道同构仍然是不够的，因为我们的目的是需要知道同构的差异，所以我们需要进一步使用差异算法来获得差异。
### 二路差异与三路差异
对于两种产生提交的方式，有两种差异算法：

1. 二路差异：应用于普通提交。判断Commit相对于Parent的差异。能直接确定非差异内容，而对于修改内容，则需要判断内容。

2. 三路差异：应用于合并后提交。判断Commit相对于ParentA和ParentB的差异。可以区分出Commit相对于两者的增加、删除。但是修改仍然要结合内容来判断。

![](https://img-blog.csdnimg.cn/836b54b0d1d14ee4b84e91d7bf3669f1.png?x-oss-process=image/watermark,type_ZHJvaWRzYW5zZmFsbGJhY2s,shadow_50,text_Q1NETiBAY3lrMDYyMA==,size_20,color_FFFFFF,t_70,g_se,x_16)
无论是二路差异还是三路差异，都无法判断用户是否是重命名了文件或目录，因为在基于名称的位置同构中，重命名等价于删除后增加。后处理中将处理这种情况。

### 后处理
- 重命名的近似判断

    已知，基于名称的同构中，如果一个文件被重命名了，那么它可能会被判定为既被删除又被增加。那么如何还原真实的状态？

    答案是只需要找到相同内容，如果相同内容下存在“删除-增加”对，则认为该目录或文件被重命名了。

	注意这只是一种近似判断，有可能用户真的是删除后又增加，但由于我们不监控用户的行为，所以统一认为是重命名。

- 冗余空目录
	
	另一个会出现bug的点在空目录。空目录是个特殊存在，所有空目录都指向同一个空目录对象，因为它们的内容相同，所以SHA1即对象名都相同。因此存在两种情况会对一个空目录作出误判：

	1. 向空目录添加文件后，会判定该空目录被删除；
	2. 将某目录清空后，会判定空目录被增加。

    那么如何处理这两种情况？其实很简单，遍历所有状态为增加或删除的目录，然后特判一下，再将差异类型修正即可。

## 实现
- ### 差异对象
	
	```cpp
	typedef struct DiffEntry { 	// 差异对象
	    char type; 				// 差异类型
	    char status; 			// 差异状态
	    int unmerge_state; 		// 未合并状态
	    unsigned char sha1[20]; // 用于解决重命名问题
	    char *name; 			// 名称
	    char *new_name;         // 新名称，仅被用于重命名情形
	    gint64 size; 			// 大小
	    gint64 origin_size;     // 原始大小，仅被用于修改情形
	} DiffEntry;
	```
- ### 差异对比选项
	```cpp
	typedef struct DiffOptions { // 比对选项
	    char store_id[37]; 		// 存储id
	    int version; 			// seafile版本
	    // 两个回调
	    DiffFileCB file_cb; 	// 文件差异处理回调
	    DiffDirCB dir_cb;		// 目录差异处理回调
	    void *data; 			// 用户参数
	} DiffOptions;
	```
	在使用中向差异对比操作传入该结构体，需要设置目录和文件回调函数。回调即差异算法。

- ### 差异对比算法
	
	- 同构位置递归
	```cpp
	static int // 文件树同构递归
	diff_trees_recursive (int n, SeafDir *trees[],
	                      const char *basedir, DiffOptions *opt);
	int // 文件树差异处理，封装
	diff_trees (int n, const char *roots[], DiffOptions *opt)
	static int // 目录差异处理
	diff_directories (int n, SeafDirent *dents[], const char *basedir, DiffOptions *opt)
	static int // 文件差异处理
	diff_files (int n, SeafDirent *dents[], const char *basedir, DiffOptions *opt)
	```
	- 差异算法
	```cpp
	static int // 二路文件差异处理
	twoway_diff_files (int n, const char *basedir, SeafDirent *files[], void *vdata)
	static int // 二路目录差异处理
	twoway_diff_dirs (int n, const char *basedir, SeafDirent *dirs[], void *vdata,
	                  gboolean *recurse)
    static int // 三路文件差异处理
	threeway_diff_files (int n, const char *basedir, SeafDirent *files[], void *vdata)
	static int // 三路目录差异处理（默认无操作）
	threeway_diff_dirs (int n, const char *basedir, SeafDirent *dirs[], void *vdata,
	                    gboolean *recurse)
	```
	
	其中有一个小环节值得一提，就是如何判断内容是否相同。这时再次体现了SHA1摘要命名的好处：我们只需要对比两个文件系统对象的id，就能直接判断它们的内容是否相同。
	- 后处理
	
	```cpp
	void // 解决重命名问题
	diff_resolve_renames (GList **diff_entries)
	void // 解决冗余空目录问题
	diff_resolve_empty_dirs (GList **diff_entries)
	```
- ### 封装
	- 普通提交
	```cpp
	int // 比对两个提交的差异
	diff_commits (SeafCommit *commit1, SeafCommit *commit2, GList **results, // 结果记录在results列表
	              gboolean fold_dir_diff);
	int // 比对两个提交的差异；给定根目录
	diff_commit_roots (const char *store_id, int version,
	                   const char *root1, const char *root2, GList **results,
	                   gboolean fold_dir_diff);
	```
	- 合并后提交

	```cpp
	int // 比对合并前后的差异（与两个父提交对比）
	diff_merge (SeafCommit *merge, GList **results, gboolean fold_dir_diff);
	int // 比对合并前后的差异；给定根目录
	diff_merge_roots (const char *store_id, int version,
	                  const char *merged_root, const char *p1_root, const char *p2_root,
	                  GList **results, gboolean fold_dir_diff);
	```
# 分支合并
## 合并、冲突与解决
合并是一种重要的手段，能将多个提交(分支)合为一体。合并的对象时两个提交，但一般被认为是分支的合并，因为需要用分支作为提交的指针。分支合并在图中的性质就是入度等于二，即一个提交的生成由两个父提交产生，并且可以携带增量内容。

合并两个提交(分支)，最重要的问题就是解决冲突。冲突的判断实际上与差异的判断类似，都是借助位置同构进行的，在此不做赘述。这里的关键问题在于如何判定冲突和解决冲突。冲突对于用户来讲敏感，用户需要通过比较选择哪一个的内容将其放到新版本中。同时冲突又有可能是用户定义的，所有非相同内容都有可能是冲突。

在此有两种解决思路：第一种是二路合并，第二种是三路合并。

二路合并很简单，仅相同内容被保留，删除增加修改全部留给用户选择。因为对二路的非相同内容完全无法判断是否应该保留还是舍弃。这样做实现很容易，但对用户来说很麻烦，不过非常保险，因为冲突选择权全部交给用户，完全可以由用户定义冲突。

三路合并则是具有一定自主性，引入了Base即两个分支的公共祖先，然后借助Base来判断保留内容还是交给用户选择。这样做提高了效率，但并不一定保险，因为算法定义的冲突有可能不符合用户定义。
## 三路合并
合并的参与者包括三个分支，Base、Head、Remote。我们的目的是将Head和Remote合并，并且Base是它们的共同祖先。

引入了Base后，所有差异内容的判断都有了判断准则。算法认为两分支相对于祖先不交叉的更改(包括增删改)都是被保留的，而交叉的更改则是冲突内容。这样的冲突定义实际上已经符合大多数情况，而且将冲突的可能范畴进一步缩小，可以说是在效率与用户需求之间找到了平衡。

我们用表格来表示合并中三个分支可能出现的情况。假设各个分支中位置同构部分的内容以大写字母表示，则可以得到如下合并结果：

|Base|Head|Remote|结果|说明|
|:-:|:-:|:-:|:-:|:-:|
|A	|A	|A	|A|相同内容|
|A|A|B|B|如果只有一方修改了，那么选择修改的|
|A|B|A|B|同上|
|A|B|B|B|如果双方拥有相同的变更，则选择修改过的|
|A|B|C|conflict|如果双方都修改了且不一样，则报告冲突，需要用户解决|

<small>转自：https://blog.csdn.net/longintchar/article/details/83049840</small>

由上述表格，我们能够直接给出各个同构位置合并是如何处理的。所以三路合并算法也自然显现：套用之前的同构递归，然后实现该表格中的判断即可。
 
## 实现
函数递归和调用流程如图：
![](https://img-blog.csdnimg.cn/598da294f483454d8b0d302737185bf1.png)
内容就是实现了三路合并算法（二路合并并未实装，将只调用回调函数）。

最后需要阐明的是合并结果的存储。对于合并后的结果，有两种选项：如果do_merge=true，则直接写入硬盘，然后返回一个根目录id到merged_tree_root；反之，只调用回调函数。

在此不再粘贴详细源码，详见：[merge-new.c](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/merge-new.c)。


# 用户与用户组

Seafile支持用户与用户组，包含一个用户与用户组管理系统。而这个子系统也被另一个项目复用：[ccnet-server](https://github.com/haiwen/ccnet-server)，它是Seafile服务器的内部通信框架和用户/组管理。由于这个项目关于用户与用户组子系统方面比seafile-server更全面，所以我们主要基于该项目进行介绍。这篇文章主要介绍后者，即用户与用户组管理；前者内容属于RPC，下面会涉及到一点。

> Ccnet-server中当然还有进程间通信，例如对等体识别、连接管理、服务调用、消息传递等，而这些内容属于协议，在[seafile : librpc 分析](https://blog.csdn.net/Minori_wen/article/details/121056932?spm=1001.2014.3001.5501)中可以找到。

- ### Ccnet

    指的是RPC协议下的用户集群。Ccnet包含唯一的服务端，以及多个客户端。服务端（ccnet-server）负责管理用户与用户组。客户端也被称为协议对等体，向服务端发起RPC。

用户与用户组中分为三个层次结构：用户、群组、组织。用户与用户组管理就是对这三个层次的结构进行维护。
![](https://img-blog.csdnimg.cn/f67a0283b5be4f4685979a267e094dc5.png)
三个层级依次递进，所管理的用户数量的数量级逐级递增。

这三个层次的相关信息可以很方便地用关系模型描述，并且需要统一在同一个数据库下进行维护，所以这个系统的数据持久化主要基于关系型数据库。

## 用户
用户与用户组中的一个个体，即一个用户。用户之间存在沟通和联系，但这不是用户与用户组管理的关键。用户与用户组管理主要做的是对用户信息进行维护，至于怎样将信息在用户与用户组内部进行协调，用户与用户组管理一概不关心。

用户管理的ER图如下：
![](https://img-blog.csdnimg.cn/fea609a9d2e245278b02bd07bc55a490.png)
用户的信息包括基本的id、邮箱、密码等。另外有两个标签：is_staff代表是否是管理员，is_active代表目前是否活跃。

### 用户绑定对等体
用户与用户组中出现了一个特殊名字，叫做对等体。我们都知道对等体之间通过协议来通信或协调其他工作，例如seafile中以自身实现的rpc协议来进行rpc操作。对等体的概念在RPC中频繁涉及，从狭义角度理解，对等体就是相对于服务端的RPC中的客户端，以`peer_id`、`peer_ip`、`peer_port`区分。其中`peer_id`区分了每个RPC连接（实际使用的是用户设备id），也就是说`peer_id`能区分每个RPC中的用户，因此采用一个绑定关系`Binding`来将`peer_id`与用户关联，即把RPC连接与请求连接的用户关联。

（对等体的内容在seafile-server的开源内容中只有声明没有实现，可能不开源；或者说在社区版中没有用到此项技术。上述结论来源于ccnet-server）

### 角色
用户可能扮演多个角色，因此创建了另外一个实体UserRole来表示用户当前的角色。这些角色并不是预定义的，可由开发者通过用户与用户组的功能自定。

### 用户管理操作
由于用户管理重点是对用户信息进行管理，所以内容就是增删查改。详细信息见[user-mgr.h](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/user-mgr.h)。

## 群组
将多个用户进行集中式管理，每个用户属于一个群组。ER图如下：
![](https://img-blog.csdnimg.cn/81beb3abc80b49efbc4cc871eec41b0e.png)
### 群组结构
群组的结构是一棵树（对应了集合的包含关系），根被称为顶级群组，每个群组都有一个指向父群组的指针。更方便的，通过一个字符串路径能够唯一确定一个群组，这个信息以实体GroupStructure表示。

### 群组管理操作
群组管理中除了对群组自身进行操作，还要设计到群组中用户的操作，内容依旧是增删查改。详见[group-mgr.h](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/group-mgr.h)。
## 组织
将多个群组进行集中式管理，每个群组属于一个组织。ER图如下：
![](https://img-blog.csdnimg.cn/80bf39ddde9d4967a096ea292e9acd3e.png)
组织就是一个用户与用户组。与组织相关的关系和实体中描述了组织中包含的用户、群组的信息。
### 组织管理操作
内容与群组类似，但并不关心群组和用户间的具体关系，重点关注组织和用户、组织和群组间的关系。具体内容详见[org-mgr.h](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/org-mgr.h)

## 用户与用户组架构
通过源码，我们能总结出上述内容所实现的用户与用户组架构。一个最基本的线索就是包含关系：群组是用户的集合、组织是群组的集合。这个关系也是我们从ER关系中作出的直观推断。实际上这里实现的用户与用户组的结构也正是如此：分层架构。
![](https://img-blog.csdnimg.cn/cc7389b605f7477db4dd2a7f2137f520.png)
现在我们已经知道了分层这样一个关系，现在思考为什么要进行分层。

首先，用户与用户组内还存在一个关系，那就是管理。管理显然是不可或缺的，因为有时候在可进行可靠交互时，必须引入第三方。但用户与用户组的管理同样也是个问题，它与管理的规模挂钩。而用户与用户组管理的分层架构，恰好体现了分而治之的思想。试想一下，如果全部交由一个中心用户来管理所有其他用户，那么中心用户的负载将会过大。于是分层架构中就诞生了群组的概念，将各个群组分摊给一些管理员，群组管理员管理群组内的用户。那么谁来管理管理员，于是需要一个级别更高管理员，来管理用户与用户组内其他所有管理员和所有用户。如果这些管理员负载也太大了，那么就进一步分为更小的群组，这也就是群组间呈现的树状关系。

为了方便管理，并且考虑到用户与用户组的动态性，我们需要将用户与用户组实体化，并在持久化部件中进行维护。这个时候又有问题了，谁来实体化？于是引入创建者的概念。一个群组由某个用户进行创建，并将其持久化，这个用户被称为创建者。

现在我们重新考虑用户与用户组内部的各种关系，包括管理与创建关系，从用户开始向上递推，于是我们得到了更加清晰的描述：
![](https://img-blog.csdnimg.cn/fa0359aa59da44c18fa25c192d9f89f7.png?x-oss-process=image/watermark,type_d3F5LXplbmhlaQ,shadow_50,text_Q1NETiBAY3lrMDYyMA==,size_20,color_FFFFFF,t_70,g_se,x_16)
上图也就是对用户与用户组管理的总结。