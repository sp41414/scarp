#include "compiler.h"
#include "chunk.h"
#include "common.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"
#include "table.h"
#include "value.h"
#include "vm.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

#define MAX_CASES 256
#define MAX_BREAKS 64

typedef struct {
  Token current;
  Token previous;
  const char *source;
  bool hadError;
  bool panicMode;
} Parser;

typedef enum { SWITCH_NONE, SWITCH_CASE, SWITCH_DEFAULT } SwitchState;

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
  PREC_UNARY,       // ! ~ -
  PREC_CALL,        // . ()
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

typedef struct {
  Token name;
  int depth;
  bool isConst;
  bool isCaptured;
} Local;

typedef struct Loop {
  struct Loop *enclosing;
  int scopeDepth;
  int jumpAddress;
  int breakJumps[MAX_BREAKS];
  int breakCount;
} Loop;

typedef struct {
  uint16_t index;
  bool isLocal;
} Upvalue;

typedef enum {
  TYPE_SCRIPT,
  TYPE_FUNCTION,
  TYPE_INITIALIZER,
  TYPE_METHOD,
  TYPE_LAMBDA
} FunctionType;

typedef struct Compiler {
  struct Compiler *enclosing;
  ObjFunction *function;
  FunctionType type;
  Upvalue *upvalues;
  int upvalueCapacity;
  Local *locals;
  int localCount;
  int localCapacity;
  int scopeDepth;
} Compiler;

typedef struct ClassCompiler {
  struct ClassCompiler *enclosing;
} ClassCompiler;

Parser parser;
Compiler *current = NULL;
ClassCompiler *currentClass = NULL;
Loop *currentLoop = NULL;

static bool match(TokenType type);
static bool check(TokenType type);
static int globalIdentifierConstant(Token *name);
static void expression(void);
static void statement(void);
static void declaration(void);
static void varDeclaration(bool isConst);
static void function(FunctionType type);
static ParseRule *getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static void growLocalsCapacity(Compiler *compiler) {
  if (compiler->localCapacity < compiler->localCount + 1) {
    compiler->localCapacity = GROW_CAPACITY(compiler->localCapacity);

    if (compiler->localCapacity > UINT16_MAX)
      compiler->localCapacity = UINT16_MAX;

    compiler->locals =
        realloc(compiler->locals, sizeof(Local) * compiler->localCapacity);
  }
}

static void growUpvaluesCapacity(Compiler *compiler) {
  if (compiler->upvalueCapacity < compiler->function->upvalueCount + 1) {
    compiler->upvalueCapacity = GROW_CAPACITY(compiler->upvalueCapacity);

    if (compiler->upvalueCapacity > UINT16_MAX)
      compiler->upvalueCapacity = UINT16_MAX;

    compiler->upvalues = realloc(compiler->upvalues,
                                 sizeof(Upvalue) * compiler->upvalueCapacity);
  }
}

static void initCompiler(Compiler *compiler, FunctionType type) {
  compiler->enclosing = current;
  compiler->function = NULL;
  compiler->scopeDepth = 0;
  compiler->type = type;
  compiler->upvalues = NULL;
  compiler->upvalueCapacity = 0;
  compiler->localCapacity = 0;
  compiler->localCount = 0;
  compiler->locals = NULL;
  growLocalsCapacity(compiler);
  compiler->function = newFunction();
  growUpvaluesCapacity(compiler);
  current = compiler;

  if (type == TYPE_FUNCTION || type == TYPE_METHOD ||
      type == TYPE_INITIALIZER) {
    current->function->name =
        copyString(parser.previous.start, parser.previous.length);
  } else if (type == TYPE_LAMBDA) {
    current->function->name = COPY_LITERAL("lambda");
  }

  Local *local = &current->locals[current->localCount++];
  local->depth = 0;
  if (type == TYPE_METHOD || type == TYPE_INITIALIZER) {
    local->name.start = "self";
    local->name.length = 4;
  } else {
    local->name.start = "";
    local->name.length = 0;
  }
  local->isCaptured = false;
}

static Chunk *currentChunk(void) { return &current->function->chunk; }

