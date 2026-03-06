#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ranges>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

import aion.core;
import aion.frontend;

namespace {

using aion::core::CompilationContext;
using aion::core::Type;
using aion::core::Verbosity;
using aion::frontend::AndPredExpr;
using aion::frontend::CompPredExpr;
using aion::frontend::Lexer;
using aion::frontend::NotExpr;
using aion::frontend::OrPredExpr;
using aion::frontend::Parser;
using aion::frontend::PredDecl;
using aion::frontend::PredExpr;
using aion::frontend::PredRefExpr;
using aion::frontend::PrimaryPredExpr;
using aion::frontend::RegexConcat;
using aion::frontend::RegexDecl;
using aion::frontend::RegexExpr;
using aion::frontend::RegexPrimary;
using aion::frontend::RegexRefExpr;
using aion::frontend::RegexStar;
using aion::frontend::RegexUnion;
using aion::frontend::RegexWildcard;
using aion::frontend::SymbolKind;
using aion::frontend::Token;

using ScalarValue = std::variant<bool, std::int64_t, double, char, std::string>;
using EventSample = std::unordered_map<std::string, ScalarValue>;
using TimelineMap = std::unordered_map<std::string, std::vector<bool>>;

struct SemanticFixture {
  std::string name;
  std::filesystem::path source_path;
  std::vector<EventSample> trace;
  TimelineMap expected;
};

struct ParsedProgram {
  std::string source;
  CompilationContext ctxt;
  std::vector<Token> tokens;
  std::unique_ptr<aion::frontend::AionFile> ast;
  aion::frontend::SymbolTable symbols;
};

struct CommandResult {
  int raw_status = -1;
  int exit_code = -1;
  std::string stdout_text;
  std::string stderr_text;
};

struct PredCacheKey {
  std::string name;
  std::size_t event_idx{};

  bool operator==(const PredCacheKey &other) const {
    return name == other.name && event_idx == other.event_idx;
  }
};

struct PredCacheKeyHash {
  std::size_t operator()(const PredCacheKey &key) const {
    const std::size_t h1 = std::hash<std::string>{}(key.name);
    const std::size_t h2 = std::hash<std::size_t>{}(key.event_idx);
    return h1 ^ (h2 + 0x9e3779b9U + (h1 << 6U) + (h1 >> 2U));
  }
};

struct RegexCacheKey {
  const RegexExpr *expr{};
  std::size_t l{};
  std::size_t r{};

  bool operator==(const RegexCacheKey &other) const { return expr == other.expr && l == other.l && r == other.r; }
};

struct RegexCacheKeyHash {
  std::size_t operator()(const RegexCacheKey &key) const {
    const std::size_t h1 = std::hash<const void *>{}(key.expr);
    const std::size_t h2 = std::hash<std::size_t>{}(key.l);
    const std::size_t h3 = std::hash<std::size_t>{}(key.r);
    std::size_t out = h1 ^ (h2 + 0x9e3779b9U + (h1 << 6U) + (h1 >> 2U));
    out ^= (h3 + 0x9e3779b9U + (out << 6U) + (out >> 2U));
    return out;
  }
};

std::filesystem::path semantic_root() { return std::filesystem::path(__FILE__).parent_path(); }
std::filesystem::path semantic_cases_root() { return semantic_root() / "cases"; }
std::filesystem::path python_oracle_path() { return semantic_root() / "oracle_py" / "semantic_oracle.py"; }

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
#endif
  return raw_status;
}

std::string read_file(const std::filesystem::path &path) {
  std::ifstream input(path);
  return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
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
  return std::filesystem::current_path() / "bin" / "aionc";
}

CommandResult run_shell_command(std::string_view command, const std::filesystem::path &stdout_path,
                                const std::filesystem::path &stderr_path) {
  const std::string cmd = std::string(command) + " > " + shell_quote(stdout_path.string()) + " 2> " +
                          shell_quote(stderr_path.string());
  CommandResult result;
  result.raw_status = std::system(cmd.c_str());
  result.exit_code = decode_exit_code(result.raw_status);
  result.stdout_text = read_file(stdout_path);
  result.stderr_text = read_file(stderr_path);
  return result;
}

std::string json_escape(std::string_view input) {
  std::string out;
  out.reserve(input.size() + 8U);
  for (const char ch : input) {
    switch (ch) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out.push_back(ch);
      break;
    }
  }
  return out;
}

std::string scalar_to_json(const ScalarValue &value) {
  return std::visit(
      [](const auto &v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
          return v ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
          return std::to_string(v);
        } else if constexpr (std::is_same_v<T, double>) {
          return std::to_string(v);
        } else if constexpr (std::is_same_v<T, char>) {
          return std::to_string(static_cast<int>(v));
        } else if constexpr (std::is_same_v<T, std::string>) {
          return "\"" + json_escape(v) + "\"";
        } else {
          return "null";
        }
      },
      value);
}

