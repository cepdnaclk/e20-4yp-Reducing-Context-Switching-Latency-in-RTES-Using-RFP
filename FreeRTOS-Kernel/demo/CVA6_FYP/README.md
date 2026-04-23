# FreeRTOS CVA6 Register-Window Tests

This demo suite validates a CVA6-first FreeRTOS register-window integration:

- kernel owns a dedicated 32-GPR window (`base=0,size=32`),
- tasks can use variable dedicated windows,
- selected tasks can borrow the kernel window when kernel is not active.

## Build

From `demo/CVA6_FYP`:

```bash
make clean
make
```

Default build settings match the CVA6 bare-metal context-switch regressions:

- `CORE_MARCH=rv32imac_zbkb_zbkx_zkne_zknd_zknh_zicsr_zifencei`
- `CORE_MABI=ilp32`
- `CROSS_COMPILE=riscv-none-elf-`

Override if needed for a different configured core variant:

```bash
make clean
make CORE_MARCH=rv32imac_zicsr_zifencei CORE_MABI=ilp32
```

Generates:

- `demo_integrity.elf`
- `demo_sequence.elf`
- `demo_borrow.elf`

## Run on CVA6 testharness

Run from `/FYP/CVA6/cva6` inside container:

```bash
bash verif/regress/freertos-windowed-cva6.sh
```

This script compiles the FreeRTOS demos and runs each ELF in `Variane_testharness`.

## Test intent

- **integrity**: checks register sentinel persistence across repeated yields with mixed windowed/borrowed tasks.
- **sequence**: checks deterministic round-robin order for equal-priority variable-window tasks.
- **borrow**: stresses mixed borrowed + dedicated tasks and validates borrow counters and shared-state behavior.

## Signature words

Each test writes:

- `0x120`: phase/test code
- `0x124`: aux0
- `0x128`: aux1
- `0x12c`: pass flag (`1` pass, `0` fail)

and then writes `tohost` (`1` pass, `0` fail).
