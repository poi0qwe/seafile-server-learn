/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Description:
  
  The function pair "seafile_encrypt/seafile_decrypt" are used to
  encrypt/decrypt data in the seafile system, using AES 128 bit ecb
  algorithm provided by openssl.
*/  

#ifndef _SEAFILE_CRYPT_H
#define _SEAFILE_CRYPT_H

#include <openssl/aes.h>
#include <openssl/evp.h>


/* Block size, in bytes. For AES it can only be 16 bytes. */
#define BLK_SIZE 16
#define ENCRYPT_BLK_SIZE BLK_SIZE

struct SeafileCrypt {
    int version;
    unsigned char key[32];   /* set when enc_version >= 1 */
    unsigned char iv[16];
};

typedef struct SeafileCrypt SeafileCrypt;

SeafileCrypt *
seafile_crypt_new (int version, unsigned char *key, unsigned char *iv);

/*
  Derive key and iv used by AES encryption from @data_in.
  key and iv is 16 bytes for version 1, and 32 bytes for version 2.

  @data_out: pointer to the output of the encrpyted/decrypted data,
  whose content must be freed by g_free when not used.

  @out_len: pointer to length of output, in bytes

  @data_in: address of input buffer

  @in_len: length of data to be encrpyted/decrypted, in bytes 

  @crypt: container of crypto info.
  
  RETURN VALUES:

  On success, 0 is returned, and the encrpyted/decrypted data is in
  *data_out, with out_len set to its length. On failure, -1 is returned
  and *data_out is set to NULL, with out_len set to -1;
*/
// key和iv都被用作AES加密；版本2前是AES-128，版本2后是SHA-256
// in, out是输入和输出缓冲
// crypt是加密信息容器（包含版本、key、iv）
// (另注，以下生成的密钥都是HEX形式)

int // 根据输入数据生成密钥
seafile_derive_key (const char *data_in, int in_len, int version,
                    const char *repo_salt,
                    unsigned char *key, unsigned char *iv);

/* @salt must be an char array of size 65 bytes. */
// 盐的长度必须是65个字节（64位HEX串，再算上末尾的'\0'）
int // 生成随机盐
seafile_generate_repo_salt (char *repo_salt);

/*
 * Generate the real key used to encrypt data.
 * The key 32 bytes long and encrpted with @passwd.
 */
// 通过密码生成随机密钥，32位长（加密后48位长）
int
seafile_generate_random_key (const char *passwd,
                             int version,
                             const char *repo_salt,
                             char *random_key);

void // 通过密码生成仓库密钥(magic)
seafile_generate_magic (int version, const char *repo_id,
                        const char *passwd,
                        const char *repo_salt,
                        char *magic);

int // 检验密码和仓库密钥是否匹配
seafile_verify_repo_passwd (const char *repo_id,
                            const char *passwd,
                            const char *magic,
                            int version,
                            const char *repo_salt);

int // 解密随机密钥的密文，再生成随机密文的密钥
seafile_decrypt_repo_enc_key (int enc_version,
                             const char *passwd, const char *random_key,
                             const char *repo_salt,
                             unsigned char *key_out, unsigned char *iv_out);

int // 更新随机密钥
seafile_update_random_key (const char *old_passwd, const char *old_random_key,
                           const char *new_passwd, char *new_random_key,
                           int enc_version, const char *repo_salt);

int // AES加密数据
seafile_encrypt (char **data_out,
                 int *out_len,
                 const char *data_in,
                 const int in_len,
                 SeafileCrypt *crypt);


int // AES解密数据
seafile_decrypt (char **data_out,
                 int *out_len,
                 const char *data_in,
                 const int in_len,
                 SeafileCrypt *crypt);

int // 初始化解密
seafile_decrypt_init (EVP_CIPHER_CTX **ctx,
                      int version,
                      const unsigned char *key,
                      const unsigned char *iv);

#endif  /* _SEAFILE_CRYPT_H */
