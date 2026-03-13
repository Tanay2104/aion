#pragma once

#include "latency/latency_runner.hpp"

#include <vector>

std::vector<aion::bench::LatencyRecord> run_group_k_latency_cases();
std::vector<aion::bench::LatencyRecord> run_group_w_latency_cases();