static void errorAt(Token *token, const char *message) {
  if (parser.panicMode)
    return;
  parser.panicMode = true;

  fprintf(stderr, "%serror%s: %s\n", RED_BOLD, RESET, message);

  if (token->type == TOKEN_EOF || token->type == TOKEN_ERROR) {
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

static void emitShortBytes(OpCode code, int byte) {
  emitBytes(code, (uint8_t)(byte & 0xff));
  emitByte((uint8_t)((byte >> 8) & 0xff));
}

static void emitLongBytes(OpCode code, int byte) {
  emitBytes(code, (uint8_t)(byte & 0xff));
  emitBytes((uint8_t)((byte >> 8) & 0xff), (uint8_t)((byte >> 16) & 0xff));
}

static int emitJump(uint8_t instruction) {
  emitByte(instruction);
  emitBytes(0xff, 0xff);
  return currentChunk()->count - 2;
}

static void emitLoop(int loopStart) {
  emitByte(OP_LOOP);
  int offset = currentChunk()->count - loopStart + 2;
  if (offset > UINT16_MAX)
    error("Loop body too large.");

  emitBytes(offset & 0xff, (offset >> 8) & 0xff);
}

static void emitReturn(void) {
  if (current->type == TYPE_INITIALIZER) {
    emitBytes(OP_GET_LOCAL, 0);
  } else {
    emitByte(OP_NIL);
  }

  emitByte(OP_RETURN);
}

static int makeConstant(Value value) {
  int constant = addConstant(currentChunk(), value);
  if (constant > 0xffffff) {
    error("Too many constants in one chunk.");
  }
  return constant;
}

static void emitConstant(OpCode code, Value value) {
  int constant = makeConstant(value);
  if (constant <= UINT8_MAX) {
    emitBytes(code, (uint8_t)constant);
  } else {
    emitLongBytes(code + 1, constant);
  }
}

static void patchJump(int offset) {
  int jump = currentChunk()->count - offset - 2;
  if (jump > UINT16_MAX) {
    error("Too much code to jump over");
  }

  currentChunk()->code[offset] = jump & 0xff;
  currentChunk()->code[offset + 1] = (jump >> 8) & 0xff;
}

static ObjFunction *endCompiler(void) {
  emitReturn();
  FREE_ARRAY(Local, current->locals, current->localCapacity);
  FREE_ARRAY(Upvalue, current->upvalues, current->upvalueCapacity);
  ObjFunction *function = current->function;
#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), function->name != NULL
                                         ? function->name->chars
                                         : "<script>");
  }
#endif
  current = current->enclosing;
  return function;
}

static void beginScope(void) { current->scopeDepth++; }

static void endScope(void) {
  current->scopeDepth--;
  int n = 0;
  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth > current->scopeDepth) {
    if (current->locals[current->localCount - 1].isCaptured) {
      while (n > 0) {
        uint8_t toPop = n > UINT8_MAX ? UINT8_MAX : n;
        emitBytes(OP_POPN, toPop);
        n -= toPop;
      }
      emitByte(OP_CLOSE_UPVALUE);
    } else {
      n++;
    }
    current->localCount--;
  }

  while (n > 0) {
    uint8_t toPop = n > UINT8_MAX ? UINT8_MAX : n;
    emitBytes(OP_POPN, toPop);
    n -= toPop;
  }
}

static void number(bool canAssign) {
  double value = strtod(parser.previous.start, NULL);
  emitConstant(OP_CONSTANT, NUMBER_VAL(value));
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
  case TOKEN_TILDE:
    emitByte(OP_BIN_NOT);
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
  case TOKEN_OR:
    emitByte(OP_BIN_OR);
    break;
  case TOKEN_AND:
    emitByte(OP_BIN_AND);
    break;
  case TOKEN_XOR:
    emitByte(OP_BIN_XOR);
    break;
  case TOKEN_SHIFT_LEFT:
    emitByte(OP_BIN_SHIFT_LEFT);
    break;
  case TOKEN_SHIFT_RIGHT:
    emitByte(OP_BIN_SHIFT_RIGHT);
    break;
  case TOKEN_SHIFT_RIGHT_UNSIGNED:
    emitByte(OP_BIN_SHIFT_RIGHT_UNSIGNED);
    break;
  default:
    return;
  }
}

