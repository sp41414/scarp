#include "scanner.h"
#include "common.h"
#include <string.h>

typedef struct {
  const char *start;
  const char *current;
  int columnStart;
  int column;
  int line;
  int commentDepth;
} Scanner;

Scanner scanner;

void initScanner(const char *source) {
  scanner.start = source;
  scanner.current = source;
  scanner.columnStart = 1;
  scanner.column = 1;
  scanner.line = 1;
}

static bool isDigit(char c) { return c >= '0' && c <= '9'; }

static bool isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static Token makeToken(TokenType type) {
  Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (int)(scanner.current - scanner.start);
  token.line = scanner.line;
  token.column = scanner.columnStart;

  return token;
}

static Token errorToken(const char *message) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner.line;
  token.column = scanner.columnStart;

  return token;
}

static bool isAtEnd(void) { return *scanner.current == '\0'; }

static char advance(void) {
  scanner.current++;
  scanner.column++;
  return scanner.current[-1];
}

static char peek(void) { return *scanner.current; }

static char peekNext(void) {
  if (isAtEnd())
    return '\0';
  return scanner.current[1];
}

static bool match(char expected) {
  if (isAtEnd())
    return false;
  if (*scanner.current != expected)
    return false;
  scanner.current++;
  scanner.column++;
  return true;
}

static void multiLineComment(void) {
  while (scanner.commentDepth != 0) {
    if (isAtEnd()) {
      errorToken("Unterminated multi-line comment.");
      return;
    }

    if (peek() == '\n') {
      scanner.line++;
      scanner.column = 0;
      advance();
    }

    if (peek() == '*' && peekNext() == '/') {
      scanner.commentDepth--;
      advance();
    } else if (peek() == '/' && peekNext() == '*') {
      scanner.commentDepth++;
      advance();
    }
    advance();
  }
}

static void skipWhitespace(void) {
  for (;;) {
    char c = peek();
    switch (c) {
    case ' ':
    case '\r':
    case '\t':
      advance();
      break;
    case '\n':
      scanner.line++;
      scanner.column = 0;
      advance();
      break;
    case '/':
      if (peekNext() == '/') {
        while (peek() != '\n' && !isAtEnd())
          advance();
      } else if (peekNext() == '*') {
        scanner.commentDepth++;
        advance();
        advance();
        multiLineComment();
      } else {
        return;
      }
      break;
    default:
      return;
    }
  }
}

static Token string(void) {
  while (peek() != '"' && !isAtEnd()) {
    if (peek() == '\n') {
      scanner.line++;
      scanner.column = 0;
    }
    advance();
  }

  if (isAtEnd())
    return errorToken("Unterminated string.");

  advance();
  return makeToken(TOKEN_STRING);
}

static Token number(void) {
  while (isDigit(peek()))
    advance();

  if (peek() == '.' && isDigit(peekNext())) {
    advance();
    while (isDigit(peek()))
      advance();
  }

  return makeToken(TOKEN_NUMBER);
}

static TokenType checkKeyword(int start, int length, const char *rest,
                              TokenType type) {
  if (scanner.current - scanner.start == start + length &&
      memcmp(scanner.start + start, rest, length) == 0) {
    return type;
  }

  return TOKEN_IDENTIFIER;
}

