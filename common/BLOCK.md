# 块

CDC分块后，文件被存储为块。

块系统有关代码的组织结构如下：

![](https://img-blog.csdnimg.cn/3640926891a948b495f099e44d4ee68e.png)

## 逻辑结构与抽象操作

[block](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/block.h)、[block-backend](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/block-backend.h)：定义块的逻辑结构与抽象操作。

- ### 块的内容

    块可以抽象为两部分信息：校验和(checksum)和块内信息。

    - 校验和

        SHA1摘要的前160位比特。以16进制串的形式表示，得到20位HEX串。(共40位字符)

    - 块内信息

        块并不关心块内信息，只关心块内信息的存储。在校验块时需要重新对块内信息计算校验和，然后与块的校验和比对，检查块内信息是否被篡改。

- ### 块的元数据

    BMetadata定义了块的元数据，包括块id和块长：

    ```c++
    struct _BMetadata {
        char        id[41]; // 块的id
        uint32_t    size; // 块的大小
    };
    ```

- ### 块的逻辑位置

    依照以下内容定位一个块：

    - block_dir

        存储块的根目录。对外界隐藏。

    - store_id

        仓库存储id。（单端存储中，存储id等于仓库id；否则是特定的存储id）

    - version

        仓库的seafile版本。（用于处理不同版本间的差异）

    - block_id

        块id。

- ### 块操作

    [block-backend](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/block-backend.h) 中规定了块的抽象操作，包括以下内容：

    |操作|说明|
    |:-|:-|
    |打开块|根据store_id、version、block_id、和操作类型，打开块，然后返回块的句柄。</br>（若参数含有句柄，则说明需要先打开块）|
    |读块|根据句柄，向缓冲写块的内容。|
    |写块|根据句柄，将缓冲中的内容写入**临时**块。|
    |提交|根据句柄，提交临时块中的内容。用临时块的内容去替换目标块。|
    |关闭|根据句柄，关闭块。|
    |存在|根据store_id、version、block_id，判断块是否存在。<br/>|
    |移除块|根据store_id、version、block_id，移除一个块。|
    |获取元数据|根据store_id、version、block_id，获取块的元数据。<br/>或根据块的句柄，获取块的元数据。|
    |释放块的句柄|释放句柄空间。|
    |遍历块|给定store_id和version，并提供用户函数和用户参数，去遍历仓库中的所有块。<br/>每访问一个块，向用户函数传入store_id、version、block_id以及用户参数。|
    |复制块|提供块A和块B的store_id、version、block_id，将A的内容复制到B；<br/>若不存在B则创建。|
    |移除仓库存储|移除仓库中的所有块。|

    （“移除仓库存储”需要仓库的seafile版本为1）

## 基于文件系统的实现

[block-backend-fs](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/block-backend-fs.c) ：以文件系统实现了块的抽象操作，并给出了块的物理结构。

- ### 块的物理位置

    在文件系统实现中，以块文件表示块。

    1. 校验和与块内信息

        校验和被用作文件名与block_id、块内信息即文件内容。

    2. 块的路径

        格式如下：

        ```s
        [block_dir] / [store_id] / [block_id[:2]] / [block_id[2:]]
        ```

        在block_dir下的一个三级目录结构，一级是store_id，二级是block_id的前两位；三级是block_id的后38位。（因为校验和被用作block_id，所以block_id共40位）

- ### 句柄结构

    ```c++
    struct _BHandle { // 句柄
        char    *store_id;      // 仓库id
        int     version;        // 版本
        char    block_id[41];   // 块id
        int     fd;             // 文件标识符
        int     rw_type;        // 读写类型
        char    *tmp_file;      // 临时文件路径，被用于写操作与提交操作
    };
    ```

    其中最主要的就是文件描述符。此后用到块句柄的操作都以此为基础实现。

- ### 基本块文件操作

    1. 合成路径

        提供store_id、version、block_id，合成块文件路径。（block_dir被隐藏在后台私有域中，也参与合成）

    2. 打开临时文件

        临时文件路径就是在块路径的基础上加上了'.XXXXXX'的后缀。打开临时文件同打开块文件。

- ### 块操作的实现

    |操作名|文件系统实现|
    |:-|:-|
    |打开块|根据store_id、version、block_id生成路径。<br/>如果是读，就以只读方式打开块文件；<br/>如果是写，就以可写与创建方式打开临时文件。<br/>最后将相关信息记录到块的句柄。|
    |读块|根据文件描述符，读取缓冲。|
    |写块|根据文件描述符，写入缓冲至临时文件。|
    |提交|用临时文件替换块文件。|
    |关闭|根据文件描述符关闭文件。|
    |存在|合成路径后，检查文件存在性。|
    |移除块|合成路径后，移除文件。|
    |获取元数据|合成路径或根据文件描述符，获取文件元数据。|
    |释放块的句柄|释放结构体空间。|
    |遍历块|合成仓库路径，遍历文件名得到block_id，随后呈递给用户函数。|
    |复制块|分别合成两个路径，然后系统调用复制文件。|
    |移除仓库存储|遍历目录下所有文件，并移除。|

## 块管理器

[block-mgr](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/block-mgr.h)：块存储系统的最高层封装。除了上述操作外，块管理器中还封装了以下操作：

1. 获取仓库中块的数量

    利用块遍历操作，获取仓库中块的数量。

2. 验证块

    用块的内容重新计算校验和，与块的校验和（文件系统实现中是block_id）对比，判断块是否被篡改。

## 块传输协议

[block-tx-utils](https://github.com/poi0qwe/seafile-server-learn/blob/main/common/block-tx-utils.h)：块传输协议实用函数。块传输系统独立于块存储系统，实现了传输协议。该部分内容是服务端与客户端共用的。

- ### 状态码

    ```c++
    enum {
        STATUS_OK = 0,                // OK
        STATUS_VERSION_MISMATCH,      // 版本不匹配
        STATUS_BAD_REQUEST,           // 请求错误
        STATUS_ACCESS_DENIED,         // 拒绝访问
        STATUS_INTERNAL_SERVER_ERROR, // 服务器内部错误
        STATUS_NOT_FOUND,             // 未找到
    };
    ```

    seafile的块传输协议有多个版本（目前的版本号为2；不同版本的加密方式不同），所以存在版本不匹配的情况。

- ### 协议头类型

    1. 握手请求/响应头

        请求：版本、密钥长度、会话密钥。

        响应：状态码。

    2. 权限头

        请求：空。（项目中无展示）

        响应：状态码。

    3. 块操作头

        请求：命令（GET/PUT）、块id。

        响应：状态码。

- ### 成帧

    一个帧的结构如下：

    |类型|描述|可选性|
    |:-|:-|:-|
    |权限头|权限请求或响应|*|
    |块操作头|块请求或响应；请求头包含命令|*|
    |块数据|块的内容|仅块响应帧|

- ### 协议流程

    1. 收方发送握手请求，发方接收握手响应，收方接收权限响应；
    2. 收方发送权限请求(空)、块请求；
    3. 发方发送权限响应、块响应、块内容。

- ### 帧加密

    定义了帧转化器FrameParser，作为加密解密的上下文：

    ```c++
    typedef struct _FrameParser { // 帧转化器
        int enc_frame_len; // 帧长度

        // 版本1的密钥和初始向量
        unsigned char key[ENC_KEY_SIZE];
        unsigned char iv[ENC_BLOCK_SIZE];
        gboolean enc_init; // 是否已初始化加密
        EVP_CIPHER_CTX *ctx; // 对称加密上下文

        // 版本2的密钥和初始向量
        unsigned char key_v2[ENC_KEY_SIZE];
        unsigned char iv_v2[ENC_BLOCK_SIZE];

        int version; // 版本

        /* Used when parsing fragments */
        int remain; // 剩余长度，解密时使用

        FrameContentCB content_cb; // 帧回调函数
        FrameFragmentCB fragment_cb; // 片段回调函数
        void *cbarg; // 用户参数
    } FrameParser;
    ```

    每个帧都以加密形式（AES-256 CBC）被发送。一个加密帧的内容如下：

    |内容|描述|长度|
    |:-|:-|:-|
    |帧长|加密帧总长度|一个32位整型数|
    |加密数据|加密帧数据，包括所有协议头|帧长|

    加密后的帧长度被记录在`enc_frame_len`中。

- ### 帧解密

    有两种解密数据的方式：

    1. 帧解密：解密整个帧，然后回调；（handle_one_frame）

        若`enc_frame_len`不为零，则帧长给定；否则从缓冲区中读第一个整型数作为帧长。

        接下来试着从缓冲区中解密**完整的帧**。若缓冲区中数据不足，就返回0；反之，将解密后的帧发送给回调函数。

    2. 片段解密：解密一个片段，然后回调。（handle_frame_segments）

        若`enc_frame_len`不为零，则帧长给定；否则从缓冲区中读第一个整型数作为帧长。

        接下来试着从缓冲区中**尽可能解密**该帧。每解密一段数据，就将其发送给回调函数。同时更新`remain`，表示该帧还剩多少数据没被解密。

    可以认为前者是粗粒度，后者是细粒度。之所以这样区分是因为在解密一个帧时，缓冲区有可能只接收到了此帧的某个片段，这时候开发者需要选择是否等待接收完整的帧。若等待，则是帧解密，开发者通过帧回调函数处理完整的帧；若不等待，则是片段解密，开发者通过片段回调函数处理这个片段。

    <small>附注：关于AES-256-CBC加密与PKCS填充
    
    1. AES-256：指的是使用256位密钥；
    2. 每个加密块大小为16个字节；需要一个填充块(padding block)来使加密数据长度为加密块长的整数倍；
    3. 假设原始数据有$n$个字节，对其进行分块加密，则可得到$\lfloor\frac{n}{16}\rfloor$个整块和一个填充块；
    4. 反推出加密后的数据总长度为`((n>>4)+1)<<4`个字节；
    5. 基于上下文的加密和解密可以从任意位置开始，处理任意长度的连续的数据；
    6. CBC加密和解密的方式类似滚动哈希，不可并发；
    7. PKCS Padding会在后面加上$n$个值为$n$的字节，使得加密后的数据长度为加密块长度的整数倍。
    
    附注：openssl相关函数

    - 加密：EVP_EncryptUpdate：进行加密；EVP_EncryptFinal_ex：对剩余部分填充并加密；
    - 解密：EVP_DecryptUpdate：进行解密；EVP_DecryptFinal_ex：对填充块解密。
    </small>
