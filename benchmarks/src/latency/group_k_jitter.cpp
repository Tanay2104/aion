#include "common/bench_common.hpp"
#include "latency/groups.hpp"

#include "group_k_jitter.hpp"

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
aion::bench::LatencyRecord run_case(const char *name, const int selectivity_percent) {
  const auto stream_len = aion::bench::resolve_stream_len(aion::bench::kDefaultStreamLen);
  const auto events = aion::bench::generate_events<Event>(stream_len, k_params(selectivity_percent));
  return aion::bench::measure_latency_case<Engine, Event>(name, events);
}

} // namespace

std::vector<aion::bench::LatencyRecord> run_group_k_latency_cases() {
  std::vector<aion::bench::LatencyRecord> records;
  records.reserve(15);

  records.push_back(run_case<aion::runtime::Engine_K_2>("K2_jitter_10", 10));
  records.push_back(run_case<aion::runtime::Engine_K_2>("K2_jitter_50", 50));
  records.push_back(run_case<aion::runtime::Engine_K_2>("K2_jitter_90", 90));

  records.push_back(run_case<aion::runtime::Engine_K_4>("K4_jitter_10", 10));
  records.push_back(run_case<aion::runtime::Engine_K_4>("K4_jitter_50", 50));
  records.push_back(run_case<aion::runtime::Engine_K_4>("K4_jitter_90", 90));

  records.push_back(run_case<aion::runtime::Engine_K_8>("K8_jitter_10", 10));
  records.push_back(run_case<aion::runtime::Engine_K_8>("K8_jitter_50", 50));
  records.push_back(run_case<aion::runtime::Engine_K_8>("K8_jitter_90", 90));

  records.push_back(run_case<aion::runtime::Engine_K_16>("K16_jitter_10", 10));
  records.push_back(run_case<aion::runtime::Engine_K_16>("K16_jitter_50", 50));
  records.push_back(run_case<aion::runtime::Engine_K_16>("K16_jitter_90", 90));

  records.push_back(run_case<aion::runtime::Engine_K_32>("K32_jitter_10", 10));
  records.push_back(run_case<aion::runtime::Engine_K_32>("K32_jitter_50", 50));
  records.push_back(run_case<aion::runtime::Engine_K_32>("K32_jitter_90", 90));

  return records;
}