std::string scalar_to_cpp_literal(const ScalarValue &value) {
  return std::visit(
      [](const auto &v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
          return v ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
          return std::to_string(v);
        } else if constexpr (std::is_same_v<T, double>) {
          std::ostringstream out;
          out << v;
          return out.str();
        } else if constexpr (std::is_same_v<T, char>) {
          std::string s;
          s += '\'';
          if (v == '\'' || v == '\\') {
            s += '\\';
          }
          s += v;
          s += '\'';
          return s;
        } else if constexpr (std::is_same_v<T, std::string>) {
          return "\"" + json_escape(v) + "\"";
        } else {
          return "0";
        }
      },
      value);
}

ScalarValue default_value_for_type(const Type type) {
  switch (type) {
  case Type::BOOL:
    return false;
  case Type::INT:
    return static_cast<std::int64_t>(0);
  case Type::FLOAT:
    return 0.0;
  case Type::CHAR:
    return static_cast<char>('\0');
  case Type::STRING:
    return std::string{};
  }
  return false;
}

bool truthy(const ScalarValue &value) {
  return std::visit(
      [](const auto &v) -> bool {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
          return v;
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
          return v != 0;
        } else if constexpr (std::is_same_v<T, double>) {
          return v != 0.0;
        } else if constexpr (std::is_same_v<T, char>) {
          return v != '\0';
        } else if constexpr (std::is_same_v<T, std::string>) {
          return !v.empty();
        } else {
          return false;
        }
      },
      value);
}

std::optional<long double> as_number(const ScalarValue &value) {
  return std::visit(
      [](const auto &v) -> std::optional<long double> {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
          return v ? 1.0L : 0.0L;
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
          return static_cast<long double>(v);
        } else if constexpr (std::is_same_v<T, double>) {
          return static_cast<long double>(v);
        } else if constexpr (std::is_same_v<T, char>) {
          return static_cast<long double>(static_cast<unsigned char>(v));
        } else {
          return std::nullopt;
        }
      },
      value);
}

bool compare_values(const ScalarValue &lhs, const ScalarValue &rhs, const aion::frontend::CompOp op) {
  if (std::holds_alternative<std::string>(lhs) && std::holds_alternative<std::string>(rhs)) {
    const std::string &l = std::get<std::string>(lhs);
    const std::string &r = std::get<std::string>(rhs);
    switch (op) {
    case aion::frontend::CompOp::EQ:
      return l == r;
    case aion::frontend::CompOp::NEQ:
      return l != r;
    case aion::frontend::CompOp::LT:
      return l < r;
    case aion::frontend::CompOp::LE:
      return l <= r;
    case aion::frontend::CompOp::GT:
      return l > r;
    case aion::frontend::CompOp::GE:
      return l >= r;
    }
  }

  const auto lhs_num = as_number(lhs);
  const auto rhs_num = as_number(rhs);
  if (!lhs_num.has_value() || !rhs_num.has_value()) {
    return false;
  }

  switch (op) {
  case aion::frontend::CompOp::EQ:
    return lhs_num.value() == rhs_num.value();
  case aion::frontend::CompOp::NEQ:
    return lhs_num.value() != rhs_num.value();
  case aion::frontend::CompOp::LT:
    return lhs_num.value() < rhs_num.value();
  case aion::frontend::CompOp::LE:
    return lhs_num.value() <= rhs_num.value();
  case aion::frontend::CompOp::GT:
    return lhs_num.value() > rhs_num.value();
  case aion::frontend::CompOp::GE:
    return lhs_num.value() >= rhs_num.value();
  }
  return false;
}

class CppSemanticOracle {
public:
  CppSemanticOracle(const ParsedProgram &program, const std::vector<EventSample> &trace) : program_(program), trace_(trace) {
    for (const PredDecl &pred : program_.ast->predicates) {
      pred_map_.emplace(std::string(pred.name), &pred);
    }
    for (const auto &field : program_.ast->event.fields) {
      field_types_.emplace(std::string(field.name), field.type);
    }
  }

  TimelineMap evaluate_all() {
    TimelineMap out;
    for (const RegexDecl &regex : program_.ast->regexes) {
      regex_cache_.clear();
      out.emplace(std::string(regex.name), evaluate_regex_timeline(regex));
    }
    return out;
  }

private:
  const ParsedProgram &program_;
  const std::vector<EventSample> &trace_;
  std::unordered_map<std::string, const PredDecl *> pred_map_;
  std::unordered_map<std::string, Type> field_types_;
  std::unordered_map<PredCacheKey, bool, PredCacheKeyHash> pred_cache_;
  std::unordered_map<RegexCacheKey, bool, RegexCacheKeyHash> regex_cache_;

