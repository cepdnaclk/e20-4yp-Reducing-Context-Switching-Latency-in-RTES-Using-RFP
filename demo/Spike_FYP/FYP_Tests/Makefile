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
all: demo_latency.elf demo_latency_legacy.elf demo_scale.elf demo_workload.elf demo_workload_legacy.elf demo_cycle_breakdown.elf demo_footprint.elf demo_jitter.elf demo_jitter_noflood.elf demo_spill.elf

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

PORT_CONTEXT_H := $(PORT)/portContext.h
portASM.o: $(PORT)/portASM.S $(PORT_CONTEXT_H)
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

# --- Phase 1: Cycle-Accurate Breakdown (requires CYCLE_BREAKDOWN_PROFILE) ---
port_cycle_profile.o: $(PORT)/port.c
	$(CC) $(CFLAGS) -DCYCLE_BREAKDOWN_PROFILE -c -o $@ $<

portASM_cycle_profile.o: $(PORT)/portASM.S $(PORT_CONTEXT_H)
	$(AS) $(ASFLAGS) -DCYCLE_BREAKDOWN_PROFILE -c -o $@ $<

cycle_breakdown_test.o: cycle_breakdown_test.c
	$(CC) $(CFLAGS) -DCYCLE_BREAKDOWN_PROFILE -c -o $@ $<

CYCLE_BREAKDOWN_O := startup.o libc_stubs.o tasks.o queue.o list.o port_cycle_profile.o portASM_cycle_profile.o
demo_cycle_breakdown.elf: cycle_breakdown_test.o $(CYCLE_BREAKDOWN_O)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# --- Phase 2: SRAM Footprint ---
footprint_test.o: footprint_test.c
	$(CC) $(CFLAGS) -c -o $@ $<

demo_footprint.elf: footprint_test.o $(COMMON_O)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# --- Phase 3: WCET & Jitter (memory flood) ---
jitter_test.o: jitter_test.c
	$(CC) $(CFLAGS) -c -o $@ $<

demo_jitter.elf: jitter_test.o $(COMMON_O)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Jitter test without flood task (quick run to verify report prints)
jitter_test_noflood.o: jitter_test.c
	$(CC) $(CFLAGS) -DFYP_JITTER_NO_FLOOD -c -o $@ $<

demo_jitter_noflood.elf: jitter_test_noflood.o $(COMMON_O)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# --- Phase 4: Window Spilling (10 tasks, 1 windowed on NXPR=64) ---
spill_test.o: spill_test.c
	$(CC) $(CFLAGS) -c -o $@ $<

demo_spill.elf: spill_test.o $(COMMON_O)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Spill test with task-id trace (for debugging "all id=0"): each run prints [SPILL] id=N
spill_test_trace.o: spill_test.c
	$(CC) $(CFLAGS) -DSPILL_DEBUG_TRACE=1 -c -o $@ $<

demo_spill_trace.elf: spill_test_trace.o $(COMMON_O)
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

run_cycle_breakdown: demo_cycle_breakdown.elf
	spike --isa=rv32gc $<

run_footprint: demo_footprint.elf
	spike --isa=rv32gc $<

run_jitter: demo_jitter.elf
	spike --isa=rv32gc $<

run_spill: demo_spill.elf
	spike --isa=rv32gc $<

clean:
	rm -f *.o *.elf

.PHONY: all run_latency run_latency_legacy run_scale run_workload run_workload_legacy run_cycle_breakdown run_footprint run_jitter run_spill clean
