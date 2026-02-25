# Spike Changes (Context Timing CSRs)

## Overview
Custom machine-level CSRs were added to Spike to expose context-switch timing captured by the simulator:
- `CSR_MCTX_START` (0x7c0): cycle count at trap/interrupt/ECALL entry.
- `CSR_MCTX_END`   (0x7c1): cycle count at `mret`/`sret` return.
- `CSR_MCTX_DELTA` (0x7c2): `end - start` cycles for the last switch.

These CSRs are read-only to software (but readable from any privilege) and are written by Spike internally at trap entry/return.

## Code touchpoints
- `riscv/encoding.h`: CSR numbers and DECLARE_CSR helpers.
- `riscv/processor.h`: state pointers for the new CSRs.
- `riscv/csr_init.cc`: registers the CSRs with a read-only class allowing user-level reads.
- `riscv/processor.cc`: captures `mctx_start` on trap entry.
- `riscv/insns/mret.h`, `riscv/insns/sret.h`: capture `mctx_end` and compute `mctx_delta` on return.

## Code snippets (changed areas)
- `riscv/encoding.h`
	- Added CSR numbers: `#define CSR_MCTX_START 0x7c0`, `CSR_MCTX_END 0x7c1`, `CSR_MCTX_DELTA 0x7c2`.
	- Added DECLARE_CSR entries: `DECLARE_CSR(mctx_start, CSR_MCTX_START)`, etc.

- `riscv/processor.h`
	- Added state pointers: `csr_t_p mctx_start; csr_t_p mctx_end; csr_t_p mctx_delta;` inside `state_t`.

- `riscv/csr_init.cc`
	- Introduced `ctx_ro_csr_t` (read-only, user-readable) and registered the three CSRs:
		- `add_csr(CSR_MCTX_START, mctx_start = std::make_shared<ctx_ro_csr_t>(...));`
		- similarly for `mctx_end`, `mctx_delta`.

- `riscv/processor.cc`
	- In `take_trap`, on trap entry: `state.mctx_start->write(state.mcycle->read());`

- `riscv/insns/mret.h` and `riscv/insns/sret.h`
	- On return: read `mcycle` into `mctx_end`, compute `mctx_delta = end - start` (with underflow guard), write both CSRs.

## Behavior
- Per-hart, overwritten on each trap/return pair.
- Values come from `mcycle` (simulated core cycles, not wall clock).
- Reads allowed at U/S/M; CSR writes trap as illegal.

## Build hint
Rebuild and install Spike after these changes:
```
cd /home/kavindu/spike-sim-fyp/build
make -j$(nproc)
make install
```
