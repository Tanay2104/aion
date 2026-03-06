#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

import aion.core;
import aion.frontend;

namespace {

using aion::core::CompilationContext;
using aion::core::Type;
using aion::core::Verbosity;
using aion::frontend::FieldMetadata;
using aion::frontend::Lexer;
using aion::frontend::Parser;
using aion::frontend::SymbolKind;
using aion::frontend::SymbolTable;

struct SymbolBuildResult {
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

SymbolBuildResult build_symbol_table(std::string source) {
  SymbolBuildResult result;
  result.source = with_minimal_event_if_missing(std::move(source));
  result.ctxt.options.verbosity = Verbosity::NONE;
  result.ctxt.diagnostics.set_source_location(result.source);

  Lexer lexer(result.source, result.ctxt);
  result.tokens = lexer.tokenize();
  Parser parser(result.tokens, result.ctxt);
  result.ast = parser.parse();
  result.symbols = aion::frontend::generate_symbol_table(*result.ast, result.ctxt);
  return result;
}

const FieldMetadata *field_meta(const SymbolTable &table, std::string_view name) {
  const auto *sym = table.resolve(name);
  if (sym == nullptr || sym->kind != SymbolKind::EVENT_FIELD) {
    return nullptr;
  }
  return &std::get<FieldMetadata>(sym->details);
}

} // namespace

TEST(SymbolTableTests, AssignsFieldOffsetsAndSizesConsistently) {
  SymbolBuildResult result = build_symbol_table(
      "event { int a; char b; bool c; float d; string e; };\n"
      "pred P = a == 1;\n"
      "regex R = P;\n");

  EXPECT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);

  const auto *a = field_meta(result.symbols, "a");
  const auto *b = field_meta(result.symbols, "b");
  const auto *c = field_meta(result.symbols, "c");
  const auto *d = field_meta(result.symbols, "d");
  const auto *e = field_meta(result.symbols, "e");

  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(d, nullptr);
  ASSERT_NE(e, nullptr);

  EXPECT_EQ(a->type, Type::INT);
  EXPECT_EQ(a->offset, 0U);
  EXPECT_EQ(a->size_in_bytes, 4U);

  EXPECT_EQ(b->type, Type::CHAR);
  EXPECT_EQ(b->offset, 4U);
  EXPECT_EQ(b->size_in_bytes, 1U);

  EXPECT_EQ(c->type, Type::BOOL);
  EXPECT_EQ(c->offset, 5U);
  EXPECT_EQ(c->size_in_bytes, 1U);

  EXPECT_EQ(d->type, Type::FLOAT);
  EXPECT_EQ(d->offset, 6U);
  EXPECT_EQ(d->size_in_bytes, 4U);

  EXPECT_EQ(e->type, Type::STRING);
  EXPECT_EQ(e->offset, 10U);
  EXPECT_EQ(e->size_in_bytes, static_cast<std::uint8_t>(aion::core::MAX_STRING_SIZE));
}

TEST(SymbolTableTests, WarnsOnDuplicateDeclarationsWithoutOverwritingOriginal) {
  SymbolBuildResult result = build_symbol_table(
      "event { int x; float x; };\n"
      "pred P = x == 1;\n"
      "pred P = x == 2;\n"
      "regex R = P;\n"
      "regex R = P;\n");

  EXPECT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  EXPECT_EQ(result.ctxt.diagnostics.get_warning_count(), 3U);

  const auto *x = field_meta(result.symbols, "x");
  ASSERT_NE(x, nullptr);
  EXPECT_EQ(x->type, Type::INT);
  EXPECT_EQ(x->offset, 0U);
  EXPECT_EQ(x->size_in_bytes, 4U);
}

TEST(SymbolTableTests, ResolveReturnsNullForUnknownName) {
  SymbolBuildResult result = build_symbol_table(
      "event { int x; };\n"
      "pred P = x == 1;\n"
      "regex R = P;\n");

  EXPECT_EQ(result.symbols.resolve("missing_symbol"), nullptr);
}

TEST(SymbolTableTests, StoresPredicateAndRegexSymbolsWithCorrectKind) {
  SymbolBuildResult result = build_symbol_table(
      "event { int x; };\n"
      "pred P = x == 1;\n"
      "regex R = P;\n");

  const auto *pred = result.symbols.resolve("P");
  const auto *regex = result.symbols.resolve("R");
  ASSERT_NE(pred, nullptr);
  ASSERT_NE(regex, nullptr);
  EXPECT_EQ(pred->kind, SymbolKind::PREDICATE);
  EXPECT_EQ(regex->kind, SymbolKind::REGEX);
}

