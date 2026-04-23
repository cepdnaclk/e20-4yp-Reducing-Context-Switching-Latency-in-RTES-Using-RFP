#include <stdint.h>

#define SAMPLES 100 
#define MODE_HARDWARE 0
#define MODE_BASELINE 1

void* memcpy(void* dest, const void* src, unsigned long n) {
    char* d = (char*)dest; const char* s = (const char*)src;
    while (n--) *d++ = *s++; return dest;
}
void* memset(void* dest, int c, unsigned long n) {
    char* d = (char*)dest; while (n--) *d++ = (char)c; return dest;
}

volatile char* const UART0 = (char*)0x10000000;
void print_str(const char* s) { while (*s) *UART0 = *s++; }
void print_dec(unsigned long val) {
    char buffer[20]; char *p = buffer;
    if (val == 0) { print_str("0"); return; }
    while (val > 0) { *p++ = (val % 10) + '0'; val /= 10; }
    while (p > buffer) *(--p) == '\0' ? 0 : (void)(*UART0 = *p);
}

volatile unsigned long global_timestamp = 0; 
volatile unsigned long latencies[SAMPLES];
volatile int sample_idx = 0;
volatile int current_mode = MODE_BASELINE;
volatile int experiment_done = 0;

typedef struct { uint32_t *sp; uint32_t pc; uint32_t window_cfg; } TCB_t;
TCB_t tcb_a, tcb_b;
TCB_t *current_task = &tcb_a;

volatile uint32_t pending_window = 0;
uint32_t stack_a[1024];
uint32_t stack_b[1024];

extern void perform_dummy_software_save_restore(void);
extern void task_b_entry(void);
extern void software_trap_entry(void);
extern void hardware_trap_entry(void);

void __attribute__((naked)) task_b_starter() {
    asm volatile ("la sp, stack_b + 4096 \n call task_b_entry \n 1: j 1b");
}

unsigned long run_scheduler(unsigned long old_pc) {
    current_task->pc = old_pc; 
    if (experiment_done) { pending_window = 0; return old_pc; }

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

void matrix_mul_4x4() {
    int A[4][4] = { {1,2,3,4}, {5,6,7,8}, {9,1,2,3}, {4,5,6,7} };
    int B[4][4] = { {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1} };
    int C[4][4];
    for(int i=0;i<4;i++) for(int j=0;j<4;j++) C[i][j]=0;
    for(int i=0; i<4; i++) for(int j=0; j<4; j++) for(int k=0; k<4; k++) C[i][j] += A[i][k] * B[k][j];
    if (C[1][1] != 6) { print_str("\n[ERROR] Task A Corrupted!\n"); while(1); }
}

void task_a_entry() {
    unsigned long now;
    while (sample_idx < SAMPLES) {
        matrix_mul_4x4();
        if (global_timestamp != 0) {
             asm volatile("csrr %0, mcycle" : "=r"(now) :: "memory");
             unsigned long lat = now - global_timestamp;
             if (sample_idx < SAMPLES) {
                 latencies[sample_idx] = lat; sample_idx++;
                 if (sample_idx % 10 == 0) print_str("A"); 
             }
        }
        asm volatile("csrr %0, mcycle" : "=r"(global_timestamp) :: "memory");
        asm volatile("ecall" ::: "memory"); // CRITICAL MEMORY BARRIER
    }
    experiment_done = 1;
}

void string_reverse() {
    char str[] = "HardwareAcceleratedRISCV"; int len = 24;
    for(int i=0; i<len/2; i++) { char temp = str[i]; str[i] = str[len-1-i]; str[len-1-i] = temp; }
    if (str[0] != 'V') { print_str("\n[ERROR] Task B Corrupted!\n"); while(1); }
}

void task_b_entry() {
    unsigned long now;
    while (sample_idx < SAMPLES) {
        string_reverse();
        if (global_timestamp != 0) {
             asm volatile("csrr %0, mcycle" : "=r"(now) :: "memory");
             unsigned long lat = now - global_timestamp;
             if (sample_idx < SAMPLES) {
                 latencies[sample_idx] = lat; sample_idx++;
                 if (sample_idx % 10 == 1) print_str("B"); 
             }
        }
        asm volatile("csrr %0, mcycle" : "=r"(global_timestamp) :: "memory");
        asm volatile("ecall" ::: "memory"); // CRITICAL MEMORY BARRIER
    }
    asm volatile("ecall" ::: "memory"); while(1);
}

void run_experiment(int mode, const char* name) {
    current_mode = mode;
    sample_idx = 0; global_timestamp = 0;
    pending_window = 0; experiment_done = 0;
    current_task = &tcb_a;

    print_str("\nRunning "); print_str(name); print_str("... \n");

    // [FIX]: Set up the Task Control Blocks for BOTH modes!
    tcb_a.window_cfg = 0x00200000; tcb_a.pc = (uint32_t)task_a_entry;
    tcb_b.window_cfg = 0x00200020; tcb_b.pc = (uint32_t)task_b_starter;

    if (mode == MODE_BASELINE) {
        // 1. Point to safe software trap handler
        asm volatile("csrw mtvec, %0" :: "r"(software_trap_entry));
        task_a_entry(); 
    } else {
        // 1. Point to fast zero-overhead hardware trap handler
        asm volatile("csrw mtvec, %0" :: "r"(hardware_trap_entry));
        
        // 2. Shift into Hardware Window A
        asm volatile(
            "csrw 0x801, %0 \n"
            "la sp, stack_a + 4096 \n"
            :: "r"(0x00200000)
        );
        
        task_a_entry(); 
        
        // 3. CRITICAL: Shift back to Kernel Window before returning!
        asm volatile("csrw 0x801, zero \n"); 
    }

    unsigned long sum = 0, min = -1UL, max = 0;
    for(int i=10; i<SAMPLES; i++) { 
        unsigned long lat = latencies[i]; sum += lat;
        if (lat < min) min = lat;
        if (lat > max) max = lat;
    }
    print_str("\nDone.\n");
    print_str("  Min: "); print_dec(min); print_str(" cycles\n");
    print_str("  Max: "); print_dec(max); print_str(" cycles\n");
    print_str("  Avg: "); print_dec(sum / (SAMPLES-10)); print_str(" cycles\n");
}

int main() {
    print_str("\n=== REAL WORKLOAD STRESS TEST ===\n");
    run_experiment(MODE_BASELINE, "BASELINE (Software Save/Restore)");
    run_experiment(MODE_HARDWARE, "PROPOSED (Hardware Windows)");
    asm volatile("li t0, 1; la t1, tohost; sw t0, 0(t1); 1: j 1b");
    return 0;
}