# CVA6 Context-Switch Bare-Metal Tests

This folder contains bare-metal tests used to validate and measure the partitioned register-window context-switch mechanism in CVA6.

## What Hardware Feature Is Being Tested

These tests target the custom register-window CSRs and trap handshake:

- `CSR 0x801`: staged/next window configuration (written by software before `ecall`)
- `CSR 0x800`: active window configuration (read after trap return to verify switch)
- `mret` path: commits staged window config back into active mapping
- `mtvec` trap handler: handles `ecall`, increments `mepc`, and returns with `mret`
- `mcycle` (`CSR 0xB00`): used for cycle delta measurement
- `mcountinhibit` (`CSR 0x320`): cleared to ensure counters run

All tests use `tohost` for pass/fail termination and write min/max/avg values to signature addresses consumed by the regression script.

## Files in This Folder

- `rr2_window_switch.S`: two-window round-robin switching test
- `scale4_window_switch.S`: four-window scaling/rotation test
- `scale4_mixed_window_switch.S`: four-window scaling test with mixed per-task window sizes
- `spill_oldest_window_switch.S`: 5-context pressure test with software spill of oldest resident partition
- `stress_rt_window100.S`: 100-task integer RT stress (windowed, forced spill/restore under slot pressure)
- `stress_rt_baseline100.S`: 100-task integer RT stress baseline (unpartitioned, software context save/restore)
- `latency_baseline_sw.S`: software save/restore baseline latency test
- `latency_hw_window.S`: hardware window-switch latency test
- `link_baremetal.ld`: bare-metal linker script used by these tests
- `context_switch_sw.c`, `context_switch_window.c`: earlier C-side benchmark stubs (not used by the main bare-metal regression flow)

## How To Run

Run from repository root (`/FYP/cva6` in your container):

```bash
DV_TARGET=cv32a6_imac_sv32 BUILD_MODEL=0 RUN_TIMEOUT_S=180 ISS_TICKS=5000000 \
bash verif/regress/context-switch-baremetal.sh
```

Notes:

- Set `BUILD_MODEL=1` if you want the script to rebuild Verilator model first.
- Output logs/traces are written under:
  - `verif/sim/out_<date>/ctxsw_baremetal/`

## Common Output Fields

For each test, the runner prints:

- `simulator_cycles=<n>`: total testharness cycles to completion
- `benchmark="MCYCLE_* min/max/avg=0x.../0x.../0x..."`
- `mcycle_hex min=... max=... avg=...`
- `mcycle_dec min=... max=... avg=...`
- `breakdown_dec_sum ...` and `breakdown_dec_avg_per_iter ...` (for all bare-metal tests in this suite)

`mcycle_dec` is derived from signature writes at `0x120/0x124/0x128`.

Additional phase counters are written as iteration sums:

- `0x12c`: workload body cycles
- `0x130`: trap round-trip cycles (`ecall` to post-`mret`)
- `0x134`: interrupt entry latency (`ecall` to first trap timestamp)
- `0x138`: trap handler body cycles (inside `trap_entry`, before `mret`)
- `0x13c`: spill/save cycles (`spill` in windowed test, `save+restore` in baseline)
- `0x140`: restore cycles (windowed; baseline is `0`)
- `0x144`: window stage cycles (`csrw 0x801`; baseline is `0`)
- `0x148`: `mret` return latency (trap exit to first post-return timestamp)
- `0x14c`: config mismatch count (windowed integrity diagnostic; baseline is `0`)

The regression script converts these sums into per-iteration averages and prints them in `breakdown_dec_avg_per_iter` using the test's loop count (`64`, `128`, `5`, or `500` depending on test).

## Test-by-Test Behavior

### 1) `rr2_window_switch.S`

**Purpose**

- Validates two-context switching using staged window configs:
  - `0x00200000` and `0x00200020`

**Hardware interaction**

- Writes target config to `CSR 0x801`
- Executes `ecall`
- Trap handler advances `mepc` and returns with `mret`
- Reads `CSR 0x800` to confirm active window equals staged window

**Expected result**

- Should print `MCYCLE_RR2 ...`
- Current validated run shows fixed low delta behavior:
  - `mcycle_dec min/max/avg = 2 / 2 / 2`

### 2) `scale4_window_switch.S`

**Purpose**

- Validates scaling to four rotating windows:
  - `0x00100000`, `0x00100010`, `0x00100020`, `0x00100030`

**Hardware interaction**

- Same staged write -> `ecall` -> `mret` flow as RR2
- Rotates through 4 windows and verifies each via `CSR 0x800`
- Uses memory-backed loop/stat state to stay robust under window remapping

**Expected result**

- Should print `MCYCLE_SCALE4 ...`
- Current validated run:
  - `mcycle_dec min/max/avg = 2 / 2 / 2`

