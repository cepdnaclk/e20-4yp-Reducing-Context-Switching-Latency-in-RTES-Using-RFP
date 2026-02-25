# Timing Analysis Package

This folder collects the context-switch timing instrumentation on Spike and a FreeRTOS demo that consumes it.

## Layout
- `SpikeChanges/` – notes on the simulator-side changes that add custom CSRs for timing.
- `FreeRTOSDemo/` – a minimal FreeRTOS RISC-V demo that reads the timing CSRs and prints results under Spike.

## Spike changes summary
- Added machine-level CSRs:
  - `CSR_MCTX_START` (0x7c0): cycle count at trap entry.
  - `CSR_MCTX_END`   (0x7c1): cycle count at trap return.
  - `CSR_MCTX_DELTA` (0x7c2): `end - start` cycles for the last switch.
- CSRs are read-only to software (U/S/M reads allowed), written by Spike on trap entry/return.
- Touchpoints: `riscv/encoding.h`, `riscv/processor.h`, `riscv/csr_init.cc`, `riscv/processor.cc`, `riscv/insns/mret.h`, `riscv/insns/sret.h`.

## FreeRTOS demo
- Two tasks alternate `taskYIELD()`; after each yield they read `mctx_start/end/delta` and print via Spike HTIF console.
- HTIF (`tohost/fromhost`) is defined in the linker script; no pk required.

### Build & run (from FreeRTOSDemo)
```
cd FreeRTOSDemo
make clean
make
make run   # runs: spike --isa=rv64gc freertos.elf
```

## Sample output
```
TaskB 0: start=2753 end=2878 delta=125
TaskA 0: start=192762 end=192892 delta=130
TaskB 1: start=402774 end=402902 delta=128
TaskA 1: start=612786 end=612917 delta=131
TaskB 2: start=822928 end=823057 delta=129
TaskA 2: start=972812 end=972980 delta=168
TaskA 3: start=1242825 end=1242956 delta=131
TaskB 3: start=1462838 end=1462967 delta=129
TaskA 4: start=1682851 end=1682982 delta=131
TaskB 4: start=1943079 end=1943195 delta=116
```
- `start/end` are `mcycle` at trap entry/return; `delta` is the cycles spent in the trap/return path (simulated cycles).
- Minor interleaving is normal because tasks print back-to-back.
