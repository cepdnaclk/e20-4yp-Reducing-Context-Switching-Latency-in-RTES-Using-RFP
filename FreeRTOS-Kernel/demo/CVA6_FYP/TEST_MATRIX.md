# CVA6 FreeRTOS Test Matrix

## Phase 0 - Baseline reproducibility

| Test flow | Command | Pass criteria |
|---|---|---|
| CVA6 bare-metal context suite | `bash verif/regress/context-switch-baremetal.sh` | All tests print `*** SUCCESS ***`; deterministic signatures written in `0x120..0x14c`. |

## Phase 1 - Port contract

| Test flow | Command | Pass criteria |
|---|---|---|
| FreeRTOS CVA6 build | `make -C FreeRTOS-Kernel/demo/CVA6_FYP` | All ELFs compile successfully with CVA6 chip extensions. |
| Legacy+window task switch smoke | `demo_integrity.elf` via regression script | No invalid minimal-restore events; no sentinel corruption. |

## Phase 2 - Borrow policy

| Test flow | Command | Pass criteria |
|---|---|---|
| Borrow policy execution | `demo_borrow.elf` | Borrow counters advance; invalid minimal counter remains zero; all task hit counts reach loop target. |

## Phase 3 - Integrity and sequencing

| Test flow | Command | Pass criteria |
|---|---|---|
| Integrity | `demo_integrity.elf` | Register sentinels preserved; pass flag `0x12c == 1`. |
| Sequence | `demo_sequence.elf` | Strict round-robin sequence preserved; pass flag `0x12c == 1`. |
| Borrow stress | `demo_borrow.elf` | Mixed borrowed/windowed tasks complete without corruption. |

## Phase 4 - Regression and evidence

| Test flow | Command | Pass criteria |
|---|---|---|
| One-command regression | `bash cva6/verif/regress/freertos-windowed-cva6.sh` | Summary marks all tests `PASS`; artifacts captured under `verif/sim/out_<date>/freertos_cva6/`. |
| Comparative report | generated markdown | Includes pass/fail, simulator cycles, and signature words for each phase test. |
