#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
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
using aion::frontend::Token;

struct PipelineResult {
  std::string source;
  CompilationContext ctxt;
  std::vector<Token> tokens;
  std::unique_ptr<aion::frontend::AionFile> ast;
  aion::frontend::SymbolTable symbols;
  std::unordered_map<std::string_view, Generic_NFA> nfas;
};

PipelineResult build_pipeline(std::string source) {
  PipelineResult result;
  result.source = std::move(source);
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

std::filesystem::path unique_temp_file(std::string_view stem, std::string_view extension) {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() / (std::string(stem) + "_" + std::to_string(stamp) + std::string(extension));
}

std::string read_file(const std::filesystem::path &path) {
  std::ifstream in(path);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

} // namespace

TEST(ContractDumpTests, TokenDumpWritesReadableTokenListing) {
  const std::string source =
      "event { int x; };\n"
      "pred P = x == 1;\n"
      "regex R = P;\n";

  CompilationContext ctxt;
  ctxt.options.verbosity = Verbosity::NONE;
  ctxt.diagnostics.set_source_location(source);
  Lexer lexer(source, ctxt);
  (void)lexer.tokenize();

  const std::filesystem::path out_path = unique_temp_file("aion_contract_tokens", ".txt");
  ctxt.options.output_filename = out_path.string();
  lexer.dump_tokens(out_path.string());

  ASSERT_TRUE(std::filesystem::exists(out_path));
  const std::string dumped = read_file(out_path);
  EXPECT_FALSE(dumped.empty());
  EXPECT_NE(dumped.find("TYPE: KW_EVENT"), std::string::npos);
  EXPECT_NE(dumped.find("TYPE: END_OF_FILE"), std::string::npos);
  EXPECT_NE(dumped.find("LINE:"), std::string::npos);

  std::filesystem::remove(out_path);
}

TEST(ContractDumpTests, AstDumpWritesStructuralTree) {
  PipelineResult result = build_pipeline(
      "event { int x; bool ok; };\n"
      "pred P = x == 1 && ok == true;\n"
      "regex R = P;\n");
  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);

  const std::filesystem::path out_path = unique_temp_file("aion_contract_ast", ".txt");
  result.ctxt.options.output_filename = out_path.string();
  aion::frontend::dump_ast(*result.ast, result.ctxt);

  ASSERT_TRUE(std::filesystem::exists(out_path));
  const std::string dumped = read_file(out_path);
  EXPECT_NE(dumped.find("AionFile"), std::string::npos);
  EXPECT_NE(dumped.find("EventDecl"), std::string::npos);
  EXPECT_NE(dumped.find("PredDecl \"P\""), std::string::npos);
  EXPECT_NE(dumped.find("RegexDecl \"R\""), std::string::npos);

  std::filesystem::remove(out_path);
}

TEST(ContractDumpTests, NfaDumpWritesDotGraphScaffold) {
  PipelineResult result = build_pipeline(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "regex R = A;\n");
  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);

  const std::filesystem::path out_path = unique_temp_file("aion_contract_nfa", ".dot");
  aion::automata::dump_nfa_dot(result.nfas, out_path.string());

  ASSERT_TRUE(std::filesystem::exists(out_path));
  const std::string dumped = read_file(out_path);
  EXPECT_NE(dumped.find("digraph AionNFA"), std::string::npos);
  EXPECT_NE(dumped.find("rankdir=LR;"), std::string::npos);
  EXPECT_NE(dumped.find("subgraph cluster_0"), std::string::npos);

  std::filesystem::remove(out_path);
}

TEST(ContractDumpTests, NfaDumpContainsRegexLabelAndStartNode) {
  PipelineResult result = build_pipeline(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "regex MyRegex = A;\n");
  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);

  const std::filesystem::path out_path = unique_temp_file("aion_contract_nfa_label", ".dot");
  aion::automata::dump_nfa_dot(result.nfas, out_path.string());
  const std::string dumped = read_file(out_path);

  EXPECT_NE(dumped.find("Regex: MyRegex"), std::string::npos);
  EXPECT_NE(dumped.find("\"MyRegex_start\""), std::string::npos);
  EXPECT_NE(dumped.find("\"MyRegex_s"), std::string::npos);

  std::filesystem::remove(out_path);
}

TEST(ContractDumpTests, NfaDumpIncludesNullableEpsilonNodeForStarRegex) {
  PipelineResult result = build_pipeline(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "regex R = A*;\n");
  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);

  const std::filesystem::path out_path = unique_temp_file("aion_contract_nfa_nullable", ".dot");
  aion::automata::dump_nfa_dot(result.nfas, out_path.string());
  const std::string dumped = read_file(out_path);

  EXPECT_NE(dumped.find("\"R_eps\""), std::string::npos);
  EXPECT_NE(dumped.find("label=\"ε\""), std::string::npos);

  std::filesystem::remove(out_path);
}

TEST(ContractDumpTests, NfaDumpOrdersRegexClustersLexicographically) {
  PipelineResult result = build_pipeline(
      "event { int x; };\n"
      "pred A = x == 1;\n"
      "regex Zeta = A;\n"
      "regex Alpha = A;\n");
  ASSERT_EQ(result.ctxt.diagnostics.get_error_count(), 0U);

  const std::filesystem::path out_path = unique_temp_file("aion_contract_nfa_order", ".dot");
  aion::automata::dump_nfa_dot(result.nfas, out_path.string());
  const std::string dumped = read_file(out_path);

  const std::size_t alpha_pos = dumped.find("Regex: Alpha");
  const std::size_t zeta_pos = dumped.find("Regex: Zeta");
  ASSERT_NE(alpha_pos, std::string::npos);
  ASSERT_NE(zeta_pos, std::string::npos);
  EXPECT_LT(alpha_pos, zeta_pos);

  std::filesystem::remove(out_path);
}
