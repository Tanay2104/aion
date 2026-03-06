#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

import aion.core;
import aion.frontend;

namespace {

using aion::core::CompilationContext;
using aion::core::Verbosity;
using aion::frontend::Lexer;
using aion::frontend::Token;
using aion::frontend::TokenType;

struct LexResult {
  std::string source;
  std::vector<Token> tokens;
};

LexResult tokenize(std::string source, CompilationContext &ctxt) {
  LexResult result;
  result.source = std::move(source);
  ctxt.options.verbosity = Verbosity::NONE;
  ctxt.diagnostics.set_source_location(result.source);
  Lexer lexer(result.source, ctxt);
  result.tokens = lexer.tokenize();
  return result;
}

std::size_t count_token_type(const std::vector<Token> &tokens, TokenType type) {
  return static_cast<std::size_t>(
      std::count_if(tokens.begin(), tokens.end(), [type](const Token &token) { return token.type == type; }));
}

bool contains_token_text(const std::vector<Token> &tokens, TokenType type, std::string_view text) {
  return std::any_of(tokens.begin(), tokens.end(), [type, text](const Token &tok) {
    return tok.type == type && tok.text == text;
  });
}

} // namespace

TEST(LexerTests, TokenizesKeywordsLiteralsAndWildcardInProgram) {
  const std::string source =
      "event {\n"
      "  int x;\n"
      "  bool ok;\n"
      "  char c;\n"
      "  float temp;\n"
      "};\n"
      "pred P = (x >= 10 && ok == true && c == 'a' && temp < 3.5);\n"
      "regex R = P . _*;\n";

  CompilationContext ctxt;
  const auto lexed = tokenize(source, ctxt);
  const auto &tokens = lexed.tokens;

  ASSERT_FALSE(tokens.empty());
  EXPECT_EQ(tokens.front().type, TokenType::KW_EVENT);
  EXPECT_EQ(tokens.back().type, TokenType::END_OF_FILE);
  EXPECT_EQ(count_token_type(tokens, TokenType::KW_EVENT), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::KW_PRED), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::KW_REGEX), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::LIT_FLOAT), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::LIT_CHAR), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::LIT_BOOL), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::ANY), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::ERROR), 0U);
}

TEST(LexerTests, EmitsEndOfFileTokenOnce) {
  CompilationContext ctxt;
  const auto lexed = tokenize("event {}; pred P = true;", ctxt);
  const auto &tokens = lexed.tokens;

  ASSERT_FALSE(tokens.empty());
  EXPECT_EQ(count_token_type(tokens, TokenType::END_OF_FILE), 1U);
  EXPECT_EQ(tokens.back().type, TokenType::END_OF_FILE);
}

TEST(LexerTests, TokenizesComparisonOperators) {
  CompilationContext ctxt;
  const auto lexed = tokenize("event {}; pred P = x == 1 && x != 2 && x > 3 && x < 4 && x >= 5 && x <= 6;", ctxt);
  const auto &tokens = lexed.tokens;

  EXPECT_EQ(count_token_type(tokens, TokenType::DOUBLE_EQUALS), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::NOT_EQUALS), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::GREATER), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::LESS), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::GREATER_EQUAL), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::LESS_EQUAL), 1U);
}

TEST(LexerTests, TokenizesLogicalOperators) {
  CompilationContext ctxt;
  const auto lexed = tokenize("event {}; pred P = !a || b && c;", ctxt);
  const auto &tokens = lexed.tokens;

  EXPECT_EQ(count_token_type(tokens, TokenType::LOGICAL_NOT), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::LOGICAL_OR), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::LOGICAL_AND), 1U);
}

TEST(LexerTests, TokenizesRegexOperators) {
  CompilationContext ctxt;
  const auto lexed = tokenize("event {}; regex R = (A | B) . _*;", ctxt);
  const auto &tokens = lexed.tokens;

  EXPECT_EQ(count_token_type(tokens, TokenType::UNION), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::DOT), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::STAR), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::ANY), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::LPAREN), 1U);
  EXPECT_EQ(count_token_type(tokens, TokenType::RPAREN), 1U);
}

TEST(LexerTests, TokenizesIntegerLiteral) {
  CompilationContext ctxt;
  const auto lexed = tokenize("event {}; pred P = x == 1234;", ctxt);
  const auto &tokens = lexed.tokens;
  EXPECT_TRUE(contains_token_text(tokens, TokenType::LIT_INTEGER, "1234"));
}

