/*
 * Partitioned-register-file context switch microbenchmark.
 *
 * Requires custom CSRs:
 *   0x800 = active window config [31:16]=size, [15:0]=base
 *   0x801 = previous window config
 */

#include <stdint.h>
#include <stdio.h>

#define ITERS 10000UL
#define WINDOW0 ((uint32_t)((32u << 16) | 0u))
#define WINDOW1 ((uint32_t)((32u << 16) | 32u))

static volatile uint64_t sink;

static inline uint32_t read_cycle(void) {
  uint32_t value;
  asm volatile("rdcycle %0" : "=r"(value));
  return value;
}

static inline void write_window_cfg(uint32_t cfg) {
  asm volatile("csrw 0x800, %0" : : "r"(cfg) : "memory");
}

static inline uint32_t read_window_cfg(void) {
  uint32_t cfg;
  asm volatile("csrr %0, 0x800" : "=r"(cfg));
  return cfg;
}

int main(void) {
  write_window_cfg(WINDOW0);
  (void) read_window_cfg();

  uint32_t start = read_cycle();
  for (uint64_t i = 0; i < ITERS; ++i) {
    write_window_cfg(WINDOW1);
    sink ^= (uint64_t) read_window_cfg();
    write_window_cfg(WINDOW0);
    sink ^= (uint64_t) read_window_cfg();
  }
  uint32_t end = read_cycle();

  uint32_t delta = end - start;
  uint32_t switches = (uint32_t) (ITERS * 2UL);
  printf("CTXSW_HW cycles=%u switches=%u cps=%u sink=%u\n",
         delta,
         switches,
         delta / switches,
         (unsigned) sink);
  return 0;
}

