#!/usr/bin/env bash
set -euo pipefail

# Pull latest history from each source repo branch into its subtree prefix.
# Usage:
#   ./scripts/update-subrepos.sh
#   ./scripts/update-subrepos.sh --squash

SQUASH_FLAG=""
if [[ "${1:-}" == "--squash" ]]; then
  SQUASH_FLAG="--squash"
fi

if [[ -n "$(git status --porcelain)" ]]; then
  echo "Working tree is not clean. Commit or stash changes before subtree updates."
  exit 1
fi

git fetch cva6-local reasearch
git subtree pull ${SQUASH_FLAG} --prefix=cva6 cva6-local reasearch -m "Update cva6 subtree from cva6/reasearch"

git fetch spike-local research-dynamic-partitions-FreeRTOS-SP
git subtree pull ${SQUASH_FLAG} --prefix=riscv-isa-sim spike-local research-dynamic-partitions-FreeRTOS-SP -m "Update riscv-isa-sim subtree from spike research branch"

git fetch freertos-local fyp-rfp-4
git subtree pull ${SQUASH_FLAG} --prefix=FreeRTOS-Kernel freertos-local fyp-rfp-4 -m "Update FreeRTOS-Kernel subtree from fyp-rfp-4"

echo "All subtree updates completed."
