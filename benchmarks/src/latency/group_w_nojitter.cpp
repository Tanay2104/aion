#include "common/bench_common.hpp"
#include "latency/groups.hpp"

#include "group_w_nojitter.hpp"

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
aion::bench::LatencyRecord run_case(const char *name, const int selectivity_percent) {
  const auto stream_len = aion::bench::resolve_stream_len(aion::bench::kDefaultStreamLen);
  const auto events = aion::bench::generate_events<Event>(stream_len, w_params(selectivity_percent));
  return aion::bench::measure_latency_case<Engine, Event>(name, events);
}

} // namespace

std::vector<aion::bench::LatencyRecord> run_group_w_latency_cases() {
  std::vector<aion::bench::LatencyRecord> records;
  records.reserve(14);

  records.push_back(run_case<aion::runtime::Engine_W0>("W0_nojitter_10", 10));
  records.push_back(run_case<aion::runtime::Engine_W0>("W0_nojitter_50", 50));

  records.push_back(run_case<aion::runtime::Engine_W1>("W1_nojitter_10", 10));
  records.push_back(run_case<aion::runtime::Engine_W1>("W1_nojitter_50", 50));

  records.push_back(run_case<aion::runtime::Engine_W2>("W2_nojitter_10", 10));
  records.push_back(run_case<aion::runtime::Engine_W2>("W2_nojitter_50", 50));

  records.push_back(run_case<aion::runtime::Engine_W4>("W4_nojitter_10", 10));
  records.push_back(run_case<aion::runtime::Engine_W4>("W4_nojitter_50", 50));

  records.push_back(run_case<aion::runtime::Engine_W8>("W8_nojitter_10", 10));
  records.push_back(run_case<aion::runtime::Engine_W8>("W8_nojitter_50", 50));

  records.push_back(run_case<aion::runtime::Engine_W16>("W16_nojitter_10", 10));
  records.push_back(run_case<aion::runtime::Engine_W16>("W16_nojitter_50", 50));

  records.push_back(run_case<aion::runtime::Engine_W32>("W32_nojitter_10", 10));
  records.push_back(run_case<aion::runtime::Engine_W32>("W32_nojitter_50", 50));
  return records;
}