  ScalarValue literal_to_scalar(const aion::frontend::Literal &literal) const {
    switch (literal.type) {
    case Type::BOOL:
      return std::get<bool>(literal.value);
    case Type::INT:
      return static_cast<std::int64_t>(std::get<int>(literal.value));
    case Type::FLOAT:
      return static_cast<double>(std::get<float>(literal.value));
    case Type::CHAR:
      return std::get<char>(literal.value);
    case Type::STRING:
      return std::string(std::get<std::string_view>(literal.value));
    }
    return false;
  }

  const ScalarValue &event_field_value(std::string_view field, const std::size_t event_idx) const {
    const std::string key(field);
    const auto it_event = trace_.at(event_idx).find(key);
    if (it_event == trace_.at(event_idx).end()) {
      throw std::runtime_error("Event missing field: " + key);
    }
    return it_event->second;
  }

  ScalarValue eval_pred_ref_value(const PredRefExpr &pred_ref, const std::size_t event_idx,
                                  std::unordered_set<std::string> &visiting) {
    const aion::frontend::Symbol *symbol = program_.symbols.resolve(pred_ref.name);
    if (symbol == nullptr) {
      return false;
    }
    if (symbol->kind == SymbolKind::EVENT_FIELD) {
      return event_field_value(pred_ref.name, event_idx);
    }
    if (symbol->kind == SymbolKind::PREDICATE) {
      return eval_predicate_by_name(pred_ref.name, event_idx, visiting);
    }
    return false;
  }

