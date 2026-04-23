#!/usr/bin/env bash
set -euo pipefail

# Pull latest commits from each subtree source remote into its prefix.
# Source remotes are configured by scripts/setup-subrepo-remotes.sh (GitHub forks by default).
#
# Usage:
#   ./scripts/update-subrepos.sh
#   ./scripts/update-subrepos.sh --squash
#
# Branch names (must match the branches you push to on each fork):
#   CVA6_SUBTREE_BRANCH (default: reasearch)
#   SPIKE_SUBTREE_BRANCH (default: research-dynamic-partitions-FreeRTOS-SP)
#   FREERTOS_SUBTREE_BRANCH (default: fyp-rfp-4)

SQUASH_FLAG=""
if [[ "${1:-}" == "--squash" ]]; then
  SQUASH_FLAG="--squash"
fi

if [[ -n "$(git status --porcelain)" ]]; then
  echo "Working tree is not clean. Commit or stash changes before subtree updates."
  exit 1
fi

CVA6_BRANCH="${CVA6_SUBTREE_BRANCH:-reasearch}"
SPIKE_BRANCH="${SPIKE_SUBTREE_BRANCH:-research-dynamic-partitions-FreeRTOS-SP}"
FREERTOS_BRANCH="${FREERTOS_SUBTREE_BRANCH:-fyp-rfp-4}"

git fetch cva6-local "${CVA6_BRANCH}"
git subtree pull ${SQUASH_FLAG} --prefix=cva6 cva6-local "${CVA6_BRANCH}" -m "Update cva6 subtree from ${CVA6_BRANCH}"

git fetch spike-local "${SPIKE_BRANCH}"
git subtree pull ${SQUASH_FLAG} --prefix=riscv-isa-sim spike-local "${SPIKE_BRANCH}" -m "Update riscv-isa-sim subtree from ${SPIKE_BRANCH}"

git fetch freertos-local "${FREERTOS_BRANCH}"
git subtree pull ${SQUASH_FLAG} --prefix=FreeRTOS-Kernel freertos-local "${FREERTOS_BRANCH}" -m "Update FreeRTOS-Kernel subtree from ${FREERTOS_BRANCH}"

echo "All subtree updates completed."