TEST(LexerTests, TokenizesFloatLiteral) {
  CompilationContext ctxt;
  const auto lexed = tokenize("event {}; pred P = y > 3.14;", ctxt);
  const auto &tokens = lexed.tokens;
  EXPECT_TRUE(contains_token_text(tokens, TokenType::LIT_FLOAT, "3.14"));
}

TEST(LexerTests, TokenizesCharLiteral) {
  CompilationContext ctxt;
  const auto lexed = tokenize("event {}; pred P = c == 'a';", ctxt);
  const auto &tokens = lexed.tokens;
  EXPECT_TRUE(contains_token_text(tokens, TokenType::LIT_CHAR, "a"));
}

TEST(LexerTests, TokenizesStringLiteralAlphabetic) {
  CompilationContext ctxt;
  const auto lexed = tokenize("event {}; pred P = s == \"alpha\";", ctxt);
  const auto &tokens = lexed.tokens;
  EXPECT_TRUE(contains_token_text(tokens, TokenType::LIT_STRING, "alpha"));
}

TEST(LexerTests, TokenizesBooleanLiterals) {
  CompilationContext ctxt;
  const auto lexed = tokenize("event {}; pred P1 = true; pred P2 = false;", ctxt);
  const auto &tokens = lexed.tokens;
  EXPECT_EQ(count_token_type(tokens, TokenType::LIT_BOOL), 2U);
  EXPECT_TRUE(contains_token_text(tokens, TokenType::LIT_BOOL, "true"));
  EXPECT_TRUE(contains_token_text(tokens, TokenType::LIT_BOOL, "false"));
}

TEST(LexerTests, SkipsCommentsAndTracksLines) {
  const std::string source =
      "// line comment\n"
      "/* multiline\n"
      "comment */\n"
      "pred P = true;\n";

  CompilationContext ctxt;
  const auto lexed = tokenize(source, ctxt);
  const auto &tokens = lexed.tokens;

  const auto pred_it =
      std::find_if(tokens.begin(), tokens.end(), [](const Token &token) { return token.type == TokenType::KW_PRED; });
  ASSERT_NE(pred_it, tokens.end());
  EXPECT_EQ(pred_it->location.line, 4U);
  EXPECT_EQ(count_token_type(tokens, TokenType::ERROR), 0U);
}

TEST(LexerTests, EmitsErrorTokenForStandaloneAmpersand) {
  const std::string source = "event {}; pred P = a & b;";

  CompilationContext ctxt;
  const auto lexed = tokenize(source, ctxt);
  const auto &tokens = lexed.tokens;

  EXPECT_EQ(count_token_type(tokens, TokenType::ERROR), 1U);
  const auto error_it =
      std::find_if(tokens.begin(), tokens.end(), [](const Token &token) { return token.type == TokenType::ERROR; });
  ASSERT_NE(error_it, tokens.end());
  EXPECT_EQ(std::string(error_it->text), "&");
}

TEST(LexerTests, EmitsErrorTokenForUnsupportedDivisionOperator) {
  const std::string source = "event {}; pred P = a / b;";

  CompilationContext ctxt;
  const auto lexed = tokenize(source, ctxt);
  const auto &tokens = lexed.tokens;
  EXPECT_EQ(count_token_type(tokens, TokenType::ERROR), 1U);
}

TEST(LexerTests, TreatsKeywordPrefixAsIdentifierWithoutSeparator) {
  const std::string source = "predx";

  CompilationContext ctxt;
  const auto lexed = tokenize(source, ctxt);
  const auto &tokens = lexed.tokens;

  ASSERT_EQ(tokens.size(), 2U);
  EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
  EXPECT_EQ(std::string(tokens[0].text), "predx");
  EXPECT_EQ(tokens[1].type, TokenType::END_OF_FILE);
}

TEST(LexerTests, KeepsIdentifiersWithUnderscoreAndDigits) {
  CompilationContext ctxt;
  const auto lexed = tokenize("pred pred_1 = x_2 == 10;", ctxt);
  const auto &tokens = lexed.tokens;

  EXPECT_TRUE(contains_token_text(tokens, TokenType::IDENTIFIER, "pred_1"));
  EXPECT_TRUE(contains_token_text(tokens, TokenType::IDENTIFIER, "x_2"));
  EXPECT_TRUE(contains_token_text(tokens, TokenType::LIT_INTEGER, "10"));
}