  ScalarValue eval_primary_value(const PrimaryPredExpr &primary, const std::size_t event_idx,
                                 std::unordered_set<std::string> &visiting) {
    return std::visit(
        [this, event_idx, &visiting](const auto &arg) -> ScalarValue {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, aion::frontend::Literal>) {
            return literal_to_scalar(arg);
          } else if constexpr (std::is_same_v<T, PredRefExpr>) {
            return eval_pred_ref_value(arg, event_idx, visiting);
          } else if constexpr (std::is_same_v<T, std::unique_ptr<PredExpr>>) {
            return eval_pred_expr_bool(*arg, event_idx, visiting);
          } else {
            return false;
          }
        },
        primary.expr);
  }

  ScalarValue eval_pred_expr_value(const PredExpr &expr, const std::size_t event_idx,
                                   std::unordered_set<std::string> &visiting) {
    if (const auto *primary = dynamic_cast<const PrimaryPredExpr *>(&expr)) {
      return eval_primary_value(*primary, event_idx, visiting);
    }
    if (const auto *pred_ref = dynamic_cast<const PredRefExpr *>(&expr)) {
      return eval_pred_ref_value(*pred_ref, event_idx, visiting);
    }
    return eval_pred_expr_bool(expr, event_idx, visiting);
  }

  bool eval_pred_expr_bool(const PredExpr &expr, const std::size_t event_idx, std::unordered_set<std::string> &visiting) {
    if (const auto *and_expr = dynamic_cast<const AndPredExpr *>(&expr)) {
      return std::ranges::all_of(and_expr->terms, [this, event_idx, &visiting](const auto &term) {
        return eval_pred_expr_bool(*term, event_idx, visiting);
      });
    }
    if (const auto *or_expr = dynamic_cast<const OrPredExpr *>(&expr)) {
      return std::ranges::any_of(or_expr->terms, [this, event_idx, &visiting](const auto &term) {
        return eval_pred_expr_bool(*term, event_idx, visiting);
      });
    }
    if (const auto *not_expr = dynamic_cast<const NotExpr *>(&expr)) {
      return !eval_pred_expr_bool(*not_expr->inner, event_idx, visiting);
    }
    if (const auto *comp_expr = dynamic_cast<const CompPredExpr *>(&expr)) {
      const ScalarValue lhs = eval_pred_expr_value(*comp_expr->lhs, event_idx, visiting);
      const ScalarValue rhs = eval_pred_expr_value(*comp_expr->rhs, event_idx, visiting);
      return compare_values(lhs, rhs, comp_expr->op);
    }
    if (const auto *primary = dynamic_cast<const PrimaryPredExpr *>(&expr)) {
      return truthy(eval_primary_value(*primary, event_idx, visiting));
    }
    if (const auto *pred_ref = dynamic_cast<const PredRefExpr *>(&expr)) {
      return truthy(eval_pred_ref_value(*pred_ref, event_idx, visiting));
    }
    return false;
  }

  bool eval_predicate_by_name(const std::string_view pred_name, const std::size_t event_idx,
                              std::unordered_set<std::string> &visiting) {
    PredCacheKey key{std::string(pred_name), event_idx};
    const auto cache_it = pred_cache_.find(key);
    if (cache_it != pred_cache_.end()) {
      return cache_it->second;
    }

    if (visiting.contains(std::string(pred_name))) {
      return false;
    }
    const auto it = pred_map_.find(std::string(pred_name));
    if (it == pred_map_.end()) {
      return false;
    }

    visiting.insert(std::string(pred_name));
    const bool value = eval_pred_expr_bool(*it->second->expr, event_idx, visiting);
    visiting.erase(std::string(pred_name));
    pred_cache_.emplace(std::move(key), value);
    return value;
  }

  bool regex_token_matches(std::string_view token_name, const std::size_t event_idx) {
    if (token_name == "_") {
      return true;
    }
    std::unordered_set<std::string> visiting;
    return eval_predicate_by_name(token_name, event_idx, visiting);
  }

  bool match_concat(const std::vector<std::unique_ptr<RegexExpr>> &parts, const std::size_t idx, const std::size_t pos,
                    const std::size_t end) {
    if (idx == parts.size()) {
      return pos == end;
    }
    for (std::size_t split = pos; split <= end; ++split) {
      if (match_expr(*parts[idx], pos, split) && match_concat(parts, idx + 1U, split, end)) {
        return true;
      }
    }
    return false;
  }

  bool match_expr(const RegexExpr &expr, const std::size_t l, const std::size_t r) {
    const RegexCacheKey key{&expr, l, r};
    const auto cache_it = regex_cache_.find(key);
    if (cache_it != regex_cache_.end()) {
      return cache_it->second;
    }

    bool matched = false;
    if (dynamic_cast<const RegexWildcard *>(&expr) != nullptr) {
      matched = (r == (l + 1U));
    } else if (const auto *ref_expr = dynamic_cast<const RegexRefExpr *>(&expr)) {
      matched = (r == (l + 1U) && regex_token_matches(ref_expr->regex_ref_expr, l));
    } else if (const auto *primary = dynamic_cast<const RegexPrimary *>(&expr)) {
      matched = std::visit(
          [this, l, r](const auto &arg) -> bool {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string_view>) {
              return (r == (l + 1U)) && regex_token_matches(arg, l);
            } else if constexpr (std::is_same_v<T, RegexWildcard>) {
              return (r == (l + 1U));
            } else if constexpr (std::is_same_v<T, std::unique_ptr<RegexExpr>>) {
              return match_expr(*arg, l, r);
            } else {
              return false;
            }
          },
          primary->expr);
    } else if (const auto *union_expr = dynamic_cast<const RegexUnion *>(&expr)) {
      matched = std::ranges::any_of(union_expr->options, [this, l, r](const auto &option) {
        return match_expr(*option, l, r);
      });
    } else if (const auto *concat_expr = dynamic_cast<const RegexConcat *>(&expr)) {
      matched = match_concat(concat_expr->sequence, 0U, l, r);
    } else if (const auto *star_expr = dynamic_cast<const RegexStar *>(&expr)) {
      if (l == r) {
        matched = true;
      } else {
        for (std::size_t mid = l + 1U; mid <= r; ++mid) {
          if (match_expr(*star_expr->inner, l, mid) && match_expr(expr, mid, r)) {
            matched = true;
            break;
          }
        }
      }
    }

    regex_cache_.emplace(key, matched);
    return matched;
  }

  std::vector<bool> evaluate_regex_timeline(const RegexDecl &regex) {
    std::vector<bool> timeline(trace_.size(), false);

    for (std::size_t end = 0; end < trace_.size(); ++end) {
      bool matched = false;
      for (std::size_t start = 0; start <= end; ++start) {
        if (match_expr(*regex.expr, start, end + 1U)) {
          matched = true;
          break;
        }
      }
      timeline[end] = matched;
    }
    return timeline;
  }
};