TEST(SymbolTableTests, AllowsModResolveForExistingSymbol) {
  SymbolBuildResult result = build_symbol_table(
      "event { int x; };\n"
      "pred P = x == 1;\n"
      "regex R = P;\n");

  auto *x = result.symbols.mod_resolve("x");
  ASSERT_NE(x, nullptr);
  EXPECT_EQ(x->kind, SymbolKind::EVENT_FIELD);
}

TEST(SymbolTableTests, ModResolveReturnsNullForMissingSymbol) {
  SymbolBuildResult result = build_symbol_table(
      "event { int x; };\n"
      "pred P = x == 1;\n"
      "regex R = P;\n");

  EXPECT_EQ(result.symbols.mod_resolve("does_not_exist"), nullptr);
}

TEST(SymbolTableTests, FieldAndPredicateNameConflictKeepsFieldSymbol) {
  SymbolBuildResult result = build_symbol_table(
      "event { int id; };\n"
      "pred id = true;\n"
      "regex R = id;\n");

  EXPECT_EQ(result.ctxt.diagnostics.get_warning_count(), 1U);
  const auto *sym = result.symbols.resolve("id");
  ASSERT_NE(sym, nullptr);
  EXPECT_EQ(sym->kind, SymbolKind::EVENT_FIELD);
}

TEST(SymbolTableTests, FieldAndRegexNameConflictKeepsFieldSymbol) {
  SymbolBuildResult result = build_symbol_table(
      "event { int R; };\n"
      "pred P = R == 1;\n"
      "regex R = P;\n");

  EXPECT_EQ(result.ctxt.diagnostics.get_warning_count(), 1U);
  const auto *sym = result.symbols.resolve("R");
  ASSERT_NE(sym, nullptr);
  EXPECT_EQ(sym->kind, SymbolKind::EVENT_FIELD);
}

TEST(SymbolTableTests, ComputesOffsetAfterLeadingStringField) {
  SymbolBuildResult result = build_symbol_table(
      "event { string name; int id; };\n"
      "pred P = id == 1;\n"
      "regex R = P;\n");

  const auto *name = field_meta(result.symbols, "name");
  const auto *id = field_meta(result.symbols, "id");
  ASSERT_NE(name, nullptr);
  ASSERT_NE(id, nullptr);
  EXPECT_EQ(name->offset, 0U);
  EXPECT_EQ(name->size_in_bytes, static_cast<std::uint8_t>(aion::core::MAX_STRING_SIZE));
  EXPECT_EQ(id->offset, aion::core::MAX_STRING_SIZE);
}

TEST(SymbolTableTests, ProducesNoWarningsForUniqueDeclarations) {
  SymbolBuildResult result = build_symbol_table(
      "event { int x; bool y; };\n"
      "pred P1 = x == 1;\n"
      "pred P2 = y == true;\n"
      "regex R1 = P1;\n"
      "regex R2 = P2;\n");

  EXPECT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);
  EXPECT_EQ(result.ctxt.diagnostics.get_warning_count(), 0U);
}

TEST(SymbolTableTests, DuplicatePredicatesGenerateSingleWarningForOneRedeclaration) {
  SymbolBuildResult result = build_symbol_table(
      "event { int x; };\n"
      "pred P = x == 1;\n"
      "pred P = x == 2;\n"
      "regex R = P;\n");

  EXPECT_EQ(result.ctxt.diagnostics.get_warning_count(), 1U);
  const auto *sym = result.symbols.resolve("P");
  ASSERT_NE(sym, nullptr);
  EXPECT_EQ(sym->kind, SymbolKind::PREDICATE);
}

TEST(SymbolTableTests, DuplicateRegexGenerateSingleWarningForOneRedeclaration) {
  SymbolBuildResult result = build_symbol_table(
      "event { int x; };\n"
      "pred P = x == 1;\n"
      "regex R = P;\n"
      "regex R = P;\n");

  EXPECT_EQ(result.ctxt.diagnostics.get_warning_count(), 1U);
  const auto *sym = result.symbols.resolve("R");
  ASSERT_NE(sym, nullptr);
  EXPECT_EQ(sym->kind, SymbolKind::REGEX);
}
