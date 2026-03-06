#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

import aion.analysis;
import aion.core;
import aion.frontend;

namespace {

using aion::core::CompilationContext;
using aion::core::Verbosity;
using aion::frontend::Lexer;
using aion::frontend::Parser;
using aion::frontend::RegexMetadata;
using aion::frontend::SymbolTable;

struct AnalysisResult {
  std::string source;
  CompilationContext ctxt;
  std::vector<aion::frontend::Token> tokens;
  std::unique_ptr<aion::frontend::AionFile> ast;
  SymbolTable symbols;
};

std::string with_minimal_event_if_missing(std::string source) {
  if (source.find("event") == std::string::npos) {
    return "event { int __test_event_field; };\n" + source;
  }
  return source;
}

AnalysisResult build_analysis(std::string source) {
  AnalysisResult result;
  result.source = with_minimal_event_if_missing(std::move(source));
  result.ctxt.options.verbosity = Verbosity::NONE;
  result.ctxt.diagnostics.set_source_location(result.source);

  Lexer lexer(result.source, result.ctxt);
  result.tokens = lexer.tokenize();
  Parser parser(result.tokens, result.ctxt);
  result.ast = parser.parse();
  result.symbols = aion::frontend::generate_symbol_table(*result.ast, result.ctxt);
  aion::analysis::fill_pos_ids(result.symbols, *result.ast, result.ctxt);
  return result;
}

const RegexMetadata *regex_meta(const SymbolTable &table, std::string_view regex_name) {
  const auto *sym = table.resolve(regex_name);
  if (sym == nullptr) {
    return nullptr;
  }
  return &std::get<RegexMetadata>(sym->details);
}

} // namespace

TEST(AlphabetTests, AssignsUniquePosIdsForRepeatedTermsAndWildcards) {
  AnalysisResult result = build_analysis(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "regex R = (A | B) . _ . A*;\n");

  EXPECT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  const RegexMetadata *meta = regex_meta(result.symbols, "R");
  ASSERT_NE(meta, nullptr);

  EXPECT_EQ(meta->node_to_pos_ids.size(), 4U);
  EXPECT_EQ(meta->pos_ids_to_names.size(), 4U);

  std::set<std::uint16_t> unique_ids;
  std::size_t a_count = 0;
  std::size_t b_count = 0;
  std::size_t wildcard_count = 0;
  for (const auto &[id, name] : meta->pos_ids_to_names) {
    unique_ids.insert(id);
    if (name == "A") {
      ++a_count;
    } else if (name == "B") {
      ++b_count;
    } else if (name == "_") {
      ++wildcard_count;
    }
  }

  EXPECT_EQ(unique_ids.size(), 4U);
  EXPECT_EQ(a_count, 2U);
  EXPECT_EQ(b_count, 1U);
  EXPECT_EQ(wildcard_count, 1U);
}

TEST(AlphabetTests, UsesIndependentPosIdsPerRegex) {
  AnalysisResult result = build_analysis(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "regex R1 = A . B;\n"
      "regex R2 = A | _;\n");

  EXPECT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);

  const RegexMetadata *meta_r1 = regex_meta(result.symbols, "R1");
  const RegexMetadata *meta_r2 = regex_meta(result.symbols, "R2");
  ASSERT_NE(meta_r1, nullptr);
  ASSERT_NE(meta_r2, nullptr);

  EXPECT_EQ(meta_r1->node_to_pos_ids.size(), 2U);
  EXPECT_EQ(meta_r2->node_to_pos_ids.size(), 2U);
  EXPECT_TRUE(meta_r1->pos_ids_to_names.contains(1U));
  EXPECT_TRUE(meta_r2->pos_ids_to_names.contains(1U));
}

TEST(AlphabetTests, ReportsErrorWhenRegexExceedsStateLimit) {
  std::string source =
      "event { int x; };\n"
      "pred A = x == 1;\n";

  source += "regex R = A";
  for (int i = 0; i < 80; ++i) {
    source += " . A";
  }
  source += ";\n";

  AnalysisResult result = build_analysis(std::move(source));
  const RegexMetadata *meta = regex_meta(result.symbols, "R");
  ASSERT_NE(meta, nullptr);

  EXPECT_GT(result.ctxt.diagnostics.get_error_count(), 0U);
  EXPECT_LE(meta->node_to_pos_ids.size(), static_cast<std::size_t>(aion::core::MAX_STATES - 1));
}

