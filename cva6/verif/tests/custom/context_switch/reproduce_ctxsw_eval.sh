#!/usr/bin/env bash

set -euo pipefail

usage() {
  cat <<'EOF'
Reproduce CVA6 context-switch evaluation from fresh clones.

Expected layout (default):
  <project-root>/
    cva6/
    FreeRTOS-Kernel/
    riscv-isa-sim/

Usage:
  bash reproduce_ctxsw_eval.sh [options]

Options:
  --project-root <path>   Parent directory containing the three repos.
  --build-model           Rebuild Verilator model before running tests.
  --with-spike            Also build/run Spike research-tests smoke pass.
  --skip-freertos         Skip FreeRTOS CVA6 suite.
  --help                  Show this help.

Environment overrides:
  RISCV        Toolchain prefix (default: /opt/riscv)
  DV_TARGET    Default: cv32a6_imac_sv32
  RUN_TIMEOUT_S Default: 180
  ISS_TICKS    Default: 5000000
  NTB_SEED     Default: 20260417
  SEED         Default: 20260417
EOF
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CVA6_ROOT="$(cd "${SCRIPT_DIR}/../../../../" && pwd)"
PROJECT_ROOT_DEFAULT="$(cd "${CVA6_ROOT}/.." && pwd)"

PROJECT_ROOT="${PROJECT_ROOT_DEFAULT}"
DO_BUILD_MODEL=0
DO_SPIKE=0
DO_FREERTOS=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    --project-root)
      PROJECT_ROOT="$2"
      shift 2
      ;;
    --build-model)
      DO_BUILD_MODEL=1
      shift
      ;;
    --with-spike)
      DO_SPIKE=1
      shift
      ;;
    --skip-freertos)
      DO_FREERTOS=0
      shift
      ;;
    --help)
      usage
      exit 0
      ;;
    *)
      echo "[REPRO] Unknown option: $1" >&2
      usage
      exit 2
      ;;
  esac
done

PROJECT_ROOT="$(cd "${PROJECT_ROOT}" && pwd)"
CVA6_DIR="${PROJECT_ROOT}/cva6"
FREERTOS_DIR="${PROJECT_ROOT}/FreeRTOS-Kernel"
SPIKE_DIR="${PROJECT_ROOT}/riscv-isa-sim"

RISCV="${RISCV:-/opt/riscv}"
DV_TARGET="${DV_TARGET:-cv32a6_imac_sv32}"
RUN_TIMEOUT_S="${RUN_TIMEOUT_S:-180}"
ISS_TICKS="${ISS_TICKS:-5000000}"
NTB_SEED="${NTB_SEED:-20260417}"
SEED="${SEED:-20260417}"

echo "[REPRO] project_root=${PROJECT_ROOT}"
echo "[REPRO] cva6=${CVA6_DIR}"
echo "[REPRO] freertos=${FREERTOS_DIR}"
echo "[REPRO] spike=${SPIKE_DIR}"

for d in "${CVA6_DIR}" "${FREERTOS_DIR}" "${SPIKE_DIR}"; do
  if [[ ! -d "${d}" ]]; then
    echo "[REPRO] Missing directory: ${d}" >&2
    exit 1
  fi
done

export RISCV
export PATH="${RISCV}/bin:${PATH}"
export DV_TARGET
export RUN_TIMEOUT_S
export ISS_TICKS
export NTB_SEED
export SEED

command -v riscv-none-elf-gcc >/dev/null || { echo "[REPRO] riscv-none-elf-gcc not found in PATH" >&2; exit 1; }

cd "${CVA6_DIR}"

if [[ "${DO_BUILD_MODEL}" -eq 1 ]]; then
  echo "[REPRO] Rebuilding Verilator model"
  make verilate verilator="verilator --no-timing" target="${DV_TARGET}"
fi

if [[ ! -x "${CVA6_DIR}/work-ver/Variane_testharness" ]]; then
  echo "[REPRO] Missing ${CVA6_DIR}/work-ver/Variane_testharness (run with --build-model)" >&2
  exit 1
fi

echo "[REPRO] Running bare-metal context-switch regression"
BUILD_MODEL=0 bash verif/regress/context-switch-baremetal.sh

if [[ "${DO_FREERTOS}" -eq 1 ]]; then
  echo "[REPRO] Running FreeRTOS CVA6 regression"
  bash verif/regress/freertos-windowed-cva6.sh
fi

if [[ "${DO_SPIKE}" -eq 1 ]]; then
  echo "[REPRO] Building Spike"
  cd "${SPIKE_DIR}"
  mkdir -p build
  cd build
  ../configure --prefix="${RISCV}"
  make -j"$(nproc)"

  echo "[REPRO] Running Spike research-tests smoke pass"
  cd "${SPIKE_DIR}"
  SPIKE_BIN="${SPIKE_DIR}/build/spike"
  ISA="rv32imac_zbkb_zbkx_zkne_zknd_zknh_zicsr_zifencei"
  for elf in research-tests/*/*.elf; do
    [[ -f "${elf}" ]] || continue
    "${SPIKE_BIN}" --isa="${ISA}" --instructions=200000 "${elf}" >/tmp/spike_ctxsw_smoke.log 2>&1 || true
    echo "[REPRO] SPIKE_SMOKE ${elf}"
  done
fi

echo "[REPRO] Done."
echo "[REPRO] Bare-metal artifacts: ${CVA6_DIR}/verif/sim/out_*/ctxsw_baremetal/"
if [[ "${DO_FREERTOS}" -eq 1 ]]; then
  echo "[REPRO] FreeRTOS artifacts: ${CVA6_DIR}/verif/sim/out_*/freertos_cva6/"
fi
