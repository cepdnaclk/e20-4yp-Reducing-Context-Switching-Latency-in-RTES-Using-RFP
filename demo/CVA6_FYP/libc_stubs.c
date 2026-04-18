/*
 * Minimal libc stubs for bare-metal FreeRTOS demo (-nostdlib).
 */

#include <stddef.h>

void * memset( void * s, int c, size_t n )
{
    unsigned char * p = ( unsigned char * ) s;

    while( n-- != 0U )
    {
        *p++ = ( unsigned char ) c;
    }

    return s;
}

void * memcpy( void * dest, const void * src, size_t n )
{
    unsigned char * d = ( unsigned char * ) dest;
    const unsigned char * s = ( const unsigned char * ) src;

    while( n-- != 0U )
    {
        *d++ = *s++;
    }

    return dest;
}

int __clzsi2( unsigned int x )
{
    int n = 0;

    if( x == 0U )
    {
        return 32;
    }

    if( x <= 0x0000FFFFU ) { n += 16; x <<= 16; }
    if( x <= 0x00FFFFFFU ) { n += 8; x <<= 8; }
    if( x <= 0x0FFFFFFFU ) { n += 4; x <<= 4; }
    if( x <= 0x3FFFFFFFU ) { n += 2; x <<= 2; }
    if( x <= 0x7FFFFFFFU ) { n += 1; }

    return n;
}
