#include "chunk.h"
#include "vm.h"
int main(int argc, const char **argv) {
  initVM();

  Chunk chunk;
  initChunk(&chunk);

  int constant = addConstant(&chunk, 1.2);
  writeChunk(&chunk, OP_CONSTANT, 69420, 67);
  writeChunk(&chunk, constant, 69420, 67);

  int longConstant = addConstant(&chunk, 6.7);
  writeChunk(&chunk, OP_CONSTANT_LONG, 69420, 68);
  writeChunk(&chunk, longConstant, 69420, 69);

  constant = addConstant(&chunk, 3.4);
  writeChunk(&chunk, OP_CONSTANT, 123, 1234);
  writeChunk(&chunk, constant, 123, 1234);

  writeChunk(&chunk, OP_ADD, 123, 1234);

  constant = addConstant(&chunk, 5.6);
  writeChunk(&chunk, OP_CONSTANT, 123, 1234);
  writeChunk(&chunk, constant, 123, 1234);

  writeChunk(&chunk, OP_DIVIDE, 123, 1234);

  writeChunk(&chunk, OP_NEGATE, 123, 1234);
  writeChunk(&chunk, OP_RETURN, 69420, 1000);
  interpret(&chunk);
  freeVM();
  freeChunk(&chunk);
  return 0;
}
