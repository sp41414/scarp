#ifndef VM_H
#define VM_H

#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"

#define STACK_MAX UINT16_MAX

typedef struct {
  Chunk *chunk;
  uint8_t *ip;
  Value *stack;
  Value *stackTop;
  int stackCapacity;
  Table globalNames;
  ValueArray globalValues;
  Table strings;
  Obj *objects;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM(void);
void freeVM(void);
InterpretResult interpret(const char *source);
void push(Value value);
Value pop(void);

#endif
