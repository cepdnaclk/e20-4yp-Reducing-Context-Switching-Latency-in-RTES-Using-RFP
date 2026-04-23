/*
 * Minimal libc stubs for bare-metal FreeRTOS demo (-nostdlib).
 * Provides memset, memcpy, and __clzsi2 so the kernel links without libc/libgcc.
 */

#include <stddef.h>

void *memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

/* Count leading zeros (for __builtin_clz when no -lgcc). Used by task selection. */
int __clzsi2(unsigned int x)
{
    if (x == 0) return 32;
    int n = 0;
    if (x <= 0x0000FFFFU) { n += 16; x <<= 16; }
    if (x <= 0x00FFFFFFU) { n += 8;  x <<= 8; }
    if (x <= 0x0FFFFFFFU) { n += 4;  x <<= 4; }
    if (x <= 0x3FFFFFFFU) { n += 2;  x <<= 2; }
    if (x <= 0x7FFFFFFFU) { n += 1; }
    return n;
}
