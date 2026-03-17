#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

namespace {

struct CommandResult {
  int raw_status = -1;
  int exit_code = -1;
  std::string stdout_text;
  std::string stderr_text;
};

std::string shell_quote(std::string_view text) {
  std::string out;
  out.reserve(text.size() + 2U);
  out.push_back('\'');
  for (const char ch : text) {
    if (ch == '\'') {
      out += "'\"'\"'";
    } else {
      out.push_back(ch);
    }
  }
  out.push_back('\'');
  return out;
}

int decode_exit_code(const int raw_status) {
#if defined(__unix__) || defined(__APPLE__)
  if (WIFEXITED(raw_status) != 0) {
    return WEXITSTATUS(raw_status);
  }
  return raw_status;
#else
  return raw_status;
#endif
}

std::filesystem::path fixture_dir() {
  return std::filesystem::path(__FILE__).parent_path() / "cases";
}

std::filesystem::path fixture_file(std::string_view name) {
  return fixture_dir() / std::string(name);
}

std::filesystem::path resolve_aionc_path() {
  std::error_code ec;
  const std::filesystem::path self = std::filesystem::canonical("/proc/self/exe", ec);
  if (!ec) {
    const std::filesystem::path sibling = self.parent_path() / "aionc";
    if (std::filesystem::exists(sibling)) {
      return sibling;
    }
  }

  const std::filesystem::path fallback = std::filesystem::current_path() / "bin" / "aionc";
  return fallback;
}

std::string read_file(const std::filesystem::path &path) {
  std::ifstream input(path);
  return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

std::string strip_ansi_codes(const std::string &input) {
  static const std::regex ansi_re("\x1B\\[[0-9;]*m");
  return std::regex_replace(input, ansi_re, "");
}

std::size_t count_lines(const std::string &text) {
  if (text.empty()) {
    return 0U;
  }
  return static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n'));
}

std::size_t count_substring(std::string_view haystack, std::string_view needle) {
  if (needle.empty()) {
    return 0U;
  }

  std::size_t count = 0U;
  std::size_t pos = haystack.find(needle);
  while (pos != std::string_view::npos) {
    ++count;
    pos = haystack.find(needle, pos + needle.size());
  }
  return count;
}

class Sandbox {
public:
  Sandbox() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    dir_ = std::filesystem::temp_directory_path() / ("aion_e2e_" + std::to_string(stamp));
    std::filesystem::create_directories(dir_);
  }

  ~Sandbox() {
    if (std::getenv("AION_E2E_KEEP_TMP") != nullptr) {
      std::cerr << "Preserving e2e sandbox: " << dir_ << '\n';
      return;
    }

    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
  }

  [[nodiscard]] const std::filesystem::path &dir() const { return dir_; }
  [[nodiscard]] std::filesystem::path file(std::string_view name) const { return dir_ / std::string(name); }

private:
  std::filesystem::path dir_;
};

CommandResult run_aionc(const std::vector<std::string> &args, const std::filesystem::path &working_dir,
                        const std::filesystem::path &stdout_file, const std::filesystem::path &stderr_file) {
  const std::filesystem::path aionc = resolve_aionc_path();
  EXPECT_TRUE(std::filesystem::exists(aionc));

  std::ostringstream cmd;
  cmd << "cd " << shell_quote(working_dir.string()) << " && ";
  cmd << shell_quote(aionc.string());
  for (const std::string &arg : args) {
    cmd << ' ' << shell_quote(arg);
  }
  cmd << " > " << shell_quote(stdout_file.string());
  cmd << " 2> " << shell_quote(stderr_file.string());

  CommandResult result;
  result.raw_status = std::system(cmd.str().c_str());
  result.exit_code = decode_exit_code(result.raw_status);
  result.stdout_text = read_file(stdout_file);
  result.stderr_text = read_file(stderr_file);
  return result;
}

class E2ECompilerTests : public ::testing::Test {
protected:
  Sandbox sandbox;
};

TEST_F(E2ECompilerTests, CompilerBinaryIsDiscoverable) {
  EXPECT_TRUE(std::filesystem::exists(resolve_aionc_path()));
}

TEST_F(E2ECompilerTests, HelpFlagPrintsUsageAndExitsZero) {
  const CommandResult run = run_aionc({"--help"}, sandbox.dir(), sandbox.file("help.stdout"), sandbox.file("help.stderr"));
  EXPECT_EQ(run.exit_code, 0);
  EXPECT_NE(run.stdout_text.find("Basic usage:"), std::string::npos);
  EXPECT_TRUE(run.stderr_text.empty());
}

TEST_F(E2ECompilerTests, VersionFlagPrintsVersionAndExitsZero) {
  const CommandResult run =
      run_aionc({"--version"}, sandbox.dir(), sandbox.file("version.stdout"), sandbox.file("version.stderr"));
  EXPECT_EQ(run.exit_code, 0);
  EXPECT_NE(run.stdout_text.find("0.1.0"), std::string::npos);
  EXPECT_TRUE(run.stderr_text.empty());
}

TEST_F(E2ECompilerTests, MissingInputFileExitsWithError) {
  const std::filesystem::path missing_file = sandbox.file("does_not_exist.regex");
  const CommandResult run = run_aionc({missing_file.string(), "-V0"}, sandbox.dir(), sandbox.file("missing.stdout"),
                                      sandbox.file("missing.stderr"));

  EXPECT_EQ(run.exit_code, 1);
  EXPECT_NE(run.stdout_text.find("Error opening input file:"), std::string::npos);
}

TEST_F(E2ECompilerTests, ValidProgramCompilesAtV0AndProducesHeader) {
  const std::filesystem::path input = fixture_file("valid_basic.regex");
  const std::filesystem::path out_base = sandbox.file("valid_out");
  const CommandResult run = run_aionc({input.string(), "-V0", "-o", out_base.string()}, sandbox.dir(),
                                      sandbox.file("valid.stdout"), sandbox.file("valid.stderr"));

  EXPECT_EQ(run.exit_code, 0);
  EXPECT_TRUE(run.stdout_text.empty());
  EXPECT_TRUE(run.stderr_text.empty());

  const std::filesystem::path generated_header = out_base.string() + ".hpp";
  ASSERT_TRUE(std::filesystem::exists(generated_header));
  EXPECT_GT(std::filesystem::file_size(generated_header), 0U);
}

TEST_F(E2ECompilerTests, DefaultOutputFilenameProducesAionOutHeader) {
  const std::filesystem::path input = fixture_file("valid_basic.regex");
  const CommandResult run = run_aionc({input.string(), "-V0"}, sandbox.dir(), sandbox.file("default.stdout"),
                                      sandbox.file("default.stderr"));

  EXPECT_EQ(run.exit_code, 0);
  const std::filesystem::path generated_header = sandbox.file("aion_out.hpp");
  EXPECT_TRUE(std::filesystem::exists(generated_header));
}

TEST_F(E2ECompilerTests, DumpTokensWritesTokenArtifactAndHeader) {
  const std::filesystem::path input = fixture_file("valid_basic.regex");
  const std::filesystem::path out_base = sandbox.file("tokens_dump");
  const CommandResult run = run_aionc({input.string(), "--dump", "TOKENS", "-V0", "-o", out_base.string()},
                                      sandbox.dir(), sandbox.file("tokens.stdout"), sandbox.file("tokens.stderr"));

  EXPECT_EQ(run.exit_code, 0);
  ASSERT_TRUE(std::filesystem::exists(out_base));
  const std::string dumped = read_file(out_base);
  EXPECT_NE(dumped.find("TYPE: KW_EVENT"), std::string::npos);
  EXPECT_NE(dumped.find("TYPE: END_OF_FILE"), std::string::npos);

  const std::filesystem::path generated_header = out_base.string() + ".hpp";
  EXPECT_TRUE(std::filesystem::exists(generated_header));
}

TEST_F(E2ECompilerTests, DumpAstWritesTreeArtifactAndHeader) {
  const std::filesystem::path input = fixture_file("valid_basic.regex");
  const std::filesystem::path out_base = sandbox.file("ast_dump");
  const CommandResult run = run_aionc({input.string(), "--dump", "AST", "-V0", "-o", out_base.string()}, sandbox.dir(),
                                      sandbox.file("ast.stdout"), sandbox.file("ast.stderr"));

  EXPECT_EQ(run.exit_code, 0);
  ASSERT_TRUE(std::filesystem::exists(out_base));
  const std::string dumped = read_file(out_base);
  EXPECT_NE(dumped.find("AionFile"), std::string::npos);
  EXPECT_NE(dumped.find("EventDecl"), std::string::npos);
  EXPECT_NE(dumped.find("RegexDecl"), std::string::npos);

  const std::filesystem::path generated_header = out_base.string() + ".hpp";
  EXPECT_TRUE(std::filesystem::exists(generated_header));
}

TEST_F(E2ECompilerTests, DumpNfaWritesDotArtifactAndHeader) {
  const std::filesystem::path input = fixture_file("valid_basic.regex");
  const std::filesystem::path out_base = sandbox.file("nfa_dump");
  const CommandResult run = run_aionc({input.string(), "--dump", "NFA", "-V0", "-o", out_base.string()}, sandbox.dir(),
                                      sandbox.file("nfa.stdout"), sandbox.file("nfa.stderr"));

  EXPECT_EQ(run.exit_code, 0);
  const std::filesystem::path dot_file = out_base.string() + ".dot";
  ASSERT_TRUE(std::filesystem::exists(dot_file));
  const std::string dot = read_file(dot_file);
  EXPECT_NE(dot.find("digraph AionNFA"), std::string::npos);
  EXPECT_NE(dot.find("Regex: R"), std::string::npos);

  const std::filesystem::path generated_header = out_base.string() + ".hpp";
  EXPECT_TRUE(std::filesystem::exists(generated_header));
}

TEST_F(E2ECompilerTests, EmitAliasProducesNfaDotArtifact) {
  const std::filesystem::path input = fixture_file("valid_basic.regex");
  const std::filesystem::path out_base = sandbox.file("emit_nfa");
  const CommandResult run = run_aionc({input.string(), "--emit", "NFA", "-V0", "-o", out_base.string()}, sandbox.dir(),
                                      sandbox.file("emit.stdout"), sandbox.file("emit.stderr"));

  EXPECT_EQ(run.exit_code, 0);
  EXPECT_TRUE(std::filesystem::exists(out_base.string() + ".dot"));
}

TEST_F(E2ECompilerTests, StarRegexNfaDumpContainsNullableEpsilonNode) {
  const std::filesystem::path input = fixture_file("valid_star.regex");
  const std::filesystem::path out_base = sandbox.file("star_nfa");
  const CommandResult run = run_aionc({input.string(), "--dump", "NFA", "-V0", "-o", out_base.string()}, sandbox.dir(),
                                      sandbox.file("star.stdout"), sandbox.file("star.stderr"));

  EXPECT_EQ(run.exit_code, 0);
  const std::string dot = read_file(out_base.string() + ".dot");
  EXPECT_NE(dot.find("\"R_eps\""), std::string::npos);
}

TEST_F(E2ECompilerTests, InvalidRegexReportsDiagnosticAndDoesNotGenerateOutput) {
  const std::filesystem::path input = fixture_file("invalid_regex_recovery.regex");
  const std::filesystem::path out_base = sandbox.file("invalid_recovery");
  const CommandResult run = run_aionc({input.string(), "-V0", "-o", out_base.string()}, sandbox.dir(),
                                      sandbox.file("invalid.stdout"), sandbox.file("invalid.stderr"));

  EXPECT_EQ(run.exit_code, 0);
  EXPECT_NE(run.stderr_text.find("expected regular expression"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(out_base.string() + ".hpp"));
}

TEST_F(E2ECompilerTests, MissingEventReportsExpectedDiagnosticContract) {
  const std::filesystem::path input = fixture_file("missing_event.regex");
  const std::filesystem::path out_base = sandbox.file("missing_event");
  const CommandResult run = run_aionc({input.string(), "-V0", "-o", out_base.string()}, sandbox.dir(),
                                      sandbox.file("missing_event.stdout"), sandbox.file("missing_event.stderr"));

  EXPECT_EQ(run.exit_code, 0);
  EXPECT_NE(run.stderr_text.find("expected keyword 'event'"), std::string::npos);
}

TEST_F(E2ECompilerTests, TokenDumpRetainsIdentifiersAfterStandaloneAmpersandAndSlash) {
  const std::filesystem::path input = fixture_file("regression_operator_errors.regex");
  const std::filesystem::path out_base = sandbox.file("operator_regression_tokens");
  const CommandResult run = run_aionc({input.string(), "--dump", "TOKENS", "-V0", "-o", out_base.string()},
                                      sandbox.dir(), sandbox.file("op_reg.stdout"), sandbox.file("op_reg.stderr"));

  ASSERT_EQ(run.exit_code, 0);
  const std::string dumped = read_file(out_base);

  const std::size_t amp_error = dumped.find("TYPE: ERROR\tTEXT: &");
  ASSERT_NE(amp_error, std::string::npos);
  const std::size_t y_after_amp = dumped.find("TYPE: IDENTIFIER\tTEXT: y", amp_error);
  EXPECT_NE(y_after_amp, std::string::npos);

  const std::size_t slash_error = dumped.find("TYPE: ERROR\tTEXT: /");
  ASSERT_NE(slash_error, std::string::npos);
  const std::size_t z_after_slash = dumped.find("TYPE: IDENTIFIER\tTEXT: z", slash_error);
  EXPECT_NE(z_after_slash, std::string::npos);
}


TEST_F(E2ECompilerTests, MultiErrorInputEmitsAtLeastTwoErrorsAndKeyMessages) {
  const std::filesystem::path input = fixture_file("multi_error.regex");
  const std::filesystem::path out_base = sandbox.file("multi_error_out");
  const CommandResult run = run_aionc({input.string(), "-V0", "-o", out_base.string()}, sandbox.dir(),
                                      sandbox.file("multi_error.stdout"), sandbox.file("multi_error.stderr"));

  ASSERT_EQ(run.exit_code, 0);
  const std::string clean_stderr = strip_ansi_codes(run.stderr_text);
  EXPECT_GE(count_substring(clean_stderr, "[ERROR]"), 2U);
  EXPECT_NE(clean_stderr.find("expected keyword 'event'"), std::string::npos);
  EXPECT_NE(clean_stderr.find("expected regular expression"), std::string::npos);
}

TEST_F(E2ECompilerTests, NfaDumpAfterRecoveryContainsOnlyValidRegex) {
  const std::filesystem::path input = fixture_file("invalid_regex_recovery.regex");
  const std::filesystem::path out_base = sandbox.file("recovery_nfa_dump");
  const CommandResult run = run_aionc({input.string(), "--dump", "NFA", "-V0", "-o", out_base.string()},
                                      sandbox.dir(), sandbox.file("recovery_nfa.stdout"),
                                      sandbox.file("recovery_nfa.stderr"));

  ASSERT_EQ(run.exit_code, 0);
  const std::string dot = read_file(out_base.string() + ".dot");
  EXPECT_NE(dot.find("Regex: Good"), std::string::npos);
  EXPECT_EQ(dot.find("Regex: Bad"), std::string::npos);
}

TEST_F(E2ECompilerTests, V1ShowsLifecycleLogsButNotV2OrV3Details) {
  const std::filesystem::path input = fixture_file("valid_basic.regex");
  const std::filesystem::path out_base = sandbox.file("v1_out");
  const CommandResult run = run_aionc({input.string(), "-V1", "-o", out_base.string()}, sandbox.dir(),
                                      sandbox.file("v1.stdout"), sandbox.file("v1.stderr"));

  ASSERT_EQ(run.exit_code, 0);
  const std::string clean_stdout = strip_ansi_codes(run.stdout_text);

  EXPECT_NE(clean_stdout.find("[Lex] Tokenization started"), std::string::npos);
  EXPECT_NE(clean_stdout.find("[Codegen] Emission finished"), std::string::npos);
  EXPECT_EQ(clean_stdout.find("[Input] Source size:"), std::string::npos);
  EXPECT_EQ(clean_stdout.find("[Lex] token type="), std::string::npos);
}

TEST_F(E2ECompilerTests, V2ShowsSummariesButNotTokenTrace) {
  const std::filesystem::path input = fixture_file("valid_basic.regex");
  const std::filesystem::path out_base = sandbox.file("v2_out");
  const CommandResult run = run_aionc({input.string(), "-V2", "-o", out_base.string()}, sandbox.dir(),
                                      sandbox.file("v2.stdout"), sandbox.file("v2.stderr"));

  ASSERT_EQ(run.exit_code, 0);
  const std::string clean_stdout = strip_ansi_codes(run.stdout_text);

  EXPECT_NE(clean_stdout.find("[Input] Source size:"), std::string::npos);
  EXPECT_NE(clean_stdout.find("[Lex] Produced"), std::string::npos);
  EXPECT_NE(clean_stdout.find("[Symbols]"), std::string::npos);
  EXPECT_EQ(clean_stdout.find("[Lex] token type="), std::string::npos);
}

TEST_F(E2ECompilerTests, V3ShowsDetailedTokenTrace) {
  const std::filesystem::path input = fixture_file("valid_basic.regex");
  const std::filesystem::path out_base = sandbox.file("v3_out");
  const CommandResult run = run_aionc({input.string(), "-V3", "-o", out_base.string()}, sandbox.dir(),
                                      sandbox.file("v3.stdout"), sandbox.file("v3.stderr"));

  ASSERT_EQ(run.exit_code, 0);
  const std::string clean_stdout = strip_ansi_codes(run.stdout_text);
  EXPECT_NE(clean_stdout.find("[Lex] token type=KW_EVENT"), std::string::npos);
  EXPECT_GT(count_lines(clean_stdout), 40U);
}

TEST_F(E2ECompilerTests, CompilerOutputsAreDeterministicAcrossRepeatedRuns) {
  const std::filesystem::path input = fixture_file("valid_basic.regex");
  const std::filesystem::path out_1 = sandbox.file("determinism_1");
  const std::filesystem::path out_2 = sandbox.file("determinism_2");

  const CommandResult run_1 = run_aionc({input.string(), "--dump", "NFA", "-V0", "-o", out_1.string()}, sandbox.dir(),
                                        sandbox.file("det1.stdout"), sandbox.file("det1.stderr"));
  const CommandResult run_2 = run_aionc({input.string(), "--dump", "NFA", "-V0", "-o", out_2.string()}, sandbox.dir(),
                                        sandbox.file("det2.stdout"), sandbox.file("det2.stderr"));

  ASSERT_EQ(run_1.exit_code, 0);
  ASSERT_EQ(run_2.exit_code, 0);

  EXPECT_EQ(read_file(out_1.string() + ".hpp"), read_file(out_2.string() + ".hpp"));
  EXPECT_EQ(read_file(out_1.string() + ".dot"), read_file(out_2.string() + ".dot"));
}

TEST_F(E2ECompilerTests, MissingOutputValueFailsFastWithInvalidArgument) {
  const std::filesystem::path input = fixture_file("valid_basic.regex");
  const CommandResult run =
      run_aionc({input.string(), "-o"}, sandbox.dir(), sandbox.file("badarg.stdout"), sandbox.file("badarg.stderr"));

  EXPECT_EQ(run.exit_code, 1);
  EXPECT_NE(run.stdout_text.find("Invalid argument"), std::string::npos);
}

} // namespace
