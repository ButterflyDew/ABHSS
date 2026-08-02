#!/usr/bin/env bash
# Run the complete P1 paper workload on Linux.
#
# Usage:
#   ./run_p1_full.sh [RUN_ID] [SHARD_INDEX] [SHARD_COUNT]
#
# Re-running with the same RUN_ID/shard arguments resumes completed work.
# Environment overrides:
#   JOBS=16            parallel build jobs (default: detected CPU count)
#   SKIP_BUILD=1       reuse existing build/abhss and build/pruneddp
#   SKIP_DEEP_CHECK=1  skip the slow full input/hash validation

set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

RUN_ID="${1:-p1_full}"
SHARD_INDEX="${2:-0}"
SHARD_COUNT="${3:-1}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
RUN_DIR="results/paper_runs/${RUN_ID}"
LOG_DIR="${RUN_DIR}/logs"
LOG_FILE="${LOG_DIR}/p1-shard-${SHARD_INDEX}-of-${SHARD_COUNT}.log"

if ! [[ "$SHARD_INDEX" =~ ^[0-9]+$ && "$SHARD_COUNT" =~ ^[1-9][0-9]*$ ]]; then
    echo "Error: SHARD_INDEX must be >= 0 and SHARD_COUNT must be >= 1." >&2
    exit 2
fi
if (( SHARD_INDEX >= SHARD_COUNT )); then
    echo "Error: require SHARD_INDEX < SHARD_COUNT." >&2
    exit 2
fi

mkdir -p "$LOG_DIR"
exec > >(tee -a "$LOG_FILE") 2>&1

echo "[$(date --iso-8601=seconds)] P1 run starting"
echo "repository=$SCRIPT_DIR"
echo "run_id=$RUN_ID run_dir=$RUN_DIR shard=$SHARD_INDEX/$SHARD_COUNT jobs=$JOBS"

if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
    make release JOBS="$JOBS"
else
    echo "Skipping build because SKIP_BUILD=1"
fi

if [[ "${SKIP_DEEP_CHECK:-0}" != "1" ]]; then
    python3 tools/experiments/validate_environment.py \
        --deep \
        --require-performance-binaries
else
    echo "Skipping deep input/hash validation because SKIP_DEEP_CHECK=1"
    python3 tools/experiments/validate_environment.py \
        --require-performance-binaries
fi

RUNNER_ARGS=(
    --run-id "$RUN_ID"
    --run-dir "$RUN_DIR"
    --suite P1_monogstplus_published
    --suite P1_gpu4gst_published
    --shard-index "$SHARD_INDEX"
    --shard-count "$SHARD_COUNT"
)

echo "[$(date --iso-8601=seconds)] Selected workload:"
python3 tools/experiments/run_experiments.py "${RUNNER_ARGS[@]}" --dry-run

echo "[$(date --iso-8601=seconds)] Starting solvers"
python3 tools/experiments/run_experiments.py "${RUNNER_ARGS[@]}"
echo "[$(date --iso-8601=seconds)] P1 run completed"
echo "records=${RUN_DIR}/records.jsonl"
echo "log=$LOG_FILE"
