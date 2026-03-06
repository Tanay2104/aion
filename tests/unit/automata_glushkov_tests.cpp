#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
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
using aion::core::Verbosity;
using aion::frontend::Lexer;
using aion::frontend::Parser;
using aion::frontend::RegexMetadata;
using aion::frontend::SymbolTable;

struct AutomataResult {
  std::string source;
  CompilationContext ctxt;
  std::vector<aion::frontend::Token> tokens;
  std::unique_ptr<aion::frontend::AionFile> ast;
  SymbolTable symbols;
  std::unordered_map<std::string_view, Generic_NFA> nfas;
};

std::string with_minimal_event_if_missing(std::string source) {
  if (source.find("event") == std::string::npos) {
    return "event { int __test_event_field; };\n" + source;
  }
  return source;
}

AutomataResult build_automata(std::string source) {
  AutomataResult result;
  result.source = with_minimal_event_if_missing(std::move(source));
  result.ctxt.options.verbosity = Verbosity::NONE;
  result.ctxt.diagnostics.set_source_location(result.source);

  Lexer lexer(result.source, result.ctxt);
  result.tokens = lexer.tokenize();
  Parser parser(result.tokens, result.ctxt);
  result.ast = parser.parse();
  result.symbols = aion::frontend::generate_symbol_table(*result.ast, result.ctxt);
  aion::analysis::fill_pos_ids(result.symbols, *result.ast, result.ctxt);
  result.nfas = aion::automata::convert_to_generic_nfa(*result.ast, result.symbols, result.ctxt);
  return result;
}

const RegexMetadata *regex_meta(const SymbolTable &table, std::string_view regex_name) {
  const auto *sym = table.resolve(regex_name);
  if (sym == nullptr) {
    return nullptr;
  }
  return &std::get<RegexMetadata>(sym->details);
}

std::optional<std::uint16_t> find_id_for_name(const RegexMetadata &meta, std::string_view target_name) {
  for (const auto &[id, name] : meta.pos_ids_to_names) {
    if (name == target_name) {
      return id;
    }
  }
  return std::nullopt;
}

std::set<std::uint16_t> to_ordered_set(const std::unordered_set<std::uint16_t> &input) {
  return std::set<std::uint16_t>(input.begin(), input.end());
}

} // namespace

TEST(GlushkovTests, ConcatRegexProducesExpectedFollowRelation) {
  AutomataResult result = build_automata(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "regex R = A . B;\n");

  EXPECT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  ASSERT_TRUE(result.nfas.contains("R"));

  const Generic_NFA &nfa = result.nfas.at("R");
  const RegexMetadata *meta = regex_meta(result.symbols, "R");
  ASSERT_NE(meta, nullptr);
  const auto id_a = find_id_for_name(*meta, "A");
  const auto id_b = find_id_for_name(*meta, "B");
  ASSERT_TRUE(id_a.has_value());
  ASSERT_TRUE(id_b.has_value());

  EXPECT_FALSE(nfa.nullable);
  EXPECT_EQ(nfa.first.size(), 1U);
  EXPECT_EQ(nfa.last.size(), 1U);
  EXPECT_TRUE(nfa.first.contains(id_a.value()));
  EXPECT_TRUE(nfa.last.contains(id_b.value()));
  EXPECT_TRUE(nfa.follow.contains(id_a.value()));
  EXPECT_TRUE(nfa.follow.at(id_a.value()).contains(id_b.value()));
}

TEST(GlushkovTests, KleeneStarRegexIsNullableAndSelfLoops) {
  AutomataResult result = build_automata(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "regex R = A*;\n");

  EXPECT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  ASSERT_TRUE(result.nfas.contains("R"));

  const Generic_NFA &nfa = result.nfas.at("R");
  const RegexMetadata *meta = regex_meta(result.symbols, "R");
  ASSERT_NE(meta, nullptr);
  const auto id_a = find_id_for_name(*meta, "A");
  ASSERT_TRUE(id_a.has_value());

  EXPECT_TRUE(nfa.nullable);
  EXPECT_TRUE(nfa.first.contains(id_a.value()));
  EXPECT_TRUE(nfa.last.contains(id_a.value()));
  EXPECT_TRUE(nfa.follow.contains(id_a.value()));
  EXPECT_TRUE(nfa.follow.at(id_a.value()).contains(id_a.value()));
}

