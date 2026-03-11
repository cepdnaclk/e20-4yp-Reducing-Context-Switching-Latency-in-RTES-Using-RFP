.global _start
.global trap_entry
.global perform_dummy_software_save_restore
.extern main
.extern run_scheduler
.extern pending_window

.section .text.init

_start:
    la sp, stack_top
    la t0, trap_entry
    csrw mtvec, t0
    call main
    li t0, 1
    la t1, tohost
    sw t0, 0(t1)
1:  j 1b

# =========================================================
# TRAMPOLINE TRAP HANDLER (CORRECTED FOR AUTO-RESET HW)
# =========================================================
.align 4
trap_entry:
    # 1. SAVE WINDOW 0 CONTEXT
    # (Since HW auto-resets, 'sp' is safely Window 0's stack)
    addi sp, sp, -48
    sw ra, 32(sp)
    sw t0, 0(sp)
    sw t1, 4(sp)
    sw t2, 8(sp)
    sw t3, 12(sp)
    sw t4, 16(sp)
    sw a0, 20(sp)
    sw a1, 24(sp)
    sw a2, 28(sp)

    # 2. RUN SCHEDULER
    csrr a0, mepc
    addi a0, a0, 4
    call run_scheduler
    csrw mepc, a0
    
    # 3. CHECK PENDING SWITCH
    la t0, pending_window
    lw t1, 0(t0)
    sw zero, 0(t0)    # Clear flag immediately

    # 4. RESTORE WINDOW 0 CONTEXT
    # We MUST do this before CSRW, otherwise we leak stack memory!
    lw ra, 32(sp)
    lw a2, 28(sp)
    lw a1, 24(sp)
    lw a0, 20(sp)
    lw t4, 16(sp)
    lw t3, 12(sp)
    lw t2, 8(sp)
    lw t1, 4(sp)
    lw t0, 0(sp)
    addi sp, sp, 48

    # 5. PERFORM HARDWARE SWITCH
    beqz t1, just_mret   # If pending_window was 0, skip shift
    csrw 0x801, t1       # Instantly swap physical registers
    
just_mret:
    mret

# =========================================================
# BASELINE SIMULATION
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
tohost: .word 0
.align 4
.skip 8192
stack_top: