#include <stdint.h>

// --- Hardware & UART ---
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

// --- Metrics ---
unsigned long read_instret() {
    unsigned long val;
    asm volatile("csrr %0, 0xB02" : "=r"(val));
    return val;
}

// --- TCB Structure ---
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
// TEST FLAGS
// =========================================================
volatile int test_stage = 0;
volatile int error_count = 0;
#define STAGE_INTEGRITY 1
#define STAGE_STRESS    2
#define STRESS_COUNT    1000

// =========================================================
// SCHEDULER
// =========================================================
unsigned long run_scheduler(unsigned long old_pc) {
    current_task->pc = old_pc;
    
    // Toggle Tasks
    TCB_t *next_task = (current_task == &tcb_a) ? &tcb_b : &tcb_a;

    // Hardware Switch
    asm volatile("csrw 0x801, %0" :: "r"(next_task->window_cfg));
    current_task = next_task;
    asm volatile("csrw mscratch, %0" :: "r"(current_task));
    
    return next_task->pc;
}

// =========================================================
// TASK A (The Validator)
// =========================================================
void task_a_main() {
    print_str("\n[TEST 1] Register Integrity (The Canary Test)\n");
    
    // 1. Fill Registers with "Pattern A" (0xAAAA...)
    // We use Callee-Saved registers (s0-s11) as Canaries.
    // If these change, the C compiler context is corrupted.
    register long s1 asm("s1") = 0xAAAAAAAA;
    register long s2 asm("s2") = 0xBBBBBBBB;
    register long s3 asm("s3") = 0xCCCCCCCC;

    print_str("[TASK A] Filling Registers (s1=A.., s2=B.., s3=C..)\n");
    print_str("[TASK A] Yielding to Task B...\n");
    
    // 2. Switch to Task B (which will fill them with different values)
    asm volatile("ecall" : "+r"(s1), "+r"(s2), "+r"(s3)); 
    
    // 3. Verify Values upon return
    print_str("[TASK A] Back in Context. Checking Canaries...\n");
    
    if (s1 != 0xAAAAAAAA) { print_str("  [FAIL] s1 corrupted! Found: "); print_hex(s1); print_str("\n"); error_count++; }
    else print_str("  [PASS] s1 preserved.\n");

    if (s2 != 0xBBBBBBBB) { print_str("  [FAIL] s2 corrupted! Found: "); print_hex(s2); print_str("\n"); error_count++; }
    else print_str("  [PASS] s2 preserved.\n");
    
    if (s3 != 0xCCCCCCCC) { print_str("  [FAIL] s3 corrupted! Found: "); print_hex(s3); print_str("\n"); error_count++; }
    else print_str("  [PASS] s3 preserved.\n");

    // --- Proceed to Stress Test ---
    test_stage = STAGE_STRESS;
    print_str("\n[TEST 2] Endurance Stress Test (1000 Switches)\n");
    
    unsigned long start_inst = read_instret();
    
    for(int i=0; i<STRESS_COUNT; i++) {
        asm volatile("ecall"); // Ping
    }
    
    unsigned long end_inst = read_instret();
    unsigned long total_inst = end_inst - start_inst;
    
    print_str("  [DONE] Completed "); print_dec(STRESS_COUNT); print_str(" switches.\n");
    print_str("  Total Instructions: "); print_dec(total_inst); print_str("\n");
    print_str("  Average Inst/Switch: "); print_dec(total_inst / (STRESS_COUNT * 2)); // *2 because A->B and B->A
    print_str("\n");
    
    if (error_count == 0) print_str("\n[SUCCESS] ALL VALIDATION TESTS PASSED.\n");
    else print_str("\n[FAILURE] ERRORS DETECTED.\n");

    // EXIT
    asm volatile("li t0, 1; la t1, tohost; sw t0, 0(t1); 1: j 1b");
}

// =========================================================
// TASK B (The "Interferer")
// =========================================================
void task_b_main() {
    // [TEST 1 Logic]
    // Task B attempts to stomp on the registers
    register long s1 asm("s1") = 0x11111111;
    register long s2 asm("s2") = 0x22222222;
    register long s3 asm("s3") = 0x33333333;

    // Verify we are in the right window (just in case)
    unsigned long current_win;
    asm volatile("csrr %0, 0x800" : "=r"(current_win));
    if ((current_win & 0xFFFF) != 0x20) { // Expecting Base 32
        print_str("[TASK B] FATAL: Wrong Window! "); print_hex(current_win); print_str("\n");
        error_count++;
    }

    print_str("    [TASK B] Hello! I am using regs s1-s3 for my own data.\n");
    
    // Yield back to A
    asm volatile("ecall" : "+r"(s1), "+r"(s2), "+r"(s3)); 
    
    // [TEST 2 Logic]
    // Simple Ping-Pong Loop
    while(1) {
        asm volatile("ecall");
    }
}

// =========================================================
// MAIN
// =========================================================
int main() {
    tcb_a.id = 0; tcb_a.window_cfg = 0x00200000;
    tcb_b.id = 1; tcb_b.window_cfg = 0x00200020;
    tcb_b.sp = &stack_b[1023];
    tcb_b.pc = (uint32_t)task_bootstrap;

    task_a_main();
    return 0;
}