class Sandbox {
public:
  Sandbox() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    dir_ = std::filesystem::temp_directory_path() / ("aion_semantic_" + std::to_string(stamp));
    std::filesystem::create_directories(dir_);
  }

  ~Sandbox() {
    if (std::getenv("AION_SEMANTIC_KEEP_TMP") != nullptr) {
      std::cerr << "Preserving semantic sandbox: " << dir_ << '\n';
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

ParsedProgram parse_program_from_source(std::string source) {
  ParsedProgram parsed;
  parsed.source = std::move(source);
  parsed.ctxt.options.verbosity = Verbosity::NONE;
  parsed.ctxt.diagnostics.set_source_location(parsed.source);
  Lexer lexer(parsed.source, parsed.ctxt);
  parsed.tokens = lexer.tokenize();
  Parser parser(parsed.tokens, parsed.ctxt);
  parsed.ast = parser.parse();
  parsed.symbols = aion::frontend::generate_symbol_table(*parsed.ast, parsed.ctxt);
  return parsed;
}

ParsedProgram parse_program_from_file(const std::filesystem::path &source_path) {
  return parse_program_from_source(read_file(source_path));
}

std::vector<bool> parse_bits(std::string_view bits) {
  std::vector<bool> out;
  out.reserve(bits.size());
  for (const char ch : bits) {
    out.push_back(ch == '1');
  }
  return out;
}

TimelineMap parse_timeline_lines(const std::string &text) {
  TimelineMap timelines;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }
    const auto colon = line.find(':');
    if (colon == std::string::npos) {
      continue;
    }
    const std::string name = line.substr(0, colon);
    const std::string bits = line.substr(colon + 1U);
    timelines.emplace(name, parse_bits(bits));
  }
  return timelines;
}

void expect_timelines_equal(const TimelineMap &expected, const TimelineMap &actual, std::string_view context) {
  ASSERT_EQ(actual.size(), expected.size()) << context;
  for (const auto &[name, exp_bits] : expected) {
    const auto it = actual.find(name);
    ASSERT_NE(it, actual.end()) << context << " missing regex timeline for '" << name << "'";
    EXPECT_EQ(it->second, exp_bits) << context << " regex='" << name << "'";
  }
}

TimelineMap run_python_oracle(const SemanticFixture &fixture, const Sandbox &sandbox) {
  const std::filesystem::path script = python_oracle_path();
  if (!std::filesystem::exists(script)) {
    throw std::runtime_error("Python oracle script missing");
  }

  const std::filesystem::path trace_file = sandbox.file(fixture.name + "_trace.json");
  {
    std::ofstream out(trace_file);
    out << "[";
    for (std::size_t i = 0; i < fixture.trace.size(); ++i) {
      if (i > 0U) {
        out << ",";
      }
      out << "{";
      std::size_t field_idx = 0U;
      for (const auto &[name, value] : fixture.trace[i]) {
        if (field_idx++ > 0U) {
          out << ",";
        }
        out << "\"" << json_escape(name) << "\":" << scalar_to_json(value);
      }
      out << "}";
    }
    out << "]";
  }

  const std::filesystem::path stdout_file = sandbox.file(fixture.name + "_py.stdout");
  const std::filesystem::path stderr_file = sandbox.file(fixture.name + "_py.stderr");
  const std::string cmd = "python3 " + shell_quote(script.string()) + " --source " + shell_quote(fixture.source_path.string()) +
                          " --trace " + shell_quote(trace_file.string());
  const CommandResult run = run_shell_command(cmd, stdout_file, stderr_file);
  if (run.exit_code != 0) {
    throw std::runtime_error("Python oracle failed: " + run.stderr_text);
  }
  return parse_timeline_lines(run.stdout_text);
}

