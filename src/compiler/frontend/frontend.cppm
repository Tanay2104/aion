/* frontend.cppm
 * The frontend of the compiler.
 * Goals are to read text, understand structure
 * Export imports lexer, parser and AST
 */

export module aion.frontend;

export import :lexer;
export import :parser;
export import :ast;
export import :symbol_table;