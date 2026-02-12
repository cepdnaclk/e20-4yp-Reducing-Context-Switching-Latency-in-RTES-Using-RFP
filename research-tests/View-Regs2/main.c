#include <stdint.h>

// Helper Macros
#define write_csr(reg, val) asm volatile ("csrw " #reg ", %0" :: "r"(val))
#define read_csr(reg) ({ unsigned long __tmp; \
  asm volatile ("csrr %0, " #reg : "=r"(__tmp)); \
  __tmp; })

// [CRITICAL] Global Variable for Stack Handoff
// Being in global memory (.data), this exists at the same address 
// for ALL windows.
volatile uint32_t global_sp_handoff = 0;

void simple_work(int id) {
    volatile int sum = 0;
    for (int i = 0; i < 5; i++) sum += (id + 1);
}

int main() {
    // Phase 1: Window 0
    simple_work(0); 

    // ------------------------------------------------
    // SWITCH TO WINDOW 1
    // ------------------------------------------------
    
    // 1. Save Stack Pointer to Global Memory
    asm volatile("sw sp, global_sp_handoff, t0");

    // 2. Prepare Switch (Base 32 | Enable)
    //    0x0020 (Size 32) << 16 | 0x0020 (Base 32)
    //    = 0x00200020
    write_csr(0x801, 0x00200020); 

    // 3. Commit Switch (using inline mret)
    asm volatile (
        "la t0, 1f\n"        
        "csrw mepc, t0\n"    
        "mret\n"             
        "1:"                 
        ::: "t0"
    );

    // ------------------------------------------------
    // INSIDE WINDOW 1
    // ------------------------------------------------
    
    // 4. Restore Stack Pointer from Global Memory
    //    If we don't do this, SP is 0x00000000 -> Crash!
    asm volatile("lw sp, global_sp_handoff");

    // 5. Do Work (This uses Window 1 registers!)
    simple_work(1);

    // ------------------------------------------------
    // SWITCH BACK TO WINDOW 0
    // ------------------------------------------------
    
    // 6. Save SP again
    asm volatile("sw sp, global_sp_handoff, t0");

    // 7. Prepare Switch (Base 0)
    write_csr(0x801, 0x00200000); 

    // 8. Commit
    asm volatile (
        "la t0, 2f\n"
        "csrw mepc, t0\n"
        "mret\n"
        "2:"
        ::: "t0"
    );

    // 9. Restore SP final time
    asm volatile("lw sp, global_sp_handoff");

    return 0;
}