TEST(GlushkovTests, UnionRegexCombinesBoundariesWithoutFollowEdges) {
  AutomataResult result = build_automata(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "regex R = A | B;\n");

  EXPECT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  ASSERT_TRUE(result.nfas.contains("R"));

  const Generic_NFA &nfa = result.nfas.at("R");
  EXPECT_FALSE(nfa.nullable);
  EXPECT_EQ(nfa.first.size(), 2U);
  EXPECT_EQ(nfa.last.size(), 2U);
  EXPECT_TRUE(nfa.follow.empty());
}

TEST(GlushkovTests, SinglePredicateRegexIsNotNullable) {
  AutomataResult result = build_automata(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "regex R = A;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  ASSERT_TRUE(result.nfas.contains("R"));
  const Generic_NFA &nfa = result.nfas.at("R");
  EXPECT_FALSE(nfa.nullable);
  EXPECT_EQ(nfa.first.size(), 1U);
  EXPECT_EQ(nfa.last.size(), 1U);
}

TEST(GlushkovTests, WildcardRegexIsNotNullable) {
  AutomataResult result = build_automata(
      "event { int x; };\n"
      "regex R = _;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  ASSERT_TRUE(result.nfas.contains("R"));
  const Generic_NFA &nfa = result.nfas.at("R");
  EXPECT_FALSE(nfa.nullable);
  EXPECT_EQ(nfa.first.size(), 1U);
  EXPECT_EQ(nfa.last.size(), 1U);
}

TEST(GlushkovTests, UnionWithStarIsNullable) {
  AutomataResult result = build_automata(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "regex R = A | B*;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  ASSERT_TRUE(result.nfas.contains("R"));
  const Generic_NFA &nfa = result.nfas.at("R");
  EXPECT_TRUE(nfa.nullable);
}

TEST(GlushkovTests, ConcatOfStarsIsNullable) {
  AutomataResult result = build_automata(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "regex R = A* . B*;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  ASSERT_TRUE(result.nfas.contains("R"));
  EXPECT_TRUE(result.nfas.at("R").nullable);
}

TEST(GlushkovTests, ConcatWithRightStarAddsFollowToStarFirst) {
  AutomataResult result = build_automata(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "regex R = A . B*;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  const Generic_NFA &nfa = result.nfas.at("R");
  const RegexMetadata *meta = regex_meta(result.symbols, "R");
  ASSERT_NE(meta, nullptr);
  const auto id_a = find_id_for_name(*meta, "A");
  const auto id_b = find_id_for_name(*meta, "B");
  ASSERT_TRUE(id_a.has_value());
  ASSERT_TRUE(id_b.has_value());
  ASSERT_TRUE(nfa.follow.contains(id_a.value()));
  EXPECT_TRUE(nfa.follow.at(id_a.value()).contains(id_b.value()));
}

TEST(GlushkovTests, StarOverConcatCreatesLoopFromLastToFirst) {
  AutomataResult result = build_automata(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "regex R = (A . B)*;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  const Generic_NFA &nfa = result.nfas.at("R");
  const RegexMetadata *meta = regex_meta(result.symbols, "R");
  ASSERT_NE(meta, nullptr);
  const auto id_a = find_id_for_name(*meta, "A");
  const auto id_b = find_id_for_name(*meta, "B");
  ASSERT_TRUE(id_a.has_value());
  ASSERT_TRUE(id_b.has_value());

  ASSERT_TRUE(nfa.follow.contains(id_a.value()));
  ASSERT_TRUE(nfa.follow.contains(id_b.value()));
  EXPECT_TRUE(nfa.follow.at(id_a.value()).contains(id_b.value()));
  EXPECT_TRUE(nfa.follow.at(id_b.value()).contains(id_a.value()));
}

TEST(GlushkovTests, ConcatWithUnionBuildsExpectedFirstAndLastSets) {
  AutomataResult result = build_automata(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "pred C = x == 3;\n"
      "regex R = (A | B) . C;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  const Generic_NFA &nfa = result.nfas.at("R");
  const RegexMetadata *meta = regex_meta(result.symbols, "R");
  ASSERT_NE(meta, nullptr);
  const auto id_a = find_id_for_name(*meta, "A");
  const auto id_b = find_id_for_name(*meta, "B");
  const auto id_c = find_id_for_name(*meta, "C");
  ASSERT_TRUE(id_a.has_value());
  ASSERT_TRUE(id_b.has_value());
  ASSERT_TRUE(id_c.has_value());

  EXPECT_EQ(to_ordered_set(nfa.first), (std::set<std::uint16_t>{id_a.value(), id_b.value()}));
  EXPECT_EQ(to_ordered_set(nfa.last), (std::set<std::uint16_t>{id_c.value()}));
  ASSERT_TRUE(nfa.follow.contains(id_a.value()));
  ASSERT_TRUE(nfa.follow.contains(id_b.value()));
  EXPECT_TRUE(nfa.follow.at(id_a.value()).contains(id_c.value()));
  EXPECT_TRUE(nfa.follow.at(id_b.value()).contains(id_c.value()));
}

TEST(GlushkovTests, UnionOfConcatsKeepsFollowInsideEachBranch) {
  AutomataResult result = build_automata(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "pred C = x == 3;\n"
      "pred D = x == 4;\n"
      "regex R = (A . B) | (C . D);\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  const Generic_NFA &nfa = result.nfas.at("R");
  const RegexMetadata *meta = regex_meta(result.symbols, "R");
  ASSERT_NE(meta, nullptr);
  const auto id_a = find_id_for_name(*meta, "A");
  const auto id_b = find_id_for_name(*meta, "B");
  const auto id_c = find_id_for_name(*meta, "C");
  const auto id_d = find_id_for_name(*meta, "D");
  ASSERT_TRUE(id_a.has_value());
  ASSERT_TRUE(id_b.has_value());
  ASSERT_TRUE(id_c.has_value());
  ASSERT_TRUE(id_d.has_value());

  ASSERT_TRUE(nfa.follow.contains(id_a.value()));
  ASSERT_TRUE(nfa.follow.contains(id_c.value()));
  EXPECT_TRUE(nfa.follow.at(id_a.value()).contains(id_b.value()));
  EXPECT_TRUE(nfa.follow.at(id_c.value()).contains(id_d.value()));
}

TEST(GlushkovTests, WildcardInConcatParticipatesInFollow) {
  AutomataResult result = build_automata(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "regex R = _ . A;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  const Generic_NFA &nfa = result.nfas.at("R");
  const RegexMetadata *meta = regex_meta(result.symbols, "R");
  ASSERT_NE(meta, nullptr);
  ASSERT_EQ(meta->pos_ids_to_names.size(), 2U);

  std::optional<std::uint16_t> wildcard_id;
  std::optional<std::uint16_t> a_id;
  for (const auto &[id, name] : meta->pos_ids_to_names) {
    if (name == "_") {
      wildcard_id = id;
    } else if (name == "A") {
      a_id = id;
    }
  }
  ASSERT_TRUE(wildcard_id.has_value());
  ASSERT_TRUE(a_id.has_value());
  ASSERT_TRUE(nfa.follow.contains(wildcard_id.value()));
  EXPECT_TRUE(nfa.follow.at(wildcard_id.value()).contains(a_id.value()));
}

TEST(GlushkovTests, GeneratesNfaForEachRegexDeclaration) {
  AutomataResult result = build_automata(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "regex R1 = A;\n"
      "regex R2 = B*;\n"
      "regex R3 = A . B;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  EXPECT_EQ(result.nfas.size(), 3U);
  EXPECT_TRUE(result.nfas.contains("R1"));
  EXPECT_TRUE(result.nfas.contains("R2"));
  EXPECT_TRUE(result.nfas.contains("R3"));
}
