#!/usr/bin/env bash
set -euo pipefail

PRESET="${1:-release}"
STAMP="$(date +%Y%m%d_%H%M%S)"
OUT_DIR="${2:-benchmarks/results/${STAMP}}"

mkdir -p "${OUT_DIR}"

cmake --preset "${PRESET}" -DAION_BUILD_BENCHMARKS=ON
cmake --build --preset "${PRESET}" \
  --target aion_bench_throughput_jitter aion_bench_throughput_nojitter aion_bench_latency_jitter aion_bench_latency_nojitter

BIN_DIR="build/${PRESET}/bin"

if [[ ! -x "${BIN_DIR}/aion_bench_throughput_jitter" ]]; then
  echo "missing benchmark binaries in ${BIN_DIR}" >&2
  exit 1
fi

RUN_PREFIX=()
if [[ -n "${AION_BENCH_CPU_CORE:-}" ]] && command -v taskset >/dev/null 2>&1; then
  RUN_PREFIX=(taskset -c "${AION_BENCH_CPU_CORE}")
fi

THROUGHPUT_FLAGS=(
  --benchmark_repetitions=5
  --benchmark_report_aggregates_only=false
  --benchmark_out_format=json
)

"${RUN_PREFIX[@]}" "${BIN_DIR}/aion_bench_throughput_jitter" \
  "${THROUGHPUT_FLAGS[@]}" \
  --benchmark_out="${OUT_DIR}/throughput_jitter.json"

"${RUN_PREFIX[@]}" "${BIN_DIR}/aion_bench_throughput_nojitter" \
  "${THROUGHPUT_FLAGS[@]}" \
  --benchmark_out="${OUT_DIR}/throughput_nojitter.json"

"${RUN_PREFIX[@]}" "${BIN_DIR}/aion_bench_latency_jitter" \
  --out "${OUT_DIR}/latency_jitter.csv"

"${RUN_PREFIX[@]}" "${BIN_DIR}/aion_bench_latency_nojitter" \
  --out "${OUT_DIR}/latency_nojitter.csv"

python3 benchmarks/scripts/summarize_results.py \
  --throughput-jitter "${OUT_DIR}/throughput_jitter.json" \
  --throughput-nojitter "${OUT_DIR}/throughput_nojitter.json" \
  --latency-jitter "${OUT_DIR}/latency_jitter.csv" \
  --latency-nojitter "${OUT_DIR}/latency_nojitter.csv" \
  --out-dir "${OUT_DIR}"

echo "results: ${OUT_DIR}"
