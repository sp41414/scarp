#include "compiler.h"
#include "chunk.h"
#include "common.h"
#include "object.h"
#include "scanner.h"
#include "value.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
  Token current;
  Token previous;
  const char *source;
  bool hadError;
  bool panicMode;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_CONDITIONAL, // ? :
  PREC_OR_OR,       // ||
  PREC_AND_AND,     // &&
  PREC_OR,          // |
  PREC_XOR,         // ^
  PREC_AND,         // &
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_SHIFT,       // >> << >>>
  PREC_TERM,        // + -
  PREC_FACTOR,      // * /
  PREC_UNARY,       // ! -
  PREC_CALL,        // . ()
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

Parser parser;
Chunk *compilingChunk;

static bool match(TokenType type);
static int identifierConstant(Token *name);
static void expression(void);
static void statement(void);
static void declaration(void);
static ParseRule *getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static Chunk *currentChunk(void) { return compilingChunk; }

static void errorAt(Token *token, const char *message) {
  if (parser.panicMode)
    return;
  parser.panicMode = true;

  fprintf(stderr, "%serror%s: %s\n", RED_BOLD, RESET, message);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, "%s -->%s %d:%d\n\n", BLUE, RESET, token->line,
            token->column);
    parser.hadError = true;
    return;
  }

  const char *lineStart = token->start;
  while (lineStart > parser.source && *(lineStart - 1) != '\n') {
    lineStart--;
  }

  const char *lineEnd = token->start;
  while (*lineEnd != '\0' && *lineEnd != '\n') {
    lineEnd++;
  }
  int lineLength = (lineEnd - lineStart);

  int pad = snprintf(NULL, 0, "%d", token->line);

  fprintf(stderr, "%*s%s --> %s%d:%d\n", pad, "", BLUE, RESET, token->line,
          token->column);
  fprintf(stderr, "%*s%s |%s\n", pad, "", BLUE, RESET);
  fprintf(stderr, "%s%d |%s %.*s\n", BLUE, token->line, RESET, lineLength,
          lineStart);

  fprintf(stderr, "%*s%s |%s %*s%s", pad, "", BLUE, RESET,
          (int)(token->start - lineStart), "", RED_BOLD);
  for (int i = 0; i < token->length; i++) {
    fprintf(stderr, "^");
  }
  fprintf(stderr, "%s\n\n", RESET);

  parser.hadError = true;
}

static void error(const char *message) { errorAt(&parser.previous, message); }

static void errorAtCurrent(const char *message) {
  errorAt(&parser.current, message);
}

static void advance(void) {
  parser.previous = parser.current;
  for (;;) {
    parser.current = scanToken();
    if (parser.current.type != TOKEN_ERROR)
      break;

    errorAtCurrent(parser.current.start);
  }
}

static void consume(TokenType type, const char *message) {
  if (type == parser.current.type) {
    advance();
    return;
  }

  errorAtCurrent(message);
}

static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line,
             parser.previous.column);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

static void emitLongBytes(OpCode code, int byte) {
  emitBytes(code, (uint8_t)(byte & 0xff));
  emitBytes((uint8_t)((byte >> 8) & 0xff), (uint8_t)((byte >> 16) & 0xff));
}

static void emitReturn(void) { emitByte(OP_RETURN); }

static int makeConstant(Value value) {
  int constant = addConstant(currentChunk(), value);
  if (constant > 0xffffff) {
    error("Too many constants in one chunk.");
  }
  return constant;
}

static void emitConstant(Value value) {
  int constant = makeConstant(value);
  if (constant <= UINT8_MAX) {
    emitBytes(OP_CONSTANT, (uint8_t)constant);
  } else {
    emitLongBytes(OP_CONSTANT_LONG, constant);
  }
}

static void endCompiler(void) {
  emitReturn();
#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), "code");
  }
#endif
}

static void number(bool canAssign) {
  double value = strtod(parser.previous.start, NULL);
  emitConstant(NUMBER_VAL(value));
}

