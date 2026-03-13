#include <benchmark/benchmark.h>

#include "common/bench_common.hpp"
#include "common/throughput_helpers.hpp"

#include "group_k_nojitter.hpp"

namespace {

using Event = aion::runtime::Event;

aion::bench::GenParams k_params(const int selectivity_percent) {
  const double p = static_cast<double>(selectivity_percent) / 100.0;
  auto params = aion::bench::with_uniform_selectivity(p);
  params.sel_valid = 0.90;
  params.seed = aion::bench::kSeed;
  return params;
}

template <typename Engine>
void run_k_case(benchmark::State &state, const int selectivity_percent) {
  const auto stream_len = aion::bench::resolve_stream_len(aion::bench::kDefaultStreamLen);
  const auto events = aion::bench::generate_events<Event>(stream_len, k_params(selectivity_percent));
  aion::bench::run_engine_throughput<Engine>(state, events);
}

static void BM_K2_nojitter_10(benchmark::State &state) { run_k_case<aion::runtime::Engine_K_2>(state, 10); }
static void BM_K2_nojitter_50(benchmark::State &state) { run_k_case<aion::runtime::Engine_K_2>(state, 50); }
static void BM_K2_nojitter_90(benchmark::State &state) { run_k_case<aion::runtime::Engine_K_2>(state, 90); }

static void BM_K4_nojitter_10(benchmark::State &state) { run_k_case<aion::runtime::Engine_K_4>(state, 10); }
static void BM_K4_nojitter_50(benchmark::State &state) { run_k_case<aion::runtime::Engine_K_4>(state, 50); }
static void BM_K4_nojitter_90(benchmark::State &state) { run_k_case<aion::runtime::Engine_K_4>(state, 90); }

static void BM_K8_nojitter_10(benchmark::State &state) { run_k_case<aion::runtime::Engine_K_8>(state, 10); }
static void BM_K8_nojitter_50(benchmark::State &state) { run_k_case<aion::runtime::Engine_K_8>(state, 50); }
static void BM_K8_nojitter_90(benchmark::State &state) { run_k_case<aion::runtime::Engine_K_8>(state, 90); }

static void BM_K16_nojitter_10(benchmark::State &state) { run_k_case<aion::runtime::Engine_K_16>(state, 10); }
static void BM_K16_nojitter_50(benchmark::State &state) { run_k_case<aion::runtime::Engine_K_16>(state, 50); }
static void BM_K16_nojitter_90(benchmark::State &state) { run_k_case<aion::runtime::Engine_K_16>(state, 90); }

static void BM_K32_nojitter_10(benchmark::State &state) { run_k_case<aion::runtime::Engine_K_32>(state, 10); }
static void BM_K32_nojitter_50(benchmark::State &state) { run_k_case<aion::runtime::Engine_K_32>(state, 50); }
static void BM_K32_nojitter_90(benchmark::State &state) { run_k_case<aion::runtime::Engine_K_32>(state, 90); }

#define AION_REGISTER(case_name) BENCHMARK(case_name)->MinWarmUpTime(aion::bench::kWarmupSeconds)->MinTime(aion::bench::kMinSeconds)->Unit(benchmark::kNanosecond)

AION_REGISTER(BM_K2_nojitter_10);
AION_REGISTER(BM_K2_nojitter_50);
AION_REGISTER(BM_K2_nojitter_90);
AION_REGISTER(BM_K4_nojitter_10);
AION_REGISTER(BM_K4_nojitter_50);
AION_REGISTER(BM_K4_nojitter_90);
AION_REGISTER(BM_K8_nojitter_10);
AION_REGISTER(BM_K8_nojitter_50);
AION_REGISTER(BM_K8_nojitter_90);
AION_REGISTER(BM_K16_nojitter_10);
AION_REGISTER(BM_K16_nojitter_50);
AION_REGISTER(BM_K16_nojitter_90);
AION_REGISTER(BM_K32_nojitter_10);
AION_REGISTER(BM_K32_nojitter_50);
AION_REGISTER(BM_K32_nojitter_90);

#undef AION_REGISTER

} // namespace
