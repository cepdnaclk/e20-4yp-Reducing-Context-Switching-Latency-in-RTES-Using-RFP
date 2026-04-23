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

unsigned long read_instret() {
    unsigned long val;
    asm volatile("csrr %0, 0xB02" : "=r"(val));
    return val;
}

// --- TCB & Scheduler ---
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

// Global Test Mode
int use_software_overhead = 0;

// --- Simulated OS Overhead ---
// Simulates saving/restoring 31 registers (approx 62 memory accesses)
void simulate_standard_overhead() {
    volatile uint32_t ctx[32]; 
    for(int i=0; i<31; i++) ctx[i] = 0xAA55AA55;
    for(int i=0; i<31; i++) {
        volatile uint32_t tmp = ctx[i];
        (void)tmp;
    }
}

// --- Scheduler ---
unsigned long run_scheduler(unsigned long old_pc) {
    current_task->pc = old_pc;
    TCB_t *next_task = (current_task == &tcb_a) ? &tcb_b : &tcb_a;
    
    // Add overhead ONLY if enabled
    if (use_software_overhead) {
        simulate_standard_overhead();
    }

    asm volatile("csrw 0x801, %0" :: "r"(next_task->window_cfg));
    current_task = next_task;
    asm volatile("csrw mscratch, %0" :: "r"(current_task));
    
    return next_task->pc;
}

// --- MATRIX MATH KERNEL ---
#define N 16
int A[N][N], B[N][N], C[N][N];

void matmul_task(int task_id, int start_row, int end_row) {
    // Initialize
    for(int i=0; i<N; i++) {
        for(int j=0; j<N; j++) {
            A[i][j] = i + j;
            B[i][j] = i - j;
        }
    }

    // Compute
    for (int i = start_row; i < end_row; i++) {
        for (int j = 0; j < N; j++) {
            int sum = 0;
            for (int k = 0; k < N; k++) {
                sum += A[i][k] * B[k][j];
            }
            C[i][j] = sum;
        }
        // YIELD AFTER EVERY ROW
        asm volatile("ecall"); 
    }
}

// --- Task Wrappers ---
void task_a_main() {
    matmul_task(0, 0, N/2);
}

void task_b_main() {
    matmul_task(1, N/2, N);
    // When done, sit in a loop so we don't crash
    while(1) asm volatile("ecall");
}

// --- HELPER TO RESET TASK B ---
void reset_task_b() {
    tcb_b.id = 1; 
    tcb_b.window_cfg = 0x00200020;
    // Reset Stack Pointer to top of stack
    tcb_b.sp = &stack_b[1023]; 
    // Reset PC to the bootstrap function
    tcb_b.pc = (uint32_t)task_bootstrap;
}

// --- Main Test Harness ---
int main() {
    // Initial Setup
    tcb_a.id = 0; tcb_a.window_cfg = 0x00200000;
    reset_task_b();
    
    // === TEST 1: HRW (Hardware Accelerated) ===
    print_str("\n[BENCHMARK] Running Matrix Multiplication (16x16)\n");
    print_str("[MODE] Hardware Accelerated (HRW)\n");
    
    use_software_overhead = 0; 
    current_task = &tcb_a; // Ensure we start as Task A
    
    unsigned long start = read_instret();
    matmul_task(0, 0, N/2); 
    unsigned long end = read_instret();
    unsigned long total_hrw = end - start;
    
    print_str("  Total Instructions: "); print_dec(total_hrw); print_str("\n");
    
    
    // === TEST 2: Baseline (Simulated Software Switch) ===
    // CRITICAL FIX: RESET TASK B SO IT DOES WORK AGAIN!
    reset_task_b();
    
    print_str("\n[MODE] Standard Software Switch (Simulated)\n");
    
    use_software_overhead = 1; 
    current_task = &tcb_a; // Ensure we start as Task A
    
    start = read_instret();
    matmul_task(0, 0, N/2);
    end = read_instret();
    unsigned long total_sw = end - start;
    
    print_str("  Total Instructions: "); print_dec(total_sw); print_str("\n");

    // === RESULTS ===
    print_str("\n=== FINAL ANALYSIS ===\n");
    print_str("Hardware (HRW) Cost: "); print_dec(total_hrw); print_str(" inst\n");
    print_str("Software (Std) Cost: "); print_dec(total_sw); print_str(" inst\n");
    
    if (total_hrw > 0) {
        print_str("Throughput Gain: "); 
        unsigned long gain = (total_sw * 10) / total_hrw;
        print_dec(gain / 10); print_str("."); print_dec(gain % 10); print_str("x\n");
    }

    // EXIT
    asm volatile("li t0, 1; la t1, tohost; sw t0, 0(t1); 1: j 1b");
    return 0;
}