/*
 * Startup for FreeRTOS CVA6 tests.
 */
.global _start
.extern main
.extern freertos_risc_v_trap_handler

.section .text.init
_start:
    la sp, stack_top
    la t0, freertos_risc_v_trap_handler
    csrw mtvec, t0
    call main

    /* Fallback exit if main returns unexpectedly. */
    li t0, 0
    la t1, tohost
    sw t0, 0(t1)
1:
    wfi
    j 1b
