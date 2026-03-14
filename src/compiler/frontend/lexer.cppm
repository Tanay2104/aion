/* lexer.cppm
 * Breaks raw file strings into chunks
 * This simplifies the parser so that it doesn't
 * deal with whitespace or comments
 */

export module aion.frontend:lexer;

import std;
import aion.core;

export namespace aion::frontend {

enum class TokenType {
  // Keywords
  KW_EVENT, // "event
  KW_PRED,  // "pred"
  KW_REGEX, // "regex"

  // Identifiers & Literals
  IDENTIFIER,  // e.g., "x", "P1", "int"
  LIT_INTEGER, // e.g., 100, 20
  LIT_FLOAT,   // e.g., 100.05
  LIT_CHAR,    // e.g., 'a'
  LIT_BOOL, // true or false

  // C++ Predicate Operators
  EQUALS,        // =
  DOUBLE_EQUALS, // ==
  NOT_EQUALS,    // !=
  GREATER,       // >
  LESS,          // <
  GREATER_EQUAL, // >=
  LESS_EQUAL,    // <=
  LOGICAL_AND,   // &&
  LOGICAL_OR,    // ||
  LOGICAL_NOT,   // !

  // Regex Operators
  UNION, // | (Union)
  STAR,  // * (Kleene Star)
  DOT,   // . (Concatenation)
  ANY,   // _ (underscore, matches any character)

  // Punctuation / Delimiters
  LPAREN,    // (
  RPAREN,    // )
  LBRACE,    // {
  RBRACE,    // }
  SEMICOLON, // ;

  // Control Tokens
  END_OF_FILE, // Special token for the end of input
  ERROR        // Special token for an unrecognized character
};

using SourceLocation = core::SourceLocation;

struct Token {
  TokenType type{};
  std::string_view text{};
  SourceLocation location;
};

class Lexer {
private:
  std::string_view source;
  std::vector<Token> tokens;
  core::CompilationContext &compile_context;
  std::size_t start = 0;   // Start of the current token.
  std::size_t current = 0; // End of the current token.
  std::size_t line = 1;    // Current line.
  std::size_t column = 1;  // Current column.

  // helper funcs.
  [[nodiscard]] bool is_at_end() const;

  // advanced current position by 1 and return the current char.
  char advance();

  char advance(std::size_t count);

  // return curr char without increasing curr, col
  [[nodiscard]] char peek() const;

  // return next char without increasing curr, col
  [[nodiscard]] char peek_next() const;

  bool is_identifier();
  // If it doesn't start with char, then not an identifier.

  bool is_pred();
  bool is_regex();

  bool is_event();

  [[nodiscard]] bool is_lit_digit() const;
  bool is_lit_int();
  [[nodiscard]] bool is_lit_char() const;
  bool is_lit_char_2();
  bool is_lit_float();
  bool is_lit_bool();

  // specific handlers.
  void scan_token();

  void add_token(TokenType type);

public:
  Lexer(std::string_view src, core::CompilationContext &ctxt);

  std::vector<Token> tokenize();

  void dump_tokens(std::string_view filename);
};
} // namespace aion::frontend
