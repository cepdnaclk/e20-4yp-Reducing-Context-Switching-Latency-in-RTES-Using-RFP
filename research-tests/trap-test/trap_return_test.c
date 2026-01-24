#include <stdint.h>

// --- Minimal Hardware Support ---
volatile char* const UART0 = (char*)0x10000000;
void print_char(char c) { *UART0 = c; }
void print_str(const char* s) { while (*s) print_char(*s++); }
void print_hex(unsigned long val) {
    char hex[] = "0123456789ABCDEF";
    print_str("0x");
    for (int i = 7; i >= 0; i--) print_char(hex[(val >> (i * 4)) & 0xF]);
}
// --------------------------------

volatile int trap_handled = 0;

void __attribute__((interrupt("machine"), aligned(128))) handle_trap(void) {
    // 1. Advance PC to skip ecall
    unsigned long mepc;
    asm volatile ("csrr %0, mepc" : "=r"(mepc));
    asm volatile ("csrw mepc, %0" :: "r"(mepc + 4));

    // 2. Mark handled
    trap_handled = 1;
    
    // 3. Return (compiler generates 'mret')
    // Phase 2 Hardware should detect this mret and restore the Task Window
}

int main() {
    print_str("\n=== Phase 2: Trap Return Test ===\n");

    // 1. Setup Trap Vector
    unsigned long mtvec_val = (unsigned long)handle_trap;
    asm volatile ("csrw mtvec, %0" :: "r"(mtvec_val & ~3));

    // 2. Define Task Window (Base 8, Size 8) -> 0x00080008
    unsigned long task_config = 0x00080008;
    unsigned long kernel_config = 0x00200000; // Base 0, Size 32
    unsigned long window_after_mret = 0;
    
    print_str("1. Switching to Task Window: ");
    print_hex(task_config);
    print_str("\n");
    print_str("2. Triggering Trap (ECALL)...\n");

    // 3. SAFE BLOCK: Switch -> Trap -> Verify -> Restore
    // We do this all in ASM so the C compiler doesn't touch the stack 
    // while we are in the Task Window.
    asm volatile (
        // A. Switch to Task Window
        "csrw 0x800, %[cfg]\n\t"
        
        // B. Trigger Trap
        //    -> Hardware forces Win 0
        //    -> Handler runs
        //    -> mret restores Win 8
        "ecall\n\t"
        
        // C. We are back! Check where we are.
        //    Read current window into output var.
        "csrr %[res], 0x800\n\t"
        
        // D. URGENT: Restore Kernel Window so C code works again!
        "csrw 0x800, %[k_cfg]\n\t"
        
        : [res] "=r" (window_after_mret)
        : [cfg] "r" (task_config), [k_cfg] "r" (kernel_config)
        : "memory"
    );

    // 4. Verification
    if (trap_handled) {
        if (window_after_mret == task_config) {
            print_str("[PASS] Window correctly restored after mret.\n");
        } else {
            print_str("[FAIL] mret did not restore window.\n");
            print_str("       Expected: "); print_hex(task_config);
            print_str("\n       Got:      "); print_hex(window_after_mret);
            print_str("\n");
        }
    } else {
        print_str("[FAIL] Trap handler did not run.\n");
    }

    // Force exit
    asm volatile (
        "li t0, 1\n\t"
        "la t1, tohost\n\t"
        "sw t0, 0(t1)\n\t"
        "1: j 1b" 
        : : : "t0", "t1", "memory"
    );
    
    return 0;
}