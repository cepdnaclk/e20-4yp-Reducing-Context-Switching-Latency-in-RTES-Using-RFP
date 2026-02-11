#include <stdint.h>

// --- Hardware Definitions ---
volatile char* const UART0 = (char*)0x10000000;
void print_str(const char* s) { while (*s) *UART0 = *s++; }
void print_hex(unsigned long val) {
    char hex[] = "0123456789ABCDEF";
    print_str("0x");
    for (int i = 7; i >= 0; i--) *UART0 = hex[(val >> (i * 4)) & 0xF];
}

// --- TCB Structure (Mini FtreeRTOS) ---
typedef struct {
    uint32_t *sp;        // Offset 0: Stack Pointer
    uint32_t pc;         // Offset 4: Program Counter (Saved mepc)
    uint32_t window_cfg; // Offset 8: Hardware Window ID (CSR 0x801)
    uint32_t id;         // For debug printing
} TCB_t;

// --- Globals ---
TCB_t tcb_a; // Kernel / Task A
TCB_t tcb_b; // User / Task B
TCB_t *current_task = &tcb_a;

// Stack for Task B (Task A uses the default system stack)
uint32_t stack_b[1024];

extern void task_bootstrap(void); // In startup.S

// =========================================================
// THE SCHEDULER (Called by assembly trap_entry)
// =========================================================
// Returns the PC to resume at.
unsigned long run_scheduler(unsigned long old_pc) {
    // 1. Save the state of the task that just paused
    current_task->pc = old_pc;

    // 2. Pick the next task (Round Robin)
    TCB_t *next_task;
    if (current_task == &tcb_a) {
        next_task = &tcb_b;
    } else {
        next_task = &tcb_a;
    }

    // 3. Hardware Context Switch Setup
    //    Write the target window to the staging CSR
    asm volatile("csrw 0x801, %0" :: "r"(next_task->window_cfg));
    
    // 4. Update OS State
    current_task = next_task;
    
    // 5. Update mscratch (So bootstrap knows where TCB is if needed)
    asm volatile("csrw mscratch, %0" :: "r"(current_task));

    // 6. Return the new PC to the Assembly handler
    return next_task->pc;
}

// =========================================================
// TASKS
// =========================================================

void task_a_main() {
    int counter = 0;
    while (1) {
        print_str("\n[TASK A] Window 0 (Kernel). Counter: "); 
        print_hex(counter++); 
        print_str("\n");
        
        // Manual Yield
        print_str("[TASK A] Yielding...\n");
        asm volatile("ecall"); 
        
        // We expect to resume here!
        print_str("[TASK A] Resumed!\n");
        
        // Delay loop to make it readable
        for(volatile int i=0; i<10000; i++);
    }
}

void task_b_main() {
    int counter = 0;
    while (1) {
        print_str("\n    [TASK B] Window 1 (User).   Counter: "); 
        print_hex(counter++); 
        print_str("\n");
        
        // Manual Yield
        print_str("    [TASK B] Yielding...\n");
        asm volatile("ecall");
        
        // We expect to resume here!
        print_str("    [TASK B] Resumed!\n");
        
        for(volatile int i=0; i<10000; i++);
    }
}

// =========================================================
// BOOTSTRAP
// =========================================================
int main() {
    print_str("\n=== Phase 4b: Round-Robin Scheduler ===\n");

    // --- Initialize Task A (Kernel) ---
    tcb_a.id = 0;
    tcb_a.window_cfg = 0x00200000; // Base 0, Size 32
    // PC/SP for A are already active, so we don't strictly need to set them yet,
    // but the scheduler will fill 'pc' when A yields.

    // --- Initialize Task B (User) ---
    tcb_b.id = 1;
    tcb_b.window_cfg = 0x00200020; // Base 32, Size 32
    tcb_b.sp = &stack_b[1023];
    tcb_b.pc = (uint32_t)task_bootstrap; // First run goes to bootstrap!

    // --- Start the System ---
    print_str("[KERNEL] Starting Task A logic...\n");
    task_a_main(); // This loop never returns
    
    return 0;
}