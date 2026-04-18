# Context-Switch Deterministic Results

## Objective

This report summarizes deterministic, one-by-one bare-metal runs for the remaining non-stress tests and both 100-task stress tests.  
The goal is to provide reproducible cycle-level evidence for context-switch behavior under:

- hardware-partitioned window switching, and
- software baseline save/restore switching.

## Deterministic Run Setup

- Platform: `Variane_testharness` in Docker container `RFP_CVA6`
- Date: `2026-04-18`
- Seed: `+ntb_random_seed=20260417`
- Timeout per test: `180s` (`+time_out=5000000`)
- ISA/MABI: `rv32imac_zbkb_zbkx_zkne_zknd_zknh_zicsr_zifencei` / `ilp32`
- Linker: `link_baremetal.ld`

Each test was compiled and executed in strict sequence (no parallel overlap):

1. `rr2_window_switch`
2. `scale4_window_switch`
3. `scale4_mixed_window_switch`
4. `spill_oldest_window_switch`
5. `latency_baseline_sw`
6. `latency_hw_window`
7. `stress_rt_window100`
8. `stress_rt_baseline100`

## Execution Summary (All Deterministic Runs)

| Test | Status | Simulator cycles |
|---|---:|---:|
| `rr2_window_switch` | PASS | 952,927 |
| `scale4_window_switch` | PASS | 813,377 |
| `scale4_mixed_window_switch` | PASS | 882,666 |
| `spill_oldest_window_switch` | PASS | 5,699 |
| `latency_baseline_sw` | PASS | 24,288 |
| `latency_hw_window` | PASS | 912,383 |
| `stress_rt_window100` | PASS | 468,675 |
| `stress_rt_baseline100` | PASS | 279,964 |

All eight tests completed with `*** SUCCESS ***` under fixed-seed deterministic execution.

## Stress-Test Comparison (Research-Oriented View)

### Primary switch-latency metric (min/max/avg per iteration)

| Metric | `stress_rt_window100` | `stress_rt_baseline100` | Relative change (window vs baseline) |
|---|---:|---:|---:|
| Min switch cycles | 106 | 388 | -72.7% |
| Max switch cycles | 1,218 | 696 | +75.0% |
| Avg switch cycles | 677 | 393 | +72.3% |

Interpretation: the windowed stress path shows lower best-case latency but higher average and tail under this 100-task, forced-pressure setup.

### Phase-level cycle breakdown (accumulators)

Address mapping follows `README.md`:
`0x12c` workload, `0x130` trap round-trip, `0x134` trap entry, `0x138` trap body, `0x13c` spill/save, `0x140` restore, `0x144` stage, `0x148` mret return, `0x14c` cfg mismatch.

| Component | `stress_rt_window100` | `stress_rt_baseline100` | Relative change (window vs baseline) |
|---|---:|---:|---:|
| Workload body (`0x12c`) | 106,991,956 | 65,541,431 | +63.2% |
| Trap round-trip (`0x130`) | 30,847 | 31,228 | -1.2% |
| Trap entry (`0x134`) | 10,680 | 10,848 | -1.5% |
| Trap body (`0x138`) | 6,072 | 6,035 | +0.6% |
| Spill/save (`0x13c`) | 124,702 | 156,260 | -20.2% |
| Restore (`0x140`) | 125,705 | 0 | N/A (window-only) |
| Stage (`0x144`) | 2,269 | 0 | N/A (window-only) |
| mret return (`0x148`) | 14,107 | 14,345 | -1.7% |
| Config mismatch (`0x14c`) | 0 | 0 | clean |

## Brief Research Description

These runs emulate embedded RT-style task switching at 100-task scale with deterministic simulation controls.  
The baseline uses software context save/restore, while the windowed variant uses hardware window staging with forced spill/restore pressure and explicit phase instrumentation.

Key finding for this dataset: trap-path overheads (`entry`, `body`, `mret`, and total round-trip) are broadly comparable, while total observed switching behavior is dominated by policy/pressure effects (spill/restore/stage behavior and task-flow work), producing higher average switch latency in the current windowed 100-task configuration despite lower best-case events.
