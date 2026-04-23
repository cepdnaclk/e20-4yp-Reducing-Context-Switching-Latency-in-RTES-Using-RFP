# Step-by-step: run context-switch and related tests

This guide runs **bare-metal CVA6** tests, **FreeRTOS on CVA6**, optional **Spike** research tests, and optional **RVFI instruction counting**. Paths use the usual Docker layout (`RFP_CVA6`, `/FYP/cva6`, `/FYP/FreeRTOS-Kernel`, `/FYP/riscv-isa-sim`); adjust if your mount points differ.

For a single clone-and-run entrypoint, use `verif/tests/custom/context_switch/reproduce_ctxsw_eval.sh`.

---

## 0. Prerequisites

1. **Docker** with container **`RFP_CVA6`** (or equivalent) where:
   - CVA6 repo is at `/FYP/cva6` (or set `ROOT` below).
   - RISC-V GCC is on `PATH` (e.g. `/opt/riscv/bin`).
2. **Built Verilator model** for CVA6: `cva6/work-ver/Variane_testharness` must exist.  
   - First-time or after RTL changes: use `BUILD_MODEL=1` (slow).  
   - If the model is already built: use `BUILD_MODEL=0` (fast).
3. **Bare-metal regression** requires **`RISCV`** (toolchain prefix), not only `PATH`:

   ```bash
   export RISCV=/opt/riscv
   export PATH="${RISCV}/bin:${PATH}"
   ```

4. Optional: **python3** (for `rvfi_count_instructions.sh` when using `--between`).

---

## 1. Enter the environment

```bash
docker exec -it RFP_CVA6 bash
```

Inside the container:

```bash
export RISCV=/opt/riscv
export PATH="${RISCV}/bin:${PATH}"
```

Confirm tools:

```bash
which riscv-none-elf-gcc
test -x /FYP/cva6/work-ver/Variane_testharness && echo "Variane_testharness: OK"
```

---

## 2. Run all bare-metal context-switch tests (CVA6 Verilator)

These are the `.S` tests in **this directory** (RR2, scale4, spill, stress, latency HW vs SW). The script compiles each test, runs `Variane_testharness`, copies RVFI trace when present, and prints `mcycle` and breakdown lines.

### Step 2.1 — Go to CVA6 root

```bash
cd /FYP/cva6
```

### Step 2.2 — Run the regression script

**Fast path** (model already built):

```bash
export DV_TARGET=cv32a6_imac_sv32
export BUILD_MODEL=0
export RUN_TIMEOUT_S=180
export ISS_TICKS=5000000
export NTB_SEED=20260417

bash verif/regress/context-switch-baremetal.sh
```

**Full rebuild** of the Verilator model first (slow, use when RTL or config changed):

```bash
export BUILD_MODEL=1
bash verif/regress/context-switch-baremetal.sh
```

### Step 2.3 — Check success

- Script exits with **0** if every test logs `*** SUCCESS ***`.
- Console lines look like: `[CTXSW_BM] RESULT <test> simulator_cycles=...`, `mcycle_dec min=... max=... avg=...`, and `breakdown_dec_avg_per_iter ...` when the trace is present.

### Step 2.4 — Find artifacts

```bash
ls -la verif/sim/out_*/ctxsw_baremetal/
```

Per test you get:

- `*.log` — simulator log  
- `*.trace_rvfi_hart_00.dasm` — RVFI disassembly (if trace was produced)

---

## 3. Run all FreeRTOS CVA6 demos (including latency ELFs)

This builds **`FreeRTOS-Kernel/demo/CVA6_FYP`** and runs each ELF on `Variane_testharness`.

### Step 3.1 — From CVA6 root

```bash
cd /FYP/cva6
```

### Step 3.2 — Run the script

```bash
export SEED=20260417
export RUN_TIMEOUT_S=180
export ISS_TICKS=5000000

bash verif/regress/freertos-windowed-cva6.sh
```

### Step 3.3 — Check success

- Open `verif/sim/out_<date>/freertos_cva6/*.log`.
- Each run should contain **`*** SUCCESS ***`** (the script marks PASS from that line).

### Step 3.4 — What was executed

Typical list (see `verif/regress/freertos-windowed-cva6.sh` if it changes):

| ELF | Role |
|-----|------|
| `demo_integrity.elf` | Integrity |
| `demo_sequence.elf` | RR order |
| `demo_borrow.elf` | Borrow policy |
| `demo_ms_baseline.elf` / `demo_ms_windowed.elf` | Matrix+string yield `mcycle` |
| `demo_yield_baseline.elf` / `demo_yield_windowed.elf` | Yield-only `mcycle` |

### Step 3.5 — Artifacts

```bash
ls verif/sim/out_*/freertos_cva6/
cat verif/sim/out_*/freertos_cva6/summary.csv
```

---

## 4. (Optional) Build and run a single FreeRTOS ELF by hand

