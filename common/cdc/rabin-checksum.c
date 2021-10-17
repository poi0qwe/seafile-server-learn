/*
拉宾指纹（滚动哈希）
https://zh.wikipedia.org/wiki/%E6%8B%89%E5%AE%BE%E6%8C%87%E7%BA%B9
*/

#include <sys/types.h>
#include "rabin-checksum.h"

#ifdef WIN32
#include <stdint.h>
#ifndef u_int
typedef unsigned int u_int;
#endif

#ifndef u_char
typedef unsigned char u_char;
#endif

#ifndef u_short
typedef unsigned short u_short;
#endif

#ifndef u_long
typedef unsigned long u_long;
#endif

#ifndef u_int16_t
typedef uint16_t u_int16_t;
#endif

#ifndef u_int32_t
typedef uint32_t u_int32_t;
#endif

#ifndef u_int64_t
typedef uint64_t u_int64_t;
#endif
#endif

#define INT64(n) n##LL
#define MSB64 INT64(0x8000000000000000)

static u_int64_t poly = 0xbfe6b8a5bf378d83LL; // GF(2)的一个不可约多项式
static u_int64_t T[256];
static u_int64_t U[256];
static int shift;

/* Highest bit set in a byte */
static const char bytemsb[0x100] = {
  0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
};
// bytemsb[x]表示x的最高位1在第几位；x是8位int

/* Find last set (most significant bit) */
static inline u_int fls32(u_int32_t v) // 对于int32，找到最高位1在第几位
{
    if (v & 0xffff0000) {
        if (v & 0xff000000)
            return 24 + bytemsb[v>>24];
        else
            return 16 + bytemsb[v>>16];
    }
    if (v & 0x0000ff00)
        return 8 + bytemsb[v>>8];
    else
        return bytemsb[v];
}

static inline char fls64(u_int64_t v) // 对于int64，找到最高位1在第几位
{
    u_int32_t h;
    if ((h = v >> 32))
        return 32 + fls32 (h);
    else
        return fls32 ((u_int32_t) v);
}

u_int64_t polymod (u_int64_t nh, u_int64_t nl, u_int64_t d) // GF(2)多项式的(nh<<64|nl)%d
{
    int i = 0;
    int k = fls64 (d) - 1;

    d <<= 63 - k;

    if (nh) {
        if (nh & MSB64)
            nh ^= d;
        for (i = 62; i >= 0; i--)
            if (nh & ((u_int64_t) 1) << i) {
                nh ^= d >> (63 - i);
                nl ^= d << (i + 1);
            }
    }
    for (i = 63; i >= k; i--)
    {  
        if (nl & INT64 (1) << i)
            nl ^= d >> (63 - i);
    }
  
    return nl;
}

void polymult (u_int64_t *php, u_int64_t *plp, u_int64_t x, u_int64_t y) // GF(2)多项式的x*y=(php<<64)|plp
{
    int i;
    u_int64_t ph = 0, pl = 0;
    if (x & 1)
        pl = y;
    for (i = 1; i < 64; i++)
        if (x & (INT64 (1) << i)) {
            ph ^= y >> (64 - i);
            pl ^= y << i;
        }
    if (php)
        *php = ph;
    if (plp)
        *plp = pl;
}

u_int64_t polymmult (u_int64_t x, u_int64_t y, u_int64_t d) // GF(2)多项式的x*y%d
{
    u_int64_t h, l;
    polymult (&h, &l, x, y);
    return polymod (h, l, d);
}

static u_int64_t append8 (u_int64_t p, u_char m) // (p*a+m)%poly
{
    return ((p << 8) | m) ^ T[p >> shift]; // (p<<8)^T[p>>shift]=(r*j%poly)^(p%2^xshift)
}

static void calcT (u_int64_t poly) // 计算T
{
    int j = 0;
    int xshift = fls64 (poly) - 1; // poly最高位1在第几位
    shift = xshift - 8;
    u_int64_t T1 = polymod (0, INT64 (1) << xshift, poly); // t=x^(xshift), a=x^8
    for (j = 0; j < 256; j++) {
        T[j] = polymmult (j, T1, poly) | ((u_int64_t) j << xshift); // j*t%poly；j<<xshift用于消去高八位
    }
}

static void calcU(int size) // 计算U=a^len*c
{
    int i;
    u_int64_t sizeshift = 1;
    for (i = 1; i < size; i++)
        sizeshift = append8 (sizeshift, 0); // 计算a^len%poly
    for (i = 0; i < 256; i++)
        U[i] = polymmult (i, sizeshift, poly); // 缓存(a^len)*i%poly
}

void rabin_init(int len) // 初始化
{
    calcT(poly); // 计算T
    calcU(len); // 初始化U
}

/*
 *   a simple 32 bit checksum that can be upadted from end
 */
unsigned int rabin_checksum(char *buf, int len) // 首次计算
{
    int i;
    unsigned int sum = 0;
    for (i = 0; i < len; ++i) {
        sum = rabin_rolling_checksum (sum, len, 0, buf[i]);
    }
    return sum;
}

unsigned int rabin_rolling_checksum(unsigned int csum, int len,
                                    char c1, char c2) // 滚动计算
{
    return append8(csum ^ U[(unsigned char)c1], c2); // (csum*a+c2-a^len*c1)%poly
}
