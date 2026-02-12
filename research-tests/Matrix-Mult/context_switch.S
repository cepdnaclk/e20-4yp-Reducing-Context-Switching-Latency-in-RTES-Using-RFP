.global perform_switch_test
.text

# unsigned long perform_switch_test(void)
perform_switch_test:
    # --- WINDOW A START ---

    # 1. Set Target Window to B
    li t0, 0x00200020
    csrw 0x801, t0     
    
    # 2. PULL THE TRIGGER (Force Trap -> mret -> Switch)
    ecall

    # --- WINDOW B START ---
    # Hardware has now switched!
    
    # 3. Read CSR 0x800 to verify
    csrr a0, 0x800
    
    # 4. Save result to MSCRATCH
    csrw mscratch, a0
    
    # 5. Set Target Window back to A
    li t0, 0x00200000
    csrw 0x801, t0

    # 6. PULL THE TRIGGER (Force Trap -> mret -> Switch)
    ecall

    # --- WINDOW A RESTORED ---
    
    # 7. Retrieve result
    csrr a0, mscratch
    
    # 8. Return
    ret