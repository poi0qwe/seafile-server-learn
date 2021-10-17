# 基于内容长度可变分块

## 滚动哈希与拉宾指纹

### 滚动哈希

考虑解决字符串匹配问题。一种方法是利用字符串哈希值进行匹配。已知模式串长度为$n$，那么我们可以依次截取匹配串中长为$n$的子串计算哈希，然后与模式串的哈希进行比对，若相等则得到一次匹配。在极大概率下，哈希碰撞到的子串与模式串相同。

然后用算法实现。在已经计算过$\small s_{i\dots i+n}$哈希的前提下，如果要计算$\small s_{i+1\dots i+n+1}$的哈希，若采用暴力方法，则需要扫描整个子串，其复杂度与暴力匹配相同，显然需要继续优化。

> 拉宾-卡普算法：[https://zh.wikipedia.org/wiki/拉宾-卡普算法](https://zh.wikipedia.org/wiki/%E6%8B%89%E5%AE%BE-%E5%8D%A1%E6%99%AE%E7%AE%97%E6%B3%95)

考虑使用长为$n$的滑动窗口解决此问题：每扫过一个字符，从原哈希中删去旧字符的影响，然后加上新字符的影响，得到新哈希：

![https://infoarena.ro/blog/rolling-hash?action=download&file=image10.png&safe_only=true](https://img-blog.csdnimg.cn/16bef34ba3934c388bd31fa691e5bd3c.png)

若采用此方法，则需要设计一个哈希函数支持动态地增删首尾字符的影响。可以考虑使用素域$\small M$上的多项式映射，其中$\small n$为窗口长度：（注意必须是素数，这样值域上各个值的概率才几乎相等）

$$hash(s_{i\dots i+n-1})=(s_{i}a^{n-1}+s_{i+1}a^{n-2}+\dots +s_{i+n-1}a+s_{i+n-1})_{\pmod{M}}$$

那么易推知：

$$hash(s_{i+1\dots i+n})=(s_{i+1}a^{n-1}+s_{i+2}a^{n-2}+\dots +s_{i+n-1}a+s_{i+n})_{\pmod{M}}$$

两式的递推关系如下：

$$hash(s_{i+1\dots i+n})=(a·hash(s_{i\dots i+n-1})_{\pmod{M}}-s_ia^{n}+s_{i+n})_{\pmod{M}}$$

因此就可以以$\small O(1)$的复杂度得到下一个窗口的哈希值。

```python
# https://www.infoarena.ro/blog/rolling-hash
# a is a constant
an = 1 # a^n
rolling_hash = 0
for i in range(0, n):
    rolling_hash = (rolling_hash * a + S[i]) % MOD
    an = (an * a) % MOD
if rolling_hash == hash_p:
    # match
for i in range(1, m - n + 1):
    rolling_hash = (rolling_hash * a + S[i + n - 1] - an * S[i - 1]) % MOD
    if rolling_hash == hash_p:
        # match
```

其中$\small a^n$是常数，可以进行预处理。

### 拉宾指纹

> 拉宾指纹：[https://zh.wikipedia.org/wiki/拉宾指纹](https://zh.wikipedia.org/wiki/%E6%8B%89%E5%AE%BE%E6%8C%87%E7%BA%B9)

拉宾指纹也是一个多项式哈希映射，但它并非在素域$\small M$上，且映射结果不是一个值。拉宾指纹使用的是有限域$\small GF(2)$上的多项式，例如：$f(x)=x^3+x^2+1$。这个多项式可以使用二进制表示为$\small 1101$。

之所以使用这样的多项式表示，是因为相比传统的值运算，$\small GF(2)$多项式运算更简单：加减都是异或，这样就完全不需要考虑进位的问题。并且多项式的乘除性质与整数相似。不过就算不需要考虑进位，乘法和除法(求余)也只能以$\small k$的复杂度完成（$\small k$为多项式的最高次幂）。

拉宾指纹的哈希函数如下：（和素域类似，模需要是个不可约多项式）

$$hash(s_{i\dots i+n})=(s_{i}a(x)^n+s_{i+1}a(x)^{n-1}+\dots +s_{i+n-1}a(x)+s_{i+n})_{\pmod{M(x)}}$$

递推式如下：

$$hash(s_{i+1\dots i+n})=((a(x)·hash(s_{i\dots i+n-1}))_{\pmod{M(x)}}-s_ia^{n}(x)+s_{i+n})_{\pmod{M(x)}}$$

### 实现

首先选择有限域$\small GF(2)$多项式的模$p(x)$，要求是个不可约多项式。这里选取了一个$\small k=64$的多项式。

```c++
static u_int64_t poly = 0xbfe6b8a5bf378d83LL;
```

考虑滑动窗口每次滑动一个字节，则可以令$a(x)=x^8$。

根据拉宾指纹递推式，若直接运算，则需要进行$\small 2$次乘法和$\small 1$次求余，其复杂度为$\small O(k)$。虽说$\small k$是个常数，但如果优化了$k$，再结合$\small GF(2)$多项式无需进位的性质，拉宾指纹的性能甚至会超过传统哈希。

将递推式分为三个部分：不可预处理的乘法部分$\small hash(s_{i\dots i+n})·a(x)$、可预处理的乘法部分$s_ia^{n+1}$、加法部分$\small s_{i+n+1}$。第一个部分不可预处理，因为哈希值是不可预知的；第二个部分可预处理，因为$s_i$作为一个字节只有$\small 2^8$种取值，且$a^n(x)$也是一个常量；最后一个部分只需一个异或运算，可忽略。

首先考虑第二部分。先实现多项式乘除法，然后易求得$a^n(x)$，接着枚举$s_i$的值并求得$s_ia^n(x)$，最后将结果缓存到$\small U$表中。对幂运算部分继续优化：假如已知处理第一部分的算法为`MUL(p, a)`，其复杂度为$\small O(1)$，那么幂运算就等价于`p=MUL(p, a)`进行$n$次，这样就优化掉了常数$k$。

然后考虑优化第一部分，我们需要计算的是$(\small p(x)·a(x))_{\pmod{M(x)}}$。优化过程如下：

1. 对乘法优化

    发现每次都是乘以$\small a(x)=x^8$，若以二进制表示，就是左移八位。左移复杂度为$\small O(1)$。

2. 对求余优化

    令$\small g(x)$等于$\small p(x)$的最高次项$x^{shiftx}$，则必有$\small g(x)\le p(x)$。原式可改写为：
    
    $$((p-p_{\pmod{g/a}}+p_{\pmod{g/a}})·a)_{\pmod{M}}$$

    对其进行变换：

    $$\begin{array}{ll}=(p_{\pmod{g/a}}·a)_{\pmod{g/a}}+((p-p_{\pmod{g/a}})·a)_{\pmod{M}}\\=((p-(p-p_{\pmod{g/a}}))·a)_{\pmod{g/a}}+((p-p_{\pmod{g/a}})·a)_{\pmod{M}}\end{array}$$

    注意到第一个子式一定小于$g$，因此一定小于$a$。令$\small j·(\frac{g}{a})=p-p_{\pmod{g/a}}$，则上式可改写为：

    $$((p-j·(\frac{g}{a}))·a)+(g·j)_{\pmod{M}}$$

    其中$\small g/a=x^{shiftx-8}$，因此$p-p_{\pmod{g/a}}$就是保留$p$的高八位其余位填零；而$j$就是$p$的高八位。将各个式子改写为二进制形式，并使用位运算，则上式等价于：

    ```c++
    (p^j)<<8+g*j%(xshift-8)
    ```

继续改写：

```c++
p ^ pg*j%(xshift-8)|j<<(xshift+8)
```

然后预处理`pg*j%(xshift-8)|j<<(xshift+8)`并缓存至$\small T$，即可以$\small O(1)$计算第一部分。最后将其与第三部分结合，得到最终代码：

```c++
(p<<8|m)^T[p>>xshift-8]
```

代码如下：

```c++
int xshift, shift
static inline char fls64(u_int64_t v); // 获取最高位的1在哪一位
u_int64_t polymod(u_int64_t nh, u_int64_t nl, u_int64_t d); // (nh<<64|nl) % d
void polymult(u_int64_t *php, u_int64_t *plp, u_int64_t x, u_int64_t y); // x * y = (php<<64|plp)
static u_int64_t append8(u_int64_t p, u_char m) {
    return ((p << 8) | m) ^ T[p >> shift];
}
static void calcT (u_int64_t poly) { // T
    int j = 0;
    xshift = fls64(poly) - 1;
    shift = xshift - 8;
    u_int64_t T1 = polymod(0, INT64 (1) << xshift, poly);
    for (j = 0; j < 256; j++) {
        T[j] = polymmult(j, T1, poly) | ((u_int64_t) j << xshift);
    }
}
static void calcU(int size) // U
{
    int i;
    u_int64_t sizeshift = 1;
    for (i = 1; i < size; i++)
        sizeshift = append8(sizeshift, 0);
    for (i = 0; i < 256; i++)
        U[i] = polymmult(i, sizeshift, poly);
}
void rabin_init(int len) { // 初始化
    calcT(poly);
    calcU(len);
}
unsigned int rabin_checksum(char *buf, int len) { // 首次计算，窗口长度为len
    int i;
    unsigned int sum = 0;
    for (i = 0; i < len; ++i) {
        sum = rabin_rolling_checksum (sum, len, 0, buf[i]);
    }
    return sum;
}
unsigned int rabin_rolling_checksum(unsigned int csum, int len,
                                    char c1, char c2) { // 滚动计算
    return append8(csum ^ U[(unsigned char)c1], c2); // (csum*a+c2-a^len*c1)%poly
}
```

> 在git中也用到了此技术，源码位于：[git/diff-delta](https://github.com/git/git/blob/142430338477d9d1bb25be66267225fb58498d92/diff-delta.c)，可作参考学习

## CDC

### 问题的引入

1. ### 为什么要分块

    对于本地文件系统，分块用于解决孔问题、方便操作系统管理空间、降低扫描次数等。而对于网络文件系统更是如此，分块后只需同步那些被修改的块，比重新上传整个文件更有效率。

2. ### 定长分块

    现在对文件进行定长分块，假设文件中的内容为`abcdefg`，每四个字节分为一块，则分块后为`abcd|efg`。假如在头部加入了一个字符，内容变更为`0abcdefg`，则分块后为`0abc|defg`，发现两个块和之前完全不一样。这意味着如果要向网络文件系统同步此次修改，则需重新上传两个块。

3. ### 基于内容可变长度的分块

    假如我们基于内容进行分块，以`d`作为分隔标志，在`d`后产生断点，那么此时分块就变成了`0abcd|efg`。发现这样分块与之前的分块仅一个块不一致，也就是说只需重新上传这个不一致的块，相比定长分块效率大大提高。

### 滚动哈希与断点

显然不能总是以相同的内容作为分隔标志，例如若文件内容为`dd...d`，则分块后为`d|d|...|d|`。每个块仅包含一个字符，非常浪费空间，更不方便管理，违背了分块的初衷。

我们希望以某种概率分布选择分隔标志使得各个块有一个平均大小，同时又保证分隔标志的某种属性相同。发现哈希恰好能满足这两个性质。

哈希是伪随机的，对于$\small b$位的二进制哈希值，其出现的概率都是$2^{-b}$。这时候考虑此前的滚动哈希，假设滑动窗口每滑动一次产生的哈希都是随机的，那么上一次碰撞到下一次碰撞的长度即块长恰好呈几何分布：

$$pdf(l)=(1-2^{-b})^{l-1}·2^{-b}$$

那么期望的块长就是$2^b$。

### 上下限

虽然哈希使得块长有预期的平均，但这是基于随机的数据，仍然存在特殊的数据使得特殊情况出现，因此需要限制块长的最小与最大值。如果以最小块长或最大块长分块（非变长分块），则它的性质类似于定长分块。

在下面的实现中，选择平均块长为$2^{20}$字节（1MB），最小块长为$2^{18}$字节（256KB），最大块长为$2^{22}$字节（4MB），可以通过几何分布计算非变长分块出现的概率：

$$P(l\le 2^{18})+P(l\ge 2^{22})=\sum_{i=1}^{2^{18}}pdf(i)+\sum_{i=2^{22}}^{\infty}pdf(i)=1-\sum_{i=2^{18}}^{2^{22}}pdf(i)\approx 0.239515$$

### 实现

```c++
/*
    最小块长略与文件缓冲区略，主要看以下分块部分
*/ 
if (cur < block_min_sz - 1) // 最小块长
    cur = block_min_sz - 1;
while (cur < tail) { // 一直扫描直到达到tail
    fingerprint = (cur == block_min_sz - 1) ?  // 计算指纹
        finger(buf + cur - BLOCK_WIN_SZ + 1, BLOCK_WIN_SZ) : // 首次计算
        rolling_finger (fingerprint, BLOCK_WIN_SZ, 
                        *(buf+cur-BLOCK_WIN_SZ), *(buf + cur)); // 滚动计算

    if (((fingerprint & block_mask) ==  ((BREAK_VALUE & block_mask)))
        || cur + 1 >= file_descr->block_max_sz) // 碰撞，找到断点
    {
        if (file_descr->block_nr == file_descr->max_block_nr) {
            seaf_warning ("Block id array is not large enough, bail out.\n");
            free (buf);
            return -1;
        }
        gint64 idx_size = cur + 1;
        WRITE_CDC_BLOCK (cur + 1, write_data); // 将buf[0..cur]写入块
        if (indexed) // 记录已处理的长度
            *indexed += idx_size;
        break;
    } else { // 继续寻找
        cur ++;
    }
}
```

## 相关函数释义

> 具体注释详见：https://github.com/poi0qwe/seafile-server-learn/tree/main/common/cdc

- 将块写入文件 / WriteblockFunc

    ```c++
    static int default_write_chunk (CDCDescriptor *chunk_descr) // 默认写块文件的方法
    {
        char filename[NAME_MAX_SZ];
        char chksum_str[CHECKSUM_LENGTH *2 + 1];
        int fd_chunk, ret;

        memset(chksum_str, 0, sizeof(chksum_str));
        rawdata_to_hex (chunk_descr->checksum, chksum_str, CHECKSUM_LENGTH); // 将checksum转为HEX串
        snprintf (filename, NAME_MAX_SZ, "./%s", chksum_str); // 设置文件名为checksum的HEX串，并限定其最大长度为`NAME_MAX_SZ-1`
        fd_chunk = g_open (filename, O_RDWR | O_CREAT | O_BINARY, 0644); // 创建文件，并写文件
        if (fd_chunk < 0) // 打开文件失败
            return -1;    
        
        ret = writen (fd_chunk, chunk_descr->block_buf, chunk_descr->len); // 将缓冲写入文件，写n个
        close (fd_chunk); // 关闭文件
        return ret; // 返回写的结果
    }
    ```

- 初始化

    ```c++
    // 给定文件，初始化它的文件分块信息（只用到了文件大小）
    static int init_cdc_file_descriptor (int fd,
                                     uint64_t file_size,
                                     CDCFileDescriptor *file_descr)
    {
        int max_block_nr = 0;
        int block_min_sz = 0;

        file_descr->block_nr = 0; // 实际块数
        // 若为空，则设置默认值
        if (file_descr->block_min_sz <= 0)
            file_descr->block_min_sz = BLOCK_MIN_SZ;
        if (file_descr->block_max_sz <= 0)
            file_descr->block_max_sz = BLOCK_MAX_SZ;
        if (file_descr->block_sz <= 0)
            file_descr->block_sz = BLOCK_SZ;

        if (file_descr->write_block == NULL)
            file_descr->write_block = (WriteblockFunc)default_write_chunk; // 默认写块文件的方法

        block_min_sz = file_descr->block_min_sz; // 块的最小大小
        max_block_nr = ((file_size + block_min_sz - 1) / block_min_sz); // 计算最大块数（极值）
        file_descr->blk_sha1s = (uint8_t *)calloc (sizeof(uint8_t),
                                                max_block_nr * CHECKSUM_LENGTH); // 按照最大块数申请空间
        file_descr->max_block_nr = max_block_nr;

        return 0;
    }
    ```

- 写数据与校验和

    ```c++
    #define WRITE_CDC_BLOCK(block_sz, write_data)                \ // 写一个块（block_sz是块的大小，write_data表示是否写入硬盘）
    do {                                                         \
        int _block_sz = (block_sz);                              \
        chunk_descr.len = _block_sz;                             \ // 设置缓冲长度等于块的大小
        chunk_descr.offset = offset;                             \ // 设置偏移
        ret = file_descr->write_block (file_descr->repo_id,      \ // 调用写块文件的方法（默认是default_write_chunk）
                                    file_descr->version,      \
                                    &chunk_descr,             \
                crypt, chunk_descr.checksum,                     \
                                    (write_data));            \
        if (ret < 0) {                                           \ // 写失败
            free (buf);                                          \
            g_warning ("CDC: failed to write chunk.\n");         \
            return -1;                                           \
        }                                                        \
        memcpy (file_descr->blk_sha1s +                          \
                file_descr->block_nr * CHECKSUM_LENGTH,          \
                chunk_descr.checksum, CHECKSUM_LENGTH);          \ // 将checksum作为此块的sha1值，加入到blk_sha1s的末尾
        SHA1_Update (&file_ctx, chunk_descr.checksum, 20);       \ // 更新SHA1至checksum
        file_descr->block_nr++;                                  \ // 块数加一
        offset += _block_sz;                                     \ // 偏移加上块的大小
                                                                \
        memmove (buf, buf + _block_sz, tail - _block_sz);        \ // 更新buf，把已处理过的移除（通过将未处理的拷贝到开头）
        tail = tail - _block_sz;                                 \
        cur = 0;                                                 \ // tail，cur也随之进行相对移动
    }while(0); // 表示执行一次
    ```

- 分块主函数

    ```c++
    int file_chunk_cdc(int fd_src, // 文件标识符
                   CDCFileDescriptor *file_descr, // 文件分块信息，新建或更新
                   SeafileCrypt *crypt, // 加密信息
                   gboolean write_data, // 是否写入硬盘
                   gint64 *indexed) // 块索引
    // 注释详见：https://github.com/poi0qwe/seafile-server-learn/blob/main/common/cdc/cdc.c
    ```