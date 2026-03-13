#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace aion::bench {

template <typename Event>
inline bool pred_hi(const Event &event) {
  return event.x > 100;
}

template <typename Event>
inline bool pred_lo(const Event &event) {
  return event.x < 30;
}

template <typename Event>
inline bool pred_mid(const Event &event) {
  return event.x >= 30 && event.x <= 100;
}

template <typename Event>
inline bool pred_ya(const Event &event) {
  return event.y == 'a';
}

template <typename Event>
inline bool pred_yb(const Event &event) {
  return event.y == 'b';
}

template <typename Event>
inline bool pred_yc(const Event &event) {
  return event.y == 'c';
}

template <typename Event>
inline bool pred_valid(const Event &event) {
  return event.valid == 1;
}

template <typename Event>
inline bool pred_a(const Event &event) {
  return pred_valid(event) && pred_hi(event) && pred_ya(event);
}

template <typename Event>
inline bool pred_b(const Event &event) {
  return pred_valid(event) && pred_mid(event) && pred_yb(event);
}

template <typename Event>
inline bool pred_c(const Event &event) {
  return pred_lo(event) && (pred_yb(event) || pred_yc(event));
}

template <typename Event>
inline bool pred_simple(const Event &event) {
  return event.x > 100;
}

template <typename Event>
inline bool pred_moderate(const Event &event) {
  return event.x > 100 && event.y == 'a';
}

template <typename Event>
inline bool pred_medium(const Event &event) {
  return event.x > 100 && event.y == 'a' && event.valid == 1;
}

template <typename Event>
inline bool pred_compound(const Event &event) {
  return event.x > 95 && event.x < 170 && event.valid == 1 && (event.y == 'a' || event.y == 'b') && event.ts < 120;
}

template <typename Event>
inline bool pred_very_compound(const Event &event) {
  return ((event.x > 90 && event.x < 170 && event.valid == 1 && (event.y == 'a' || event.y == 'b') && event.ts < 120) ||
          (event.x > 140 && event.valid == 1 && event.ts < 50 && !(event.y == 'c')) ||
          (event.x > 100 && event.valid == 1 && event.y == 'a'));
}

template <typename Event, std::size_t Repetitions>
class HandSequenceEngine {
public:
  HandSequenceEngine() = default;

  bool process_event(const Event &event) {
    std::uint64_t m = 0ULL;
    for (std::size_t pos = 1; pos <= Repetitions; ++pos) {
      const bool match = ((pos % 2U) == 1U) ? pred_a(event) : pred_b(event);
      m |= ((0ULL - static_cast<std::uint64_t>(match)) & (1ULL << pos));
    }

    std::uint64_t s_topo = first_;
    std::uint64_t active = s_;
    while (active != 0ULL) {
      const int bit_idx = std::countr_zero(active);
      s_topo |= follow_[static_cast<std::size_t>(bit_idx)];
      active &= (active - 1ULL);
    }

    s_ = s_topo & m;
    return (s_ & last_) != 0ULL;
  }

  void reset() { s_ = 0ULL; }

private:
  static consteval std::array<std::uint64_t, 64> build_follow() {
    std::array<std::uint64_t, 64> out{};
    for (std::size_t i = 1; i < Repetitions; ++i) {
      out[i] = (1ULL << (i + 1ULL));
    }
    return out;
  }

  static constexpr std::array<std::uint64_t, 64> follow_ = build_follow();
  static constexpr std::uint64_t first_ = (1ULL << 1ULL);
  static constexpr std::uint64_t last_ = (1ULL << Repetitions);
  std::uint64_t s_{0ULL};
};

template <typename Event, std::size_t Repetitions, bool (*Matcher)(const Event &)>
class HandRepeatedEngine {
public:
  HandRepeatedEngine() = default;

  bool process_event(const Event &event) {
    std::uint64_t m = 0ULL;
    const bool match = Matcher(event);
    for (std::size_t pos = 1; pos <= Repetitions; ++pos) {
      m |= ((0ULL - static_cast<std::uint64_t>(match)) & (1ULL << pos));
    }

    std::uint64_t s_topo = first_;
    std::uint64_t active = s_;
    while (active != 0ULL) {
      const int bit_idx = std::countr_zero(active);
      s_topo |= follow_[static_cast<std::size_t>(bit_idx)];
      active &= (active - 1ULL);
    }

    s_ = s_topo & m;
    return (s_ & last_) != 0ULL;
  }

  void reset() { s_ = 0ULL; }

private:
  static consteval std::array<std::uint64_t, 64> build_follow() {
    std::array<std::uint64_t, 64> out{};
    for (std::size_t i = 1; i < Repetitions; ++i) {
      out[i] = (1ULL << (i + 1ULL));
    }
    return out;
  }

  static constexpr std::array<std::uint64_t, 64> follow_ = build_follow();
  static constexpr std::uint64_t first_ = (1ULL << 1ULL);
  static constexpr std::uint64_t last_ = (1ULL << Repetitions);
  std::uint64_t s_{0ULL};
};

template <typename Event>
using HandPSimpleEngine = HandRepeatedEngine<Event, 4, pred_simple<Event>>;

template <typename Event>
using HandPModerateEngine = HandRepeatedEngine<Event, 4, pred_moderate<Event>>;

template <typename Event>
using HandPMediumEngine = HandRepeatedEngine<Event, 4, pred_medium<Event>>;

template <typename Event>
using HandPCompoundEngine = HandRepeatedEngine<Event, 4, pred_compound<Event>>;

template <typename Event>
using HandPVeryCompoundEngine = HandRepeatedEngine<Event, 4, pred_very_compound<Event>>;

} // namespace aion::bench
