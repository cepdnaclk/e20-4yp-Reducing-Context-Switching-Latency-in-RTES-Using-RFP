# Reducing Context-Switching Latency in RTES Using Register-File Partitioning

![License](https://img.shields.io/badge/License-MIT-green.svg)
![Architecture](https://img.shields.io/badge/Architecture-RISC--V-informational)
![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS-blue)
![RTL](https://img.shields.io/badge/RTL-SystemVerilog-orange)
![Simulator](https://img.shields.io/badge/Simulator-Verilator-yellow)
![ISS](https://img.shields.io/badge/ISS-Spike-lightgrey)
![Language](https://img.shields.io/badge/Primary%20Language-C-00599C)

Research repository for a hardware-software co-design that reduces context-switch overhead in real-time embedded systems on RISC-V by combining:

- RTL support for partitioned integer register-file windows in CVA6
- FreeRTOS kernel integration for partition-aware scheduling paths
- Bare-metal and RTOS regression suites for cycle and behavior validation
- Reproducible scripts for evaluation artifacts used in the paper

## 1) Overview

The main hypothesis is that context-switch overhead in RT systems is dominated by software save and restore of general-purpose registers. This project evaluates whether partition-aware switching can remove most of that memory traffic on critical paths while keeping software compatibility for non-partitioned tasks.

The implementation includes:

- CVA6 RTL and verification flows in `cva6/`
- FreeRTOS port integration in `FreeRTOS-Kernel/`
- Spike support and smoke checks in `riscv-isa-sim/`
- Paper sources and generated tables in `Paper/`

## 2) Repository Structure

```text
<project-root>/
  cva6/                 RTL, DV, regression, context-switch tests
  FreeRTOS-Kernel/      FreeRTOS kernel and CVA6 demos
  riscv-isa-sim/        Spike fork and research test binaries
  Paper/                Manuscript and generated evaluation tables
  web-page/             Project website assets
```

Primary evaluation entrypoint:

- `cva6/verif/tests/custom/context_switch/reproduce_ctxsw_eval.sh`

## 3) Environment and Prerequisites

Recommended environment:

- Linux shell or Docker container
- RISC-V GNU toolchain with `riscv-none-elf-gcc`
- Verilator build dependencies required by CVA6

Required environment variables:

```bash
export RISCV=/opt/riscv
export PATH="${RISCV}/bin:${PATH}"
```

Common defaults used by scripts:

- `DV_TARGET=cv32a6_imac_sv32`
- `RUN_TIMEOUT_S=180`
- `ISS_TICKS=5000000`
- `NTB_SEED=20260417`
- `SEED=20260417`

## 4) Quick Start: Reproducible End-to-End Sweep

From the `cva6` directory:

```bash
cd /path/to/project-root/cva6
bash verif/tests/custom/context_switch/reproduce_ctxsw_eval.sh --project-root /path/to/project-root
```

What this runs:

1. Bare-metal context-switch regression  
   `verif/regress/context-switch-baremetal.sh`
2. Integer-only register-footprint assay  
   `verif/tests/custom/context_switch/measure_task_register_footprint.sh`
3. FreeRTOS CVA6 regression  
   `verif/regress/freertos-windowed-cva6.sh`

Optional flags:

- `--build-model` rebuilds Verilator model first
- `--with-spike` builds Spike and runs research-tests smoke pass
- `--skip-freertos` runs bare-metal plus footprint only

## 5) Manual Run Paths

### 5.1 Bare-metal campaign

```bash
cd /path/to/project-root/cva6
DV_TARGET=cv32a6_imac_sv32 BUILD_MODEL=0 RUN_TIMEOUT_S=180 ISS_TICKS=5000000 \
bash verif/regress/context-switch-baremetal.sh
```

### 5.2 FreeRTOS CVA6 suite

```bash
cd /path/to/project-root/cva6
bash verif/regress/freertos-windowed-cva6.sh
```

This compiles and runs demos from `FreeRTOS-Kernel/demo/CVA6_FYP`, including integrity, sequence, borrow, yield-latency, and matrix-string latency variants.

### 5.3 Integer-only register-footprint assay

```bash
cd /path/to/project-root/cva6
export RISCV=/opt/riscv
export PATH="${RISCV}/bin:${PATH}"
bash verif/tests/custom/context_switch/measure_task_register_footprint.sh
```

The assay compiles `register_footprint_tasks.c` and emits static GPR usage summaries for:

- `pid`
- `aha_mont64`
- `crc32`
- `cubic`
- `edn`
- `huffbench`
- `matmult_int`
- `minver`
- `iot_st`

## 6) Expected Results

Validated bare-metal cycle summaries from the current campaign:

- `rr2_window_switch`: `mcycle_dec min/max/avg = 2/2/2`
- `scale4_window_switch`: `2/2/2`
- `scale4_mixed_window_switch`: `2/2/2`
- `latency_hw_window`: `2/2/2`
- `latency_baseline_sw`: `62/96/62`
- `spill_oldest_window_switch`: `1134/2300/1571`
- `stress_rt_window100`: `107/1249/709`
- `stress_rt_baseline100`: `388/696/393`

Interpretation guidance:

- Windowed micro-tests should show very low fixed switch deltas
- Baseline software save and restore is higher and more variable
- Stress cases include spill pressure and show wider distributions

## 7) Output Artifacts

### 7.1 Regression output directories

- Bare-metal logs and signatures  
  `cva6/verif/sim/out_<date>/ctxsw_baremetal/`
- FreeRTOS regression outputs  
  `cva6/verif/sim/out_<date>/freertos_cva6/`
- Register-footprint assay outputs  
  `cva6/verif/sim/out_<date>/ctxsw_register_footprint/`

### 7.2 Register-footprint files

- `register_footprint_counts.csv`
- `register_footprint_report.md`
- `register_footprint_tasks.<target>.S`

### 7.3 Paper-integrated generated files

- `Paper/generated/register_footprint_rows.tex`
- `Paper/generated/register_footprint_summary.tex`

## 8) Key Measurement Signals

The bare-metal tests write signature values consumed by regression scripts:

- `0x120/0x124/0x128`: `mcycle` min, max, avg
- `0x12c..0x14c`: workload and trap phase counters

Printed summaries usually include:

- `simulator_cycles`
- `mcycle_hex` and `mcycle_dec` triplets
- `breakdown_dec_sum`
- `breakdown_dec_avg_per_iter`

## 9) Troubleshooting

### Toolchain not found

Symptom:

- `riscv-none-elf-gcc not found in PATH`

Fix:

```bash
export RISCV=/opt/riscv
export PATH="${RISCV}/bin:${PATH}"
```

### Missing Verilator harness

Symptom:

- Missing `cva6/work-ver/Variane_testharness`

Fix:

```bash
cd /path/to/project-root/cva6
make verilate verilator="verilator --no-timing" target=cv32a6_imac_sv32
```

Or run reproducible sweep with:

```bash
bash verif/tests/custom/context_switch/reproduce_ctxsw_eval.sh --build-model --project-root /path/to/project-root
```

### FreeRTOS regression failures

Checks:

- Rebuild demos from `FreeRTOS-Kernel/demo/CVA6_FYP`
- Confirm target ISA and ABI match the configured `DV_TARGET`
- Inspect latest logs under `verif/sim/out_<date>/freertos_cva6/`

### Test hangs or unexpected behavior

Checks:

- Confirm `*** SUCCESS ***` markers in log files
- Inspect generated traces in output directories
- Verify script layout assumptions: sibling `cva6`, `FreeRTOS-Kernel`, and `riscv-isa-sim`

## 10) Reproducibility and Publishing Checklist

Before paper update or release:

1. Run `reproduce_ctxsw_eval.sh` and archive output folders
2. Regenerate register-footprint CSV and TeX files
3. Confirm `Paper/main.tex` consumes updated generated artifacts
4. Record command line, target, seeds, and date in experiment notes

## 11) Additional Internal Documentation

- `cva6/verif/tests/custom/context_switch/README.md`
- `cva6/verif/tests/custom/context_switch/REPRODUCIBLE_RUN_README.md`
- `cva6/verif/tests/custom/context_switch/JOURNAL_EVALUATION_SUPPLEMENT.md`
- `FreeRTOS-Kernel/demo/CVA6_FYP/README.md`

## 12) Scope Notes

- Current campaign focuses on integer-only workload analysis for register-footprint assays
- Cycle claims are taken from CVA6 RTL simulation outputs
- Spike is used for functional and smoke validation workflows, not primary cycle reporting

