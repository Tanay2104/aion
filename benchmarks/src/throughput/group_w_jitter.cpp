#include <benchmark/benchmark.h>

#include "common/bench_common.hpp"
#include "common/throughput_helpers.hpp"

#include "group_w_jitter.hpp"

namespace {

using Event = aion::runtime::Event;

aion::bench::GenParams w_params(const int selectivity_percent) {
  const double p = static_cast<double>(selectivity_percent) / 100.0;
  return aion::bench::GenParams{
      .sel_hi = p,
      .sel_lo = p,
      .sel_ya = p,
      .sel_yb = p,
      .sel_valid = 0.90,
      .sel_early = 0.50,
      .seed = aion::bench::kSeed,
  };
}

template <typename Engine>
void run_w_case(benchmark::State &state, const int selectivity_percent) {
  const auto stream_len = aion::bench::resolve_stream_len(aion::bench::kDefaultStreamLen);
  const auto events = aion::bench::generate_events<Event>(stream_len, w_params(selectivity_percent));
  aion::bench::run_engine_throughput<Engine>(state, events);
}

static void BM_W0_jitter_10(benchmark::State &state) { run_w_case<aion::runtime::Engine_W0>(state, 10); }
static void BM_W0_jitter_50(benchmark::State &state) { run_w_case<aion::runtime::Engine_W0>(state, 50); }

static void BM_W1_jitter_10(benchmark::State &state) { run_w_case<aion::runtime::Engine_W1>(state, 10); }
static void BM_W1_jitter_50(benchmark::State &state) { run_w_case<aion::runtime::Engine_W1>(state, 50); }

static void BM_W2_jitter_10(benchmark::State &state) { run_w_case<aion::runtime::Engine_W2>(state, 10); }
static void BM_W2_jitter_50(benchmark::State &state) { run_w_case<aion::runtime::Engine_W2>(state, 50); }

static void BM_W4_jitter_10(benchmark::State &state) { run_w_case<aion::runtime::Engine_W4>(state, 10); }
static void BM_W4_jitter_50(benchmark::State &state) { run_w_case<aion::runtime::Engine_W4>(state, 50); }

static void BM_W8_jitter_10(benchmark::State &state) { run_w_case<aion::runtime::Engine_W8>(state, 10); }
static void BM_W8_jitter_50(benchmark::State &state) { run_w_case<aion::runtime::Engine_W8>(state, 50); }

static void BM_W16_jitter_10(benchmark::State &state) { run_w_case<aion::runtime::Engine_W16>(state, 10); }
static void BM_W16_jitter_50(benchmark::State &state) { run_w_case<aion::runtime::Engine_W16>(state, 50); }

static void BM_W32_jitter_10(benchmark::State &state) { run_w_case<aion::runtime::Engine_W32>(state, 10); }
static void BM_W32_jitter_50(benchmark::State &state) { run_w_case<aion::runtime::Engine_W32>(state, 50); }

#define AION_REGISTER(case_name) BENCHMARK(case_name)->MinWarmUpTime(aion::bench::kWarmupSeconds)->MinTime(aion::bench::kMinSeconds)->Unit(benchmark::kNanosecond)

AION_REGISTER(BM_W0_jitter_10);
AION_REGISTER(BM_W0_jitter_50);
AION_REGISTER(BM_W1_jitter_10);
AION_REGISTER(BM_W1_jitter_50);
AION_REGISTER(BM_W2_jitter_10);
AION_REGISTER(BM_W2_jitter_50);
AION_REGISTER(BM_W4_jitter_10);
AION_REGISTER(BM_W4_jitter_50);
AION_REGISTER(BM_W8_jitter_10);
AION_REGISTER(BM_W8_jitter_50);
AION_REGISTER(BM_W16_jitter_10);
AION_REGISTER(BM_W16_jitter_50);
AION_REGISTER(BM_W32_jitter_10);
AION_REGISTER(BM_W32_jitter_50);

#undef AION_REGISTER

} // namespace