TimelineMap run_generated_runtime(const SemanticFixture &fixture, const ParsedProgram &program, const Sandbox &sandbox,
                                  const bool replay_with_reset = false) {
  const std::filesystem::path aionc = resolve_aionc_path();
  if (!std::filesystem::exists(aionc)) {
    throw std::runtime_error("aionc binary not found");
  }

  const std::filesystem::path out_base = sandbox.file(fixture.name + "_generated");
  const std::filesystem::path compile_stdout = sandbox.file(fixture.name + "_aionc.stdout");
  const std::filesystem::path compile_stderr = sandbox.file(fixture.name + "_aionc.stderr");

  const std::string aionc_cmd = shell_quote(aionc.string()) + " " + shell_quote(fixture.source_path.string()) + " -V0 -o " +
                                shell_quote(out_base.string());
  const CommandResult compile_run = run_shell_command(aionc_cmd, compile_stdout, compile_stderr);
  if (compile_run.exit_code != 0) {
    throw std::runtime_error("aionc invocation failed");
  }

  const std::filesystem::path header = out_base.string() + ".hpp";
  if (!std::filesystem::exists(header)) {
    throw std::runtime_error("Generated header missing");
  }

  std::vector<std::string> regex_names;
  regex_names.reserve(program.ast->regexes.size());
  for (const auto &regex : program.ast->regexes) {
    regex_names.emplace_back(regex.name);
  }

  const std::filesystem::path driver = sandbox.file(fixture.name + "_driver.cpp");
  {
    std::ofstream out(driver);
    out << "#include <cstddef>\n";
    out << "#include <iostream>\n";
    out << "#include <string>\n";
    out << "#include " << "\"" << header.string() << "\"\n\n";
    out << "int main() {\n";

    for (const auto &name : regex_names) {
      out << "  aion::runtime::Engine_" << name << " engine_" << name << ";\n";
      out << "  std::string out_" << name << ";\n";
      if (replay_with_reset) {
        out << "  std::string out_" << name << "_reset1;\n";
        out << "  std::string out_" << name << "_reset2;\n";
      }
    }

    out << "  for (std::size_t i = 0; i < " << fixture.trace.size() << "; ++i) {\n";
    out << "    aion::runtime::Event event{};\n";
    for (std::size_t i = 0; i < fixture.trace.size(); ++i) {
      out << "    if (i == " << i << ") {\n";
      for (const auto &field : program.ast->event.fields) {
        const std::string field_name(field.name);
        const auto it = fixture.trace[i].find(field_name);
        const ScalarValue value =
            (it != fixture.trace[i].end()) ? it->second : default_value_for_type(field.type);
        out << "      event." << field_name << " = " << scalar_to_cpp_literal(value) << ";\n";
      }
      out << "    }\n";
    }
    for (const auto &name : regex_names) {
      out << "    out_" << name << ".push_back(engine_" << name << ".process_event(event) ? '1' : '0');\n";
    }
    out << "  }\n";

    if (replay_with_reset) {
      for (const auto &name : regex_names) {
        out << "  engine_" << name << ".reset();\n";
      }
      out << "  for (std::size_t i = 0; i < " << fixture.trace.size() << "; ++i) {\n";
      out << "    aion::runtime::Event event{};\n";
      for (std::size_t i = 0; i < fixture.trace.size(); ++i) {
        out << "    if (i == " << i << ") {\n";
        for (const auto &field : program.ast->event.fields) {
          const std::string field_name(field.name);
          const auto it = fixture.trace[i].find(field_name);
          const ScalarValue value =
              (it != fixture.trace[i].end()) ? it->second : default_value_for_type(field.type);
          out << "      event." << field_name << " = " << scalar_to_cpp_literal(value) << ";\n";
        }
        out << "    }\n";
      }
      for (const auto &name : regex_names) {
        out << "    out_" << name << "_reset1.push_back(engine_" << name << ".process_event(event) ? '1' : '0');\n";
      }
      out << "  }\n";

      for (const auto &name : regex_names) {
        out << "  engine_" << name << ".reset();\n";
      }
      out << "  for (std::size_t i = 0; i < " << fixture.trace.size() << "; ++i) {\n";
      out << "    aion::runtime::Event event{};\n";
      for (std::size_t i = 0; i < fixture.trace.size(); ++i) {
        out << "    if (i == " << i << ") {\n";
        for (const auto &field : program.ast->event.fields) {
          const std::string field_name(field.name);
          const auto it = fixture.trace[i].find(field_name);
          const ScalarValue value =
              (it != fixture.trace[i].end()) ? it->second : default_value_for_type(field.type);
          out << "      event." << field_name << " = " << scalar_to_cpp_literal(value) << ";\n";
        }
        out << "    }\n";
      }
      for (const auto &name : regex_names) {
        out << "    out_" << name << "_reset2.push_back(engine_" << name << ".process_event(event) ? '1' : '0');\n";
      }
      out << "  }\n";
    }

    for (const auto &name : regex_names) {
      out << "  std::cout << \"" << name << ":\" << out_" << name << " << \"\\n\";\n";
      if (replay_with_reset) {
        out << "  std::cout << \"" << name << "__reset1:\" << out_" << name << "_reset1 << \"\\n\";\n";
        out << "  std::cout << \"" << name << "__reset2:\" << out_" << name << "_reset2 << \"\\n\";\n";
      }
    }
    out << "  return 0;\n";
    out << "}\n";
  }

  const std::string cxx = std::getenv("CXX") != nullptr ? std::string(std::getenv("CXX")) : "c++";
  const std::filesystem::path driver_bin = sandbox.file(fixture.name + "_driver");
  const std::filesystem::path cxx_stdout = sandbox.file(fixture.name + "_cxx.stdout");
  const std::filesystem::path cxx_stderr = sandbox.file(fixture.name + "_cxx.stderr");

  const std::string cxx_cmd =
      shell_quote(cxx) + " -std=c++23 " + shell_quote(driver.string()) + " -O0 -o " + shell_quote(driver_bin.string());
  const CommandResult cxx_run = run_shell_command(cxx_cmd, cxx_stdout, cxx_stderr);
  if (cxx_run.exit_code != 0) {
    throw std::runtime_error("Driver compile failed: " + cxx_run.stderr_text);
  }

  const std::filesystem::path run_stdout = sandbox.file(fixture.name + "_driver.stdout");
  const std::filesystem::path run_stderr = sandbox.file(fixture.name + "_driver.stderr");
  const CommandResult run = run_shell_command(shell_quote(driver_bin.string()), run_stdout, run_stderr);
  if (run.exit_code != 0) {
    throw std::runtime_error("Driver run failed");
  }
  return parse_timeline_lines(run.stdout_text);
}

