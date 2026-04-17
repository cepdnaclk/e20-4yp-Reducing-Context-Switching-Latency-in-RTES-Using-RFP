#!/bin/bash

# Bare-metal context-switch and trap-latency regression for CVA6.
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
BUILD_MODEL="${BUILD_MODEL:-1}"

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

OUT_DIR="$ROOT_PROJECT/verif/sim/out_$(date +%Y-%m-%d)/ctxsw_baremetal"
mkdir -p "$OUT_DIR"

echo "[CTXSW_BM] ROOT_PROJECT=$ROOT_PROJECT"
echo "[CTXSW_BM] DV_TARGET=$DV_TARGET"
echo "[CTXSW_BM] ISA=$ISA MABI=$MABI"
echo "[CTXSW_BM] ISS_TICKS=$ISS_TICKS RUN_TIMEOUT_S=$RUN_TIMEOUT_S"

if [ "$BUILD_MODEL" = "1" ]; then
  echo "[CTXSW_BM] Building Verilator model..."
  make -C "$ROOT_PROJECT" verilate verilator="verilator --no-timing" target="$DV_TARGET"
fi

run_one() {
  local test_name="$1"
  local src="$ROOT_PROJECT/verif/tests/custom/context_switch/${test_name}.S"
  local elf="$OUT_DIR/${test_name}.o"
  local log="$OUT_DIR/${test_name}.${DV_TARGET}.log"
  local trace="$OUT_DIR/${test_name}.${DV_TARGET}.trace_rvfi_hart_00.dasm"
  local tohost
  local status=0

  echo "[CTXSW_BM] Compiling $test_name"
  riscv-none-elf-gcc "$src" \
    -T "$ROOT_PROJECT/verif/tests/custom/context_switch/link_baremetal.ld" \
    -nostdlib -nostartfiles -static \
    -march="$ISA" -mabi="$MABI" \
    -o "$elf"

  tohost="$(riscv-none-elf-nm -B "$elf" | awk '$3 == "tohost" {print $1}' | tail -n 1)"
  if [ -z "$tohost" ]; then
    echo "[CTXSW_BM] ERROR: tohost symbol missing in $elf"
    exit 1
  fi

  echo "[CTXSW_BM] Running $test_name (tohost=0x$tohost)"
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
    echo "[CTXSW_BM] ERROR: simulator exited with status=$status"
    tail -n 20 "$log"
    exit 1
  fi

  if ! grep -q "\*\*\* SUCCESS \*\*\*" "$log"; then
    echo "[CTXSW_BM] ERROR: missing SUCCESS marker in $log"
    tail -n 20 "$log"
    exit 1
  fi

  local sim_cycles
  local bench_line
  local sig_min sig_max sig_avg
  local dec_min dec_max dec_avg
  local addr_min addr_max addr_avg
  sim_cycles="$(awk 'match($0,/after [0-9]+ cycles/){print substr($0,RSTART+6,RLENGTH-13)}' "$log" | tail -n 1)"
  bench_line="$(awk '/MCYCLE_/ {line=$0} END{print line}' "$log")"
  if [ -z "$sim_cycles" ]; then
    sim_cycles="N/A"
  fi
  echo "[CTXSW_BM] RESULT $test_name simulator_cycles=$sim_cycles log=$log"
  if [ -n "$bench_line" ]; then
    echo "[CTXSW_BM] RESULT $test_name benchmark=\"$bench_line\""
  else
    echo "[CTXSW_BM] RESULT $test_name benchmark=\"(no MCYCLE line captured)\""
  fi
  if [ -f "$ROOT_PROJECT/trace_rvfi_hart_00.dasm" ]; then
    cp "$ROOT_PROJECT/trace_rvfi_hart_00.dasm" "$trace"
    addr_min="00000120"
    addr_max="00000124"
    addr_avg="00000128"
    sig_min="$(grep -aE "mem 0x0*${addr_min} " "$trace" | tail -n 1 | awk '{print $NF}' || true)"
    sig_max="$(grep -aE "mem 0x0*${addr_max} " "$trace" | tail -n 1 | awk '{print $NF}' || true)"
    sig_avg="$(grep -aE "mem 0x0*${addr_avg} " "$trace" | tail -n 1 | awk '{print $NF}' || true)"
    echo "[CTXSW_BM] RESULT $test_name mcycle_hex min=${sig_min:-N/A} max=${sig_max:-N/A} avg=${sig_avg:-N/A} trace=$trace"
    if [ -n "${sig_min:-}" ] && [ -n "${sig_max:-}" ] && [ -n "${sig_avg:-}" ]; then
      dec_min="$((16#${sig_min#0x}))"
      dec_max="$((16#${sig_max#0x}))"
      dec_avg="$((16#${sig_avg#0x}))"
      echo "[CTXSW_BM] RESULT $test_name mcycle_dec min=$dec_min max=$dec_max avg=$dec_avg"
    fi
  else
    echo "[CTXSW_BM] RESULT $test_name mcycle_hex unavailable (trace missing)"
  fi
}

run_one "rr2_window_switch"
run_one "scale4_window_switch"
run_one "latency_baseline_sw"
run_one "latency_hw_window"

echo "[CTXSW_BM] Done."

