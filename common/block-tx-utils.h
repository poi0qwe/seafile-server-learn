/* 块传输协议实用方法 */

#ifndef BLOCK_TX_UTILS_H
#define BLOCK_TX_UTILS_H

#include <event2/buffer.h>
#include <event2/util.h>
#include <openssl/evp.h>

/* Common structures and contents shared by the client and server. */
// 客户端、服务端通用数据结构和内容

/* We use AES 256 */ // AES-256加密
#define ENC_KEY_SIZE 32 // 密钥长度
#define ENC_BLOCK_SIZE 16 // 加密块长度

#define BLOCK_PROTOCOL_VERSION 2 // 协议版本

enum
{                                 // 状态
    STATUS_OK = 0,                // OK
    STATUS_VERSION_MISMATCH,      // 版本不匹配
    STATUS_BAD_REQUEST,           // 请求错误
    STATUS_ACCESS_DENIED,         // 拒绝访问
    STATUS_INTERNAL_SERVER_ERROR, // 服务器内部错误
    STATUS_NOT_FOUND,             // 未找到
};

struct _HandshakeRequest { // 握手请求
    gint32 version; // 版本
    gint32 key_len; // 密钥长度
    char enc_session_key[0]; // 会话密钥
} __attribute__((__packed__));

typedef struct _HandshakeRequest HandshakeRequest;

struct _HandshakeResponse { // 握手响应
    gint32 status; // 状态
    gint32 version; // 版本
} __attribute__((__packed__));

typedef struct _HandshakeResponse HandshakeResponse;

struct _AuthResponse { // 权限响应
    gint32 status; // 状态
} __attribute__((__packed__));

typedef struct _AuthResponse AuthResponse;

enum { // 请求命令
    REQUEST_COMMAND_GET = 0, // GET
    REQUEST_COMMAND_PUT, // PUT
};

struct _RequestHeader { // 请求头
    gint32 command; // 命令
    char block_id[40]; // 块id
} __attribute__((__packed__));

typedef struct _RequestHeader RequestHeader;

struct _ResponseHeader { // 响应头
    gint32 status; // 状态
} __attribute__((__packed__));

typedef struct _ResponseHeader ResponseHeader;

/* Utility functions for encryption. */
// 加密实用方法

void
blocktx_generate_encrypt_key (unsigned char *session_key, int sk_len,
                              unsigned char *key, unsigned char *iv); // 生成密钥

int
blocktx_encrypt_init (EVP_CIPHER_CTX **ctx,
                      const unsigned char *key,
                      const unsigned char *iv); // 初始化加密

int
blocktx_decrypt_init (EVP_CIPHER_CTX **ctx,
                      const unsigned char *key,
                      const unsigned char *iv); // 初始化解密

/*
 * Encrypted data is sent in "frames". 加密数据以帧发送，帧的格式如下：
 * Format of a frame:
 *
 * length of data in the frame after encryption + encrypted data.
 * 加密解密后该帧的数据长度
 *
 * Each frame can contain three types of contents:
 * 1. Auth request or response;
 * 2. Block request or response header;
 * 3. Block content.
 * 每帧包含三种类型的内容：
 * 1. 权限请求或响应
 * 2. 块请求头或响应头
 * 3. 块数据
 */

/* 发送帧 */
int // 开始发送加密帧
send_encrypted_data_frame_begin (evutil_socket_t data_fd, // socket
                                 int frame_len); // 帧长度

int // 发送加密帧
send_encrypted_data (EVP_CIPHER_CTX *ctx, // 加密上下文
                     evutil_socket_t data_fd, // socket
                     const void *buf, int len); // 缓冲及其长度

int // 结束发送加密帧
send_encrypted_data_frame_end (EVP_CIPHER_CTX *ctx, // 加密上下文
                               evutil_socket_t data_fd); // socket

/* 接收帧 */
typedef int (*FrameContentCB) (char *, int, void *); // 定义帧内容回调函数（缓冲，长度，用户参数）

typedef int (*FrameFragmentCB) (char *, int, int, void *); // 定义帧片段回调函数（缓冲，长度，是否是帧尾，用户参数）

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
    int remain; // 剩余长度

    FrameContentCB content_cb;
    FrameFragmentCB fragment_cb;
    void *cbarg; // 用户参数
} FrameParser;

/* Handle entire frame all at once.
 * parser->content_cb() will be called after the entire frame is read.
 */
// 处理帧，结束后调用转化器中的content_cb；
int
handle_one_frame (struct evbuffer *buf, FrameParser *parser);

/* Handle a frame fragment by fragment.
 * parser->fragment_cb() will be called when any amount data is read.
 */
// 处理片段，每处理一个片段调用一次转化器中的fragment_cb；
int
handle_frame_fragments (struct evbuffer *buf, FrameParser *parser);

#endif
