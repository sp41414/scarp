#ifndef DEBUG_H
#define DEBUG_H

#include "chunk.h"
#include "vm.h"

#define GET_INDEX(chunk, offset) (chunk->code[offset + 1])
#define GET_SHORT_INDEX(chunk, offset)                                         \
  (chunk->code[offset + 1] | (chunk->code[offset + 2] << 8))
#define GET_LONG_INDEX(chunk, offset)                                          \
  (chunk->code[offset + 1] | (chunk->code[offset + 2] << 8) |                  \
   (chunk->code[offset + 3] << 16))

void traceStack(VM *vm);
void disassembleChunk(Chunk *chunk, const char *name);
int disassembleInstruction(Chunk *chunk, int offset);

#endif
