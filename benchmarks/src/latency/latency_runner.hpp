#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#endif

namespace aion::bench {

struct LatencyRecord {
  std::string benchmark;
  std::size_t events{};
  std::uint64_t p50_cycles{};
  std::uint64_t p95_cycles{};
  std::uint64_t p99_cycles{};
  double mean_cycles{};
};

inline std::uint64_t tsc_begin() {
#if defined(__x86_64__) || defined(__i386__)
  _mm_lfence();
  return __rdtsc();
#else
  return 0ULL;
#endif
}

inline std::uint64_t tsc_end() {
#if defined(__x86_64__) || defined(__i386__)
  unsigned int aux = 0;
  const std::uint64_t t = __builtin_ia32_rdtscp(&aux);
  _mm_lfence();
  return t;
#else
  return 0ULL;
#endif
}

inline std::uint64_t percentile_cycles(const std::vector<std::uint64_t> &sorted, const double percentile) {
  if (sorted.empty()) {
    return 0ULL;
  }
  const double clamped = std::max(0.0, std::min(1.0, percentile));
  const std::size_t idx = static_cast<std::size_t>(clamped * static_cast<double>(sorted.size() - 1ULL));
  return sorted[idx];
}

template <typename Engine, typename Event>
LatencyRecord measure_latency_case(std::string name, const std::vector<Event> &events) {
  Engine engine;
  std::vector<std::uint64_t> samples;
  samples.reserve(events.size());

  for (const auto &event : events) {
    const std::uint64_t t0 = tsc_begin();
    const bool matched = engine.process_event(event);
    const std::uint64_t t1 = tsc_end();
    (void)matched;
    samples.push_back(t1 - t0);
  }

  std::sort(samples.begin(), samples.end());
  const auto sum = std::accumulate(samples.begin(), samples.end(), 0.0);

  LatencyRecord record;
  record.benchmark = std::move(name);
  record.events = events.size();
  record.p50_cycles = percentile_cycles(samples, 0.50);
  record.p95_cycles = percentile_cycles(samples, 0.95);
  record.p99_cycles = percentile_cycles(samples, 0.99);
  record.mean_cycles = (samples.empty() ? 0.0 : (sum / static_cast<double>(samples.size())));
  return record;
}

inline bool write_latency_csv(const std::string_view path, const std::vector<LatencyRecord> &records) {
  std::ofstream out{std::string(path)};
  if (!out) {
    return false;
  }

  out << "benchmark,events,p50_cycles,p95_cycles,p99_cycles,mean_cycles\n";
  for (const auto &record : records) {
    out << record.benchmark << ',' << record.events << ',' << record.p50_cycles << ',' << record.p95_cycles << ','
        << record.p99_cycles << ',' << record.mean_cycles << '\n';
  }
  return true;
}

} // namespace aion::bench
