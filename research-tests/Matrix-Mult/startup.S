.global _start
.global trap_entry
.global task_bootstrap
.extern main
.extern run_scheduler  # The C function we call in Phase 4b

.section .text.init

# -----------------------------------------------------------
# 1. SYSTEM STARTUP
# -----------------------------------------------------------
_start:
    # A. Setup Stack for Kernel (Task A)
    la sp, stack_top

    # B. Setup Trap Vector
    la t0, trap_entry
    csrw mtvec, t0

    # C. Jump to C Main
    call main

    # D. Exit Mechanism (Write to tohost)
    li t0, 1
    la t1, tohost
    sw t0, 0(t1)
1:  j 1b

# -----------------------------------------------------------
# 2. SCHEDULER TRAP HANDLER (Phase 4b)
# -----------------------------------------------------------
.align 4
trap_entry:
    # 1. Read the PC where the trap happened
    csrr a0, mepc
    
    # 2. Advance PC by 4 (to skip the 'ecall' instruction)
    addi a0, a0, 4
    
    # 3. Call the C Scheduler
    #    Input:  a0 = Old Task's PC
    #    Output: a0 = New Task's PC
    call run_scheduler
    
    # 4. Update mepc with the new task's address
    csrw mepc, a0
    
    # 5. Switch! (Hardware reads CSR 0x801, which C code just updated)
    mret

# -----------------------------------------------------------
# 3. TASK BOOTSTRAP (The "Trampoline")
# -----------------------------------------------------------
# This is where a NEW task lands after its very first mret.
.align 4
task_bootstrap:
    # 1. Recover the TCB Pointer from mscratch
    #    (The kernel stored it there before switching)
    csrr t0, mscratch
    
    # 2. Load the Stack Pointer from the TCB
    #    FreeRTOS rule: SP is always at offset 0 of TCB
    lw sp, 0(t0)            
    
    # 3. Jump to the Task's C code
    #    (Since this is the first run, we don't need to pop registers)
    call task_b_main

    # 4. Safety: If task returns, trap back to kernel
    ecall

# -----------------------------------------------------------
# 4. DATA SECTION (The Missing Part!)
# -----------------------------------------------------------
.section .data
.align 8             # Align to 8 bytes to be safe
.global tohost
tohost: .word 0      # Spike watches this memory location for exit

# Define a small stack for the kernel
.align 4
            .skip 4096
stack_top: