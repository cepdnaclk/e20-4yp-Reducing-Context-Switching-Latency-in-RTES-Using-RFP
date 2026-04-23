# Reproducible context-switch evaluation (clone-and-run)

This guide is for a user who clones only:

- `cva6`
- `FreeRTOS-Kernel`
- `riscv-isa-sim`

and wants a reproducible run of the validated CVA6 context-switch campaign.

## 1) Required clone layout

Use sibling directories:

```text
<project-root>/
  cva6/
  FreeRTOS-Kernel/
  riscv-isa-sim/
```

If your layout differs, pass `--project-root` to the script.

## 2) Environment requirements

- Linux or Docker shell
- RISC-V toolchain at `${RISCV}/bin` (default expected: `/opt/riscv`)
- CVA6 Verilator dependencies installed

Set toolchain:

```bash
export RISCV=/opt/riscv
export PATH="${RISCV}/bin:${PATH}"
```

## 3) One-command reproducible run

From inside `cva6`:

```bash
cd /path/to/project-root/cva6
bash verif/tests/custom/context_switch/reproduce_ctxsw_eval.sh --project-root /path/to/project-root
```

What it runs:

1. Bare-metal regression: `verif/regress/context-switch-baremetal.sh`
2. FreeRTOS CVA6 regression: `verif/regress/freertos-windowed-cva6.sh`

Optional:

- Add `--build-model` to rebuild Verilator model first.
- Add `--with-spike` for Spike build + research-tests smoke pass.
- Add `--skip-freertos` for bare-metal-only runs.

## 4) Expected validated bare-metal numbers

From the current validated campaign:

- `rr2_window_switch`: `mcycle_dec min/max/avg = 2/2/2`
- `scale4_window_switch`: `2/2/2`
- `scale4_mixed_window_switch`: `2/2/2`
- `latency_hw_window`: `2/2/2`
- `latency_baseline_sw`: `62/96/62`
- `spill_oldest_window_switch`: `1134/2300/1571`
- `stress_rt_window100`: `107/1249/709`
- `stress_rt_baseline100`: `388/696/393`

## 5) Artifact locations

- Bare-metal:
  - `cva6/verif/sim/out_<date>/ctxsw_baremetal/`
- FreeRTOS:
  - `cva6/verif/sim/out_<date>/freertos_cva6/`

## 6) Commit checklist

Before publishing/review:

1. Ensure these files are tracked:
   - `verif/tests/custom/context_switch/reproduce_ctxsw_eval.sh`
   - `verif/tests/custom/context_switch/REPRODUCIBLE_RUN_README.md`
2. Ensure docs point to this reproducible entrypoint.
3. Re-run the script and keep resulting logs under `verif/sim/out_<date>/...` for evidence.
