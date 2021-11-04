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

        ```c++
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

```c++
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