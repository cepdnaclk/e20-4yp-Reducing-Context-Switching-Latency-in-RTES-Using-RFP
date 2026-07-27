#!/bin/bash

# FreeRTOS CVA6 register-window regression.
# Runs on Verilator testharness (no proxy kernel).

set -eo pipefail

if ! [ -n "${RISCV:-}" ]; then
  echo "Error: RISCV variable undefined"
  exit 1
fi

ROOT_PROJECT="$(readlink -f "$(dirname "${BASH_SOURCE[0]}")/../..")"

DV_TARGET="${DV_TARGET:-cv32a6_imac_sv32}"
ISS_TICKS="${ISS_TICKS:-5000000}"
RUN_TIMEOUT_S="${RUN_TIMEOUT_S:-180}"
NTB_SEED="${NTB_SEED:-20260417}"
BUILD_MODEL="${BUILD_MODEL:-0}"

case "$DV_TARGET" in
  cv32a6_imac_sv32)
    ISA="rv32imac_zbkb_zbkx_zkne_zknd_zknh_zicsr_zifencei"
    MABI="ilp32"
    ;;
  cv64a6_imafdc_sv39)
    ISA="rv64gc_zba_zbb_zbs_zbc_zbkb_zbkx_zkne_zknd_zknh"
    MABI="lp64d"
    ;;
  *)
    echo "Unsupported DV_TARGET: $DV_TARGET"
    exit 1
    ;;
esac

OUT_DIR="$ROOT_PROJECT/verif/sim/out_$(date +%Y-%m-%d)/freertos_cva6"
mkdir -p "$OUT_DIR"

echo "[FREERTOS_CVA6] ROOT_PROJECT=$ROOT_PROJECT"
echo "[FREERTOS_CVA6] DV_TARGET=$DV_TARGET"
echo "[FREERTOS_CVA6] ISA=$ISA MABI=$MABI"
echo "[FREERTOS_CVA6] ISS_TICKS=$ISS_TICKS RUN_TIMEOUT_S=$RUN_TIMEOUT_S"

if [ "$BUILD_MODEL" = "1" ]; then
  echo "[FREERTOS_CVA6] Building Verilator model..."
  make -C "$ROOT_PROJECT" verilate verilator="verilator --no-timing" target="$DV_TARGET"
fi

DEMO_DIR="$ROOT_PROJECT/../FreeRTOS-Kernel/demo/CVA6_FYP"
echo "[FREERTOS_CVA6] Compiling FreeRTOS CVA6 demos in $DEMO_DIR..."
make -C "$DEMO_DIR" clean
make -C "$DEMO_DIR" CORE_MARCH="$ISA" CORE_MABI="$MABI" CROSS_COMPILE="riscv-none-elf-"

run_one() {
  local test_name="$1"
  local elf="$DEMO_DIR/${test_name}.elf"
  local log="$OUT_DIR/${test_name}.${DV_TARGET}.log"
  local tohost
  local status=0

  if [ ! -f "$elf" ]; then
    echo "[FREERTOS_CVA6] ERROR: ELF missing $elf"
    exit 1
  fi

  tohost="$(riscv-none-elf-nm -B "$elf" | awk '$3 == "tohost" {print $1}' | tail -n 1)"
  if [ -z "$tohost" ]; then
    echo "[FREERTOS_CVA6] ERROR: tohost symbol missing in $elf"
    exit 1
  fi

  echo "[FREERTOS_CVA6] Running $test_name (tohost=0x$tohost)"
  set +e
  timeout "${RUN_TIMEOUT_S}s" "$ROOT_PROJECT/work-ver/Variane_testharness" "$elf" \
    +elf_file="$elf" "++$elf" \
    +tohost_addr="$tohost" \
    +time_out="$ISS_TICKS" \
    +ntb_random_seed="$NTB_SEED" \
    > "$log" 2>&1
  status=$?
  set -e

  if [ "$status" -ne 0 ] && [ "$status" -ne 124 ]; then
    echo "[FREERTOS_CVA6] ERROR: simulator exited with status=$status"
    tail -n 20 "$log"
    return 1
  fi

  if ! grep -q "\*\*\* SUCCESS \*\*\*" "$log"; then
    echo "[FREERTOS_CVA6] ERROR: missing SUCCESS marker in $log"
    tail -n 20 "$log"
    return 1
  fi

  local sim_cycles
  sim_cycles="$(awk 'match($0,/after [0-9]+ cycles/){print substr($0,RSTART+6,RLENGTH-13)}' "$log" | tail -n 1)"
  if [ -z "$sim_cycles" ]; then
    sim_cycles="N/A"
  fi
  echo "[FREERTOS_CVA6] RESULT $test_name simulator_cycles=$sim_cycles log=$log"
}

FAILURES=0
run_one "demo_integrity" || ((FAILURES++))
run_one "demo_sequence" || ((FAILURES++))
run_one "demo_borrow" || ((FAILURES++))

if [ "$FAILURES" -ne 0 ]; then
  echo "[FREERTOS_CVA6] Done with $FAILURES failures."
  exit 1
else
  echo "[FREERTOS_CVA6] Done. All tests passed."
fi