static TokenType identifierType(void) {
  int length = (int)(scanner.current - scanner.start);

  switch (scanner.start[0]) {
  case 'b':
    if (length < 2)
      break;
    switch (scanner.start[1]) {
    case 'r':
      return checkKeyword(2, 3, "eak", TOKEN_BREAK);
    case 'a':
      return checkKeyword(2, 2, "ase", TOKEN_BASE);
    }
    break;
  case 'c':
    if (length < 2)
      break;
    switch (scanner.start[1]) {
    case 'a':
      return checkKeyword(2, 2, "se", TOKEN_CASE);
    case 'l':
      return checkKeyword(2, 3, "ass", TOKEN_CLASS);
    case 'o':
      if (length < 4)
        break;
      switch (scanner.start[3]) {
      case 't':
        return checkKeyword(4, 4, "inue", TOKEN_CONTINUE);
      case 's':
        return checkKeyword(4, 1, "t", TOKEN_CONST);
      }
      break;
    }
    break;
  case 'd':
    return checkKeyword(1, 6, "efault", TOKEN_DEFAULT);
  case 'e':
    return checkKeyword(1, 3, "lse", TOKEN_ELSE);
  case 'f':
    if (length > 1) {
      switch (scanner.start[1]) {
      case 'a':
        return checkKeyword(2, 3, "lse", TOKEN_FALSE);
      case 'o':
        return checkKeyword(2, 1, "r", TOKEN_FOR);
      }
    }
    return checkKeyword(1, 1, "n", TOKEN_FUNCTION);
  case 'i':
    return checkKeyword(1, 1, "f", TOKEN_IF);
  case 'l':
    return checkKeyword(1, 2, "et", TOKEN_LET);
  case 'n':
    return checkKeyword(1, 2, "il", TOKEN_NIL);
  case 'p':
    return checkKeyword(1, 4, "rint", TOKEN_PRINT);
  case 'r':
    return checkKeyword(1, 2, "et", TOKEN_RETURN);
  case 's':
    if (length < 2)
      break;
    switch (scanner.start[1]) {
    case 'e':
      return checkKeyword(2, 2, "lf", TOKEN_SELF);
    case 'w':
      return checkKeyword(2, 4, "itch", TOKEN_SWITCH);
    }
    break;
  case 't':
    return checkKeyword(1, 3, "rue", TOKEN_TRUE);
  case 'w':
    return checkKeyword(1, 4, "hile", TOKEN_WHILE);
  }
  return TOKEN_IDENTIFIER;
}

static Token identifier(void) {
  while (isAlpha(peek()) || isDigit(peek()))
    advance();

  return makeToken(identifierType());
}

Token scanToken(void) {
  skipWhitespace();
  scanner.start = scanner.current;
  scanner.columnStart = scanner.column;

  if (isAtEnd())
    return makeToken(TOKEN_EOF);

  char c = advance();
  if (isAlpha(c))
    return identifier();
  if (isDigit(c))
    return number();

  switch (c) {
  case '(':
    return makeToken(TOKEN_LEFT_PAREN);
  case ')':
    return makeToken(TOKEN_RIGHT_PAREN);
  case '{':
    return makeToken(TOKEN_LEFT_BRACE);
  case '}':
    return makeToken(TOKEN_RIGHT_BRACE);
  case ';':
    return makeToken(TOKEN_SEMICOLON);
  case ',':
    return makeToken(TOKEN_COMMA);
  case '.':
    return makeToken(TOKEN_DOT);
  case '-':
    return makeToken(TOKEN_MINUS);
  case '+':
    return makeToken(TOKEN_PLUS);
  case '/':
    return makeToken(TOKEN_SLASH);
  case '*':
    return makeToken(TOKEN_STAR);
  case '?':
    return makeToken(TOKEN_QUESTION);
  case ':':
    return makeToken(TOKEN_COLON);
  case '^':
    return makeToken(TOKEN_XOR);
  case '~':
    return makeToken(TOKEN_TILDE);
  case '!':
    return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
  case '=':
    return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
  case '<':
    if (match('<')) {
      return makeToken(TOKEN_SHIFT_LEFT);
    } else if (match('=')) {
      return makeToken(TOKEN_LESS_EQUAL);
    } else {
      return makeToken(TOKEN_LESS);
    }
  case '>':
    if (match('>')) {
      if (match('>')) {
        return makeToken(TOKEN_SHIFT_RIGHT_UNSIGNED);
      }
      return makeToken(TOKEN_SHIFT_RIGHT);
    } else if (match('=')) {
      return makeToken(TOKEN_GREATER_EQUAL);
    } else {
      return makeToken(TOKEN_GREATER);
    }
  case '&':
    return makeToken(match('&') ? TOKEN_AND_AND : TOKEN_AND);
  case '|':
    return makeToken(match('|') ? TOKEN_OR_OR : TOKEN_OR);
  case '"':
    return string();
  }

  return errorToken("Unexpected character.");
}
