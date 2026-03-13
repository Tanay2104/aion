#include <benchmark/benchmark.h>

#include "common/bench_common.hpp"
#include "common/hand_baselines.hpp"
#include "common/throughput_helpers.hpp"

#include "group_s_jitter.hpp"

namespace {

using Event = aion::runtime::Event;

template <typename Engine, std::size_t Repetitions>
void run_sequence_case(benchmark::State &state, const int selectivity_percent) {
  const auto stream_len = aion::bench::resolve_stream_len(aion::bench::kDefaultStreamLen);
  const auto events =
      aion::bench::generate_sequence_events<Event>(stream_len, selectivity_percent, Repetitions);
  aion::bench::run_engine_throughput<Engine>(state, events);
}

using HandS2 = aion::bench::HandSequenceEngine<Event, 2>;
using HandS4 = aion::bench::HandSequenceEngine<Event, 4>;
using HandS8 = aion::bench::HandSequenceEngine<Event, 8>;
using HandS16 = aion::bench::HandSequenceEngine<Event, 16>;
using HandS32 = aion::bench::HandSequenceEngine<Event, 32>;

static void BM_S2_generated_50(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S2, 2>(state, 50); }
static void BM_S2_generated_100(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S2, 2>(state, 100); }
static void BM_S4_generated_50(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S4, 4>(state, 50); }
static void BM_S4_generated_100(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S4, 4>(state, 100); }
static void BM_S8_generated_50(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S8, 8>(state, 50); }
static void BM_S8_generated_100(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S8, 8>(state, 100); }
static void BM_S16_generated_50(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S16, 16>(state, 50); }
static void BM_S16_generated_100(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S16, 16>(state, 100); }
static void BM_S32_generated_50(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S32, 32>(state, 50); }
static void BM_S32_generated_100(benchmark::State &state) { run_sequence_case<aion::runtime::Engine_S32, 32>(state, 100); }

static void BM_S2_hand_50(benchmark::State &state) { run_sequence_case<HandS2, 2>(state, 50); }
static void BM_S2_hand_100(benchmark::State &state) { run_sequence_case<HandS2, 2>(state, 100); }
static void BM_S4_hand_50(benchmark::State &state) { run_sequence_case<HandS4, 4>(state, 50); }
static void BM_S4_hand_100(benchmark::State &state) { run_sequence_case<HandS4, 4>(state, 100); }
static void BM_S8_hand_50(benchmark::State &state) { run_sequence_case<HandS8, 8>(state, 50); }
static void BM_S8_hand_100(benchmark::State &state) { run_sequence_case<HandS8, 8>(state, 100); }
static void BM_S16_hand_50(benchmark::State &state) { run_sequence_case<HandS16, 16>(state, 50); }
static void BM_S16_hand_100(benchmark::State &state) { run_sequence_case<HandS16, 16>(state, 100); }
static void BM_S32_hand_50(benchmark::State &state) { run_sequence_case<HandS32, 32>(state, 50); }
static void BM_S32_hand_100(benchmark::State &state) { run_sequence_case<HandS32, 32>(state, 100); }

#define AION_REGISTER(case_name) BENCHMARK(case_name)->MinWarmUpTime(aion::bench::kWarmupSeconds)->MinTime(aion::bench::kMinSeconds)->Unit(benchmark::kNanosecond)

AION_REGISTER(BM_S2_generated_50);
AION_REGISTER(BM_S2_generated_100);
AION_REGISTER(BM_S4_generated_50);
AION_REGISTER(BM_S4_generated_100);
AION_REGISTER(BM_S8_generated_50);
AION_REGISTER(BM_S8_generated_100);
AION_REGISTER(BM_S16_generated_50);
AION_REGISTER(BM_S16_generated_100);
AION_REGISTER(BM_S32_generated_50);
AION_REGISTER(BM_S32_generated_100);
AION_REGISTER(BM_S2_hand_50);
AION_REGISTER(BM_S2_hand_100);
AION_REGISTER(BM_S4_hand_50);
AION_REGISTER(BM_S4_hand_100);
AION_REGISTER(BM_S8_hand_50);
AION_REGISTER(BM_S8_hand_100);
AION_REGISTER(BM_S16_hand_50);
AION_REGISTER(BM_S16_hand_100);
AION_REGISTER(BM_S32_hand_50);
AION_REGISTER(BM_S32_hand_100);

#undef AION_REGISTER

} // namespace
