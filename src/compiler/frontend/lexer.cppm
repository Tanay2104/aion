/* lexer.cppm
 * Breaks raw file strings into chunks
 * This simplifies the parser so that it doesn't
 * deal with whitespace or comments
 */

export module aion.frontend:lexer;

import std;
import aion.core;

export namespace aion::frontend {
constexpr std::array<std::string, 29> enum_to_str = {
  "KW_EVENT",      // "event
"KW_PRED",       // "pred"
"KW_REGEX",      // "regex"

// Identifiers & Literals
"IDENTIFIER",    // e.g., "x", "P1", "int"
"LIT_INTEGER",   // e.g., 100, 20
"LIT_FLOAT",     // e.g., 100.05
"LIT_CHAR",      // e.g., 'a'
"LIT_STRING",    // e.g., "hello"

// C++ Predicate Operators
"EQUALS",        // =
"DOUBLE_EQUALS", // ==
"NOT_EQUALS",    // !=
"GREATER",       // >
"LESS",          // <
"GREATER_EQUAL", // >=
"LESS_EQUAL",    // <=
"LOGICAL_AND",   // &&
"LOGICAL_OR",    // ||
"LOGICAL_NOT",   // !

// Regex Operators
"UNION",          // | (Union)
"STAR",          // * (Kleene Star)
"DOT",           // . (Concatenation)
  "ANY",          // _ (underscore, matches any character)

// Punctuation / Delimiters
"LPAREN",        // (
"RPAREN",        // )
"LBRACE",        // {
"RBRACE",        // }
"SEMICOLON",     // ;

// Control Tokens
"END_OF_FILE",   // Special token for the end of input
"ERROR"          // Special token for an unrecognized character
};
enum class TokenType {
  // Keywords
  KW_EVENT,      // "event
  KW_PRED,       // "pred"
  KW_REGEX,      // "regex"

  // Identifiers & Literals
  IDENTIFIER,    // e.g., "x", "P1", "int"
  LIT_INTEGER,   // e.g., 100, 20
  LIT_FLOAT,     // e.g., 100.05
  LIT_CHAR,      // e.g., 'a'
  LIT_STRING,    // e.g., "hello"

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
  UNION,          // | (Union)
  STAR,          // * (Kleene Star)
  DOT,           // . (Concatenation)
  ANY,          // _ (underscore, matches any character)

  // Punctuation / Delimiters
  LPAREN,        // (
  RPAREN,        // )
  LBRACE,        // {
  RBRACE,        // }
  SEMICOLON,     // ;

  // Control Tokens
  END_OF_FILE,   // Special token for the end of input
  ERROR          // Special token for an unrecognized character
};

struct SourceLocation {
    std::size_t line = 1;
    std::size_t column = 1;
    SourceLocation(const std::size_t line, const std::size_t column) : line(line), column(column) {}
};

struct Token {
    TokenType type{};
    std::string_view text{};
    SourceLocation location;
};


class Lexer {
private:
  std::string_view source;
  std::vector<Token> tokens;
  core::CompilationContext& compile_context;
  std::size_t start = 0; // Start of the current token.
  std::size_t current = 0; // End of the current token.
  std::size_t line = 1;         // Current line.
  std::size_t column = 1;        // Current column.

  // helper funcs.
  bool is_at_end() const {
    return (current >= source.length());
  }

  // advanced current position by 1 and return the current char.
  char advance() {
    ++column;
    return source[current++];
  }
  char advance(const std::size_t count) {
    column+=count;
    std::size_t old_curr = current;
    current+=count;
    return source[old_curr];
  }

  // return curr char without increasing curr, col
  [[nodiscard]] char peek() const {
    return source[current];
  }

  // return next char without increasing curr, col
  [[nodiscard]] char peek_next() {
    if (current + 1 >= source.length()) {
      return '\0'; // EOF
    }
    return source[current+1];
  }

  bool is_identifier() {
    // If it doesn't start with char, then not an identifier.
    if (!is_lit_char()) {
      return false;
    }
    while (is_lit_char() || is_lit_digit()) {
      advance();
    }
    if (current == start) {
      return false;
    }
    return true;
  }
  bool is_pred() {
    if ((source.substr(start, 4) == "pred") &&
        source[start+4] == ' ') {
      return true;
    }
    return false;
  }

  bool is_regex() {
    if ((source.substr(start, 5) == "regex")&&
      source[start+5] == ' ') {
      return true;
    }
    return false;
  }

  bool is_event() {
    if ((source.substr(start, 5) == "event") &&
      source[start+5] == ' ' ){
      return true;
    }
    return false;
  }

