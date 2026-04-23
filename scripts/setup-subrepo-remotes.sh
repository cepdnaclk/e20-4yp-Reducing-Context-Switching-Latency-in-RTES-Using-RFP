#!/usr/bin/env bash
set -euo pipefail

# Configure local remotes that point to your working sub-repositories.
# Override defaults by exporting these variables before running:
#   CVA6_PATH, SPIKE_PATH, FREERTOS_PATH

CVA6_PATH="${CVA6_PATH:-c:/FYP/CVA6/cva6}"
SPIKE_PATH="${SPIKE_PATH:-c:/FYP/CVA6/riscv-isa-sim}"
FREERTOS_PATH="${FREERTOS_PATH:-c:/FYP/CVA6/FreeRTOS-Kernel}"

if git remote get-url cva6-local >/dev/null 2>&1; then
  git remote set-url cva6-local "${CVA6_PATH}"
else
  git remote add cva6-local "${CVA6_PATH}"
fi

if git remote get-url spike-local >/dev/null 2>&1; then
  git remote set-url spike-local "${SPIKE_PATH}"
else
  git remote add spike-local "${SPIKE_PATH}"
fi

if git remote get-url freertos-local >/dev/null 2>&1; then
  git remote set-url freertos-local "${FREERTOS_PATH}"
else
  git remote add freertos-local "${FREERTOS_PATH}"
fi

echo "Configured remotes:"
git remote -v
