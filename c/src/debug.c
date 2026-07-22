#include "debug.h"
#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"
#include <stdint.h>
#include <stdio.h>

void traceStack(VM *vm) {
  printf("          ");
  for (Value *slot = vm->stack; slot < vm->stackTop; slot++) {
    printf("[");
    printValue(*slot);
    printf("]");
  }
  printf("\n");
}

void disassembleChunk(Chunk *chunk, const char *name) {
  printf("== %s ==\n", name);

  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

static int simpleInstruction(const char *name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int constantInstruction(const char *name, Chunk *chunk, int offset) {
  uint8_t constant = GET_INDEX(chunk, offset);
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 2;
}

static int longConstantInstruction(const char *name, Chunk *chunk, int offset) {
  uint32_t constant = GET_LONG_INDEX(chunk, offset);
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 4;
}

static void printGlobalName(int slot) {
  for (int i = 0; i < vm.globalNames.capacity; i++) {
    Entry *entry = &vm.globalNames.entries[i];
    if (!IS_NIL(entry->key) && IS_NUMBER(entry->value)) {
      if ((int)AS_NUMBER(entry->value) == slot) {
        printValue(entry->key);
        return;
      }
    }
  }
  printf("unknown");
}

static int globalInstruction(const char *name, Chunk *chunk, int offset) {
  uint8_t slot = GET_INDEX(chunk, offset);
  printf("%-16s %4d '", name, slot);
  printGlobalName(slot);
  printf("'\n");
  return offset + 2;
}

static int longGlobalInstruction(const char *name, Chunk *chunk, int offset) {
  uint32_t slot = GET_LONG_INDEX(chunk, offset);
  printf("%-16s %4d '", name, slot);
  printGlobalName(slot);
  printf("'\n");
  return offset + 4;
}

static int byteInstruction(const char *name, Chunk *chunk, int offset) {
  uint8_t slot = GET_INDEX(chunk, offset);
  printf("%-16s %4d\n", name, slot);
  return offset + 2;
}

static int longByteInstruction(const char *name, Chunk *chunk, int offset) {
  uint16_t slot = GET_SHORT_INDEX(chunk, offset);
  printf("%-16s %4d\n", name, slot);
  return offset + 3;
}

static int jumpInstruction(const char *name, int sign, Chunk *chunk,
                           int offset) {
  uint16_t jump = GET_SHORT_INDEX(chunk, offset);
  printf("%-16s %4d -> %4d\n", name, offset, offset + 3 + sign * jump);
  return offset + 3;
}

static int closureInstruction(Chunk *chunk, int offset) {
  uint8_t constant = GET_INDEX(chunk, offset);
  printf("%-16s %4d ", "OP_CLOSURE", constant);
  printValue(chunk->constants.values[constant]);
  printf("\n");
  offset += 2;

  ObjFunction *function = AS_FUNCTION(chunk->constants.values[constant]);
  for (int j = 0; j < function->upvalueCount; ++j) {
    uint8_t isLocal = chunk->code[offset++];
    int idx = GET_SHORT_INDEX(chunk, offset);

    offset += 2;

    printf("%04d      |                     %s %d\n", offset - 3,
           isLocal ? "local" : "upvalue", idx);
  }
  return offset;
}

static int longClosureInstruction(Chunk *chunk, int offset) {
  uint32_t constant = GET_LONG_INDEX(chunk, offset);
  printf("%-16s %4d '", "OP_CLOSURE_LONG", constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  offset += 4;

  ObjFunction *function = AS_FUNCTION(chunk->constants.values[constant]);
  for (int j = 0; j < function->upvalueCount; ++j) {
    uint8_t isLocal = chunk->code[offset++];
    int idx = GET_SHORT_INDEX(chunk, offset);

    offset += 2;

    printf("%04d      |                     %s %d\n", offset - 3,
           isLocal ? "local" : "upvalue", idx);
  }

  return offset;
}

int disassembleInstruction(Chunk *chunk, int offset) {
  printf("%04d ", offset);

  int line = getLine(chunk, offset);
  int column = getColumn(chunk, offset);
  if (offset > 0 && line == getLine(chunk, offset - 1)) {
    printf("%4d   | ", column);
  } else {
    printf("%4d: %4d ", line, column);
  }

  uint8_t instruction = chunk->code[offset];
  switch (instruction) {
  case OP_RETURN:
    return simpleInstruction("OP_RETURN", offset);
  case OP_CLASS:
    return constantInstruction("OP_CLASS", chunk, offset);
  case OP_CLASS_LONG:
    return longConstantInstruction("OP_CLASS_LONG", chunk, offset);
  case OP_CONSTANT:
    return constantInstruction("OP_CONSTANT", chunk, offset);
  case OP_CONSTANT_LONG:
    return longConstantInstruction("OP_CONSTANT_LONG", chunk, offset);
  case OP_DUP:
    return simpleInstruction("OP_DUP", offset);
  case OP_GET_GLOBAL:
    return globalInstruction("OP_GET_GLOBAL", chunk, offset);
  case OP_GET_GLOBAL_LONG:
    return longGlobalInstruction("OP_GET_GLOBAL_LONG", chunk, offset);
  case OP_DEFINE_GLOBAL:
    return globalInstruction("OP_DEFINE_GLOBAL", chunk, offset);
  case OP_DEFINE_GLOBAL_LONG:
    return longGlobalInstruction("OP_DEFINE_GLOBAL_LONG", chunk, offset);
  case OP_DEFINE_GLOBAL_CONST:
    return globalInstruction("OP_DEFINE_GLOBAL_CONST", chunk, offset);
  case OP_DEFINE_GLOBAL_CONST_LONG:
    return longGlobalInstruction("OP_DEFINE_GLOBAL_CONST_LONG", chunk, offset);
  case OP_SET_GLOBAL:
    return globalInstruction("OP_SET_GLOBAL", chunk, offset);
  case OP_SET_GLOBAL_LONG:
    return longGlobalInstruction("OP_SET_GLOBAL_LONG", chunk, offset);
  case OP_GET_UPVALUE:
    return byteInstruction("OP_GET_UPVALUE", chunk, offset);
  case OP_SET_UPVALUE:
    return byteInstruction("OP_SET_UPVALUE", chunk, offset);
  case OP_GET_UPVALUE_LONG:
    return longByteInstruction("OP_GET_UPVALUE_LONG", chunk, offset);
  case OP_SET_UPVALUE_LONG:
    return longByteInstruction("OP_SET_UPVALUE_LONG", chunk, offset);
  case OP_GET_PROPERTY:
    return constantInstruction("OP_GET_PROPERTY", chunk, offset);
  case OP_SET_PROPERTY:
    return constantInstruction("OP_SET_PROPERTY", chunk, offset);
  case OP_GET_PROPERTY_LONG:
    return longConstantInstruction("OP_GET_PROPERTY_LONG", chunk, offset);
  case OP_SET_PROPERTY_LONG:
    return longConstantInstruction("OP_SET_PROPERTY_LONG", chunk, offset);
  case OP_NIL:
    return simpleInstruction("OP_NIL", offset);
  case OP_TRUE:
    return simpleInstruction("OP_TRUE", offset);
  case OP_FALSE:
    return simpleInstruction("OP_FALSE", offset);
  case OP_POP:
    return simpleInstruction("OP_POP", offset);
  case OP_POPN:
    return byteInstruction("OP_POPN", chunk, offset);
  case OP_GET_LOCAL:
    return byteInstruction("OP_GET_LOCAL", chunk, offset);
  case OP_GET_LOCAL_LONG:
    return longByteInstruction("OP_GET_LOCAL_LONG", chunk, offset);
  case OP_SET_LOCAL:
    return byteInstruction("OP_SET_LOCAL", chunk, offset);
  case OP_SET_LOCAL_LONG:
    return longByteInstruction("OP_SET_LOCAL_LONG", chunk, offset);
  case OP_EQUAL:
    return simpleInstruction("OP_EQUAL", offset);
  case OP_GREATER:
    return simpleInstruction("OP_GREATER", offset);
  case OP_LESS:
    return simpleInstruction("OP_LESS", offset);
  case OP_NEGATE:
    return simpleInstruction("OP_NEGATE", offset);
  case OP_PRINT:
    return simpleInstruction("OP_PRINT", offset);
  case OP_JUMP:
    return jumpInstruction("OP_JUMP", 1, chunk, offset);
  case OP_JUMP_IF_FALSE:
    return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
  case OP_LOOP:
    return jumpInstruction("OP_LOOP", -1, chunk, offset);
  case OP_CALL:
    return byteInstruction("OP_CALL", chunk, offset);
  case OP_CLOSURE:
    return closureInstruction(chunk, offset);
  case OP_CLOSURE_LONG:
    return longClosureInstruction(chunk, offset);
  case OP_CLOSE_UPVALUE:
    return simpleInstruction("OP_CLOSE_UPVALUE", offset);
  case OP_NOT:
    return simpleInstruction("OP_NOT", offset);
  case OP_ADD:
    return simpleInstruction("OP_ADD", offset);
  case OP_SUBTRACT:
    return simpleInstruction("OP_SUBTRACT", offset);
  case OP_MULTIPLY:
    return simpleInstruction("OP_MULTIPLY", offset);
  case OP_DIVIDE:
    return simpleInstruction("OP_DIVIDE", offset);
  case OP_BIN_AND:
    return simpleInstruction("OP_BIN_AND", offset);
  case OP_BIN_OR:
    return simpleInstruction("OP_BIN_OR", offset);
  case OP_BIN_XOR:
    return simpleInstruction("OP_BIN_XOR", offset);
  case OP_BIN_NOT:
    return simpleInstruction("OP_BIN_NOT", offset);
  case OP_BIN_SHIFT_RIGHT:
    return simpleInstruction("OP_BIN_SHIFT_RIGHT", offset);
  case OP_BIN_SHIFT_RIGHT_UNSIGNED:
    return simpleInstruction("OP_BIN_SHIFT_RIGHT_UNSIGNED", offset);
  case OP_BIN_SHIFT_LEFT:
    return simpleInstruction("OP_BIN_SHIFT_LEFT", offset);
  default:
    printf("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}
