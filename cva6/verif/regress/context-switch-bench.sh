#!/bin/bash

# Context-switch benchmark helper for CVA6 Verilator testharness + PK.
# It supports:
#   - baseline-compatible software save/restore benchmark
#   - partitioned-register-file window-switch benchmark

set -eo pipefail

if ! [ -n "${RISCV:-}" ]; then
  echo "Error: RISCV variable undefined"
  exit 1
fi

DV_TARGET="${DV_TARGET:-cv64a6_imafdc_sv39}"
BENCH_MODE="${BENCH_MODE:-both}"   # sw | window | both
ISS_TICKS="${ISS_TICKS:-12000000}"
RUN_TIMEOUT_S="${RUN_TIMEOUT_S:-260}"
NTB_SEED="${NTB_SEED:-202604171}"
NUM_JOBS="${NUM_JOBS:-1}"

case "$DV_TARGET" in
  cv64a6_imafdc_sv39)
    PK_ARCH="rv64gc_zba_zbb_zbs_zbc_zbkb_zbkx_zkne_zknd_zknh"
    PK_MABI="lp64d"
    TOHOST_ADDR="000000008000c008"
    ;;
  cv32a6_imac_sv32)
    PK_ARCH="rv32imac_zbkb_zbkx_zkne_zknd_zknh_zicsr_zifencei"
    PK_MABI="ilp32"
    TOHOST_ADDR="8000c008"
    ;;
  *)
    echo "Unsupported DV_TARGET: $DV_TARGET"
    echo "Supported targets: cv64a6_imafdc_sv39, cv32a6_imac_sv32"
    exit 1
    ;;
esac

ROOT_PROJECT="$(readlink -f "$(dirname "${BASH_SOURCE[0]}")/../..")"
SIM_DIR="$ROOT_PROJECT/verif/sim"

echo "[CTXSW] ROOT_PROJECT=$ROOT_PROJECT"
echo "[CTXSW] DV_TARGET=$DV_TARGET"
echo "[CTXSW] BENCH_MODE=$BENCH_MODE"
echo "[CTXSW] ISS_TICKS=$ISS_TICKS"
echo "[CTXSW] RUN_TIMEOUT_S=$RUN_TIMEOUT_S"
echo "[CTXSW] NTB_SEED=$NTB_SEED"

source "$ROOT_PROJECT/verif/regress/install-verilator.sh"
if [ "${SKIP_PK_INSTALL:-0}" = "1" ]; then
  export PK_INSTALL_DIR="${PK_INSTALL_DIR:-$ROOT_PROJECT/tools/pk}"
  export PATH="$PK_INSTALL_DIR/bin:$PATH"
  echo "[CTXSW] Skipping PK install (SKIP_PK_INSTALL=1)"
else
  source "$ROOT_PROJECT/verif/regress/install-pk.sh" "$PK_ARCH" "$PK_MABI"
fi
source "$ROOT_PROJECT/verif/sim/setup-env.sh"

run_c_test() {
  local c_test_rel="$1"
  local name="$2"
  local out_dir="$SIM_DIR/out_$(date +%Y-%m-%d)/ctxsw_direct"
  local elf="$out_dir/${name}.o"
  local run_log="$out_dir/${name}.${DV_TARGET}.log"
  local status=0

  mkdir -p "$out_dir"

  echo "[CTXSW] Running $name ($c_test_rel)"
  (
    cd "$ROOT_PROJECT"
    riscv-none-elf-gcc "$c_test_rel" -o "$elf" -march="$PK_ARCH" -mabi="$PK_MABI"
    set +e
    timeout "${RUN_TIMEOUT_S}s" ./work-ver/Variane_testharness \
      ./tools/pk/riscv-none-elf/bin/pk "$elf" \
      +tohost_addr="$TOHOST_ADDR" \
      +time_out="$ISS_TICKS" \
      +ntb_random_seed="$NTB_SEED" \
      > "$run_log" 2>&1
    status=$?
    set -e
    if [ "$status" -ne 0 ] && [ "$status" -ne 124 ]; then
      echo "[CTXSW] ERROR: simulator exited with status=$status"
      exit "$status"
    fi
  )

  local sim_cycles
  sim_cycles="$(awk 'match($0,/after [0-9]+ cycles/){print substr($0,RSTART+6,RLENGTH-13)}' "$run_log" | tail -n 1)"
  local bench_line
  bench_line="$(awk '/CTXSW_(SW|HW)/{line=$0} END{print line}' "$run_log")"
  local success_line
  success_line="$(awk '/\*\*\* SUCCESS \*\*\*/{line=$0} END{print line}' "$run_log")"

  if [ -z "$success_line" ]; then
    echo "[CTXSW] ERROR: SUCCESS marker missing in $run_log"
    echo "[CTXSW] Last 20 lines:"
    tail -n 20 "$run_log"
    exit 1
  fi

  echo "[CTXSW] RESULT $name simulator_cycles=${sim_cycles:-N/A}"
  if [ -n "$bench_line" ]; then
    echo "[CTXSW] RESULT $name benchmark=\"$bench_line\""
  else
    echo "[CTXSW] RESULT $name benchmark=\"(no CTXSW line captured)\""
  fi
  echo "[CTXSW] RESULT $name log=$run_log"
}

if [[ "$BENCH_MODE" == "sw" || "$BENCH_MODE" == "both" ]]; then
  run_c_test "verif/tests/custom/context_switch/context_switch_sw.c" "context_switch_sw"
fi

if [[ "$BENCH_MODE" == "window" || "$BENCH_MODE" == "both" ]]; then
  run_c_test "verif/tests/custom/context_switch/context_switch_window.c" "context_switch_window"
fi

echo "[CTXSW] Done."