static void ternary(bool canAssign) {
  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  parsePrecedence(PREC_CONDITIONAL);
  int elseJump = emitJump(OP_JUMP);
  patchJump(thenJump);

  consume(TOKEN_COLON, "Expect ':' after then condition");

  emitByte(OP_POP);
  parsePrecedence(PREC_CONDITIONAL);
  patchJump(elseJump);
}

static uint8_t argumentList(void) {
  uint8_t argCount = 0;
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression();
      if (argCount == UINT8_MAX) {
        error("Can't have more than 255 arguments");
      }
      argCount++;
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments");

  return argCount;
}

static void call(bool canAssign) {
  uint8_t argCount = argumentList();
  emitBytes(OP_CALL, argCount);
}

static void dot(bool canAssign) {
  consume(TOKEN_IDENTIFIER, "Expect property name after '.'");

  ObjString *name = copyString(parser.previous.start, parser.previous.length);
  push(OBJ_VAL(name));

  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitConstant(OP_SET_PROPERTY, OBJ_VAL(name));
  } else if (match(TOKEN_LEFT_PAREN)) {
    uint8_t argCount = argumentList();
    emitConstant(OP_INVOKE, OBJ_VAL(name));
    emitByte(argCount);
  } else {
    emitConstant(OP_GET_PROPERTY, OBJ_VAL(name));
  }
  pop();
}

static void string(bool canAssign) {
  emitConstant(OP_CONSTANT, OBJ_VAL(copyString(parser.previous.start + 1,
                                               parser.previous.length - 2)));
}

static bool identifiersEqual(Token *id1, Token *id2) {
  if (id1->length != id2->length)
    return false;

  return memcmp(id1->start, id2->start, id1->length) == 0;
}

static int resolveLocal(Compiler *compiler, Token *name) {
  for (int i = compiler->localCount - 1; i >= 0; --i) {
    Local *local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer");
      }
      return i;
    }
  }

  return -1;
}

static int addUpvalue(Compiler *compiler, int idx, bool isLocal) {
  int upvalueCount = compiler->function->upvalueCount;
  if (upvalueCount == UINT16_MAX) {
    error("Too many closure variables in function");
    return 0;
  }

  for (int i = 0; i < upvalueCount; ++i) {
    Upvalue *upvalue = &compiler->upvalues[i];
    if (upvalue->index == idx && upvalue->isLocal == isLocal) {
      return i;
    }
  }

  growUpvaluesCapacity(compiler);

  compiler->upvalues[upvalueCount].isLocal = isLocal;
  compiler->upvalues[upvalueCount].index = idx;
  return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler *compiler, Token *name) {
  if (compiler->enclosing == NULL)
    return -1;

  int local = resolveLocal(compiler->enclosing, name);
  if (local != -1) {
    compiler->enclosing->locals[local].isCaptured = true;
    return addUpvalue(compiler, local, true);
  }

  int upvalue = resolveUpvalue(compiler->enclosing, name);
  if (upvalue != -1) {
    return addUpvalue(compiler, upvalue, false);
  }

  return -1;
}

static void namedVariable(Token name, bool canAssign) {
  uint8_t getOp, setOp;
  bool isLocalConst = false;

  int arg = resolveLocal(current, &name);
  if (arg != -1) {
    if (arg <= UINT8_MAX) {
      getOp = OP_GET_LOCAL;
      setOp = OP_SET_LOCAL;
    } else {
      getOp = OP_GET_LOCAL_LONG;
      setOp = OP_SET_LOCAL_LONG;
    }
    isLocalConst = current->locals[arg].isConst;
  } else if ((arg = resolveUpvalue(current, &name)) != -1) {
    if (arg <= UINT8_MAX) {
      getOp = OP_GET_UPVALUE;
      setOp = OP_SET_UPVALUE;
    } else {
      getOp = OP_GET_UPVALUE_LONG;
      setOp = OP_SET_UPVALUE_LONG;
    }
  } else {
    arg = globalIdentifierConstant(&name);
    if (arg <= UINT8_MAX) {
      getOp = OP_GET_GLOBAL;
      setOp = OP_SET_GLOBAL;
    } else {
      getOp = OP_GET_GLOBAL_LONG;
      setOp = OP_SET_GLOBAL_LONG;
    }
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    if (isLocalConst) {
      error("Cannot reassign to a constant variable");
    }
    expression();
    if (arg <= UINT8_MAX) {
      emitBytes(setOp, arg);
    } else if (setOp == OP_SET_LOCAL_LONG) {
      emitShortBytes(setOp, arg);
    } else {
      emitLongBytes(setOp, arg);
    }
  } else {
    if (arg <= UINT8_MAX) {
      emitBytes(getOp, arg);
    } else if (getOp == OP_GET_LOCAL_LONG) {
      emitShortBytes(getOp, arg);
    } else {
      emitLongBytes(getOp, arg);
    }
  }
}

