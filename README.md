# Reducing Context Switching Latency in Real-Time Embedded Systems Using Register File Partitioning

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![RISC-V](https://img.shields.io/badge/RISC-V-ISA-009688?logo=riscv)](https://riscv.org)
[![Docker](https://img.shields.io/badge/Docker-Environment-0db7ed?logo=docker)](docker/)

## Research Context

Context switching latency poses a critical challenge in real-time embedded systems, where deterministic task switching is essential for meeting hard deadlines. Traditional context switching requires full register file save/restore sequences—introducing non-deterministic overhead that scales with register count and disrupts pipeline execution. This problem is particularly acute in RISC-V embedded profiles (RV32E/EMC), where minimal register sets still incur significant cycle penalties during frequent task switches in RTOS environments.

Prior research has explored architectural techniques to mitigate this overhead:
- **Register windows** (SPARC, IA-64): Overlapping register sets reduce spill/fill operations but introduce complexity in window management and limit task count scalability
- **Banked registers** (ARM exception levels): Dedicated register sets per privilege mode eliminate save/restore for mode transitions but remain inaccessible to user-level task switching
- **Shadow register sets** (MIPS MT): Hardware-threaded register replication supports concurrent contexts but increases silicon area proportionally to thread count

Recent literature (Abdallah et al., 2020; Mariani et al., 2022) identifies a research gap in *lightweight*, *static partitioning* schemes suitable for deeply embedded systems with strict area constraints. Unlike dynamic windowing or full replication, partitioning divides the physical register file into dedicated subsets assigned to specific tasks or priority levels—eliminating save/restore overhead entirely for partitioned registers while maintaining RISC-V's minimalist philosophy. This approach aligns with emerging interest in *application-specific ISA customization* for real-time workloads (Waterman & Asanović, 2021), where predictable latency outweighs general-purpose flexibility.

This project investigates a hardware-aware register file partitioning scheme for RISC-V, evaluating whether static allocation of register subsets to critical real-time tasks can eliminate context switch overhead without compromising the base ISA's simplicity or requiring full register replication.

## Research Objective

To design, implement, and empirically evaluate a register file partitioning mechanism for RISC-V that reduces context switch latency by:
1. **Eliminating save/restore operations** for partitioned registers through static hardware allocation to specific tasks or priority levels
2. **Preserving software transparency** via minimal CSR extensions that control partition boundaries without modifying the base integer instruction set
3. **Maintaining area efficiency** by avoiding full register replication—partitioning shares physical storage while enforcing access isolation through decode logic

The implementation targets the Spike instruction set simulator augmented with partitioning logic and access control mechanisms, enabling cycle-accurate evaluation of context switch latency under realistic RTOS workloads. This methodology follows established practices for pre-silicon architectural exploration in RISC-V research (Celio et al., 2021), allowing quantitative comparison against conventional software-managed context switching.

## Keeping Sub-Repos in Sync

This repository tracks three external codebases as git subtrees:
- `cva6/` from remote `cva6-local`, branch `reasearch`
- `riscv-isa-sim/` from remote `spike-local`, branch `research-dynamic-partitions-FreeRTOS-SP`
- `FreeRTOS-Kernel/` from remote `freertos-local`, branch `fyp-rfp-4`

Before pulling updates (or after cloning this main repo on a new machine), configure the local source remotes:

```bash
./scripts/setup-subrepo-remotes.sh
```

After you add new commits in those source repositories, update this main repo with:

```bash
./scripts/update-subrepos.sh
```

If you prefer one squashed commit per subtree update:

```bash
./scripts/update-subrepos.sh --squash
```

You can also update a single subtree manually, for example:

```bash
git fetch spike-local research-dynamic-partitions-FreeRTOS-SP
git subtree pull --prefix=riscv-isa-sim spike-local research-dynamic-partitions-FreeRTOS-SP -m "Update riscv-isa-sim subtree"
```
