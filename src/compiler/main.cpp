/* main.cpp
 * The entry point and orchestration
 * Parse command line args
 * source = read_file(args.input_file)
 * tokens = lexer.tokenize()
 * ast = parser.parse(tokens)
 * symbol_table smbt
 * smbt.analyse(ast)
 * nfa = automata::glushkov(ast, syms)
 * codegen.cpp_emit(nfa, args.output_path)
 * NOTE: Right now the system does some logging.
 * Later, we should add if def flags which eliminate logging
 * for better performance in the compilation step.
 */
import aion.utils;
import aion.frontend;
import aion.core;
import aion.analysis;
import aion.automata;
import aion.codegen;
import std;

int main(int argc, char** argv)
{
  using namespace aion;

  if (argc < 2) {
    std::print("Usage: ./aionc <filename> [options]\nSee ./aionc --help for more details\n");
    return 1;
  }
  core::Options options = utils::argparse(argc, argv);

  core::CompilationContext compilation_context;
  // Hopefully compiler will auto-generate apt move constructor.
  compilation_context.options = std::move(options);
  // Other fields are default constructed.

  compilation_context.log(1, std::format("[Input] Reading source file '{}'", argv[1]));
  std::ifstream input_file(argv[1]);
  if (!input_file) {
    std::println("Error opening input file: {}", argv[1]);
    return 1;
  }
  std::string program((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
  compilation_context.diagnostics.set_source_location(program);
  compilation_context.log(1, "[Input] Source loaded");
  compilation_context.log(2, std::format("[Input] Source size: {} bytes", program.size()));

  // Lexing!!
  compilation_context.log(1, "[Lex] Tokenization started");
  frontend::Lexer lexer(program, compilation_context);
  std::vector<frontend::Token> tokens = lexer.tokenize();
  compilation_context.log(1, "[Lex] Tokenization finished");
  if (compilation_context.options.ir == core::IR::TOKENS) {
    compilation_context.log(1, std::format("[Dump] Writing tokens to '{}'", compilation_context.options.output_filename));
    lexer.dump_tokens(compilation_context.options.output_filename);
    compilation_context.log(1, "[Dump] Tokens written");
  }

  // Parsing!!
  compilation_context.log(1, "[Parse] Parsing started");
  frontend::Parser parser(tokens, compilation_context);
  auto ast = parser.parse();
  // NOTE: ast existance does not imply ast validity.
  if (compilation_context.diagnostics.get_error_count() > core::MAX_ERROR_COUNT)
  {
    compilation_context.log(1, "[Parse] Too many errors, stopping now", aion::core::RED);
    std::exit(1);
  }

  compilation_context.log(1, "[Parse] Parsing finished");
  if (compilation_context.options.ir == core::IR::AST)
  {
    compilation_context.log(1, std::format("[Dump] Writing AST to '{}'", compilation_context.options.output_filename));
    frontend::dump_ast(*ast, compilation_context);
    compilation_context.log(1, "[Dump] AST written");
  }

  compilation_context.log(1, "[Analysis] Semantic analysis started");
  // Symbol table stuff!
  compilation_context.log(2, "[Analysis] Building symbol table");
  frontend::SymbolTable symbol_table = frontend::generate_symbol_table(*ast, compilation_context);
  compilation_context.log(2, "[Analysis] Symbol table pass complete");

  compilation_context.log(2, "[Analysis] Assigning regex position ids");
  analysis::fill_pos_ids(symbol_table, *ast, compilation_context);
  compilation_context.log(2, "[Analysis] Position-id assignment complete");
  compilation_context.log(1, "[Analysis] Semantic analysis finished");

  // Finally making NFA's
  compilation_context.log(1, "[NFA] Construction started");
  auto all_nfa = automata::convert_to_generic_nfa(*ast, symbol_table, compilation_context);
  compilation_context.log(1, "[NFA] Construction finished");
  compilation_context.log(2, std::format("[NFA] Built {} automata", all_nfa.size()));
  if (compilation_context.options.ir == core::IR::NFA)
  {
    const std::string nfa_dump_file = compilation_context.options.output_filename + ".dot";
    compilation_context.log(1, std::format("[Dump] Writing NFA DOT to '{}'", nfa_dump_file));
    automata::dump_nfa_dot(all_nfa, nfa_dump_file);
    compilation_context.log(1, "[Dump] NFA DOT written");
  }

  if (compilation_context.diagnostics.get_error_count() > 0)
  {
    compilation_context.log(1, "[Session] Errors in given code. Skipping remaining steps", aion::core::RED);
    std::exit(0);
  }


  compilation_context.log(1, "[Codegen] Emission started");
  codegen::CEmitter emitter;
  codegen::emit_headers(emitter);

  compilation_context.log(2, "[Codegen] Emitting event model");
  codegen::emit_event(ast->event, symbol_table, emitter);

  compilation_context.log(2, "[Codegen] Emitting predicate evaluators");
  codegen::emit_predicates(*ast, symbol_table, emitter, compilation_context);

  compilation_context.log(2, "[Codegen] Emitting regex engines");
  codegen::emit_regex_engines(*ast, symbol_table, emitter, compilation_context, all_nfa);

  codegen::emit_footers(emitter);
  const std::string header_path = compilation_context.options.output_filename + ".hpp";
  emitter.dump(header_path);
  compilation_context.log(1, "[Codegen] Emission finished");
  compilation_context.log(2, std::format("[Codegen] Generated header '{}'", header_path));
  compilation_context.log(2, std::format("[Session] diagnostics: errors={}, warnings={}",
                                         compilation_context.diagnostics.get_error_count(),
                                         compilation_context.diagnostics.get_warning_count()));
}
