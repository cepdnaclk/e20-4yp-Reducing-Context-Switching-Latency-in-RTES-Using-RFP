.section .text.init
.global _start
.global tohost

_start:
    # 1. Setup Stack Pointer (using symbol from link.ld)
    la sp, stack_top

    # 2. Jump to C Main
    call main

    # 3. Exit Trap
    li t0, 1
    la t1, tohost
    sw t0, 0(t1)
1:  j 1b

.section .data
.align 8
tohost: .word 0