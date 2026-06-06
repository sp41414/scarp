package org.scarp;

enum TokenType {
  // Single-character tokens.
  LEFT_PAREN,
  RIGHT_PAREN,
  LEFT_BRACE,
  RIGHT_BRACE,
  COMMA,
  DOT,
  MINUS,
  PLUS,
  SEMICOLON,
  SLASH,
  STAR,
  QUESTION,
  COLON,

  // One or two character tokens.
  BANG,
  BANG_EQUAL,
  EQUAL,
  EQUAL_EQUAL,
  GREATER,
  GREATER_EQUAL,
  LESS,
  LESS_EQUAL,
  AND_AND,
  OR_OR,

  // Bitwise operators.
  AND,
  OR,
  XOR,
  TILDE,
  SHIFT_LEFT,
  SHIFT_RIGHT,
  SHIFT_RIGHT_UNSIGNED,

  // Literals.
  IDENTIFIER,
  STRING,
  NUMBER,

  // Keywords.
  CLASS,
  ELSE,
  FALSE,
  FUNCTION,
  FOR,
  IF,
  NIL,
  PRINT,
  RETURN,
  BASE,
  SELF,
  STATIC,
  PRIVATE,
  WITH,
  TRUE,
  VAR,
  WHILE,
  BREAK,

  EOF
}
