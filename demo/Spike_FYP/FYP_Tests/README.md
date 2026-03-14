# FYP_Tests – Performance tests for FreeRTOS + Spike register windows

These tests measure the performance impact of the register-window context switch (FYP_Tests.txt style) on the FreeRTOS RISC-V port.

## Build

From this directory (`demo/Spike_FYP/FYP_Tests`):

```bash
make
```

Requires `riscv32-unknown-elf-gcc`. Uses `../link.ld`, `../startup.S`, `../libc_stubs.c`, and `../FreeRTOSConfig.h` from the parent demo.

## Tests

### 1. Latency test (context-switch round-trip cycles)

- **Legacy vs Windowed** (default): Task A legacy (full save/restore), Task B windowed (minimal save/restore). Measures round-trip cycles (mcycle before yield → after resume) over 1000 samples; reports min/max/avg (after 10 warmup).
- **Legacy vs Legacy** (baseline): Both tasks legacy. Build with `make demo_latency_legacy.elf`.

```bash
make demo_latency.elf
spike --isa=rv32gc demo_latency.elf
```

```bash
make demo_latency_legacy.elf
spike --isa=rv32gc demo_latency_legacy.elf
```

Compare the **Avg** (and min/max) of the two runs to see the cycle reduction from the windowed path.

### 2. Scale test (4 tasks)

- Four tasks at priority 1 (T0, T1, T2 legacy; T3 windowed). Round-robin; each task increments a counter and prints. Shows correct behaviour with mixed legacy/windowed tasks.

```bash
make demo_scale.elf
spike --isa=rv32gc demo_scale.elf
```

### 3. Workload test (latency with workload)

- Task A: 4×4 matrix multiply then yield; Task B: string reverse then yield. Same round-trip timing as latency test but with workload between yields. 100 samples; reports min/max/avg.
- **Legacy vs Windowed**: `make demo_workload.elf`
- **Legacy vs Legacy** (baseline): `make demo_workload_legacy.elf`

```bash
make demo_workload.elf
spike --isa=rv32gc demo_workload.elf
```

## Interpreting results

- **Latency**: Lower average round-trip cycles for `demo_latency.elf` vs `demo_latency_legacy.elf` indicates fewer cycles spent in the windowed context switch (no GPR save/restore to stack for the windowed task).
- **Workload**: Same idea under load; the delta between legacy and windowed averages reflects the switch-cost difference.
- Run on your **FYP-modified Spike** (64 GPRs, CSR 0x800/0x801, trap/mret window behaviour). Standard Spike does not implement the windowed path.

### Why Legacy and Windowed can show the same Avg

1. **Round-trip measures two switches**  
   Each sample is “yield → other task runs once → back”. So one sample = 2 context switches + a few instructions. The windowed test has one full switch (to/from legacy) and one minimal switch (to/from windowed) per round-trip. The saving is only for that one minimal switch per sample, so the delta can be small (tens of cycles) and get lost in noise.

2. **Tick interrupts dilute the effect**  
   With the tick enabled (default), timer interrupts cause extra context switches (full save/restore in the tick path). Those add cycles and mix full/minimal behaviour, so the measured round-trip may look similar for both tests.

3. **Simulator may not model cycle difference**  
   In the windowed run you should see **"B"** (windowed task ran). If "B" appears but Avg still matches legacy, your Spike build may not reflect cycle savings for minimal save/restore (e.g. no memory latency, or CSR/mret cost similar to store/load).

**To see a clearer cycle difference:** disable the tick so only voluntary yields run. In `../FreeRTOSConfig.h` set:

```c
#define configMTIME_BASE_ADDRESS               ( 0UL )
#define configMTIMECMP_BASE_ADDRESS            ( 0UL )
```

Then rebuild and run both ELFs again:

```bash
make clean && make
spike --isa=rv32gc demo_latency_legacy.elf
spike --isa=rv32gc demo_latency.elf
```

With the tick off, each round-trip is exactly two voluntary switches and the windowed run should show a lower Avg (e.g. tens of cycles less). Restore the original MTIME/MTIMECMP values when you are done with latency comparison.

### Identical Min/Max/Avg and hardware dumps

If **Legacy** and **Windowed** still show the same Min/Max/Avg (e.g. 322 / 340 / 331) even with the tick disabled:

- The port is taking the **minimal path** when the windowed task (B) yields (you see "B" and correct behaviour). The most likely reason for identical cycles is that **the simulator does not model extra cycles for the 32 GPR stores/loads** in the full path, so both paths report the same cycle count. You can document this as a simulator limitation; on real hardware with memory latency, the windowed path would be expected to save cycles.
- **Hardware dumps** (e.g. on write to CSR 0x801): They are taken while still in kernel (window 0). So p0–p31 show kernel/task state; p32–p63 are the other bank. Seeing p32–p63 all zero can mean: (1) the dump runs before the windowed task has run, or (2) the dump view is at a point where the windowed bank has not been updated in the trace. It does not by itself indicate a bug in the port.
- To confirm the **windowed path is correct**, run the windowed latency test and check the "Window check:" line: if it prints `s0 persisted` then the windowed task’s register state survived yield (minimal save/restore is working). If it prints **`s0 lost (check MRET applies 0x801 in sim)`**, see **SPIKE_DEBUG.md**: MRET must apply **state.window_staged** (not previous_window_config) to the register window (see SPIKE_DEBUG.md for the exact fix).