std::vector<SemanticFixture> semantic_fixtures() {
  const auto root = semantic_cases_root();
  return {
      {
          .name = "basic_concat_star",
          .source_path = root / "basic_concat_star.regex",
          .trace = {
              {{"x", static_cast<std::int64_t>(0)}, {"ok", false}},
              {{"x", static_cast<std::int64_t>(1)}, {"ok", false}},
              {{"x", static_cast<std::int64_t>(0)}, {"ok", true}},
              {{"x", static_cast<std::int64_t>(0)}, {"ok", true}},
              {{"x", static_cast<std::int64_t>(1)}, {"ok", false}},
              {{"x", static_cast<std::int64_t>(0)}, {"ok", false}},
          },
          .expected = {{"R", {false, true, true, true, true, false}}},
      },
      {
          .name = "union_basic",
          .source_path = root / "union_basic.regex",
          .trace = {
              {{"x", static_cast<std::int64_t>(0)}},
              {{"x", static_cast<std::int64_t>(1)}},
              {{"x", static_cast<std::int64_t>(2)}},
              {{"x", static_cast<std::int64_t>(3)}},
          },
          .expected = {{"R", {false, true, true, false}}},
      },
      {
          .name = "predicate_precedence",
          .source_path = root / "predicate_precedence.regex",
          .trace = {
              {{"a", false}, {"b", false}, {"c", false}},
              {{"a", false}, {"b", true}, {"c", true}},
              {{"a", false}, {"b", true}, {"c", false}},
              {{"a", true}, {"b", false}, {"c", true}},
          },
          .expected = {{"R", {false, false, true, true}}},
      },
      {
          .name = "wildcard_concat",
          .source_path = root / "wildcard_concat.regex",
          .trace = {
              {{"x", static_cast<std::int64_t>(0)}},
              {{"x", static_cast<std::int64_t>(1)}},
              {{"x", static_cast<std::int64_t>(0)}},
              {{"x", static_cast<std::int64_t>(1)}},
          },
          .expected = {{"R", {false, true, false, true}}},
      },
      {
          .name = "nullable_star",
          .source_path = root / "nullable_star.regex",
          .trace = {
              {{"x", static_cast<std::int64_t>(0)}},
              {{"x", static_cast<std::int64_t>(0)}},
              {{"x", static_cast<std::int64_t>(1)}},
              {{"x", static_cast<std::int64_t>(0)}},
          },
          .expected = {{"R", {false, false, true, false}}},
      },
      {
          .name = "multi_regex",
          .source_path = root / "multi_regex.regex",
          .trace = {
              {{"x", static_cast<std::int64_t>(0)}},
              {{"x", static_cast<std::int64_t>(1)}},
              {{"x", static_cast<std::int64_t>(0)}},
          },
          .expected = {{"R1", {false, false, true}}, {"R2", {true, true, true}}},
      },
  };
}

bool python3_available() {
  const int rc = std::system("python3 --version > /dev/null 2>&1");
  return decode_exit_code(rc) == 0;
}

TEST(SemanticValidityTests, HumanTruthTablesMatchCppOracle) {
  for (const SemanticFixture &fixture : semantic_fixtures()) {
    SCOPED_TRACE(fixture.name);
    ASSERT_TRUE(std::filesystem::exists(fixture.source_path));

    ParsedProgram program = parse_program_from_file(fixture.source_path);
    ASSERT_EQ(program.ctxt.diagnostics.get_error_count(), 0U);

    CppSemanticOracle oracle(program, fixture.trace);
    const TimelineMap actual = oracle.evaluate_all();
    expect_timelines_equal(fixture.expected, actual, "cpp-oracle-vs-human");
  }
}

TEST(SemanticValidityTests, PythonOracleMatchesCppOracle) {
  if (!python3_available()) {
    GTEST_SKIP() << "python3 unavailable; skipping Python oracle differential test";
  }

  for (const SemanticFixture &fixture : semantic_fixtures()) {
    SCOPED_TRACE(fixture.name);
    ParsedProgram program = parse_program_from_file(fixture.source_path);
    ASSERT_EQ(program.ctxt.diagnostics.get_error_count(), 0U);

    CppSemanticOracle cpp_oracle(program, fixture.trace);
    const TimelineMap cpp_out = cpp_oracle.evaluate_all();

    Sandbox sandbox;
    const TimelineMap py_out = run_python_oracle(fixture, sandbox);
    expect_timelines_equal(cpp_out, py_out, "python-oracle-vs-cpp-oracle");
  }
}

