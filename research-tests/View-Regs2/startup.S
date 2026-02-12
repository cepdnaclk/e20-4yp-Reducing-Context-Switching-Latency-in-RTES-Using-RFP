.global _start
.global trap_entry
.global task_bootstrap
.extern main
.extern run_scheduler
.extern context_handoff_ptr  # [FIX] Import the global variable

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
    addi a0, a0, 4
    
    call run_scheduler
    
    csrw mepc, a0
    mret

# -----------------------------------------------------------
# 3. TASK BOOTSTRAP (The "Trampoline")
# -----------------------------------------------------------
.align 4
task_bootstrap:
    # [FIX] Load TCB Pointer from Global Memory, not mscratch
    # This works even if mscratch is 0 or banked per-window
    la t0, context_handoff_ptr
    lw t0, 0(t0)            # t0 now holds (TCB_t*)
    
    # Load Stack Pointer from TCB
    lw sp, 0(t0)            
    
    # Jump to C
    call task_b_main

    # Safety trap
    ecall

# -----------------------------------------------------------
# 4. DATA SECTION
# -----------------------------------------------------------
.section .data
.align 8
.global tohost
tohost: .word 0

.align 4
            .skip 4096
stack_top: