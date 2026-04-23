#include <stdint.h>

// --- Hardware Definitions ---
volatile char* const UART0 = (char*)0x10000000;
void print_str(const char* s) { while (*s) *UART0 = *s++; }
void print_hex(unsigned long val) {
    char hex[] = "0123456789ABCDEF";
    print_str("0x");
    for (int i = 7; i >= 0; i--) *UART0 = hex[(val >> (i * 4)) & 0xF];
}
void print_dec(unsigned long val) {
    char buffer[20];
    char *p = buffer;
    if (val == 0) { print_str("0"); return; }
    while (val > 0) { *p++ = (val % 10) + '0'; val /= 10; }
    while (p > buffer) *(--p) == '\0' ? 0 : (void)(*UART0 = *p);
}

// --- Metrics Helper ---
unsigned long read_instret() {
    unsigned long val;
    asm volatile("csrr %0, 0xB02" : "=r"(val)); // 0xB02 is minstret
    return val;
}

// --- TCB & Global State ---
typedef struct {
    uint32_t *sp;
    uint32_t pc;
    uint32_t window_cfg;
    uint32_t id;
} TCB_t;

TCB_t tcb_a;
TCB_t tcb_b;
TCB_t *current_task = &tcb_a;
uint32_t stack_b[1024];

extern void task_bootstrap(void);

// =========================================================
// SIMULATED BASELINE (The "Old Way")
// =========================================================
// We run this loop to calculate how many instructions 
// a standard RTOS wastes saving registers.
void simulate_software_overhead() {
    volatile uint32_t dummy_stack[32];
    // Simulating: sw x1, 0(sp); sw x2, 4(sp) ... sw x31, 120(sp)
    for(int i=0; i<31; i++) {
        dummy_stack[i] = 0xDEADBEEF;
    }
    // Simulating: lw x1, 0(sp) ...
    for(int i=0; i<31; i++) {
        uint32_t temp = dummy_stack[i];
        (void)temp;
    }
}

// =========================================================
// SCHEDULER
// =========================================================
unsigned long run_scheduler(unsigned long old_pc) {
    current_task->pc = old_pc;
    
    TCB_t *next_task = (current_task == &tcb_a) ? &tcb_b : &tcb_a;

    asm volatile("csrw 0x801, %0" :: "r"(next_task->window_cfg));
    current_task = next_task;
    asm volatile("csrw mscratch, %0" :: "r"(current_task));
    
    return next_task->pc;
}

// =========================================================
// TASKS
// =========================================================

// Global metrics
unsigned long start_inst, end_inst;

void task_a_main() {
    print_str("\n[TASK A] Measuring HRW Switch Overhead...\n");
    
    // 1. Snapshot Instruction Count
    start_inst = read_instret();
    
    // 2. Yield (This triggers the switch)
    asm volatile("ecall"); 
    
    // Resume here...
    while(1);
}

void task_b_main() {
    // 3. Immediately Snapshot Instruction Count
    end_inst = read_instret();
    
    unsigned long delta = end_inst - start_inst;
    
    print_str("\n=== RESEARCH RESULTS ===\n");
    print_str("1. Hardware Window Switch (HRW): "); 
    print_dec(delta); print_str(" instructions\n");
    
    // Now Measure the Baseline overhead
    unsigned long base_start = read_instret();
    simulate_software_overhead();
    unsigned long base_end = read_instret();
    unsigned long base_cost = base_end - base_start;
    
    print_str("2. Standard Software Switch (Simulated): ~");
    print_dec(base_cost + delta); // Cost = Overhead + Trap Overhead
    print_str(" instructions\n");
    
    print_str("3. IMPACT: ");
    print_dec((base_cost + delta) / delta);
    print_str("x Efficiency Improvement\n");
    
    // Stop simulation
    asm volatile("li t0, 1; la t1, tohost; sw t0, 0(t1); 1: j 1b");
}

// =========================================================
// BOOTSTRAP
// =========================================================
int main() {
    tcb_a.id = 0; tcb_a.window_cfg = 0x00200000;
    tcb_b.id = 1; tcb_b.window_cfg = 0x00200020;
    tcb_b.sp = &stack_b[1023];
    tcb_b.pc = (uint32_t)task_bootstrap;

    task_a_main();
    return 0;
}