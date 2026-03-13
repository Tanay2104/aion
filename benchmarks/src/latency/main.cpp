#include "latency/groups.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string timestamp_now() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  localtime_s(&tm, &now_time);
#else
  localtime_r(&now_time, &tm);
#endif
  char buffer[32]{};
  std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &tm);
  return std::string(buffer);
}

std::string parse_output_path(int argc, char **argv) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg(argv[i]);
    if (arg.rfind("--out=", 0) == 0) {
      return arg.substr(6);
    }
    if (arg == "--out" && (i + 1) < argc) {
      return std::string(argv[i + 1]);
    }
  }

  return "benchmarks/results/latency_" + timestamp_now() + ".csv";
}

} // namespace

int main(int argc, char **argv) {
  const std::string out_path = parse_output_path(argc, argv);

  std::vector<aion::bench::LatencyRecord> records;
  auto group_k = run_group_k_latency_cases();
  auto group_w = run_group_w_latency_cases();

  records.reserve(group_k.size() + group_w.size());
  records.insert(records.end(), group_k.begin(), group_k.end());
  records.insert(records.end(), group_w.begin(), group_w.end());

  std::filesystem::create_directories(std::filesystem::path(out_path).parent_path());
  if (!aion::bench::write_latency_csv(out_path, records)) {
    std::cerr << "failed to write latency report: " << out_path << '\n';
    return 1;
  }

  std::cout << "latency records=" << records.size() << " out=" << out_path << '\n';
  return 0;
}
