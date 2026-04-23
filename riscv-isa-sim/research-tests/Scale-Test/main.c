#include <stdint.h>

// --- UART & HELPER FUNCTIONS ---
volatile char* const UART0 = (char*)0x10000000;
void print_str(const char* s) { while (*s) *UART0 = *s++; }

// --- TCB STRUCTURE ---
typedef struct {
    uint32_t *sp;
    uint32_t pc;
    uint32_t window_cfg;
    uint32_t id;
} TCB_t;

TCB_t tcbs[4];
TCB_t *current_task = &tcbs[0];
volatile TCB_t* volatile context_handoff_ptr = 0;

// =========================================================
// SCHEDULER
// =========================================================
unsigned long run_scheduler(unsigned long old_pc) {
    current_task->pc = old_pc;

    // Cycle: 0 -> 1 -> 2 -> 3 -> 0
    int next_id = (current_task->id + 1) % 4;
    TCB_t *next_task = &tcbs[next_id];

    // Stage Switch (Triggers DUMP)
    asm volatile("csrw 0x801, %0" :: "r"(next_task->window_cfg));
    
    current_task = next_task;
    context_handoff_ptr = current_task;
    
    return next_task->pc;
}

// =========================================================
// TASK 0: KERNEL
// =========================================================
void task_0_main() {
    print_str("[TASK 0] Kernel Started. Cycling through tasks...\n");
    for (int i=0; i<3; i++) {
        print_str("[TASK 0] Switching...\n");
        asm volatile("ecall"); 
    }
    print_str("[TASK 0] Done. Exiting.\n");
    asm volatile("li t0, 1; la t1, tohost; sw t0, 0(t1); 1: j 1b");
}

// =========================================================
// NANO TASKS (Assembly)
// =========================================================
void __attribute__((naked)) task_1_nano() {
    asm volatile (
        "li x1, 0x11111111\n"
        "li x2, 0x11112222\n"
        "ecall\n"
        "j task_1_nano\n"
    );
}

void __attribute__((naked)) task_2_nano() {
    asm volatile (
        "li x1, 0x22221111\n"
        "li x2, 0x22222222\n"
        "ecall\n"
        "j task_2_nano\n"
    );
}

void __attribute__((naked)) task_3_nano() {
    asm volatile (
        "li x1, 0x33331111\n"
        "li x2, 0x33332222\n"
        "ecall\n"
        "j task_3_nano\n"
    );
}

// =========================================================
// SETUP
// =========================================================
void setup_task(int id, void (*func)(), uint32_t base, uint32_t size) {
    tcbs[id].id = id;
    tcbs[id].window_cfg = (size << 16) | base; 
    
    // [FIX] Point PC directly to the function
    tcbs[id].pc = (uint32_t)func;
}

int main() {
    tcbs[0].id = 0;
    tcbs[0].window_cfg = 0x00200000; // Base 0
    
    setup_task(1, task_1_nano, 32, 10);
    setup_task(2, task_2_nano, 42, 10);
    setup_task(3, task_3_nano, 52, 10);

    task_0_main();
    return 0;
}