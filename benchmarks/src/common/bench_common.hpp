#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <vector>

namespace aion::bench {

inline constexpr std::uint64_t kSeed = 0xdeadbeef600dcafeULL;
inline constexpr std::size_t kDefaultStreamLen = 10'000'000ULL;

struct GenParams {
  double sel_hi{};
  double sel_lo{};
  double sel_ya{};
  double sel_yb{};
  double sel_valid{};
  double sel_early{};
  std::uint64_t seed{kSeed};
};

enum class SelectivityPreset : std::uint8_t {
  LOW,
  MEDIUM,
  HIGH,
  FULL,
};

inline double clamp_probability(const double value) {
  return std::max(0.0, std::min(1.0, value));
}

inline GenParams preset_params(const SelectivityPreset preset) {
  switch (preset) {
  case SelectivityPreset::LOW:
    return GenParams{.sel_hi = 0.05, .sel_lo = 0.05, .sel_ya = 0.05, .sel_yb = 0.05, .sel_valid = 0.80,
                     .sel_early = 0.05, .seed = kSeed};
  case SelectivityPreset::MEDIUM:
    return GenParams{.sel_hi = 0.20, .sel_lo = 0.10, .sel_ya = 0.20, .sel_yb = 0.20, .sel_valid = 0.80,
                     .sel_early = 0.20, .seed = kSeed};
  case SelectivityPreset::HIGH:
    return GenParams{.sel_hi = 0.50, .sel_lo = 0.15, .sel_ya = 0.50, .sel_yb = 0.50, .sel_valid = 0.90,
                     .sel_early = 0.50, .seed = kSeed};
  case SelectivityPreset::FULL:
    return GenParams{.sel_hi = 0.99, .sel_lo = 0.01, .sel_ya = 0.99, .sel_yb = 0.01, .sel_valid = 0.99,
                     .sel_early = 0.01, .seed = kSeed};
  }
  return preset_params(SelectivityPreset::MEDIUM);
}

inline GenParams with_uniform_selectivity(const double selectivity) {
  const double p = clamp_probability(selectivity);
  return GenParams{.sel_hi = p, .sel_lo = p, .sel_ya = p, .sel_yb = p, .sel_valid = 0.80, .sel_early = p, .seed = kSeed};
}

inline std::size_t resolve_stream_len(const std::size_t fallback) {
  const char *raw = std::getenv("AION_BENCH_STREAM_LEN");
  if (raw == nullptr || *raw == '\0') {
    return fallback;
  }

  char *end = nullptr;
  const auto parsed = std::strtoull(raw, &end, 10);
  if (end == raw || *end != '\0' || parsed == 0ULL) {
    return fallback;
  }

  return static_cast<std::size_t>(parsed);
}

template <typename Event>
std::vector<Event> generate_events(const std::size_t n, const GenParams &params) {
  std::mt19937_64 rng(params.seed);
  std::bernoulli_distribution hi(clamp_probability(params.sel_hi));
  std::bernoulli_distribution lo(clamp_probability(params.sel_lo));
  std::bernoulli_distribution ya(clamp_probability(params.sel_ya));
  std::bernoulli_distribution yb(clamp_probability(params.sel_yb));
  std::bernoulli_distribution valid(clamp_probability(params.sel_valid));
  std::bernoulli_distribution early(clamp_probability(params.sel_early));

  std::vector<Event> events;
  events.reserve(n);

  for (std::size_t i = 0; i < n; ++i) {
    Event event{};

    if (hi(rng)) {
      event.x = 150;
    } else if (lo(rng)) {
      event.x = 15;
    } else {
      event.x = 65;
    }

    if (ya(rng)) {
      event.y = 'a';
    } else if (yb(rng)) {
      event.y = 'b';
    } else {
      event.y = 'c';
    }

    event.valid = valid(rng) ? 1 : 0;
    event.ts = early(rng) ? 25 : 175;
    events.push_back(event);
  }

  return events;
}

template <typename Event>
std::vector<Event> generate_sequence_events(const std::size_t n,
                                            const int selectivity_percent,
                                            const std::size_t sequence_len) {
  std::vector<Event> events;
  events.reserve(n);

  for (std::size_t i = 0; i < n; ++i) {
    Event event{};
    const bool use_matching_block = (selectivity_percent >= 100) ||
                                    (((i / sequence_len) % 2U) == 0U);

    if (use_matching_block) {
      if ((i % 2U) == 0U) {
        event.x = 150;
        event.y = 'a';
      } else {
        event.x = 65;
        event.y = 'b';
      }
      event.valid = 1;
      event.ts = 25;
    } else {
      event.x = 65;
      event.y = 'c';
      event.valid = 0;
      event.ts = 175;
    }
    events.push_back(event);
  }

  return events;
}

template <typename Event>
std::vector<Event> generate_predicate_events_50(const std::size_t n) {
  std::vector<Event> events;
  events.reserve(n);

  for (std::size_t i = 0; i < n; ++i) {
    Event event{};
    if ((i % 2U) == 0U) {
      // Matches SIMPLE/MODERATE/MEDIUM/COMPOUND/VERY_COMPOUND.
      event.x = 120;
      event.y = 'a';
      event.valid = 1;
      event.ts = 25;
    } else {
      // Misses all five predicate levels.
      event.x = 20;
      event.y = 'c';
      event.valid = 0;
      event.ts = 175;
    }
    events.push_back(event);
  }

  return events;
}
} // namespace aion::bench