  bool is_lit_digit() {
    if (peek() - '0' >= 0 && peek() - '0' <= 9) {
      return true;
    }
    return false;
  }
  bool is_lit_int() {
    // A number cannot be followed by a name.
    while (is_lit_digit()) {
      advance();
    }
    if (current == start) {
      // error at first.
      return false;
    }
    return true;
  }
  bool is_lit_char() {
    if (((peek() - 'a' >= 0 && peek()- 'a' <= 26)
        || (peek() - 'A' >= 0 && peek() - 'A' <= 26))
        || (peek() == '_')) {
      return true;
    }
    return false;
  }
  bool is_lit_string() {
    if (!(peek() == '\'' || peek() == '\"')) {
      return false;
    }
    while (is_lit_char()) {
      advance();
    }
    if (!(peek() == '\'' || peek() == '\"')) {
      return false;
    }
    advance();
    if (current == start) {
      return false;
    }
    return true;
  }
  bool is_lit_float() {
    while (is_lit_digit()) {
      advance();
    }
    if (peek() != '.') {
      return false;
    }
    while (is_lit_digit()) {
      advance();
    }
    if (current == start) {
      return false;
    }
    return true;
  }
// LIT_FLOAT,     // e.g., 100.05
  // specific handlers.
  void scan_token() {
    char c = advance();
    switch (c) {
    // Most cases are clear by the names.
    case '(':
      add_token(TokenType::LPAREN);
      break;
    case ')':
      add_token(TokenType::RPAREN);
      break;
    case '{':
      add_token(TokenType::LBRACE);
      break;
    case '}':
      add_token(TokenType::RBRACE);
      break;
    case ';':
      add_token(TokenType::SEMICOLON);
      break;
    case '*':
      add_token(TokenType::STAR);
      break;
    case '.':
      add_token(TokenType::DOT);
      break;
    case '=':
      if (peek() == '=') {
        advance();
        add_token(TokenType::DOUBLE_EQUALS);
      }
      else {
        // it was a signal equals.
        add_token(TokenType::EQUALS);
      }
      break;
    case '&':
      if (peek() == '&') {
        advance();
        add_token(TokenType::LOGICAL_AND);
      }
      else {
        add_token(TokenType::ERROR);
      }
      break;
    case '_':
      // Assuming variables names don't start with underscore???
      add_token(TokenType::ANY);
      break;
    case '|':
      if (peek() == '|') {
        advance();
        add_token(TokenType::LOGICAL_OR);
      }
      else {
        add_token(TokenType::UNION);
      }
      break;
    case '!':
      if (peek() == '=') {
        advance();
        add_token(TokenType::NOT_EQUALS);
      }
      else {
        add_token(TokenType::LOGICAL_NOT);
      }
      break;

    case '>':
      if (peek() == '=') {
        advance();
        add_token(TokenType::GREATER_EQUAL);
      }
      else {
        add_token(TokenType::GREATER);
      }
      break;

    case '<':
      if (peek() == '=') {
        advance();
        add_token(TokenType::LESS_EQUAL);
      }
      else {
        add_token(TokenType::LESS);
      }
      break;

    case '/':
      // need to check for comments.
      if (peek() == '/') {
        // comment.
        while (peek() != '\n' && !is_at_end()) {
          advance();
        }
      }
      else if (peek() == '*') {
        // Multiline comment.
        if (advance() == '\n') {
          ++line;
          column = 1;
        }
        while (!(peek() == '*' && peek_next() ==  '/')) {
          if (advance() == '\n') {
            ++line;
            column = 1;
          }
        }
        advance(2);
      }
      else {
        // We do not support any sort of division right now.
        add_token(TokenType::ERROR);
      }
      break;

    case ' ':
      // Nothing yet. Just advance.
      break;
    case '\r':
      break;
    case '\n':
      ++line;
      column = 1;
      break;
    case '\t':
      break;
    default:
      // Need to reduce because the first advance() increased.
      --current;
      --column;
      // need to handler identifiers here.
      if (is_pred()) {
        advance(4);
        add_token(TokenType::KW_PRED);
      }
      else if (is_regex()) {
        advance(5);
        add_token(TokenType::KW_REGEX);
      }
      else if (is_event()) {
        advance(5);
        add_token(TokenType::KW_EVENT);
      }
      // else if (is_lit_char()) {
      //   advance();
      //   // std::println("Lit character found");
      //   add_token(TokenType::LIT_CHAR);
      // }
      else if (is_lit_string()) {
        add_token(TokenType::LIT_STRING);
      }
      else if (is_lit_float()) {
        add_token(TokenType::LIT_FLOAT);
      }
      else if (is_lit_int()) {
        add_token(TokenType::LIT_INTEGER);
      }
      else if (is_identifier()) {
        add_token(TokenType::IDENTIFIER);
      }
      else {
        add_token(TokenType::ERROR);
      }
      break;
    }
  }

  void add_token(TokenType type) {
    std::string_view text = source.substr(start, current - start);
    tokens.push_back(Token{type, text, SourceLocation{line, column-1}});
    compile_context.log(3,
      std::format("Found Token: type {} text {} line {} col {}",
        enum_to_str[static_cast<std::size_t>(type)], text, line, column));
  }


public:
  Lexer(std::string_view src, core::CompilationContext& ctxt) : source(src), compile_context(ctxt){}

  std::vector<Token> tokenize() {
    while (!is_at_end()) {
      // std::cout << "Curr : " << current << std::endl;
      start = current; // Reset start for the next token
      scan_token();
    }
    tokens.push_back(Token{TokenType::END_OF_FILE, "", SourceLocation{line, column}});
    compile_context.log(2, std::format("Number of tokens: {}", tokens.size()));
    return tokens;
  }

  void dump_tokens(std::string_view filename) {
    std::ofstream output_file;
    output_file.open(filename);
    if (compile_context.options.output_filename == "aion_out") {
      // This is the default filename.
    }
    std::ostream& out = ((compile_context.options.output_filename == "aion_out") ? std::cout : output_file);
    // If the name is default then dump on std::cout.
    for (Token& token : tokens) {
      out << "TYPE: " << enum_to_str[static_cast<std::size_t>(token.type)] << "\t" <<
        "TEXT: " << token.text << "\t" <<
          "LINE: " << token.location.line << "\t"
      "COL: " << token.location.column << std::endl;
    }
    output_file.close();
  }
};
}