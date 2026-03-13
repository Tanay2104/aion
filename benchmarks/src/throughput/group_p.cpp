#include <benchmark/benchmark.h>

#include "common/bench_common.hpp"
#include "common/hand_baselines.hpp"
#include "common/throughput_helpers.hpp"

#include "group_p_jitter.hpp"

namespace {

using Event = aion::runtime::Event;

template <typename Engine>
void run_predicate_case(benchmark::State &state) {
  const auto stream_len = aion::bench::resolve_stream_len(aion::bench::kDefaultStreamLen);
  const auto events = aion::bench::generate_predicate_events_50<Event>(stream_len);
  aion::bench::run_engine_throughput<Engine>(state, events);
}

using HandSimple = aion::bench::HandPSimpleEngine<Event>;
using HandModerate = aion::bench::HandPModerateEngine<Event>;
using HandMedium = aion::bench::HandPMediumEngine<Event>;
using HandCompound = aion::bench::HandPCompoundEngine<Event>;
using HandVeryCompound = aion::bench::HandPVeryCompoundEngine<Event>;

static void BM_P_simple_generated(benchmark::State &state) { run_predicate_case<aion::runtime::Engine_P_simple>(state); }
static void BM_P_moderate_generated(benchmark::State &state) { run_predicate_case<aion::runtime::Engine_P_moderate>(state); }
static void BM_P_medium_generated(benchmark::State &state) { run_predicate_case<aion::runtime::Engine_P_medium>(state); }
static void BM_P_compound_generated(benchmark::State &state) {
  run_predicate_case<aion::runtime::Engine_P_compound>(state);
}
static void BM_P_very_compound_generated(benchmark::State &state) {
  run_predicate_case<aion::runtime::Engine_P_very_compound>(state);
}

static void BM_P_simple_hand(benchmark::State &state) { run_predicate_case<HandSimple>(state); }
static void BM_P_moderate_hand(benchmark::State &state) { run_predicate_case<HandModerate>(state); }
static void BM_P_medium_hand(benchmark::State &state) { run_predicate_case<HandMedium>(state); }
static void BM_P_compound_hand(benchmark::State &state) { run_predicate_case<HandCompound>(state); }
static void BM_P_very_compound_hand(benchmark::State &state) { run_predicate_case<HandVeryCompound>(state); }

#define AION_REGISTER(case_name) BENCHMARK(case_name)->MinWarmUpTime(aion::bench::kWarmupSeconds)->MinTime(aion::bench::kMinSeconds)->Unit(benchmark::kNanosecond)

AION_REGISTER(BM_P_simple_generated);
AION_REGISTER(BM_P_moderate_generated);
AION_REGISTER(BM_P_medium_generated);
AION_REGISTER(BM_P_compound_generated);
AION_REGISTER(BM_P_very_compound_generated);
AION_REGISTER(BM_P_simple_hand);
AION_REGISTER(BM_P_moderate_hand);
AION_REGISTER(BM_P_medium_hand);
AION_REGISTER(BM_P_compound_hand);
AION_REGISTER(BM_P_very_compound_hand);

#undef AION_REGISTER

} // namespace
