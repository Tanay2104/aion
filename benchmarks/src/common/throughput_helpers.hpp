#pragma once

#include <benchmark/benchmark.h>

#include <cstdint>
#include <vector>

namespace aion::bench {

inline constexpr double kWarmupSeconds = 0.25;
inline constexpr double kMinSeconds = 2.5;

template <typename Engine, typename Event>
void run_engine_throughput(benchmark::State &state, const std::vector<Event> &events) {
  Engine engine;
  std::uint64_t sink = 0ULL;

  for (auto _ : state) {
    engine.reset();
    for (const auto &event : events) {
      sink += static_cast<std::uint64_t>(engine.process_event(event));
    }
    benchmark::DoNotOptimize(sink);
  }

  const auto iterations = static_cast<int64_t>(state.iterations());
  const auto events_per_iteration = static_cast<int64_t>(events.size());
  state.SetItemsProcessed(iterations * events_per_iteration);
  state.counters["events_per_sec"] = benchmark::Counter(
      static_cast<double>(iterations) * static_cast<double>(events.size()), benchmark::Counter::kIsRate);
}

template <typename Engine, typename Event>
void run_engine_throughput_replay(benchmark::State &state, const std::vector<Event> &chunk,
                                  const std::size_t logical_event_count) {
  Engine engine;
  std::uint64_t sink = 0ULL;
  const std::size_t full_replays = logical_event_count / chunk.size();
  const std::size_t remainder = logical_event_count % chunk.size();

  for (auto _ : state) {
    engine.reset();

    for (std::size_t replay = 0; replay < full_replays; ++replay) {
      for (const auto &event : chunk) {
        sink += static_cast<std::uint64_t>(engine.process_event(event));
      }
    }

    for (std::size_t i = 0; i < remainder; ++i) {
      sink += static_cast<std::uint64_t>(engine.process_event(chunk[i]));
    }

    benchmark::DoNotOptimize(sink);
  }

  const auto iterations = static_cast<int64_t>(state.iterations());
  const auto logical_events = static_cast<int64_t>(logical_event_count);
  state.SetItemsProcessed(iterations * logical_events);
  state.counters["events_per_sec"] =
      benchmark::Counter(static_cast<double>(iterations) * static_cast<double>(logical_event_count),
                         benchmark::Counter::kIsRate);
}

} // namespace aion::bench