static void variable(bool canAssign) {
  namedVariable(parser.previous, canAssign);
}

static void and_and(bool canAssign) {
  int endJump = emitJump(OP_JUMP_IF_FALSE);

  emitByte(OP_POP);
  parsePrecedence(PREC_AND_AND);

  patchJump(endJump);
}

static void or_or(bool canAssign) {
  int elseJump = emitJump(OP_JUMP_IF_FALSE);
  int jump = emitJump(OP_JUMP);

  patchJump(elseJump);
  emitByte(OP_POP);
  parsePrecedence(PREC_OR_OR);
  patchJump(jump);
}

static void lambda(bool canAssign) {
  if (!check(TOKEN_LEFT_PAREN)) {
    errorAtCurrent("Expect '(' after 'fn' in lambda expression");
    return;
  }

  function(TYPE_LAMBDA);
}

static void self(bool canAssign) {
  if (currentClass == NULL) {
    error("Cannot use 'self' outside of a class");
    return;
  }

  variable(false);
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, dot, PREC_CALL},
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
    [TOKEN_AND_AND] = {NULL, and_and, PREC_AND_AND},
    [TOKEN_OR_OR] = {NULL, or_or, PREC_OR_OR},
    [TOKEN_AND] = {NULL, binary, PREC_AND},
    [TOKEN_OR] = {NULL, binary, PREC_OR},
    [TOKEN_XOR] = {NULL, binary, PREC_XOR},
    [TOKEN_TILDE] = {unary, NULL, PREC_UNARY},
    [TOKEN_SHIFT_RIGHT] = {NULL, binary, PREC_SHIFT},
    [TOKEN_SHIFT_LEFT] = {NULL, binary, PREC_SHIFT},
    [TOKEN_SHIFT_RIGHT_UNSIGNED] = {NULL, binary, PREC_SHIFT},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUNCTION] = {lambda, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_BASE] = {NULL, NULL, PREC_NONE},
    [TOKEN_SELF] = {self, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_LET] = {NULL, NULL, PREC_NONE},
    [TOKEN_CONST] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_SWITCH] = {NULL, NULL, PREC_NONE},
    [TOKEN_BREAK] = {NULL, NULL, PREC_NONE},
    [TOKEN_CONTINUE] = {NULL, NULL, PREC_NONE},
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

static int globalIdentifierConstant(Token *name) {
  Value string = OBJ_VAL(copyString(name->start, name->length));
  push(string);

  Value idx;
  if (tableGet(&vm.globalNames, string, &idx)) {
    pop();
    return (int)AS_NUMBER(idx);
  }

  int newIdx = vm.globalValues.count;
  writeValueArray(&vm.globalValues, UNDEFINED_VAL);

  tableSet(&vm.globalNames, string, NUMBER_VAL((double)newIdx));
  pop();
  return newIdx;
}

static void addLocal(bool isConst, Token token) {
  if (current->localCount >= UINT16_MAX) {
    error("Too many local variables in function");
    return;
  }

  growLocalsCapacity(current);

  Local *local = &current->locals[current->localCount++];
  local->name = token;
  local->depth = -1;
  local->isConst = isConst;
  local->isCaptured = false;
}

static void declareVariable(bool isConst) {
  if (current->scopeDepth == 0)
    return;

  Token *name = &parser.previous;

  for (int i = current->localCount - 1; i >= 0; --i) {
    Local *local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break;
    }

    if (identifiersEqual(name, &local->name)) {
      error("Already a variable with this name in this scope.");
    }
  }

  addLocal(isConst, *name);
}

