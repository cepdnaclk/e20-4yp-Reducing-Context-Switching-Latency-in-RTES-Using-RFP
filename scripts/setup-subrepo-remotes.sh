#!/usr/bin/env bash
set -euo pipefail

# Point subtree source remotes at your GitHub forks (default), or override URLs / use local clones.
#
# Defaults (HTTPS):
#   https://github.com/ChethiyaB/cva6.git
#   https://github.com/ChethiyaB/riscv-isa-sim.git
#   https://github.com/ChethiyaB/FreeRTOS-Kernel.git
#
# Override before running, for example:
#   CVA6_REMOTE_URL="https://github.com/you/cva6.git" ./scripts/setup-subrepo-remotes.sh
#
# Use local working copies instead (same as before):
#   USE_LOCAL_CLONES=1 ./scripts/setup-subrepo-remotes.sh
#   # optional: CVA6_PATH, SPIKE_PATH, FREERTOS_PATH

CVA6_REMOTE_URL="${CVA6_REMOTE_URL:-https://github.com/ChethiyaB/cva6.git}"
SPIKE_REMOTE_URL="${SPIKE_REMOTE_URL:-https://github.com/ChethiyaB/riscv-isa-sim.git}"
FREERTOS_REMOTE_URL="${FREERTOS_REMOTE_URL:-https://github.com/ChethiyaB/FreeRTOS-Kernel.git}"

CVA6_PATH="${CVA6_PATH:-c:/FYP/CVA6/cva6}"
SPIKE_PATH="${SPIKE_PATH:-c:/FYP/CVA6/riscv-isa-sim}"
FREERTOS_PATH="${FREERTOS_PATH:-c:/FYP/CVA6/FreeRTOS-Kernel}"

if [[ "${USE_LOCAL_CLONES:-0}" == "1" ]]; then
  CVA6_REMOTE_URL="${CVA6_PATH}"
  SPIKE_REMOTE_URL="${SPIKE_PATH}"
  FREERTOS_REMOTE_URL="${FREERTOS_PATH}"
fi

if git remote get-url cva6-local >/dev/null 2>&1; then
  git remote set-url cva6-local "${CVA6_REMOTE_URL}"
else
  git remote add cva6-local "${CVA6_REMOTE_URL}"
fi

if git remote get-url spike-local >/dev/null 2>&1; then
  git remote set-url spike-local "${SPIKE_REMOTE_URL}"
else
  git remote add spike-local "${SPIKE_REMOTE_URL}"
fi

if git remote get-url freertos-local >/dev/null 2>&1; then
  git remote set-url freertos-local "${FREERTOS_REMOTE_URL}"
else
  git remote add freertos-local "${FREERTOS_REMOTE_URL}"
fi

echo "Configured remotes:"
git remote -v
