#ifndef DEBUG_H
#define DEBUG_H

#include "chunk.h"
#include "vm.h"

void traceStack(VM *vm);
void disassembleChunk(Chunk *chunk, const char *name);
int disassembleInstruction(Chunk *chunk, int offset);

#endif
