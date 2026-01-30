/* lexer.cppm
 * Breaks raw file strings into chunks
 * This simplifies the parser so that it doesn't
 * deal with whitespace or comments
 */

export module aion.frontend:lexer;

import std;

export enum class TokenType {
  // Keywords
  KW_EVENT,      // "event"
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