#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

import aion.core;
import aion.frontend;

namespace {

using aion::core::CompilationContext;
using aion::core::Verbosity;
using aion::frontend::AndPredExpr;
using aion::frontend::CompOp;
using aion::frontend::CompPredExpr;
using aion::frontend::Lexer;
using aion::frontend::NotExpr;
using aion::frontend::OrPredExpr;
using aion::frontend::Parser;
using aion::frontend::PrimaryPredExpr;
using aion::frontend::RegexConcat;
using aion::frontend::RegexPrimary;
using aion::frontend::RegexStar;
using aion::frontend::RegexUnion;

struct ParseResult {
  std::string source;
  CompilationContext ctxt;
  std::vector<aion::frontend::Token> tokens;
  std::unique_ptr<aion::frontend::AionFile> ast;
};

std::string with_minimal_event_if_missing(std::string source) {
  if (source.find("event") == std::string::npos) {
    return "event { int __test_event_field; };\n" + source;
  }
  return source;
}

ParseResult parse_program(std::string source) {
  ParseResult result;
  result.source = with_minimal_event_if_missing(std::move(source));
  result.ctxt.options.verbosity = Verbosity::NONE;
  result.ctxt.diagnostics.set_source_location(result.source);

  Lexer lexer(result.source, result.ctxt);
  result.tokens = lexer.tokenize();

  Parser parser(result.tokens, result.ctxt);
  result.ast = parser.parse();
  return result;
}

const aion::frontend::PredDecl *find_predicate(const ParseResult &parsed, std::string_view name) {
  for (const auto &pred : parsed.ast->predicates) {
    if (pred.name == name) {
      return &pred;
    }
  }
  return nullptr;
}

const aion::frontend::RegexDecl *find_regex(const ParseResult &parsed, std::string_view name) {
  for (const auto &regex : parsed.ast->regexes) {
    if (regex.name == name) {
      return &regex;
    }
  }
  return nullptr;
}

} // namespace

TEST(ParserTests, ParsesCompleteValidProgramStructure) {
  ParseResult parsed = parse_program(
      "event { int x; bool ok; };\n"
      "pred P1 = (x == 10);\n"
      "pred P2 = ok == true;\n"
      "regex R1 = P1 . P2;\n");

  ASSERT_NE(parsed.ast, nullptr);
  EXPECT_EQ(parsed.ctxt.diagnostics.get_error_count(), 0U);
  EXPECT_EQ(parsed.ast->event.fields.size(), 2U);
  EXPECT_EQ(parsed.ast->predicates.size(), 2U);
  EXPECT_EQ(parsed.ast->regexes.size(), 1U);
  EXPECT_EQ(std::string(parsed.ast->event.fields[0].name), "x");
  EXPECT_EQ(std::string(parsed.ast->predicates[0].name), "P1");
  EXPECT_EQ(std::string(parsed.ast->regexes[0].name), "R1");
}

TEST(ParserTests, RespectsPredicatePrecedenceAndAssociativity) {
  ParseResult parsed = parse_program(
      "event { bool a; bool b; bool c; };\n"
      "pred P = a || b && !c;\n"
      "regex R = P;\n");

  ASSERT_NE(parsed.ast, nullptr);
  EXPECT_EQ(parsed.ctxt.diagnostics.get_error_count(), 0U);
  ASSERT_EQ(parsed.ast->predicates.size(), 1U);

  const auto *root_or = dynamic_cast<const OrPredExpr *>(parsed.ast->predicates[0].expr.get());
  ASSERT_NE(root_or, nullptr);
  ASSERT_EQ(root_or->terms.size(), 2U);

  const auto *lhs_primary = dynamic_cast<const PrimaryPredExpr *>(root_or->terms[0].get());
  const auto *rhs_and = dynamic_cast<const AndPredExpr *>(root_or->terms[1].get());
  ASSERT_NE(lhs_primary, nullptr);
  ASSERT_NE(rhs_and, nullptr);
  ASSERT_EQ(rhs_and->terms.size(), 2U);

  const auto *rhs_not = dynamic_cast<const NotExpr *>(rhs_and->terms[1].get());
  EXPECT_NE(rhs_not, nullptr);
}

