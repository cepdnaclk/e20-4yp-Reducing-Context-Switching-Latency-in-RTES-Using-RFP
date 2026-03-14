/*
 * Startup for FreeRTOS on Spike RV32.
 * Sets sp, mtvec to the kernel trap handler, then calls main().
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
    /* Exit for Spike: write 1 to tohost (symbol provided by link.ld). */
    li t0, 1
    la t1, tohost
    sw t0, 0(t1)
1:  j 1b
