#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

import aion.analysis;
import aion.automata;
import aion.core;
import aion.frontend;

namespace {

using aion::automata::Generic_NFA;
using aion::core::CompilationContext;
using aion::core::Type;
using aion::core::Verbosity;
using aion::frontend::FieldMetadata;
using aion::frontend::Lexer;
using aion::frontend::Parser;
using aion::frontend::RegexMetadata;
using aion::frontend::SymbolKind;
using aion::frontend::SymbolTable;
using aion::frontend::Token;
using aion::frontend::TokenType;

struct PipelineResult {
  std::string source;
  CompilationContext ctxt;
  std::vector<Token> tokens;
  std::unique_ptr<aion::frontend::AionFile> ast;
  SymbolTable symbols;
  std::unordered_map<std::string_view, Generic_NFA> nfas;
};

std::string with_minimal_event_if_missing(std::string source) {
  if (source.find("event") == std::string::npos) {
    return "event { int contractField; };\n" + source;
  }
  return source;
}

PipelineResult build_pipeline(std::string source, bool run_analysis_and_automata = true, bool inject_minimal_event = true) {
  PipelineResult result;
  result.source = inject_minimal_event ? with_minimal_event_if_missing(std::move(source)) : std::move(source);
  result.ctxt.options.verbosity = Verbosity::NONE;
  result.ctxt.diagnostics.set_source_location(result.source);

  Lexer lexer(result.source, result.ctxt);
  result.tokens = lexer.tokenize();
  Parser parser(result.tokens, result.ctxt);
  result.ast = parser.parse();

  if (run_analysis_and_automata) {
    result.symbols = aion::frontend::generate_symbol_table(*result.ast, result.ctxt);
    aion::analysis::fill_pos_ids(result.symbols, *result.ast, result.ctxt);
    result.nfas = aion::automata::convert_to_generic_nfa(*result.ast, result.symbols, result.ctxt);
  }
  return result;
}

std::size_t count_token_type(const std::vector<Token> &tokens, TokenType type) {
  return static_cast<std::size_t>(
      std::count_if(tokens.begin(), tokens.end(), [type](const Token &token) { return token.type == type; }));
}

const aion::frontend::RegexDecl *find_regex_decl(const PipelineResult &result, std::string_view name) {
  for (const auto &regex : result.ast->regexes) {
    if (regex.name == name) {
      return &regex;
    }
  }
  return nullptr;
}

const RegexMetadata *regex_meta(const SymbolTable &table, std::string_view regex_name) {
  const auto *sym = table.resolve(regex_name);
  if (sym == nullptr || sym->kind != SymbolKind::REGEX) {
    return nullptr;
  }
  return &std::get<RegexMetadata>(sym->details);
}

const FieldMetadata *field_meta(const SymbolTable &table, std::string_view name) {
  const auto *sym = table.resolve(name);
  if (sym == nullptr || sym->kind != SymbolKind::EVENT_FIELD) {
    return nullptr;
  }
  return &std::get<FieldMetadata>(sym->details);
}

std::optional<std::size_t> find_first_token_index(const std::vector<Token> &tokens, TokenType type) {
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    if (tokens[i].type == type) {
      return i;
    }
  }
  return std::nullopt;
}

bool has_identifier_after_index(const std::vector<Token> &tokens, std::size_t index, std::string_view text) {
  for (std::size_t i = index + 1; i < tokens.size(); ++i) {
    if (tokens[i].type == TokenType::IDENTIFIER && tokens[i].text == text) {
      return true;
    }
  }
  return false;
}

std::set<std::uint16_t> collect_nfa_ids(const Generic_NFA &nfa) {
  std::set<std::uint16_t> ids;
  for (const auto id : nfa.first) {
    ids.insert(id);
  }
  for (const auto id : nfa.last) {
    ids.insert(id);
  }
  for (const auto &[from, to_set] : nfa.follow) {
    ids.insert(from);
    for (const auto to : to_set) {
      ids.insert(to);
    }
  }
  return ids;
}

} // namespace

