#ifndef CHUNK_H
#define CHUNK_H

#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_CONSTANT_LONG,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_GET_LOCAL,
  OP_GET_LOCAL_LONG,
  OP_GET_GLOBAL,
  OP_GET_GLOBAL_LONG,
  OP_POP,
  OP_POPN,
  OP_DEFINE_GLOBAL,
  OP_DEFINE_GLOBAL_LONG,
  OP_DEFINE_GLOBAL_CONST,
  OP_DEFINE_GLOBAL_CONST_LONG,
  OP_SET_LOCAL,
  OP_SET_LOCAL_LONG,
  OP_SET_GLOBAL,
  OP_SET_GLOBAL_LONG,
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  OP_ADD,
  OP_SUBTRACT,
  OP_DIVIDE,
  OP_MULTIPLY,
  OP_NEGATE,
  OP_NOT,
  OP_BIN_NOT,
  OP_BIN_OR,
  OP_BIN_AND,
  OP_BIN_XOR,
  OP_BIN_SHIFT_LEFT,
  OP_BIN_SHIFT_RIGHT,
  OP_BIN_SHIFT_RIGHT_UNSIGNED,
  OP_PRINT,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_RETURN,
} OpCode;

typedef struct {
  int line;
  int length;
} Line;

typedef struct {
  ValueArray constants;
  uint8_t *code;
  Line *lines;
  uint16_t *columns;

  int capacity;
  int count;
  int lineCount;
  int lineCapacity;
} Chunk;

void initChunk(Chunk *chunk);
void freeChunk(Chunk *chunk);

int addConstant(Chunk *chunk, Value value);
void writeChunk(Chunk *chunk, uint8_t byte, int line, int col);
void writeConstant(Chunk *chunk, Value value, int line, int col);

int getLine(Chunk *chunk, int idx);
int getColumn(Chunk *chunk, int idx);

#endif
