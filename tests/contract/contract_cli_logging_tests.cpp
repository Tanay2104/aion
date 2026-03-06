#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

import aion.core;
import aion.utils;

namespace {

using aion::core::CompilationContext;
using aion::core::IR;
using aion::core::Verbosity;

aion::core::Options parse_options(const std::vector<std::string> &extra_args) {
  std::vector<std::string> storage;
  storage.reserve(extra_args.size() + 2U);
  storage.push_back("aionc");
  storage.push_back("input.regex");
  storage.insert(storage.end(), extra_args.begin(), extra_args.end());

  std::vector<char *> argv;
  argv.reserve(storage.size());
  for (auto &arg : storage) {
    argv.push_back(arg.data());
  }

  int argc = static_cast<int>(argv.size());
  return aion::utils::argparse(argc, argv.data());
}

std::string capture_log(const CompilationContext &ctxt, int level, std::string_view message) {
  ::testing::internal::CaptureStdout();
  ctxt.log(level, message);
  return ::testing::internal::GetCapturedStdout();
}

} // namespace

TEST(ContractCliTests, DefaultsAreStableWithoutOptionalFlags) {
  const auto opts = parse_options({});
  EXPECT_EQ(opts.verbosity, Verbosity::FIRST_LEVEL);
  EXPECT_EQ(opts.ir, IR::NONE);
  EXPECT_EQ(opts.output_filename, aion::core::DEFAULT_NAME);
  EXPECT_TRUE(opts.jitter);
}

TEST(ContractCliTests, DumpAliasSetsNfaIr) {
  const auto opts = parse_options({"--dump", "NFA"});
  EXPECT_EQ(opts.ir, IR::NFA);
}

TEST(ContractCliTests, EmitFlagSetsTokensIr) {
  const auto opts = parse_options({"--emit", "TOKENS"});
  EXPECT_EQ(opts.ir, IR::TOKENS);
}

TEST(ContractCliTests, UnknownDumpTargetFallsBackToNone) {
  const auto opts = parse_options({"--dump", "UNKNOWN"});
  EXPECT_EQ(opts.ir, IR::NONE);
}

TEST(ContractCliTests, VerboseLongFlagMapsToV2) {
  const auto opts = parse_options({"--verbose"});
  EXPECT_EQ(opts.verbosity, Verbosity::SECOND_LEVEL);
}

TEST(ContractCliTests, ExplicitV0AndV3MapCorrectly) {
  const auto opts_v0 = parse_options({"-V0"});
  const auto opts_v3 = parse_options({"-V3"});
  EXPECT_EQ(opts_v0.verbosity, Verbosity::NONE);
  EXPECT_EQ(opts_v3.verbosity, Verbosity::THIRD_LEVEL);
}

TEST(ContractCliTests, NoJitterFlagDisablesJitterMode) {
  const auto opts = parse_options({"-nj"});
  EXPECT_FALSE(opts.jitter);
}

TEST(ContractCliTests, OutputFlagOverridesOutputFilename) {
  const auto opts = parse_options({"-o", "/tmp/contract_out"});
  EXPECT_EQ(opts.output_filename, "/tmp/contract_out");
}

TEST(ContractLoggingTests, HigherLevelMessagesAreSuppressed) {
  CompilationContext ctxt;
  ctxt.options.verbosity = Verbosity::FIRST_LEVEL;

  const std::string out = capture_log(ctxt, 2, "hidden");
  EXPECT_TRUE(out.empty());
}

TEST(ContractLoggingTests, MessagesAtConfiguredLevelArePrinted) {
  CompilationContext ctxt;
  ctxt.options.verbosity = Verbosity::SECOND_LEVEL;

  const std::string out = capture_log(ctxt, 2, "visible-message");
  EXPECT_NE(out.find("visible-message"), std::string::npos);
}

TEST(ContractLoggingTests, V0SuppressesAllInfoLogs) {
  CompilationContext ctxt;
  ctxt.options.verbosity = Verbosity::NONE;

  const std::string out = capture_log(ctxt, 1, "must-not-print");
  EXPECT_TRUE(out.empty());
}