static int parseVariable(bool isConst, const char *message) {
  consume(TOKEN_IDENTIFIER, message);

  declareVariable(isConst);
  if (current->scopeDepth > 0)
    return 0;

  return globalIdentifierConstant(&parser.previous);
}

static void markInitialized(void) {
  if (current->scopeDepth == 0)
    return;
  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(bool isConst, int global) {
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }

  if (global <= UINT8_MAX) {
    emitBytes(isConst ? OP_DEFINE_GLOBAL_CONST : OP_DEFINE_GLOBAL,
              (uint8_t)global);
  } else {
    emitLongBytes(isConst ? OP_DEFINE_GLOBAL_CONST_LONG : OP_DEFINE_GLOBAL_LONG,
                  global);
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
    case TOKEN_CONST:
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

static inline void beginLoop(Loop *loop, int loopStart) {
  loop->enclosing = currentLoop;
  loop->scopeDepth = current->scopeDepth;
  loop->jumpAddress = loopStart;
  loop->breakCount = 0;

  currentLoop = loop;
}

static inline void endLoop(void) { currentLoop = currentLoop->enclosing; }

static void forStatement(void) {
  beginScope();
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'");

  int loopVariable = -1;
  Token loopVariableName;

  bool isConst = false;

  if (match(TOKEN_SEMICOLON)) {
  } else if (match(TOKEN_LET)) {
    loopVariableName = parser.current;
    varDeclaration(false);
    loopVariable = current->localCount - 1;
  } else if (match(TOKEN_CONST)) {
    isConst = true;
    varDeclaration(isConst);
  } else {
    expressionStatement();
  }

  int loopStart = currentChunk()->count;
  int exitJump = -1;
  if (!match(TOKEN_SEMICOLON)) {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after loop condition");

    exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
  }

  if (!match(TOKEN_RIGHT_PAREN)) {
    int bodyLoop = emitJump(OP_JUMP);
    int incrementStart = currentChunk()->count;
    expression();
    emitByte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses");

    emitLoop(loopStart);
    loopStart = incrementStart;
    patchJump(bodyLoop);
  }

  Loop loop;
  beginLoop(&loop, loopStart);

  int innerLoopVariable = -1;
  if (loopVariable != -1 && !isConst) {
    beginScope();
    if (loopVariable <= UINT8_MAX) {
      emitBytes(OP_GET_LOCAL, loopVariable);
    } else {
      emitShortBytes(OP_GET_LOCAL_LONG, loopVariable);
    }
    addLocal(false, loopVariableName);
    markInitialized();

    innerLoopVariable = current->localCount - 1;
  }

  statement();

  if (loopVariable != -1 && !isConst) {
    if (innerLoopVariable <= UINT8_MAX) {
      emitBytes(OP_GET_LOCAL, innerLoopVariable);
    } else {
      emitShortBytes(OP_GET_LOCAL_LONG, innerLoopVariable);
    }

    if (loopVariable <= UINT8_MAX) {
      emitBytes(OP_SET_LOCAL, loopVariable);
    } else {
      emitShortBytes(OP_SET_LOCAL_LONG, loopVariable);
    }

    emitByte(OP_POP);
    endScope();
  }

  emitLoop(loopStart);
  if (exitJump != -1) {
    patchJump(exitJump);
    emitByte(OP_POP);
  }
  for (int i = 0; i < loop.breakCount; ++i) {
    patchJump(loop.breakJumps[i]);
  }
  endScope();
  endLoop();
}

static void ifStatement(void) {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition");

  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();
  int elseJump = emitJump(OP_JUMP);

  patchJump(thenJump);
  emitByte(OP_POP);

  if (match(TOKEN_ELSE))
    statement();

  patchJump(elseJump);
}

static void closeCase(int *exitJumps, int *exitCount, int previousCaseSkip) {
  if (*exitCount < MAX_CASES) {
    exitJumps[(*exitCount)++] = emitJump(OP_JUMP);
  } else {
    error("Too many cases in switch");
  }
  patchJump(previousCaseSkip);
  emitByte(OP_POP);
}

static void switchStatement(void) {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'switch'");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression");

  consume(TOKEN_LEFT_BRACE, "Expect '{'");
  beginScope();

  int exitJumps[MAX_CASES];
  int exitCount = 0;
  int previousCaseSkip = -1;
  SwitchState state = SWITCH_NONE;
  while (!match(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    if (match(TOKEN_CASE) || match(TOKEN_DEFAULT)) {
      TokenType caseType = parser.previous.type;

      if (state == SWITCH_DEFAULT) {
        error("Cannot have another 'case' or 'default' after the default case");
      }

      if (state == SWITCH_CASE) {
        closeCase(exitJumps, &exitCount, previousCaseSkip);
      }

      if (caseType == TOKEN_CASE) {
        state = SWITCH_CASE;
        emitByte(OP_DUP);
        expression();
        consume(TOKEN_COLON, "Expect ':' after case expression");

        emitByte(OP_EQUAL);
        previousCaseSkip = emitJump(OP_JUMP_IF_FALSE);

        emitByte(OP_POP);
      } else {
        state = SWITCH_DEFAULT;
        consume(TOKEN_COLON, "Expect ':' after 'default'");
        previousCaseSkip = -1;
      }
    } else {
      if (state == SWITCH_NONE) {
        error("Cannot have any statements before 'case' or 'default'");
      }
      statement();
    }
  }

  if (state == SWITCH_CASE) {
    closeCase(exitJumps, &exitCount, previousCaseSkip);
  }

  endScope();

  for (int i = 0; i < exitCount; ++i) {
    patchJump(exitJumps[i]);
  }
  emitByte(OP_POP);
}

static void whileStatement(void) {
  int loopStart = currentChunk()->count;
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition");

  Loop loop;
  beginLoop(&loop, loopStart);

  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();
  emitLoop(loopStart);

  patchJump(exitJump);
  emitByte(OP_POP);

  for (int i = 0; i < loop.breakCount; ++i) {
    patchJump(loop.breakJumps[i]);
  }
  endLoop();
}

static void emitLoopPops(void) {
  int localsToPop = 0;
  for (int i = current->localCount - 1; i >= 0; --i) {
    if (current->locals[i].depth > currentLoop->scopeDepth) {
      if (current->locals[i].isCaptured) {
        while (localsToPop > 0) {
          uint8_t toPop = localsToPop > UINT8_MAX ? UINT8_MAX : localsToPop;
          emitBytes(OP_POPN, toPop);
          localsToPop -= toPop;
        }
        emitByte(OP_CLOSE_UPVALUE);
      } else {
        localsToPop++;
      }
    } else {
      break;
    }
  }

  while (localsToPop > 0) {
    uint8_t toPop = localsToPop > UINT8_MAX ? UINT8_MAX : localsToPop;
    emitBytes(OP_POPN, toPop);
    localsToPop -= toPop;
  }
}

static void continueStatement(void) {
  if (currentLoop == NULL)
    error("Cannot use 'continue' outside of a loop");
  consume(TOKEN_SEMICOLON, "Expect ';' after 'continue'");
  emitLoopPops();
  emitLoop(currentLoop->jumpAddress);
}

static void breakStatement(void) {
  if (currentLoop == NULL)
    error("Cannot use 'break' outside of a loop");
  consume(TOKEN_SEMICOLON, "Expect ';' after 'break'");
  if (currentLoop->breakCount + 1 > MAX_BREAKS) {
    error("Too many break statements in a loop");
  }

  emitLoopPops();
  currentLoop->breakJumps[currentLoop->breakCount++] = emitJump(OP_JUMP);
}

static void returnStatement(void) {
  if (current->type == TYPE_SCRIPT) {
    error("Can't return from top-level code");
  }

  if (match(TOKEN_SEMICOLON)) {
    emitReturn();
  } else {
    if (current->type == TYPE_INITIALIZER) {
      error("Can't return a value from an initializer");
    }
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after return");
    emitByte(OP_RETURN);
  }
}

static void varDeclaration(bool isConst) {
  int global = parseVariable(isConst, "Expect variable name");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else if (isConst) {
    error("Constant variables must be initialized at the same time they are "
          "declared");
    emitByte(OP_NIL);
  } else {
    emitByte(OP_NIL);
  }

  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration");

  defineVariable(isConst, global);
}

static void block(void) {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block");
}

static void function(FunctionType type) {
  Compiler compiler;
  initCompiler(&compiler, type);
  beginScope();

  consume(TOKEN_LEFT_PAREN, "Expect '(' after function name");
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->arity++;
      if (current->function->arity > 255) {
        errorAtCurrent("Cannot define more than 255 parameters");
      }
      int constant = parseVariable(false, "Expected parameter name");
      defineVariable(false, constant);
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters");

  consume(TOKEN_LEFT_BRACE, "Expect '{' before function body");
  block();

  ObjFunction *tempFunction = current->function;
  int upvalueCount = tempFunction->upvalueCount;

  Upvalue tempUpvalues[upvalueCount];
  for (int i = 0; i < upvalueCount; ++i) {
    tempUpvalues[i] = current->upvalues[i];
  }

  ObjFunction *function = endCompiler();

  if (function->upvalueCount > 0) {
    emitConstant(OP_CLOSURE, OBJ_VAL(function));

    for (int i = 0; i < upvalueCount; ++i) {
      Upvalue upvalue = tempUpvalues[i];
      emitShortBytes(upvalue.isLocal ? 1 : 0, upvalue.index);
    }
  } else {
    emitConstant(OP_CONSTANT, OBJ_VAL(function));
  }
}

static void fnDeclaration(void) {
  int global = parseVariable(false, "Expect function name");
  markInitialized();
  function(TYPE_FUNCTION);
  defineVariable(false, global);
}

static void method(void) {
  consume(TOKEN_IDENTIFIER, "Expect method name");
  Value name =
      OBJ_VAL(copyString(parser.previous.start, parser.previous.length));
  push(name);

  FunctionType type = TYPE_METHOD;
  if (parser.previous.length == 4 &&
      memcmp(parser.previous.start, "init", 4) == 0) {
    type = TYPE_INITIALIZER;
  }
  function(type);

  emitConstant(OP_METHOD, name);

  pop();
}

static void classDeclaration(void) {
  consume(TOKEN_IDENTIFIER, "Expect class name");
  Token name = parser.previous;

  int nameSymbol = 0;
  if (current->scopeDepth > 0) {
    declareVariable(false);
  } else {
    nameSymbol = globalIdentifierConstant(&name);
  }

  Value nameConstant = OBJ_VAL(copyString(name.start, name.length));
  push(nameConstant);
  emitConstant(OP_CLASS, nameConstant);
  pop();

  defineVariable(false, nameSymbol);

  ClassCompiler classCompiler;
  classCompiler.enclosing = currentClass;
  currentClass = &classCompiler;

  namedVariable(name, false);
  consume(TOKEN_LEFT_BRACE, "Expect '{' before class body");
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    method();
  }
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body");
  emitByte(OP_POP);

  currentClass = currentClass->enclosing;
}

static void statement(void) {
  if (match(TOKEN_PRINT)) {
    printStatement();
  } else if (match(TOKEN_FOR)) {
    forStatement();
  } else if (match(TOKEN_IF)) {
    ifStatement();
  } else if (match(TOKEN_SWITCH)) {
    switchStatement();
  } else if (match(TOKEN_WHILE)) {
    whileStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } else if (match(TOKEN_CONTINUE)) {
    continueStatement();
  } else if (match(TOKEN_BREAK)) {
    breakStatement();
  } else if (match(TOKEN_RETURN)) {
    returnStatement();
  } else {
    expressionStatement();
  }
}

static void declaration(void) {
  if (match(TOKEN_CLASS)) {
    classDeclaration();
  } else if (match(TOKEN_FUNCTION)) {
    fnDeclaration();
  } else if (match(TOKEN_LET)) {
    varDeclaration(false);
  } else if (match(TOKEN_CONST)) {
    varDeclaration(true);
  } else {
    statement();
  }
  if (parser.panicMode)
    synchronize();
}

ObjFunction *compile(const char *source) {
  initScanner(source);
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);

  parser.source = source;
  parser.hadError = false;
  parser.panicMode = false;

  advance();

  while (!match(TOKEN_EOF))
    declaration();

  ObjFunction *function = endCompiler();
  return parser.hadError ? NULL : function;
}

void markCompilerRoots(void) {
  Compiler *compiler = current;
  while (compiler != NULL) {
    markObject((Obj *)compiler->function);
    compiler = compiler->enclosing;
  }
}
