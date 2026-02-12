#include <stdint.h>

// --- TCB & Config ---
typedef struct {
    uint32_t *sp;
    uint32_t pc;
    uint32_t window_cfg;
    uint32_t id;
} TCB_t;

TCB_t tcb_a = {0, 0, 0x00200000, 0}; // Window 0
TCB_t tcb_b = {0, 0, 0x00200020, 1}; // Window 1
TCB_t *current_task = &tcb_a;
uint32_t stack_b[1024];

extern void task_bootstrap(void);

// --- Scheduler (Trigger for the Debug Dump) ---
unsigned long run_scheduler(unsigned long old_pc) {
    current_task->pc = old_pc;
    TCB_t *next_task = (current_task == &tcb_a) ? &tcb_b : &tcb_a;
    
    // The write to 0x801 will trigger our new C++ Debug Print
    asm volatile("csrw 0x801, %0" :: "r"(next_task->window_cfg));
    
    current_task = next_task;
    asm volatile("csrw mscratch, %0" :: "r"(current_task));
    return next_task->pc;
}

// --- Task A: The "Alpha" Pattern ---
void task_a_main() {
    // Fill registers with 0xAAAA....
    register long x1  asm("x1")  = 0xAAAA0001;
    register long x2  asm("x2")  = 0xAAAA0002;
    register long x3  asm("x3")  = 0xAAAA0003;
    register long x4  asm("x4")  = 0xAAAA0004;
    // ... we can stop at x4 for brevity, but the logic holds for all.

    // Yield to B
    asm volatile("ecall" : "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4)); 
    
    // When we return, x1 MUST still be 0xAAAA0001
    while(1); 
}

// --- Task B: The "Bravo" Pattern ---
void task_b_main() {
    // Fill registers with 0xBBBB....
    register long x1  asm("x1")  = 0xBBBB0001;
    register long x2  asm("x2")  = 0xBBBB0002;
    register long x3  asm("x3")  = 0xBBBB0003;
    register long x4  asm("x4")  = 0xBBBB0004;

    // Yield back to A (Triggering the dump)
    asm volatile("ecall" : "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4)); 
    
    while(1);
}

volatile char* const UART0 = (char*)0x10000000;
void print_str(const char* s) { while (*s) *UART0 = *s++; }

// We use a naked function to prevent the compiler from generating 
// stack code that would crash during the window switch.
void __attribute__((naked)) hardware_test_asm() {
    asm volatile (
        // ---------------------------------------------------
        // PHASE 1: FILL WINDOW 0
        // ---------------------------------------------------
        "li  x3, 0xAAAA0003\n"
        "li  x4, 0xAAAA0004\n"

        // ---------------------------------------------------
        // PHASE 2: PREPARE THE SWITCH
        // ---------------------------------------------------
        // We want to jump to label '1f' (forward) after the switch
        "la  t0, 1f\n"       
        "csrw mepc, t0\n"     // Set return address

        // PREPARE WINDOW 1 CONFIG (Enable | Base 32) -> 0x00200020
        "li  t0, 0x00200020\n"
        "csrw 0x801, t0\n"    // Write to Staged CSR

        // ---------------------------------------------------
        // PHASE 3: EXECUTE SWITCH (The Magic Moment)
        // ---------------------------------------------------
        // This triggers the commit: Active Window becomes 1
        "mret\n"

        // ---------------------------------------------------
        // PHASE 4: FILL WINDOW 1
        // ---------------------------------------------------
        "1:\n"                // Label where we land
        "li  x3, 0xBBBB0003\n" // This should go to Window 1
        "li  x4, 0xBBBB0004\n"

        // ---------------------------------------------------
        // PHASE 5: SWITCH BACK TO WINDOW 0
        // ---------------------------------------------------
        "la  t0, 2f\n"
        "csrw mepc, t0\n"
        
        "li  t0, 0x00200000\n" // Config: Enable | Base 0
        "csrw 0x801, t0\n"     // Triggers your DUMP 1
        "mret\n"

        "2:\n"
        // ---------------------------------------------------
        // FINISH (Trigger DUMP 2 to verify)
        // ---------------------------------------------------
        // Trigger one last dump by writing 0x801 again without moving
        "li  t0, 0x00200000\n"
        "csrw 0x801, t0\n" 
        
        // Loop forever
        "3: j 3b\n"
    );
}

int main() {
    print_str("\n[TEST] Starting True Hardware Switch Test (using MRET)...\n");
    hardware_test_asm();
    return 0;
}