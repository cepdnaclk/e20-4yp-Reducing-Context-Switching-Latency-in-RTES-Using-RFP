#ifndef CVA6_FYP_COMMON_H
#define CVA6_FYP_COMMON_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"

#define UART0_ADDR    ( ( volatile uint8_t * ) 0x10000000UL )

extern volatile uint64_t tohost;

static inline void cv6_putstr( const char * s )
{
    while( *s != '\0' )
    {
        *UART0_ADDR = ( uint8_t ) *s++;
    }
}

static inline void cv6_puthex32( uint32_t x )
{
    static const char hc[] = "0123456789ABCDEF";
    int i;

    cv6_putstr( "0x" );

    for( i = 7; i >= 0; --i )
    {
        *UART0_ADDR = ( uint8_t ) hc[ ( x >> ( i * 4 ) ) & 0xFU ];
    }
}

static inline void cv6_write_signature( uint32_t pass,
                                        uint32_t phaseCode,
                                        uint32_t aux0,
                                        uint32_t aux1 )
{
    volatile uint32_t * const sig = ( volatile uint32_t * ) 0x120UL;

    sig[ 0 ] = phaseCode;
    sig[ 1 ] = aux0;
    sig[ 2 ] = aux1;
    sig[ 3 ] = pass;
}

static inline void cv6_finish( BaseType_t xPass,
                               uint32_t phaseCode,
                               uint32_t aux0,
                               uint32_t aux1 )
{
    cv6_write_signature( ( xPass != pdFALSE ) ? 1U : 0U, phaseCode, aux0, aux1 );
    tohost = ( xPass != pdFALSE ) ? 1ULL : 0ULL;

    for( ; ; )
    {
        __asm volatile ( "wfi" );
    }
}

void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                    StackType_t ** ppxIdleTaskStackBuffer,
                                    size_t * pulIdleTaskStackSize );

#endif /* CVA6_FYP_COMMON_H */
