#include <stdio.h>
#include <stdint.h>

volatile unsigned long saved_sp, saved_s0, saved_gp;

// Use 'ra' (x1) as scratch. 
// x4 (tp) is needed for Task 1 data.
#define SET_WINDOW(base, size) \
    asm volatile ( \
        /* 1. Construct Config in x1 (RA) */ \
        "li ra, %0\n\t" \
        "slli ra, ra, 16\n\t" \
        "ori ra, ra, %1\n\t" \
        \
        /* 2. Execute Switch */ \
        "csrw 0x800, ra\n\t" \
        \
        /* 3. Restore Anchors using x1 (RA) */ \
        ".option push\n\t" ".option norelax\n\t" \
        "la ra, saved_sp\n\t" "lw sp, 0(ra)\n\t" \
        "la ra, saved_s0\n\t" "lw s0, 0(ra)\n\t" \
        "la ra, saved_gp\n\t" "lw gp, 0(ra)\n\t" \
        ".option pop\n\t" \
        : : "i"(size), "i"(base) : "ra", "memory" \
    )

#define SAVE_GLOBAL_CONTEXT() \
    asm volatile ( \
        ".option push\n\t" ".option norelax\n\t" \
        "la ra, saved_sp\n\t" "sw sp, 0(ra)\n\t" \
        "la ra, saved_s0\n\t" "sw s0, 0(ra)\n\t" \
        "la ra, saved_gp\n\t" "sw gp, 0(ra)\n\t" \
        ".option pop\n\t" \
        : : : "ra", "memory" \
    )

int main() {
    long res_t1, res_t1_illegal, res_t2, res_t3;
    
    setvbuf(stdout, NULL, _IONBF, 0);

    SAVE_GLOBAL_CONTEXT();

    printf("=== 3-Task Scalability Test (Silent + Clean) ===\n");

    // ========================================================
    // CLEANUP PHASE
    // ========================================================
    // Ensure Phys 5 is 0 before we start. 
    // Otherwise, we might read leftover kernel debris and think the test failed.
    SET_WINDOW(0, 32);
    asm volatile ("li x5, 0");

    // ========================================================
    // EXECUTION PHASE (No Printf allowed)
    // ========================================================
    
    // --- TASK 1 (Base 0, Size 5) ---
    // Registers: x0-x4. 
    SET_WINDOW(0, 5);
    asm volatile ("li x4, 0x1111"); 
    
    // Attempt Illegal Write:
    // If blocked, Phys 5 remains 0.
    // If failed, Phys 5 becomes 0xBAD1.
    asm volatile ("li x5, 0xBAD1"); 

    // --- TASK 2 (Base 5, Size 10) ---
    // Registers: x0-x9. Maps to Phys 5-14.
    SET_WINDOW(5, 10);
    asm volatile ("li x9, 0x2222");

    // --- TASK 3 (Base 15, Size 15) ---
    // Registers: x0-x14. Maps to Phys 15-29.
    SET_WINDOW(15, 15);
    asm volatile ("li x14, 0x3333");

    // ========================================================
    // VERIFICATION PHASE
    // ========================================================
    
    // Switch to Monitor Mode (Base 0, Full View)
    SET_WINDOW(0, 32);

    // Capture ALL values BEFORE calling printf
    asm volatile ("mv %0, x4" : "=r"(res_t1));
    asm volatile ("mv %0, x5" : "=r"(res_t1_illegal));
    asm volatile ("mv %0, x14" : "=r"(res_t2));
    asm volatile ("mv %0, x29" : "=r"(res_t3));

    // ========================================================
    // REPORTING PHASE
    // ========================================================
    
    // Check T1
    if (res_t1 == 0x1111) printf("[PASS] Task 1 Data preserved.\n");
    else printf("[FAIL] Task 1 Corrupted! Got 0x%lx\n", res_t1);

    // Check Illegal Write
    if (res_t1_illegal == 0) printf("[PASS] Task 1 Illegal Write Blocked.\n");
    else if (res_t1_illegal == 0xBAD1) printf("[FAIL] Task 1 Wrote 0xBAD1 to Phys 5 (Protection Failed)!\n");
    else printf("[FAIL] Task 1 Leaked into Phys 5! Found garbage 0x%lx\n", res_t1_illegal);

    // Check T2
    if (res_t2 == 0x2222) printf("[PASS] Task 2 Data preserved.\n");
    else printf("[FAIL] Task 2 Corrupted! Got 0x%lx\n", res_t2);

    // Check T3
    if (res_t3 == 0x3333) printf("[PASS] Task 3 Data preserved.\n");
    else printf("[FAIL] Task 3 Corrupted! Got 0x%lx\n", res_t3);

    return 0;
}