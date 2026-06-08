#ifndef CHUNK_H
#define CHUNK_H

#include "common.h"
#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_CONSTANT_LONG,
  OP_ADD,
  OP_SUBTRACT,
  OP_DIVIDE,
  OP_MULTIPLY,
  OP_NEGATE,
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
