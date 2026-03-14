# FYP_Tests – Performance tests for FreeRTOS + Spike register windows

These tests measure the performance impact of the register-window context switch (FYP_Tests.txt style) on the FreeRTOS RISC-V port.

| Phase | Test | Target |
|-------|------|--------|
| 1 | Cycle-Accurate Latency Breakdown | `demo_cycle_breakdown.elf` |
| 2 | SRAM / Memory Footprint | `demo_footprint.elf` |
| 3 | WCET & Jitter (memory flood) | `demo_jitter.elf` |
| 4 | Window Spilling (10 tasks, graceful degradation) | `demo_spill.elf` |
| — | Latency, Scale, Workload | `demo_latency.elf`, `demo_scale.elf`, `demo_workload.elf` |

## Build

From this directory (`demo/Spike_FYP/FYP_Tests`):

```bash
make
```

Requires `riscv32-unknown-elf-gcc`. Uses `../link.ld`, `../startup.S`, `../libc_stubs.c`, and `../FreeRTOSConfig.h` from the parent demo.

**After changing port files** (e.g. `portContext.h`, `portASM.S` in `portable/GCC/RISC-V/`), do a clean rebuild so the port object is recompiled:

```bash
make clean
make demo_jitter_noflood.elf   # or make, or the specific .elf you need
```

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

### 3. Phase 1: Cycle-Accurate Latency Breakdown

- **Baseline**: Full path — cycles to push 31 regs (save) and pop 31 regs (restore).
- **Custom**: Minimal path — cycles for write 0x801 + minimal save/restore (no GPR push/pop).
- Build with profile instrumentation: `make demo_cycle_breakdown.elf` (builds port with `-DCYCLE_BREAKDOWN_PROFILE`).
- Run: `spike --isa=rv32gc demo_cycle_breakdown.elf`. Output reports min/max/avg cycles for full save, full restore, minimal save, minimal restore.

```bash
make demo_cycle_breakdown.elf
spike --isa=rv32gc demo_cycle_breakdown.elf
```

### 4. Phase 2: SRAM / Memory Footprint

- Reports context frame size (bytes), minimal path stored bytes, and **bytes saved per yield per windowed task**.
- Run: `make demo_footprint.elf && spike --isa=rv32gc demo_footprint.elf`.

### 5. Phase 3: WCET & Jitter (memory stress)

- Runs two latency-measurement tasks plus a **memory-flood** background task. Reports min/max/avg and **jitter (max − min)**. Under bus contention, windowed path should show lower jitter than legacy.
- Run: `make demo_jitter.elf && spike --isa=rv32gc demo_jitter.elf`.

### 6. Phase 4: Window Spilling (scalability)

- Spawns **10 tasks**; only the first `WINDOWED_TASKS` (1 for NXPR=64, set to 7 for NXPR=256) get a hardware window; the rest use **full save/restore** (graceful degradation). Verifies no crash and all 10 tasks run.
- Run: `make demo_spill.elf && spike --isa=rv32gc demo_spill.elf`. In `spill_test.c`, set `WINDOWED_TASKS` to 7 for an 8-window (NXPR=256) build.

### 7. Workload test (latency with workload)

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

## Tracing with spike -l (instruction log)

The spill test does **not** crash — it completes and prints the report. To see **where** execution goes and whether mret/window logic is involved, use Spike's log option and grep.

**Log every instruction to a file:**

```bash
spike --isa=rv32gc -l demo_spill.elf 2>&1 | tee trace.log
```

Then:

- **Count mret executions** (each context switch ends with mret):
  ```bash
  grep -c mret trace.log
  ```
- **See the instruction immediately after each mret** (the first instruction of the restored task):
  ```bash
  grep -A1 ' mret' trace.log
  ```
  If the window is applied correctly, that next instruction should run in the task's partition; if not, it may still be in window 0.

- **Find csrw 0x801** (port writes the staged window before mret):
  ```bash
  grep 'csrw' trace.log | head -50
  ```
  (Spike disassembly may show the CSR number in a comment or as a symbol; adjust the pattern if needed.)

- **Narrow to a short run** so the log is manageable: reduce `YIELDS_PER_TASK` or `NUM_TASKS` in `spill_test.c` (e.g. 2 tasks, 3 yields), rebuild, then:
  ```bash
  spike --isa=rv32gc -l demo_spill.elf 2>&1 | tee short.log
  grep -E 'mret|csrw' short.log
  ```

**If Spike actually crashes or hangs**, the last lines of `trace.log` (or the last printed PC in the log) show the instruction that was about to execute when it stopped. Grep for that PC or the preceding few instructions to see the failing sequence.

For the **spill "0,0,0"** issue there is no single crash PC; the fix is in the simulator (apply window **before** PC in mret, and ensure `take_trap` sets `state.window_staged`). The log is still useful to confirm that mret is executed and to inspect the instruction stream around context switches.

### Interpreting the spill trace

If you see:

- **`grep -c mret trace.log`** → e.g. 25 (many mrets, so context switch path is taken).
- **`grep -A1 ' mret' trace.log`** → after each mret, the next line is either `>>>>  vTaskSpill` (return to task entry) or a PC like `0x80000130 ... c.j` (return to task loop). Same PC 0x80000130 for different tasks is normal (same code, different stacks); the **register** a0 (task id) should differ per task, but the trace does not show register values.
- **`grep 'csrw' trace.log`** → `csrw unknown_801, t0` (or t2) confirms the port is writing the staged window to 0x801 before mret.

So the trace confirms: **port does csrw 0x801 then mret, and execution resumes at the correct PC**. The wrong "0,0,0" result means that when that next instruction runs, **a0 is not the task's id** — i.e. the simulator is not applying the window so the visible GPRs are from the restored partition. Fix: in Spike mret, apply the window **before** setting PC (see SPIKE_DEBUG.md).

### Spill trace: which task id (a0) does each run see?

To see **every time** a task runs and **which id it sees** (the value in `a0` = pvParameters), build and run the spill test with the task-id trace enabled:

```bash
make demo_spill_trace.elf
spike --isa=rv32gc demo_spill_trace.elf 2>&1 | tee spill.log
```

Each time a task runs, it prints `[SPILL] id=N` (N = 0..9). Then:

- **List all id lines** (see the sequence of task runs):
  ```bash
  grep '\[SPILL\]' spill.log
  ```
  If windowed tasks 1–6 are wrong, you will see many `id=0` lines and few or no `id=1` … `id=6`.

- **Count how many times each id was seen** (how often each task ran with that id):
  ```bash
  grep -o 'id=[0-9]*' spill.log | sort | uniq -c
  ```
  Expected when correct: a spread of counts for id=0,1,…,9. If you see almost all `id=0` and zero or very few for id=1..6, then after mret the CPU is not using the windowed GPRs (a0 is always from window 0).

- **Combine with Spike instruction log** (see which instruction runs after mret):
  ```bash
  spike --isa=rv32gc -l demo_spill_trace.elf 2>&1 | tee spill_insn.log
  grep -E '\[SPILL\]| mret' spill_insn.log | head -80
  ```
  This shows mret and the next `[SPILL] id=N`; if id is always 0 after every mret, the window is not applied for the next instruction.