TEST(AlphabetTests, AssignsSinglePositionForSinglePredicateRegex) {
  AnalysisResult result = build_analysis(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "regex R = A;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  const RegexMetadata *meta = regex_meta(result.symbols, "R");
  ASSERT_NE(meta, nullptr);
  ASSERT_EQ(meta->pos_ids_to_names.size(), 1U);
  EXPECT_TRUE(meta->pos_ids_to_names.contains(1U));
  EXPECT_EQ(meta->pos_ids_to_names.at(1U), "A");
}

TEST(AlphabetTests, AssignsSinglePositionForWildcardOnlyRegex) {
  AnalysisResult result = build_analysis(
      "event { int x; };\n"
      "regex R = _;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  const RegexMetadata *meta = regex_meta(result.symbols, "R");
  ASSERT_NE(meta, nullptr);
  ASSERT_EQ(meta->pos_ids_to_names.size(), 1U);
  EXPECT_EQ(meta->pos_ids_to_names.at(1U), "_");
}

TEST(AlphabetTests, AssignsDistinctIdsForRepeatedPredicateInConcat) {
  AnalysisResult result = build_analysis(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "regex R = A . A . A;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  const RegexMetadata *meta = regex_meta(result.symbols, "R");
  ASSERT_NE(meta, nullptr);
  ASSERT_EQ(meta->pos_ids_to_names.size(), 3U);

  std::size_t a_count = 0;
  for (const auto &[id, name] : meta->pos_ids_to_names) {
    (void)id;
    if (name == "A") {
      ++a_count;
    }
  }
  EXPECT_EQ(a_count, 3U);
}

TEST(AlphabetTests, MaintainsEqualSizesForNodeAndNameMaps) {
  AnalysisResult result = build_analysis(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "regex R = (A | B) . (A | _) . B;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  const RegexMetadata *meta = regex_meta(result.symbols, "R");
  ASSERT_NE(meta, nullptr);
  EXPECT_EQ(meta->node_to_pos_ids.size(), meta->pos_ids_to_names.size());
}

TEST(AlphabetTests, KeepsMappingsIndependentAcrossMultipleRegexes) {
  AnalysisResult result = build_analysis(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "regex R1 = A . B;\n"
      "regex R2 = B . _;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  const RegexMetadata *meta_r1 = regex_meta(result.symbols, "R1");
  const RegexMetadata *meta_r2 = regex_meta(result.symbols, "R2");
  ASSERT_NE(meta_r1, nullptr);
  ASSERT_NE(meta_r2, nullptr);
  EXPECT_TRUE(meta_r1->pos_ids_to_names.at(1U) == "A" || meta_r1->pos_ids_to_names.at(1U) == "B");
  EXPECT_TRUE(meta_r2->pos_ids_to_names.at(1U) == "B" || meta_r2->pos_ids_to_names.at(1U) == "_");
}

TEST(AlphabetTests, HandlesNestedRegexStructure) {
  AnalysisResult result = build_analysis(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "pred C = x == 3;\n"
      "regex R = ((A | B) . (C | _))*;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  const RegexMetadata *meta = regex_meta(result.symbols, "R");
  ASSERT_NE(meta, nullptr);
  EXPECT_EQ(meta->pos_ids_to_names.size(), 4U);
}

TEST(AlphabetTests, NoErrorsForRegexNearStateLimit) {
  std::string source =
      "event { int x; };\n"
      "pred A = x == 1;\n";
  source += "regex R = A";
  for (int i = 0; i < 20; ++i) {
    source += " . A";
  }
  source += ";\n";

  AnalysisResult result = build_analysis(std::move(source));
  EXPECT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
}

TEST(AlphabetTests, PositionIdsArePositiveAndStartFromOne) {
  AnalysisResult result = build_analysis(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "regex R = A . B;\n");

  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  const RegexMetadata *meta = regex_meta(result.symbols, "R");
  ASSERT_NE(meta, nullptr);
  EXPECT_TRUE(meta->pos_ids_to_names.contains(1U));
  for (const auto &[id, name] : meta->pos_ids_to_names) {
    (void)name;
    EXPECT_GE(id, 1U);
  }
}
