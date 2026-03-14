# FYP_Tests: performance tests for FreeRTOS + Spike register windows
# Build from this directory (demo/Spike_FYP/FYP_Tests).
# Path: FYP_Tests -> .. = Spike_FYP, ../.. = demo, ../../.. = FreeRTOS-Kernel.

KERNEL := ../../..
PORT   := $(KERNEL)/portable/GCC/RISC-V
DEMO   := ..

CC     := riscv32-unknown-elf-gcc
AS     := riscv32-unknown-elf-gcc
CFLAGS := -march=rv32gc -mabi=ilp32 -O1 -g -Wall -Wextra
CFLAGS += -I. -I$(DEMO) -I$(KERNEL)/include -I$(PORT)
CFLAGS += -ffreestanding -nostdlib

ASFLAGS := -march=rv32gc -mabi=ilp32 -DportasmSPIKE_FYP_CHIP
ASFLAGS += -I$(abspath $(PORT)) -I. -I$(DEMO)

LDFLAGS := -T $(DEMO)/link.ld -nostartfiles -nostdlib

# Shared object files (kernel + port + demo common)
PORT_C   := $(PORT)/port.c
PORT_S   := $(PORT)/portASM.S
COMMON_O := startup.o libc_stubs.o tasks.o queue.o list.o port.o portASM.o

# Default target: build all test ELFs
all: demo_latency.elf demo_latency_legacy.elf demo_scale.elf demo_workload.elf demo_workload_legacy.elf

startup.o: $(DEMO)/startup.S
	$(AS) $(ASFLAGS) -c -o $@ $<

libc_stubs.o: $(DEMO)/libc_stubs.c
	$(CC) $(CFLAGS) -c -o $@ $<

tasks.o: $(KERNEL)/tasks.c
	$(CC) $(CFLAGS) -c -o $@ $<

queue.o: $(KERNEL)/queue.c
	$(CC) $(CFLAGS) -c -o $@ $<

list.o: $(KERNEL)/list.c
	$(CC) $(CFLAGS) -c -o $@ $<

port.o: $(PORT)/port.c
	$(CC) $(CFLAGS) -c -o $@ $<

portASM.o: $(PORT)/portASM.S
	$(AS) $(ASFLAGS) -c -o $@ $<

# --- Latency test (Legacy vs Windowed) ---
latency_test.o: latency_test.c
	$(CC) $(CFLAGS) -c -o $@ $<

demo_latency.elf: latency_test.o $(COMMON_O)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# --- Latency test baseline (Legacy vs Legacy) ---
latency_test_legacy.o: latency_test.c
	$(CC) $(CFLAGS) -DFYP_LATENCY_LEGACY_ONLY -c -o $@ $<

demo_latency_legacy.elf: latency_test_legacy.o $(COMMON_O)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# --- Scale test (4 tasks) ---
scale_test.o: scale_test.c
	$(CC) $(CFLAGS) -c -o $@ $<

demo_scale.elf: scale_test.o $(COMMON_O)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# --- Workload test (Legacy vs Windowed + workload) ---
workload_test.o: workload_test.c
	$(CC) $(CFLAGS) -c -o $@ $<

demo_workload.elf: workload_test.o $(COMMON_O)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# --- Workload test baseline ---
workload_test_legacy.o: workload_test.c
	$(CC) $(CFLAGS) -DFYP_WORKLOAD_LEGACY_ONLY -c -o $@ $<

demo_workload_legacy.elf: workload_test_legacy.o $(COMMON_O)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# --- Run targets ---
run_latency: demo_latency.elf
	spike --isa=rv32gc $<

run_latency_legacy: demo_latency_legacy.elf
	spike --isa=rv32gc $<

run_scale: demo_scale.elf
	spike --isa=rv32gc $<

run_workload: demo_workload.elf
	spike --isa=rv32gc $<

run_workload_legacy: demo_workload_legacy.elf
	spike --isa=rv32gc $<

clean:
	rm -f *.o *.elf

.PHONY: all run_latency run_latency_legacy run_scale run_workload run_workload_legacy clean