TEST(SemanticValidityTests, RuntimeMatchesOracleForAllFixtures) {
  for (const SemanticFixture &fixture : semantic_fixtures()) {
    SCOPED_TRACE(fixture.name);
    ParsedProgram program = parse_program_from_file(fixture.source_path);
    ASSERT_EQ(program.ctxt.diagnostics.get_error_count(), 0U);

    CppSemanticOracle oracle(program, fixture.trace);
    const TimelineMap expected = oracle.evaluate_all();

    Sandbox sandbox;
    const TimelineMap runtime = run_generated_runtime(fixture, program, sandbox);
    expect_timelines_equal(expected, runtime, "runtime-vs-oracle");
  }
}

TEST(SemanticValidityTests, RuntimeResetReplaysTraceDeterministically) {
  for (const SemanticFixture &fixture : semantic_fixtures()) {
    SCOPED_TRACE(fixture.name);
    ParsedProgram program = parse_program_from_file(fixture.source_path);
    ASSERT_EQ(program.ctxt.diagnostics.get_error_count(), 0U);

    Sandbox sandbox;
    const TimelineMap runtime = run_generated_runtime(fixture, program, sandbox, true);
    for (const auto &regex : program.ast->regexes) {
      const std::string name(regex.name);
      const std::string replay_name_1 = name + "__reset1";
      const std::string replay_name_2 = name + "__reset2";
      ASSERT_TRUE(runtime.contains(name));
      ASSERT_TRUE(runtime.contains(replay_name_1));
      ASSERT_TRUE(runtime.contains(replay_name_2));
      EXPECT_EQ(runtime.at(name), runtime.at(replay_name_1));
      EXPECT_EQ(runtime.at(name), runtime.at(replay_name_2));
      EXPECT_EQ(runtime.at(replay_name_1), runtime.at(replay_name_2));
    }
  }
}

TEST(SemanticValidityTests, MetamorphicUnionCommutativityHoldsInOracle) {
  const std::string src_a =
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "regex R = A | B;\n";
  const std::string src_b =
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "regex R = B | A;\n";

  const std::vector<EventSample> trace = {
      {{"x", static_cast<std::int64_t>(0)}},
      {{"x", static_cast<std::int64_t>(1)}},
      {{"x", static_cast<std::int64_t>(2)}},
      {{"x", static_cast<std::int64_t>(3)}},
  };

  ParsedProgram prog_a = parse_program_from_source(src_a);
  ParsedProgram prog_b = parse_program_from_source(src_b);
  ASSERT_EQ(prog_a.ctxt.diagnostics.get_error_count(), 0U);
  ASSERT_EQ(prog_b.ctxt.diagnostics.get_error_count(), 0U);

  CppSemanticOracle oracle_a(prog_a, trace);
  CppSemanticOracle oracle_b(prog_b, trace);
  const TimelineMap out_a = oracle_a.evaluate_all();
  const TimelineMap out_b = oracle_b.evaluate_all();

  ASSERT_TRUE(out_a.contains("R"));
  ASSERT_TRUE(out_b.contains("R"));
  EXPECT_EQ(out_a.at("R"), out_b.at("R"));
}

TEST(SemanticValidityTests, MetamorphicStarIdempotenceHoldsInOracle) {
  const std::string src_a =
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "regex R = A*;\n";
  const std::string src_b =
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "regex R = (A*)*;\n";

  const std::vector<EventSample> trace = {
      {{"x", static_cast<std::int64_t>(0)}},
      {{"x", static_cast<std::int64_t>(1)}},
      {{"x", static_cast<std::int64_t>(0)}},
  };

  ParsedProgram prog_a = parse_program_from_source(src_a);
  ParsedProgram prog_b = parse_program_from_source(src_b);
  ASSERT_EQ(prog_a.ctxt.diagnostics.get_error_count(), 0U);
  ASSERT_EQ(prog_b.ctxt.diagnostics.get_error_count(), 0U);

  CppSemanticOracle oracle_a(prog_a, trace);
  CppSemanticOracle oracle_b(prog_b, trace);
  const TimelineMap out_a = oracle_a.evaluate_all();
  const TimelineMap out_b = oracle_b.evaluate_all();

  ASSERT_TRUE(out_a.contains("R"));
  ASSERT_TRUE(out_b.contains("R"));
  EXPECT_EQ(out_a.at("R"), out_b.at("R"));
}

} // namespace