TEST(ParserTests, ParsesNotOperatorChain) {
  ParseResult parsed = parse_program(
      "event { bool a; };\n"
      "pred P = !!!a;\n"
      "regex R = P;\n");

  ASSERT_NE(parsed.ast, nullptr);
  EXPECT_EQ(parsed.ctxt.diagnostics.get_error_count(), 0U);
  const auto *pred = find_predicate(parsed, "P");
  ASSERT_NE(pred, nullptr);
  const auto *n1 = dynamic_cast<const NotExpr *>(pred->expr.get());
  ASSERT_NE(n1, nullptr);
  const auto *n2 = dynamic_cast<const NotExpr *>(n1->inner.get());
  ASSERT_NE(n2, nullptr);
  const auto *n3 = dynamic_cast<const NotExpr *>(n2->inner.get());
  EXPECT_NE(n3, nullptr);
}

TEST(ParserTests, RespectsRegexPrecedence) {
  ParseResult parsed = parse_program(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "pred C = x == 3;\n"
      "regex R = A | B . C*;\n");

  ASSERT_NE(parsed.ast, nullptr);
  EXPECT_EQ(parsed.ctxt.diagnostics.get_error_count(), 0U);
  ASSERT_EQ(parsed.ast->regexes.size(), 1U);

  const auto *root_union = dynamic_cast<const RegexUnion *>(parsed.ast->regexes[0].expr.get());
  ASSERT_NE(root_union, nullptr);
  ASSERT_EQ(root_union->options.size(), 2U);

  const auto *lhs_primary = dynamic_cast<const RegexPrimary *>(root_union->options[0].get());
  const auto *rhs_concat = dynamic_cast<const RegexConcat *>(root_union->options[1].get());
  ASSERT_NE(lhs_primary, nullptr);
  ASSERT_NE(rhs_concat, nullptr);
  ASSERT_EQ(rhs_concat->sequence.size(), 2U);

  const auto *rhs_star = dynamic_cast<const RegexStar *>(rhs_concat->sequence[1].get());
  EXPECT_NE(rhs_star, nullptr);
}

TEST(ParserTests, ParsesRegexParenthesizedConcatThenStar) {
  ParseResult parsed = parse_program(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "regex R = (A . B)*;\n");

  ASSERT_NE(parsed.ast, nullptr);
  EXPECT_EQ(parsed.ctxt.diagnostics.get_error_count(), 0U);
  ASSERT_EQ(parsed.ast->regexes.size(), 1U);
  EXPECT_NE(dynamic_cast<const RegexStar *>(parsed.ast->regexes[0].expr.get()), nullptr);
}

TEST(ParserTests, ReportsErrorsOnInvalidPredicateButKeepsParsing) {
  ParseResult parsed = parse_program(
      "event { int x; };\n"
      "pred Broken = (x == );\n"
      "regex R = _;\n");

  ASSERT_NE(parsed.ast, nullptr);
  EXPECT_GT(parsed.ctxt.diagnostics.get_error_count(), 0U);
  EXPECT_EQ(parsed.ast->regexes.size(), 1U);
}

TEST(ParserTests, ReportsErrorsOnInvalidRegexButKeepsLaterRegex) {
  ParseResult parsed = parse_program(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "regex Bad = (A | );\n"
      "regex Good = A;\n");

  ASSERT_NE(parsed.ast, nullptr);
  EXPECT_GT(parsed.ctxt.diagnostics.get_error_count(), 0U);
  EXPECT_NE(find_regex(parsed, "Good"), nullptr);
}

TEST(ParserTests, ReportsErrorOnMissingEventSemicolon) {
  ParseResult parsed = parse_program(
      "event { int x; }\n"
      "pred P = x == 1;\n"
      "regex R = P;\n");

  ASSERT_NE(parsed.ast, nullptr);
  EXPECT_GT(parsed.ctxt.diagnostics.get_error_count(), 0U);
}

TEST(ParserTests, RecoversFromInvalidFieldDeclaration) {
  ParseResult parsed = parse_program(
      "event { xyz bad; int x; };\n"
      "pred P = x == 1;\n"
      "regex R = P;\n");

  ASSERT_NE(parsed.ast, nullptr);
  EXPECT_GT(parsed.ctxt.diagnostics.get_error_count(), 0U);
  EXPECT_EQ(parsed.ast->regexes.size(), 1U);
}

