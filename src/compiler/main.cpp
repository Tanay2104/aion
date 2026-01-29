/* main.cpp
 * The entry point and orchestration
 * Parse command line args
 * source = read_file(args.input_file)
 * tokens = lexer.tokenize()
 * ast = parser.parse(tokens)
 * symbol_table smbt
 * smbt.analyse(ast)
 * nfa = automata::glushkov(ast, syms)
 * codgen.cpp_emit(nfa, args.output_path)
 */