TEST(ContractPipelineTests, ValidProgramProducesConsistentArtifactsAcrossStages) {
  PipelineResult result = build_pipeline(
      "event { int x; bool ok; };\n"
      "pred A = x == 1;\n"
      "pred B = ok == true;\n"
      "regex R1 = A;\n"
      "regex R2 = A . B*;\n");

  ASSERT_NE(result.ast, nullptr);
  EXPECT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  EXPECT_EQ(result.tokens.back().type, TokenType::END_OF_FILE);
  EXPECT_EQ(result.ast->predicates.size(), 2U);
  EXPECT_EQ(result.ast->regexes.size(), 2U);
  EXPECT_EQ(result.nfas.size(), result.ast->regexes.size());
  EXPECT_TRUE(result.nfas.contains("R1"));
  EXPECT_TRUE(result.nfas.contains("R2"));
}

TEST(ContractPipelineTests, NfaStatesReferenceOnlyAssignedPositionIds) {
  PipelineResult result = build_pipeline(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "regex R = (A | B) . A*;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  ASSERT_TRUE(result.nfas.contains("R"));
  const RegexMetadata *meta = regex_meta(result.symbols, "R");
  ASSERT_NE(meta, nullptr);

  const std::set<std::uint16_t> nfa_ids = collect_nfa_ids(result.nfas.at("R"));
  ASSERT_FALSE(nfa_ids.empty());
  for (const auto id : nfa_ids) {
    EXPECT_TRUE(meta->pos_ids_to_names.contains(id));
  }
}

TEST(ContractPipelineTests, ParserRecoveryKeepsLaterRegexAfterBrokenRegex) {
  PipelineResult result = build_pipeline(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "regex Bad = (A | );\n"
      "regex Good = A;\n",
      false);

  ASSERT_NE(result.ast, nullptr);
  EXPECT_GT(result.ctxt.diagnostics.get_error_count(), 0U);
  ASSERT_NE(find_regex_decl(result, "Good"), nullptr);
  EXPECT_EQ(result.ast->regexes.size(), 1U);
}

TEST(ContractPipelineTests, MissingEventBlockEmitsErrorContract) {
  PipelineResult result = build_pipeline(
      "pred A = true;\n"
      "regex R = A;\n",
      false, false);

  ASSERT_NE(result.ast, nullptr);
  EXPECT_GT(result.ctxt.diagnostics.get_error_count(), 0U);
}

TEST(ContractPipelineTests, StandaloneAmpersandDoesNotConsumeFollowingIdentifier) {
  PipelineResult result = build_pipeline("event { int x; };\npred P = x & y;\nregex R = P;\n", false);
  const auto error_index = find_first_token_index(result.tokens, TokenType::ERROR);
  ASSERT_TRUE(error_index.has_value());
  EXPECT_TRUE(has_identifier_after_index(result.tokens, error_index.value(), "y"));
}

TEST(ContractPipelineTests, UnsupportedSlashDoesNotConsumeFollowingIdentifier) {
  PipelineResult result = build_pipeline("event { int x; };\npred P = x / y;\nregex R = P;\n", false);
  const auto error_index = find_first_token_index(result.tokens, TokenType::ERROR);
  ASSERT_TRUE(error_index.has_value());
  EXPECT_TRUE(has_identifier_after_index(result.tokens, error_index.value(), "y"));
}

TEST(ContractPipelineTests, DuplicateFieldDeclarationPreservesFirstTypeContract) {
  PipelineResult result = build_pipeline(
      "event { int id; float id; };\n"
      "pred P = id == 1;\n"
      "regex R = P;\n");

  EXPECT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  EXPECT_EQ(result.ctxt.diagnostics.get_warning_count(), 1U);
  const FieldMetadata *id = field_meta(result.symbols, "id");
  ASSERT_NE(id, nullptr);
  EXPECT_EQ(id->type, Type::INT);
}

TEST(ContractPipelineTests, BooleanLiteralsAreTokenizedAndParsedWithoutErrors) {
  PipelineResult result = build_pipeline(
      "event { bool ok; };\n"
      "pred P = ok == true || ok == false;\n"
      "regex R = P;\n",
      false);

  EXPECT_EQ(count_token_type(result.tokens, TokenType::LIT_BOOL), 2U);
  EXPECT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
}

TEST(ContractPipelineTests, StarRegexProducesNullableNfa) {
  PipelineResult result = build_pipeline(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "regex R = A*;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  ASSERT_TRUE(result.nfas.contains("R"));
  EXPECT_TRUE(result.nfas.at("R").nullable);
}

TEST(ContractPipelineTests, NonStarRegexProducesNonNullableNfa) {
  PipelineResult result = build_pipeline(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "regex R = A;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  ASSERT_TRUE(result.nfas.contains("R"));
  EXPECT_FALSE(result.nfas.at("R").nullable);
}
