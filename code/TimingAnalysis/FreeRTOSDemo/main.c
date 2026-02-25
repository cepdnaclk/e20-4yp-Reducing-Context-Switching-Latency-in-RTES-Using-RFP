#include <stdint.h>
#include <stddef.h>

#include "FreeRTOS.h"
#include "task.h"

// Custom CSRs provided by modified Spike to expose context-switch timing.
#define CSR_MCTX_START 0x7c0
#define CSR_MCTX_END   0x7c1
#define CSR_MCTX_DELTA 0x7c2

static inline uint64_t read_mctx_start(void)
{
    uint64_t v;
    asm volatile("csrr %0, %1" : "=r"(v) : "i"(CSR_MCTX_START));
    return v;
}

static inline uint64_t read_mctx_end(void)
{
    uint64_t v;
    asm volatile("csrr %0, %1" : "=r"(v) : "i"(CSR_MCTX_END));
    return v;
}

static inline uint64_t read_mctx_delta(void)
{
    uint64_t v;
    asm volatile("csrr %0, %1" : "=r"(v) : "i"(CSR_MCTX_DELTA));
    return v;
}

// HTIF console support for Spike (no pk needed)
static volatile uint64_t tohost __attribute__((section(".tohost")));
static volatile uint64_t fromhost __attribute__((section(".fromhost")));

static void htif_putc(char ch)
{
    uint64_t payload = (uint64_t)(uint8_t)ch;
    uint64_t cmd = ((uint64_t)1 << 48) | ((uint64_t)1 << 56) | payload; // device=1 console, cmd=1 write
    while (tohost != 0) {
    }
    tohost = cmd;
}

static void put_str(const char *s)
{
    while (*s) {
        htif_putc(*s++);
    }
}

static void put_u64(uint64_t v)
{
    char buf[32];
    int i = 0;
    if (v == 0) {
        buf[i++] = '0';
    } else {
        while (v > 0 && i < (int)sizeof(buf)) {
            buf[i++] = (char)('0' + (v % 10));
            v /= 10;
        }
    }
    for (int l = 0, r = i - 1; l < r; l++, r--) {
        char tmp = buf[l];
        buf[l] = buf[r];
        buf[r] = tmp;
    }
    for (int k = 0; k < i; k++) {
        htif_putc(buf[k]);
    }
}

static void log_switch(const char *tag, int iter)
{
    put_str(tag);
    put_str(" ");
    put_u64((uint64_t)iter);
    put_str(": start=");
    put_u64(read_mctx_start());
    put_str(" end=");
    put_u64(read_mctx_end());
    put_str(" delta=");
    put_u64(read_mctx_delta());
    put_str("\n");
}

static void vTaskA(void *pvParameters)
{
    (void)pvParameters;
    for (int i = 0; i < 5; i++) {
        taskYIELD();
        log_switch("TaskA", i);
    }
    for (;;)
        taskYIELD();
}

static void vTaskB(void *pvParameters)
{
    (void)pvParameters;
    for (int i = 0; i < 5; i++) {
        taskYIELD();
        log_switch("TaskB", i);
    }
    for (;;)
        taskYIELD();
}

int main(void)
{
    (void)xTaskCreate(vTaskA, "A", 256, NULL, tskIDLE_PRIORITY + 1, NULL);
    (void)xTaskCreate(vTaskB, "B", 256, NULL, tskIDLE_PRIORITY + 1, NULL);

    vTaskStartScheduler();
    for (;;)
        ;
}
