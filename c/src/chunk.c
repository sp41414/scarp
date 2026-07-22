#include "chunk.h"
#include "memory.h"
#include "vm.h"
#include <stdint.h>

void initChunk(Chunk *chunk) {
  chunk->capacity = 0;
  chunk->count = 0;
  chunk->lineCount = 0;
  chunk->lineCapacity = 0;
  chunk->code = NULL;
  chunk->lines = NULL;
  chunk->columns = NULL;
  initValueArray(&chunk->constants);
}

void writeChunk(Chunk *chunk, uint8_t byte, int line, int col) {
  if (chunk->capacity < chunk->count + 1) {
    int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);
    chunk->code =
        GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    chunk->columns =
        GROW_ARRAY(uint16_t, chunk->columns, oldCapacity, chunk->capacity);
  }

  chunk->code[chunk->count] = byte;
  chunk->columns[chunk->count] = (uint16_t)col;

  if (chunk->lineCount > 0 && chunk->lines[chunk->lineCount - 1].line == line) {
    chunk->lines[chunk->lineCount - 1].length++;
  } else {
    if (chunk->lineCapacity < chunk->lineCount + 1) {
      int oldCapacity = chunk->lineCapacity;
      chunk->lineCapacity = GROW_CAPACITY(oldCapacity);
      chunk->lines =
          GROW_ARRAY(Line, chunk->lines, oldCapacity, chunk->lineCapacity);
    }

    Line *run = &chunk->lines[chunk->lineCount];
    run->line = line;
    run->length = 1;
    chunk->lineCount++;
  }

  chunk->count++;
}

void writeConstant(Chunk *chunk, Value value, int line, int col) {
  int idx = addConstant(chunk, value);
  if (idx <= UINT8_MAX) {
    writeChunk(chunk, OP_CONSTANT, line, col);
    writeChunk(chunk, (uint8_t)idx, line, col);
  } else {
    writeChunk(chunk, OP_CONSTANT_LONG, line, col);
    writeChunk(chunk, (uint8_t)(idx & 0xff), line, col);
    writeChunk(chunk, (uint8_t)((idx >> 8) & 0xff), line, col);
    writeChunk(chunk, (uint8_t)((idx >> 16) & 0xff), line, col);
  }
}

int addConstant(Chunk *chunk, Value value) {
  push(value);
  writeValueArray(&chunk->constants, value);
  pop();
  return chunk->constants.count - 1;
}

void freeChunk(Chunk *chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
  FREE_ARRAY(uint16_t, chunk->columns, chunk->capacity);
  FREE_ARRAY(Line, chunk->lines, chunk->lineCapacity);
  freeValueArray(&chunk->constants);
  initChunk(chunk);
}

int getLine(Chunk *chunk, int idx) {
  int low = 0;
  int high = chunk->lineCount - 1;

  while (low <= high) {
    int mid = low + (high - low) / 2;

    int startIdx = 0;
    for (int i = 0; i < mid; i++) {
      startIdx += chunk->lines[i].length;
    }
    int endIdx = chunk->lines[mid].length + startIdx;

    if (idx >= startIdx && idx < endIdx) {
      return chunk->lines[mid].line;
    }

    if (idx < startIdx) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }

  return -1;
}

int getColumn(Chunk *chunk, int idx) {
  if (idx < 0 || idx >= chunk->count)
    return -1;
  return chunk->columns[idx];
}
