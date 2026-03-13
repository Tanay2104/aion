#include <benchmark/benchmark.h>

#include "common/bench_common.hpp"
#include "common/throughput_helpers.hpp"

#include "group_s_nojitter.hpp"

namespace {

using Event = aion::runtime::Event;

template <typename Engine, std::size_t Repetitions>
void run_sequence_case(benchmark::State &state, const int selectivity_percent) {
  const auto stream_len = aion::bench::resolve_stream_len(aion::bench::kDefaultStreamLen);
  const auto events =
      aion::bench::generate_sequence_events<Event>(stream_len, selectivity_percent, Repetitions);
  aion::bench::run_engine_throughput<Engine>(state, events);
}

static void BM_S2_nojitter_50(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S2, 2>(state, 50); }
static void BM_S2_nojitter_100(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S2, 2>(state, 100); }
static void BM_S4_nojitter_50(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S4, 4>(state, 50); }
static void BM_S4_nojitter_100(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S4, 4>(state, 100); }
static void BM_S8_nojitter_50(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S8, 8>(state, 50); }
static void BM_S8_nojitter_100(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S8, 8>(state, 100); }
static void BM_S16_nojitter_50(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S16, 16>(state, 50); }
static void BM_S16_nojitter_100(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S16, 16>(state, 100); }
static void BM_S32_nojitter_50(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S32, 32>(state, 50); }
static void BM_S32_nojitter_100(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S32, 32>(state, 100); }

#define AION_REGISTER(case_name) BENCHMARK(case_name)->MinWarmUpTime(aion::bench::kWarmupSeconds)->MinTime(aion::bench::kMinSeconds)->Unit(benchmark::kNanosecond)

AION_REGISTER(BM_S2_nojitter_50);
AION_REGISTER(BM_S2_nojitter_100);
AION_REGISTER(BM_S4_nojitter_50);
AION_REGISTER(BM_S4_nojitter_100);
AION_REGISTER(BM_S8_nojitter_50);
AION_REGISTER(BM_S8_nojitter_100);
AION_REGISTER(BM_S16_nojitter_50);
AION_REGISTER(BM_S16_nojitter_100);
AION_REGISTER(BM_S32_nojitter_50);
AION_REGISTER(BM_S32_nojitter_100);

#undef AION_REGISTER

} // namespace
