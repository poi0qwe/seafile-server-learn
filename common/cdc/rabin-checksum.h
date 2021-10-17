/*
拉宾指纹（滚动哈希）
https://zh.wikipedia.org/wiki/%E6%8B%89%E5%AE%BE%E6%8C%87%E7%BA%B9
*/

#ifndef _RABIN_CHECKSUM_H
#define _RABIN_CHECKSUM_H

unsigned int rabin_checksum(char *buf, int len); // 拉宾指纹，第一次生成

unsigned int rabin_rolling_checksum(unsigned int csum, int len, char c1, char c2); // 拉宾指纹，滚动生成

void rabin_init (int len); // 初始化

#endif