struct ComparisonCase {
  std::string op;
  CompOp expected;
};

class ParserComparisonOpTests : public ::testing::TestWithParam<ComparisonCase> {};

TEST_P(ParserComparisonOpTests, ParsesAllComparisonOperators) {
  const auto &tc = GetParam();
  ParseResult parsed = parse_program(
      "event { int x; int y; };\n"
      "pred P = x " + tc.op + " y;\n"
      "regex R = P;\n");

  ASSERT_NE(parsed.ast, nullptr);
  ASSERT_EQ(parsed.ctxt.diagnostics.get_error_count(), 0U);
  const auto *pred = find_predicate(parsed, "P");
  ASSERT_NE(pred, nullptr);
  const auto *comp = dynamic_cast<const CompPredExpr *>(pred->expr.get());
  ASSERT_NE(comp, nullptr);
  EXPECT_EQ(comp->op, tc.expected);
}

INSTANTIATE_TEST_SUITE_P(
    AllCompOps, ParserComparisonOpTests,
    ::testing::Values(ComparisonCase{"==", CompOp::EQ}, ComparisonCase{"!=", CompOp::NEQ},
                      ComparisonCase{">", CompOp::GT}, ComparisonCase{"<", CompOp::LT},
                      ComparisonCase{">=", CompOp::GE}, ComparisonCase{"<=", CompOp::LE}),
    [](const testing::TestParamInfo<ComparisonCase> &info) {
      if (info.param.op == "==") return std::string("Eq");
      if (info.param.op == "!=") return std::string("Neq");
      if (info.param.op == ">") return std::string("Gt");
      if (info.param.op == "<") return std::string("Lt");
      if (info.param.op == ">=") return std::string("Ge");
      if (info.param.op == "<=") return std::string("Le");
      return std::string("Unknown");
    });

enum class RegexRootKind {
  PRIMARY,
  UNION,
  CONCAT,
  STAR
};

struct RegexRootCase {
  std::string expr;
  RegexRootKind kind;
};

class ParserRegexRootTests : public ::testing::TestWithParam<RegexRootCase> {};

TEST_P(ParserRegexRootTests, ParsesRegexRootKinds) {
  const auto &tc = GetParam();
  ParseResult parsed = parse_program(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "pred B = x == 2;\n"
      "regex R = " +
      tc.expr + ";\n");

  ASSERT_NE(parsed.ast, nullptr);
  ASSERT_EQ(parsed.ctxt.diagnostics.get_error_count(), 0U);
  ASSERT_EQ(parsed.ast->regexes.size(), 1U);
  const auto *expr = parsed.ast->regexes[0].expr.get();

  switch (tc.kind) {
  case RegexRootKind::PRIMARY:
    EXPECT_NE(dynamic_cast<const RegexPrimary *>(expr), nullptr);
    break;
  case RegexRootKind::UNION:
    EXPECT_NE(dynamic_cast<const RegexUnion *>(expr), nullptr);
    break;
  case RegexRootKind::CONCAT:
    EXPECT_NE(dynamic_cast<const RegexConcat *>(expr), nullptr);
    break;
  case RegexRootKind::STAR:
    EXPECT_NE(dynamic_cast<const RegexStar *>(expr), nullptr);
    break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    RootKinds, ParserRegexRootTests,
    ::testing::Values(RegexRootCase{"A", RegexRootKind::PRIMARY}, RegexRootCase{"A | B", RegexRootKind::UNION},
                      RegexRootCase{"A . B", RegexRootKind::CONCAT}, RegexRootCase{"A*", RegexRootKind::STAR}),
    [](const testing::TestParamInfo<RegexRootCase> &info) {
      switch (info.param.kind) {
      case RegexRootKind::PRIMARY:
        return std::string("Primary");
      case RegexRootKind::UNION:
        return std::string("Union");
      case RegexRootKind::CONCAT:
        return std::string("Concat");
      case RegexRootKind::STAR:
        return std::string("Star");
      }
      return std::string("Unknown");
    });
