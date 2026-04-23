.global _start
.global software_trap_entry
.global hardware_trap_entry
.global perform_dummy_software_save_restore
.extern main
.extern run_scheduler
.extern pending_window

.section .text.init

_start:
    la sp, stack_top
    call main
shutdown:
    li t0, 1
    la t1, tohost
    sw t0, 0(t1)
    sw zero, 4(t1)
1:  wfi             
    j 1b

# =========================================================
# BASELINE: SOFTWARE TRAP HANDLER (Keep 16 reg save/restore)
# =========================================================
.align 4
software_trap_entry:
    addi sp, sp, -64
    sw ra, 0(sp)
    sw t0, 4(sp)
    sw t1, 8(sp)
    sw t2, 12(sp)
    sw t3, 16(sp)
    sw t4, 20(sp)
    sw t5, 24(sp)
    sw t6, 28(sp)
    sw a0, 32(sp)
    sw a1, 36(sp)
    sw a2, 40(sp)
    sw a3, 44(sp)
    sw a4, 48(sp)
    sw a5, 52(sp)
    sw a6, 56(sp)
    sw a7, 60(sp)

    csrr t0, mcause
    li t1, 8
    beq t0, t1, 1f
    li t1, 11
    beq t0, t1, 1f
panic_soft: j panic_soft  

1:  csrr a0, mepc
    addi a0, a0, 4
    call run_scheduler   
    csrw mepc, a0        

    lw ra, 0(sp)
    lw t0, 4(sp)
    lw t1, 8(sp)
    lw t2, 12(sp)
    lw t3, 16(sp)
    lw t4, 20(sp)
    lw t5, 24(sp)
    lw t6, 28(sp)
    lw a0, 32(sp)
    lw a1, 36(sp)
    lw a2, 40(sp)
    lw a3, 44(sp)
    lw a4, 48(sp)
    lw a5, 52(sp)
    lw a6, 56(sp)
    lw a7, 60(sp)
    addi sp, sp, 64
    mret

# =========================================================
# PROPOSED: HARDWARE TRAP HANDLER (Zero Overhead!)
# =========================================================
.align 4
hardware_trap_entry:
    # No user registers to save! HW already swapped to Window 0.
    # We only save 'ra' so the C compiler doesn't break our return path.
    addi sp, sp, -16
    sw ra, 12(sp)

    # 1. Update MEPC & Run Scheduler
    csrr a0, mepc
    addi a0, a0, 4
    call run_scheduler   
    csrw mepc, a0        

    # 2. Restore Window 0 'ra' and stack pointer
    lw ra, 12(sp)
    addi sp, sp, 16
    
    # 3. Handle HW Window Shift
    la t0, pending_window
    lw t1, 0(t0)
    sw zero, 0(t0)
    beqz t1, 2f
    csrw 0x801, t1
2:  mret

# =========================================================
# BASELINE DUMMY SAVE SIMULATION
# =========================================================
.align 4
perform_dummy_software_save_restore:
    addi sp, sp, -128
    li t0, 32
    mv t1, sp
1:  sw t0, 0(t1)
    addi t1, t1, 4
    addi t0, t0, -1
    bnez t0, 1b
    li t0, 32
    mv t1, sp
2:  lw t2, 0(t1)
    addi t1, t1, 4
    addi t0, t0, -1
    bnez t0, 2b
    addi sp, sp, 128
    ret

.section .data
.align 8
.global tohost
.global fromhost
tohost: .dword 0
fromhost: .dword 0
.align 4
.skip 8192
stack_top: