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
import std;

int main(int argc, char** argv) {
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

  compilation_context.log(1, std::format("Reading input program: {}", argv[1]));
  std::ifstream input_file(argv[1]);
  if (!input_file) {
    std::println("Error opening input file: {}", argv[1]);
    return 1;
  }
  std::string program((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
  compilation_context.diagnostics.set_source_location(program);
  compilation_context.log(1, "Read successful");

  // Lexing!!
  compilation_context.log(1, "Tokenizing input program");
  frontend::Lexer lexer(program, compilation_context);
  std::vector<frontend::Token> tokens = lexer.tokenize();
  compilation_context.log(1, "Tokenizing successful");
  if (compilation_context.options.ir == core::IR::TOKENS) {
    compilation_context.log(1, std::format("Dumping tokens to {}", compilation_context.options.output_filename));
    lexer.dump_tokens(compilation_context.options.output_filename);
    compilation_context.log(1, "Dumped tokens");
  }

  // Parsing!!
  compilation_context.log(1, "Parsing input program");
  frontend::Parser parser(tokens, compilation_context);
  auto ast = parser.parse();
  compilation_context.log(1, "Parsing successful");
  if (compilation_context.options.ir == core::IR::AST)
  {
    compilation_context.log(1, std::format("Dumping ast to {}", compilation_context.options.output_filename));
    frontend::dump_ast(*ast);
    compilation_context.log(1, "Dumped ast");
  }
}
