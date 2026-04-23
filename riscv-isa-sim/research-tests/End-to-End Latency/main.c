#include <stdint.h>

#define SAMPLES 1000
#define MODE_HARDWARE 0
#define MODE_BASELINE 1

volatile char* const UART0 = (char*)0x10000000;
void print_str(const char* s) { while (*s) *UART0 = *s++; }
void print_dec(unsigned long val) {
    char buffer[20];
    char *p = buffer;
    if (val == 0) { print_str("0"); return; }
    while (val > 0) { *p++ = (val % 10) + '0'; val /= 10; }
    while (p > buffer) *(--p) == '\0' ? 0 : (void)(*UART0 = *p);
}

// GLOBAL VARS
volatile unsigned long global_timestamp = 0; 
volatile unsigned long latencies[SAMPLES];
volatile int sample_idx = 0;
volatile int current_mode = MODE_BASELINE;
volatile int experiment_done = 0;

typedef struct { uint32_t *sp; uint32_t pc; uint32_t window_cfg; } TCB_t;
TCB_t tcb_a, tcb_b;
TCB_t *current_task = &tcb_a;

volatile uint32_t pending_window = 0;
uint32_t stack_b[1024];

extern void perform_dummy_software_save_restore(void);
extern void task_b_entry(void);

// TRAMPOLINE FOR TASK B
void __attribute__((naked)) task_b_starter() {
    asm volatile (
        "la sp, stack_b + 4096 \n"
        "call task_b_entry \n"
        "1: j 1b"
    );
}

// SCHEDULER
unsigned long run_scheduler(unsigned long old_pc) {
    current_task->pc = old_pc; 
    
    if (experiment_done) {
        pending_window = 0;
        return old_pc;
    }

    TCB_t *next = (current_task == &tcb_a) ? &tcb_b : &tcb_a;
    current_task = next;

    if (current_mode == MODE_HARDWARE) {
        pending_window = next->window_cfg;
        return next->pc;
    } else {
        pending_window = 0;
        perform_dummy_software_save_restore();
        return old_pc; 
    }
}

// TASK A
void task_a_entry() {
    unsigned long now;
    while (sample_idx < SAMPLES) {
        if (global_timestamp != 0) {
             asm volatile("csrr %0, mcycle" : "=r"(now));
             unsigned long lat = now - global_timestamp;
             if (sample_idx < SAMPLES) {
                 latencies[sample_idx] = lat;
                 sample_idx++;
                 if (sample_idx % 100 == 0) print_str("."); 
             }
        }
        asm volatile("csrr %0, mcycle" : "=r"(global_timestamp));
        asm volatile("ecall"); 
    }
    experiment_done = 1;
}

// TASK B
void task_b_entry() {
    unsigned long now;
    while (sample_idx < SAMPLES) {
        if (global_timestamp != 0) {
             asm volatile("csrr %0, mcycle" : "=r"(now));
             unsigned long lat = now - global_timestamp;
             if (sample_idx < SAMPLES) {
                 latencies[sample_idx] = lat;
                 sample_idx++;
                 if (sample_idx % 100 == 0) print_str("."); 
             }
        }
        asm volatile("csrr %0, mcycle" : "=r"(global_timestamp));
        asm volatile("ecall"); 
    }
    asm volatile("ecall"); 
    while(1);
}

void run_experiment(int mode, const char* name) {
    current_mode = mode;
    sample_idx = 0;
    global_timestamp = 0;
    pending_window = 0;
    experiment_done = 0;

    // [FIX 1] PRINT FIRST (Before nuking registers)
    print_str("Running "); print_str(name); print_str("... ");

    // [FIX 2] Now it is safe to reset hardware
    asm volatile("csrw 0x801, %0" :: "r"(0x00200000));
    
    tcb_a.window_cfg = 0x00200000; tcb_a.pc = (uint32_t)task_a_entry;
    tcb_b.window_cfg = 0x00200020; tcb_b.pc = (uint32_t)task_b_starter;
    
    task_a_entry(); 
    
    unsigned long sum = 0, min = -1UL, max = 0;
    for(int i=10; i<SAMPLES; i++) { 
        unsigned long lat = latencies[i];
        sum += lat;
        if (lat < min) min = lat;
        if (lat > max) max = lat;
    }
    print_str("Done.\n");
    print_str("  Min: "); print_dec(min); print_str(" cycles\n");
    print_str("  Max: "); print_dec(max); print_str(" cycles\n");
    print_str("  Avg: "); print_dec(sum / (SAMPLES-10)); print_str(" cycles\n\n");
}

int main() {
    print_str("\n=== BENCHMARK START ===\n");
    run_experiment(MODE_BASELINE, "BASELINE (Software Save/Restore)");
    run_experiment(MODE_HARDWARE, "PROPOSED (Hardware Windows)");
    asm volatile("li t0, 1; la t1, tohost; sw t0, 0(t1); 1: j 1b");
    return 0;
}