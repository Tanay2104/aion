#include <benchmark/benchmark.h>

#include "common/bench_common.hpp"
#include "common/throughput_helpers.hpp"

#include "group_r_jitter.hpp"

namespace {

using Event = aion::runtime::Event;

inline constexpr std::size_t kChunkCapForReplay = 1'000'000ULL;

void run_r_case(benchmark::State &state, const aion::bench::SelectivityPreset preset, const std::size_t stream_len) {
  const std::size_t effective_stream_len = aion::bench::resolve_stream_len(stream_len);
  auto params = aion::bench::preset_params(preset);
  const std::size_t chunk_len = (effective_stream_len > 10'000'000ULL) ? kChunkCapForReplay : effective_stream_len;
  const auto events = aion::bench::generate_events<Event>(chunk_len, params);

  if (chunk_len == effective_stream_len) {
    aion::bench::run_engine_throughput<aion::runtime::Engine_R_real>(state, events);
    return;
  }

  aion::bench::run_engine_throughput_replay<aion::runtime::Engine_R_real>(state, events, effective_stream_len);
}

static void BM_R_low_jitter_1M(benchmark::State &state) {
  run_r_case(state, aion::bench::SelectivityPreset::LOW, 1'000'000ULL);
}
static void BM_R_low_jitter_10M(benchmark::State &state) {
  run_r_case(state, aion::bench::SelectivityPreset::LOW, 10'000'000ULL);
}
static void BM_R_low_jitter_100M(benchmark::State &state) {
  run_r_case(state, aion::bench::SelectivityPreset::LOW, 100'000'000ULL);
}

static void BM_R_medium_jitter_1M(benchmark::State &state) {
  run_r_case(state, aion::bench::SelectivityPreset::MEDIUM, 1'000'000ULL);
}
static void BM_R_medium_jitter_10M(benchmark::State &state) {
  run_r_case(state, aion::bench::SelectivityPreset::MEDIUM, 10'000'000ULL);
}
static void BM_R_medium_jitter_100M(benchmark::State &state) {
  run_r_case(state, aion::bench::SelectivityPreset::MEDIUM, 100'000'000ULL);
}

static void BM_R_high_jitter_1M(benchmark::State &state) {
  run_r_case(state, aion::bench::SelectivityPreset::HIGH, 1'000'000ULL);
}
static void BM_R_high_jitter_10M(benchmark::State &state) {
  run_r_case(state, aion::bench::SelectivityPreset::HIGH, 10'000'000ULL);
}
static void BM_R_high_jitter_100M(benchmark::State &state) {
  run_r_case(state, aion::bench::SelectivityPreset::HIGH, 100'000'000ULL);
}

#define AION_REGISTER(case_name) BENCHMARK(case_name)->MinWarmUpTime(aion::bench::kWarmupSeconds)->MinTime(aion::bench::kMinSeconds)->Unit(benchmark::kNanosecond)

AION_REGISTER(BM_R_low_jitter_1M);
AION_REGISTER(BM_R_low_jitter_10M);
AION_REGISTER(BM_R_low_jitter_100M);
AION_REGISTER(BM_R_medium_jitter_1M);
AION_REGISTER(BM_R_medium_jitter_10M);
AION_REGISTER(BM_R_medium_jitter_100M);
AION_REGISTER(BM_R_high_jitter_1M);
AION_REGISTER(BM_R_high_jitter_10M);
AION_REGISTER(BM_R_high_jitter_100M);

#undef AION_REGISTER

} // namespace
