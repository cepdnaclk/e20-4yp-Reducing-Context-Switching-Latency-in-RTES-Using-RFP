.global _start
.global trap_entry
.extern main
.extern run_scheduler

.section .text.init

# -----------------------------------------------------------
# 1. SYSTEM STARTUP
# -----------------------------------------------------------
_start:
    la sp, stack_top
    la t0, trap_entry
    csrw mtvec, t0
    call main
    li t0, 1
    la t1, tohost
    sw t0, 0(t1)
1:  j 1b

# -----------------------------------------------------------
# 2. SCHEDULER TRAP HANDLER
# -----------------------------------------------------------
.align 4
trap_entry:
    csrr a0, mepc
    addi a0, a0, 4   # Skip ecall
    call run_scheduler
    csrw mepc, a0    # Set return address
    mret             # TRIGGERS HARDWARE SWITCH

# (Note: task_bootstrap is removed)

.section .data
.align 8
.global tohost
tohost: .word 0
.align 4
.skip 4096
stack_top: