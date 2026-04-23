/* Baseline-safe benchmark scaffold for software switching path. */

#include <stdint.h>
#include <stdio.h>

int main(void) {
  volatile uint32_t a = 0;
  volatile uint32_t b = 1;
  for (uint32_t i = 0; i < 200000; ++i) {
    a += (b ^ i);
    b += (a << 1);
  }
  printf("CTXSW_SW marker a=%u b=%u\n", a, b);
  return 0;
}