### 3) `latency_baseline_sw.S`

**Purpose**

- Baseline for software-style context save/restore overhead.
- Includes explicit register save/restore routine (`dummy_save_restore`) each iteration.

**Hardware interaction**

- Trap still uses `ecall`/`mret`, but measured path includes software save/restore cost.
- `mcycle` sampling fixed to avoid trap-clobbered timestamp register artifacts.

**Expected result**

- Should print `MCYCLE_LAT_SW ...`
- Current validated run (representative):
  - `mcycle_dec min/max/avg = 27 / 34 / 27`

### 4) `scale4_mixed_window_switch.S`

**Purpose**

- Validates four-way context rotation with non-uniform windows:
  - Task0: size 8, base 0 (`0x00080000`)
  - Task1: size 12, base 8 (`0x000c0008`)
  - Task2: size 16, base 20 (`0x00100014`)
  - Task3: size 20, base 36 (`0x00140024`)

**Hardware interaction**

- Same staged write to `CSR 0x801`, `ecall`, trap return via `mret`
- Confirms active config through `CSR 0x800`
- Uses memory-backed control/stat state and heartbeat at `0x118` for robustness

**Expected result**

- Should print `MCYCLE_SCALE4_MIXED ...`
- Typical behavior in this micro-benchmark setup is low fixed deltas similar to other windowed tests.

### 5) `spill_oldest_window_switch.S`

**Purpose**

- Forces partition pressure: 5 logical tasks share only 4 resident window slots (size 16 each over 64 regs).
- When task 4 is selected and no free slot exists, the test applies an oldest-first victim policy and simulates spill/restore memory traffic before reusing that slot.

**Hardware interaction**

- Resident slot windows use standard staged switch path:
  - write target config to `CSR 0x801`
  - `ecall` -> trap handler -> `mret`
- sample active config from `CSR 0x800` while continuing pressure-sequence measurement
- Oldest-slot metadata and stats are maintained in memory.
- `spill_simulate` models eviction/reload cost via explicit memory store/load loops.

**Expected result**

- Should print `MCYCLE_SPILL_5WAY ...`
- Compared to pure windowed tests, this case can show higher deltas because of software spill simulation overhead.

### 6) `latency_hw_window.S`

**Purpose**

- Measures trap+hardware window-switch path without software register save/restore.

**Hardware interaction**

- Same staged window write to `CSR 0x801`
- `ecall` trap return through `mret`
- Verifies active config via `CSR 0x800`

**Expected result**

- Should print `MCYCLE_LAT_HW ...`
- Current validated run:
  - `mcycle_dec min/max/avg = 2 / 2 / 2`

### 7) `stress_rt_window100.S`

**Purpose**

- Simulates a practical RT embedded workload mix across 100 tasks (PID-style control update, integer matrix workload, telemetry filtering/checksum), with no floating point usage.
- Models partition pressure where available resident windows are fewer than tasks and software performs spill/restore around slot reuse.

**Hardware interaction**

- Uses staged window switching (`CSR 0x801`) and active verification via `CSR 0x800`.
- Trap return path remains `ecall -> trap -> mret`.
- Includes software spill/restore simulation hooks to model context movement when no resident slot is free.

**Expected result**

- Should print `MCYCLE_STRESS_WIN100 ...`, plus `breakdown_dec ...`.
- Writes aggregate min/max/avg and phase breakdown values to `0x120..0x148`.
- Expected to show higher and more variable deltas than minimal synthetic windowed tests.

### 8) `stress_rt_baseline100.S`

**Purpose**

- Baseline for the same 100-task integer RT workload mix without partitioned windows.
- Each switch includes software context-save/restore simulation to represent unpartitioned register-file switching overhead.

**Hardware interaction**

- Uses standard trap return (`ecall` + `mret`) without `CSR 0x801` window staging for per-task partition changes.

**Expected result**

- Should print `MCYCLE_STRESS_BASE100 ...`, plus `breakdown_dec ...`.
- Writes aggregate min/max/avg and phase breakdown values to `0x120..0x148`.
- Intended for direct comparison against `stress_rt_window100.S`.

## Interpreting The Results

- `baseline_sw` larger than windowed methods is expected due to software save/restore work.
- Windowed tests currently show fixed minimum-like deltas in this micro-benchmark shape and simulation setup.
- Compare both:
  - `simulator_cycles` for total test runtime, and
  - `mcycle_dec` for per-switch measured delta.

## Troubleshooting

- If a test hangs/fails, check:
  - `*.log` for `*** SUCCESS ***`
  - `*.trace_rvfi_hart_00.dasm` for signature writes and heartbeat writes (`0x118`)
  - unexpected trap diagnostics (`0x10c` region)
- Ensure the test is linked with `link_baremetal.ld` and starts at `_start` with initialized stack.