static void grouping(bool canAssign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary(bool canAssign) {
  TokenType operatorType = parser.previous.type;
  parsePrecedence(PREC_UNARY);

  switch (operatorType) {
  case TOKEN_BANG:
    emitByte(OP_NOT);
    break;
  case TOKEN_MINUS:
    emitByte(OP_NEGATE);
    break;
  default:
    return;
  }
}

static void literal(bool canAssign) {
  switch (parser.previous.type) {
  case TOKEN_NIL:
    emitByte(OP_NIL);
    break;
  case TOKEN_FALSE:
    emitByte(OP_FALSE);
    break;
  case TOKEN_TRUE:
    emitByte(OP_TRUE);
    break;
  default:
    return;
  }
}

static void binary(bool canAssign) {
  TokenType operatorType = parser.previous.type;
  ParseRule *rule = getRule(operatorType);
  parsePrecedence((Precedence)(rule->precedence + 1));

  switch (operatorType) {
  case TOKEN_BANG_EQUAL:
    emitBytes(OP_EQUAL, OP_NOT);
    break;
  case TOKEN_EQUAL_EQUAL:
    emitByte(OP_EQUAL);
    break;
  case TOKEN_GREATER:
    emitByte(OP_GREATER);
    break;
  case TOKEN_GREATER_EQUAL:
    emitBytes(OP_LESS, OP_NOT);
    break;
  case TOKEN_LESS:
    emitByte(OP_LESS);
    break;
  case TOKEN_LESS_EQUAL:
    emitBytes(OP_GREATER, OP_NOT);
    break;
  case TOKEN_PLUS:
    emitByte(OP_ADD);
    break;
  case TOKEN_MINUS:
    emitByte(OP_SUBTRACT);
    break;
  case TOKEN_STAR:
    emitByte(OP_MULTIPLY);
    break;
  case TOKEN_SLASH:
    emitByte(OP_DIVIDE);
    break;
  default:
    return;
  }
}

static void ternary(bool canAssign) {
  // TODO: ternary operator
}

static void string(bool canAssign) {
  emitConstant(OBJ_VAL(
      copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void namedVariable(Token name, bool canAssign) {
  int arg = identifierConstant(&name);
  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    if (arg <= UINT8_MAX) {
      emitBytes(OP_SET_GLOBAL, arg);
    } else {
      emitLongBytes(OP_SET_GLOBAL_LONG, arg);
    }
  } else {
    if (arg <= UINT8_MAX) {
      emitBytes(OP_GET_GLOBAL, arg);
    } else {
      emitLongBytes(OP_GET_GLOBAL_LONG, arg);
    }
  }
}

static void variable(bool canAssign) {
  namedVariable(parser.previous, canAssign);
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_QUESTION] = {NULL, ternary, PREC_CONDITIONAL},
    [TOKEN_COLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_AND_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_OR_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_XOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_TILDE] = {NULL, NULL, PREC_NONE},
    [TOKEN_SHIFT_RIGHT] = {NULL, NULL, PREC_NONE},
    [TOKEN_SHIFT_LEFT] = {NULL, NULL, PREC_NONE},
    [TOKEN_SHIFT_RIGHT_UNSIGNED] = {NULL, NULL, PREC_NONE},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUNCTION] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_BASE] = {NULL, NULL, PREC_NONE},
    [TOKEN_SELF] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_LET] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
  if (!check(type))
    return false;
  advance();
  return true;
}

static void parsePrecedence(Precedence precedence) {
  advance();

  ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (!prefixRule) {
    error("Expect expression.");
    return;
  }

  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(canAssign);

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    error("Invalid assignment target");
  }
}

static int identifierConstant(Token *name) {
  return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static int parseVariable(const char *message) {
  consume(TOKEN_IDENTIFIER, message);
  return identifierConstant(&parser.previous);
}

static void defineVariable(int global) {
  if (global <= UINT8_MAX) {
    emitBytes(OP_DEFINE_GLOBAL, global);
  } else {
    emitBytes(OP_DEFINE_GLOBAL_LONG, (uint8_t)(global & 0xff));
    emitBytes((uint8_t)((global >> 8) & 0xff),
              (uint8_t)((global >> 16) & 0xff));
  }
}

static ParseRule *getRule(TokenType type) { return &rules[type]; }

static void expression(void) { parsePrecedence(PREC_ASSIGNMENT); }

static void synchronize(void) {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON)
      return;

    switch (parser.current.type) {
    case TOKEN_CLASS:
    case TOKEN_FUNCTION:
    case TOKEN_LET:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
      return;
    default:
      break;
    }
    advance();
  }
}

static void printStatement(void) {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after statement");
  emitByte(OP_PRINT);
}

static void expressionStatement(void) {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression");
  emitByte(OP_POP);
}

static void varDeclaration(void) {
  int global = parseVariable("Expect variable name");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitByte(OP_NIL);
  }

  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration");

  defineVariable(global);
}

static void statement(void) {
  if (match(TOKEN_PRINT)) {
    printStatement();
  } else {
    expressionStatement();
  }
}

static void declaration(void) {
  if (match(TOKEN_LET)) {
    varDeclaration();
  } else {
    statement();
  }
  if (parser.panicMode)
    synchronize();
}

bool compile(const char *source, Chunk *chunk) {
  initScanner(source);
  compilingChunk = chunk;

  parser.source = source;
  parser.hadError = false;
  parser.panicMode = false;

  advance();

  while (!match(TOKEN_EOF))
    declaration();

  endCompiler();
  return !parser.hadError;
}
