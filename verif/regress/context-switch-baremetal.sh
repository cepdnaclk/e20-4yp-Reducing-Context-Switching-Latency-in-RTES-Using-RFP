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
  local sig_workload sig_trap_rt sig_trap_entry sig_trap_body sig_save sig_restore sig_stage sig_mret_return sig_mismatch
  local dec_workload dec_trap_rt dec_trap_entry dec_trap_body dec_save dec_restore dec_stage dec_mret_return dec_mismatch
  local avg_workload avg_trap_rt avg_trap_entry avg_trap_body avg_save avg_restore avg_stage avg_mret_return
  local iter_count
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

    sig_workload="$(grep -aE "mem 0x0*0000012c " "$trace" | tail -n 1 | awk '{print $NF}' || true)"
    sig_trap_rt="$(grep -aE "mem 0x0*00000130 " "$trace" | tail -n 1 | awk '{print $NF}' || true)"
    sig_trap_entry="$(grep -aE "mem 0x0*00000134 " "$trace" | tail -n 1 | awk '{print $NF}' || true)"
    sig_trap_body="$(grep -aE "mem 0x0*00000138 " "$trace" | tail -n 1 | awk '{print $NF}' || true)"
    sig_save="$(grep -aE "mem 0x0*0000013c " "$trace" | tail -n 1 | awk '{print $NF}' || true)"
    sig_restore="$(grep -aE "mem 0x0*00000140 " "$trace" | tail -n 1 | awk '{print $NF}' || true)"
    sig_stage="$(grep -aE "mem 0x0*00000144 " "$trace" | tail -n 1 | awk '{print $NF}' || true)"
    sig_mret_return="$(grep -aE "mem 0x0*00000148 " "$trace" | tail -n 1 | awk '{print $NF}' || true)"
    sig_mismatch="$(grep -aE "mem 0x0*0000014c " "$trace" | tail -n 1 | awk '{print $NF}' || true)"
    if [ -n "${sig_workload:-}" ] && [ -n "${sig_trap_rt:-}" ] && [ -n "${sig_trap_entry:-}" ] && [ -n "${sig_trap_body:-}" ] && [ -n "${sig_save:-}" ] && [ -n "${sig_restore:-}" ] && [ -n "${sig_stage:-}" ] && [ -n "${sig_mret_return:-}" ] && [ -n "${sig_mismatch:-}" ]; then
      echo "[CTXSW_BM] RESULT $test_name breakdown_hex_sum workload=${sig_workload} trap_roundtrip=${sig_trap_rt} trap_entry=${sig_trap_entry} trap_body=${sig_trap_body} spill_or_save=${sig_save} restore=${sig_restore} stage=${sig_stage} mret_return=${sig_mret_return} cfg_mismatch=${sig_mismatch}"
      dec_workload="$((16#${sig_workload#0x}))"
      dec_trap_rt="$((16#${sig_trap_rt#0x}))"
      dec_trap_entry="$((16#${sig_trap_entry#0x}))"
      dec_trap_body="$((16#${sig_trap_body#0x}))"
      dec_save="$((16#${sig_save#0x}))"
      dec_restore="$((16#${sig_restore#0x}))"
      dec_stage="$((16#${sig_stage#0x}))"
      dec_mret_return="$((16#${sig_mret_return#0x}))"
      dec_mismatch="$((16#${sig_mismatch#0x}))"
      echo "[CTXSW_BM] RESULT $test_name breakdown_dec_sum workload=$dec_workload trap_roundtrip=$dec_trap_rt trap_entry=$dec_trap_entry trap_body=$dec_trap_body spill_or_save=$dec_save restore=$dec_restore stage=$dec_stage mret_return=$dec_mret_return cfg_mismatch=$dec_mismatch"
      case "$test_name" in
        rr2_window_switch|scale4_window_switch|scale4_mixed_window_switch) iter_count=64 ;;
        latency_baseline_sw|latency_hw_window) iter_count=128 ;;
        spill_oldest_window_switch) iter_count=5 ;;
        stress_rt_window100|stress_rt_baseline100) iter_count=500 ;;
        *) iter_count=1 ;;
      esac
      avg_workload="$((dec_workload / iter_count))"
      avg_trap_rt="$((dec_trap_rt / iter_count))"
      avg_trap_entry="$((dec_trap_entry / iter_count))"
      avg_trap_body="$((dec_trap_body / iter_count))"
      avg_save="$((dec_save / iter_count))"
      avg_restore="$((dec_restore / iter_count))"
      avg_stage="$((dec_stage / iter_count))"
      avg_mret_return="$((dec_mret_return / iter_count))"
      echo "[CTXSW_BM] RESULT $test_name breakdown_dec_avg_per_iter workload=$avg_workload trap_roundtrip=$avg_trap_rt trap_entry=$avg_trap_entry trap_body=$avg_trap_body spill_or_save=$avg_save restore=$avg_restore stage=$avg_stage mret_return=$avg_mret_return cfg_mismatch=$dec_mismatch iterations=$iter_count"
    fi
  else
    echo "[CTXSW_BM] RESULT $test_name mcycle_hex unavailable (trace missing)"
  fi
}

run_one "rr2_window_switch"
run_one "scale4_window_switch"
run_one "scale4_mixed_window_switch"
run_one "spill_oldest_window_switch"
run_one "stress_rt_window100"
run_one "stress_rt_baseline100"
run_one "latency_baseline_sw"
run_one "latency_hw_window"

echo "[CTXSW_BM] Done."