Use this to debug one binary without the full matrix.

### Step 4.1 — Build

```bash
cd /FYP/FreeRTOS-Kernel/demo/CVA6_FYP
make clean
make demo_ms_baseline.elf \
  CROSS_COMPILE="${RISCV}/bin/riscv-none-elf-" \
  CORE_MARCH=rv32imac_zbkb_zbkx_zkne_zknd_zknh_zicsr_zifencei \
  CORE_MABI=ilp32
```

### Step 4.2 — Resolve `tohost` and run

```bash
cd /FYP/cva6
ELF=/FYP/FreeRTOS-Kernel/demo/CVA6_FYP/demo_ms_baseline.elf
TOHOST=$(riscv-none-elf-nm -B "$ELF" | awk '$3=="tohost"{print $1}' | tail -n 1)
./work-ver/Variane_testharness "$ELF" +elf_file="$ELF" ++"$ELF" \
  +tohost_addr="$TOHOST" +time_out=5000000 +ntb_random_seed=20260417 \
  2>&1 | tee /tmp/single_freertos.log
```

### Step 4.3 — Copy trace (if generated at repo root)

```bash
test -f trace_rvfi_hart_00.dasm && cp trace_rvfi_hart_00.dasm /tmp/single_freertos.trace
```

---

## 5. (Optional) Spike — research-tests and FreeRTOS-on-Spike

Use **patched Spike** from your tree; see `riscv-isa-sim/research-tests/README.md`.

### Step 5.1 — Build Spike (once)

```bash
cd /FYP/riscv-isa-sim
mkdir -p build && cd build
../configure --prefix=/opt/riscv
make -j"$(nproc)"
```

### Step 5.2 — Instruction-capped research ELFs

```bash
cd /FYP/riscv-isa-sim
SPIKE=./build/spike
ISA=rv32imac_zbkb_zbkx_zkne_zknd_zknh_zicsr_zifencei
for elf in research-tests/*/*.elf; do
  test -f "$elf" || continue
  "$SPIKE" --isa="$ISA" --instructions=200000 "$elf" >/tmp/spike_one.log 2>&1
  echo "RESULT|$?|$elf"
done
```

### Step 5.3 — Spike FreeRTOS demos (`Spike_FYP`)

Follow the **“FreeRTOS Spike_FYP matrix”** block in `riscv-isa-sim/research-tests/README.md` (`make` in `demo/Spike_FYP/FYP_Tests`, then `spike --isa=... --instructions=...` per ELF).

---

## 6. (Optional) Count retired instructions in an RVFI trace

After any run that leaves `trace_rvfi_hart_00.dasm` (or a copied `.trace`):

```bash
cd /FYP/cva6/verif/tests/custom/context_switch
bash rvfi_count_instructions.sh /path/to/trace_rvfi_hart_00.dasm
```

Between two PCs (hex with or without `0x`; requires **python3**):

```bash
bash rvfi_count_instructions.sh /path/to/trace_rvfi_hart_00.dasm --between 0x80003700 0x800037d4
```

Pick `START`/`END` from `riscv-none-elf-objdump -d your.elf`.

---

## 7. Methodology write-up (for the paper)

Use **`JOURNAL_EVALUATION_SUPPLEMENT.md`** in this same folder for trampoline description, latency decomposition mapping, and table templates.

---

## 8. Troubleshooting

| Symptom | What to do |
|---------|------------|
| `RISCV variable undefined` | `export RISCV=/opt/riscv` (or your install prefix). |
| `riscv-none-elf-gcc: not found` | `export PATH="${RISCV}/bin:${PATH}"`. |
| `Variane_testharness: not found` | Build CVA6 Verilator model (`BUILD_MODEL=1` once). |
| Bare-metal script times out | Increase `RUN_TIMEOUT_S` or `ISS_TICKS`. |
| No `*.trace_rvfi_hart_00.dasm` | Harness may not emit RVFI for that build; `mcycle` lines may still appear in log. |
| FreeRTOS `summary.csv` says FAIL but log shows SUCCESS | Use log line `*** SUCCESS ***` as ground truth; script uses `awk` to detect it. |

---

## Quick reference (copy-paste block)

```bash
docker exec -it RFP_CVA6 bash
export RISCV=/opt/riscv
export PATH="${RISCV}/bin:${PATH}"
cd /FYP/cva6

# Bare-metal (skip model rebuild if already built)
export DV_TARGET=cv32a6_imac_sv32 BUILD_MODEL=0 RUN_TIMEOUT_S=180 ISS_TICKS=5000000 NTB_SEED=20260417
bash verif/regress/context-switch-baremetal.sh

# FreeRTOS on CVA6
export SEED=20260417 RUN_TIMEOUT_S=180 ISS_TICKS=5000000
bash verif/regress/freertos-windowed-cva6.sh
```
