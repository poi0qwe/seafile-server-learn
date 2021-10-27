# Seafile对象

广义上是指所有用到的结构体，狭义上是指本项目中几个重要概念所对应的结构体（类似于Java Bean）。你可以在`lib/*.vala`文件里看到项目中所有Seafile对象的定义。

一些Seafile对象需要持久化至硬盘。它们的存储方式有两种，一种是非关系型存储，另一种是关系型存储。前者要求对象必须含有id。

# 非关系型存储

**非关系型存储主要被Seafile文件管理系统所使用。因为Seafile文件管理系统是分布式的，而非关系型存储能为分布式存储提供良好的基础。**

相关代码的结构如下：

<small>与块系统类似，并且有多种实现方式。</small>

使用这种方式存储的的Seafile对象包括：commit、seafile、seafdir、seafdirent等。

- ### 对象的逻辑位置
    
    只考虑Seafile文件管理系统中的Seafile对象，我们可以通过以下信息定位一个Seafile对象：

    1. repo_id

        仓库存储id。与块的store_id相同。

    2. version

        仓库的seafile版本。

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

    其中`obj_dir`由对象存储器给定，格式如下：

    ```s
    [seaf_dir] / storage / [obj_type]
    ```

    1. seaf_dir：即配置文件中的`seafile_dir`；
    2. obj_type：Seafile对象类型。

- ### 对象存储操作的实现

    主要利用的是文件系统调用，略。

## 基于riak实现

Riak是一个分布式NoSQL数据存储系统，使用键值存储。

- ### 对象的物理位置

- ### 对象存储操作的实现

    对象的物理存储位置如下：

# 关系型存储

部分Seafile对象采用的是关系型存储方式。相关代码的结构如下：

使用的数据库可以是mysql，也可以是sqlite，二选一，默认后者。

使用这种方式存储的的Seafile对象包括：branch、group等。