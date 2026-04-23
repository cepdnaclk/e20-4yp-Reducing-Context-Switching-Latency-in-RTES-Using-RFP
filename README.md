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

This repository tracks three external codebases as **git subtrees**. The **source of truth** for each subtree is your GitHub fork (so work survives even if you delete local working copies, as long as it was pushed):

- [ChethiyaB/cva6](https://github.com/ChethiyaB/cva6) → `cva6/`
- [ChethiyaB/riscv-isa-sim](https://github.com/ChethiyaB/riscv-isa-sim) → `riscv-isa-sim/`
- [ChethiyaB/FreeRTOS-Kernel](https://github.com/ChethiyaB/FreeRTOS-Kernel) → `FreeRTOS-Kernel/`

Remotes in this repo are named `cva6-local`, `spike-local`, and `freertos-local` (they point at those URLs by default).

**Default branches** used for subtree pulls (override with env vars if you rename branches on the forks):

| Subtree | Remote name | Default branch | Override env |
|---------|-------------|----------------|--------------|
| `cva6/` | `cva6-local` | `reasearch` | `CVA6_SUBTREE_BRANCH` |
| `riscv-isa-sim/` | `spike-local` | `research-dynamic-partitions-FreeRTOS-SP` | `SPIKE_SUBTREE_BRANCH` |
| `FreeRTOS-Kernel/` | `freertos-local` | `fyp-rfp-4` | `FREERTOS_SUBTREE_BRANCH` |

**Workflow:** commit and **push** on the fork branch you use for that subtree, then in this repository:

```bash
./scripts/setup-subrepo-remotes.sh   # once per clone; defaults to HTTPS forks above
./scripts/update-subrepos.sh
```

To point remotes at local clones instead (optional):

```bash
USE_LOCAL_CLONES=1 ./scripts/setup-subrepo-remotes.sh
```

To use different fork URLs:

```bash
CVA6_REMOTE_URL="https://github.com/you/cva6.git" ./scripts/setup-subrepo-remotes.sh
```

If you prefer one squashed commit per subtree update:

```bash
./scripts/update-subrepos.sh --squash
```

**Subtree note:** `git subtree pull` must follow the **same branch** that was used when the subtree was first added (or a descendant of that history). If your fork’s default branch is `master` but the subtree tracks `reasearch`, push your work to a branch named `reasearch` on the fork (or set `CVA6_SUBTREE_BRANCH` to match what you actually push).

You can also update a single subtree manually, for example:

```bash
git fetch spike-local research-dynamic-partitions-FreeRTOS-SP
git subtree pull --prefix=riscv-isa-sim spike-local research-dynamic-partitions-FreeRTOS-SP -m "Update riscv-isa-sim subtree"
```

## GitHub “Contributors” graph (upstream authors)

The **Insights → Contributors** view on [this repository](https://github.com/cepdnaclk/e20-4yp-Reducing-Context-Switching-Latency-in-RTES-Using-RFP) is built from **commit authors on the default branch**. Importing `cva6/`, `riscv-isa-sim/`, and `FreeRTOS-Kernel/` with **`git subtree add` brought in their full histories**, so GitHub correctly lists everyone who ever authored those commits—not only your team.

GitHub **does not** offer a setting to exclude specific directories or “vendor” paths from that graph (see [Viewing a repository’s contributors](https://docs.github.com/en/repositories/viewing-activity-and-data-for-your-repository/viewing-a-projects-contributors)).

**Ways to change what the graph shows (all involve trade-offs):**

1. **Keep full history on a non-default branch, use a slimmer default branch**  
   Push the current `main` to a branch such as `archive/full-subtree-history`, then replace **default** `main` with a new history that only contains your org’s meta commits (for example one squashed snapshot per release, or **git submodules** pointing at your forks). The Contributors graph for the default branch then mostly reflects people who commit to that meta history. Anyone who needs full per-file history can use the archive branch or the forks.

2. **Submodules instead of subtrees for future integration**  
   The default branch then mostly records submodule pointer bumps; upstream authors stay on the separate repositories’ graphs, not mixed into this repo’s graph the same way.

3. **Do nothing**  
   Leave the graph as-is; treat it as “authors present in imported histories,” not “people who worked on this FYP only.”

There is **no** repository toggle that hides upstream authors while keeping the same default-branch subtree history unchanged.
