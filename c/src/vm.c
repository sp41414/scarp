#include "vm.h"
#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

VM vm;

static void resetStack(void) { vm.stackTop = vm.stack; }

static void runtimeError(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "%serror:%s ", RED_BOLD, RESET);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fputs("\n", stderr);

  size_t instruction = vm.ip - vm.chunk->code - 1;
  int line = getLine(vm.chunk, instruction);
  int col = getColumn(vm.chunk, instruction);
  fprintf(stderr, "%sline %d, column %d in script%s\n", BOLD, line, col, RESET);
  resetStack();
}

void initVM(void) {
  resetStack();
  initValueArray(&vm.globalValues);
  initTable(&vm.globalNames);
  initTable(&vm.strings);
  vm.objects = NULL;
}

void freeVM(void) {
  freeObjects();
  freeValueArray(&vm.globalValues);
  freeTable(&vm.globalNames);
  freeTable(&vm.strings);
}

void push(Value value) {
  if (vm.stackTop >= &vm.stack[STACK_MAX]) {
    fprintf(stderr, "Stack overflow\n");
    exit(1);
  }
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop(void) {
  vm.stackTop--;
  return *vm.stackTop;
}

static Value peek(int distance) { return vm.stackTop[-1 - distance]; }

static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static ObjString *stringify(Value value) {
  if (IS_NIL(value)) {
    return copyString("nil", 3);
  }
  if (IS_BOOL(value)) {
    return AS_BOOL(value) ? copyString("true", 4) : copyString("false", 5);
  }
  if (IS_NUMBER(value)) {
    double num = AS_NUMBER(value);

    char buf[32];
    int length = snprintf(buf, sizeof(buf), "%g", num);

    return copyString(buf, length);
  }
  if (IS_STRING(value)) {
    return AS_STRING(value);
  }

  runtimeError("Unknown type passed to stringify().");
  exit(1);
}

static void concatenate(void) {
  Value bValue = pop();
  Value aValue = pop();

  ObjString *b = IS_STRING(bValue) ? AS_STRING(bValue) : stringify(bValue);
  ObjString *a = IS_STRING(aValue) ? AS_STRING(aValue) : stringify(aValue);

  int length = a->length + b->length;
  char *temp = ALLOCATE(char, length + 1);
  memcpy(temp, a->chars, a->length);
  memcpy(temp + a->length, b->chars, b->length);
  temp[length] = '\0';

  uint32_t hash = hashString(temp, length);

  ObjString *interned = tableFindString(&vm.strings, temp, length, hash);
  if (interned != NULL) {
    FREE_ARRAY(char, temp, length + 1);
    push(OBJ_VAL(interned));
    return;
  }

  ObjString *string = makeString(length, hash);
  memcpy(string->chars, temp, length);
  string->chars[length] = '\0';
  FREE_ARRAY(char, temp, length + 1);

  tableSet(&vm.strings, OBJ_VAL(string), NIL_VAL);
  push(OBJ_VAL(string));
}

static inline InterpretResult comparison(uint8_t op) {
  if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
    ObjString *b = AS_STRING(pop());
    ObjString *a = AS_STRING(pop());
    int cmp = strcmp(a->chars, b->chars);
    push(BOOL_VAL(op == OP_GREATER ? cmp > 0 : cmp < 0));
  } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
    double b = AS_NUMBER(pop());
    double a = AS_NUMBER(pop());
    push(BOOL_VAL(op == OP_GREATER ? a > b : a < b));
  } else {
    runtimeError("Operands must be two numbers or two strings.");
    return INTERPRET_RUNTIME_ERROR;
  }
  return INTERPRET_OK;
}

static InterpretResult run(void) {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_CONSTANT_LONG()                                                   \
  (vm.ip += 3, vm.chunk->constants                                             \
                   .values[vm.ip[-3] | (vm.ip[-2] << 8) | (vm.ip[-1] << 16)])
#define BINARY_OP(valueType, op)                                               \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
      runtimeError("Operands must be numbers.");                               \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(peek(0));                                             \
    double a = AS_NUMBER(peek(1));                                             \
    vm.stackTop -= 2;                                                          \
    push(valueType(a op b));                                                   \
  } while (false)

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    traceStack(&vm);
    disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT_LONG: {
      Value constant = READ_CONSTANT_LONG();
      push(constant);
      break;
    }
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      push(constant);
      break;
    }
    case OP_NIL:
      push(NIL_VAL);
      break;
    case OP_TRUE:
      push(BOOL_VAL(true));
      break;
    case OP_FALSE:
      push(BOOL_VAL(false));
      break;
    case OP_POP:
      pop();
      break;
    case OP_GET_GLOBAL: {
      Value value = vm.globalValues.values[READ_BYTE()];
      if (IS_UNDEFINED(value)) {
        runtimeError("Undefined variable");
        return INTERPRET_RUNTIME_ERROR;
      }
      push(value);
      break;
    }
    case OP_GET_GLOBAL_LONG: {
      vm.ip += 3;
      Value value =
          vm.globalValues
              .values[vm.ip[-3] | (vm.ip[-2] << 8) | (vm.ip[-1] << 16)];
      if (IS_UNDEFINED(value)) {
        runtimeError("Undefined variable");
        return INTERPRET_RUNTIME_ERROR;
      }
      push(value);
      break;
    }
    case OP_DEFINE_GLOBAL: {
      vm.globalValues.values[READ_BYTE()] = pop();
      break;
    }
    case OP_DEFINE_GLOBAL_LONG: {
      vm.ip += 3;
      vm.globalValues.values[vm.ip[-3] | (vm.ip[-2] << 8) | (vm.ip[-1] << 16)] =
          pop();
      break;
    }
    case OP_SET_GLOBAL: {
      uint8_t idx = READ_BYTE();
      if (IS_UNDEFINED(vm.globalValues.values[idx])) {
        runtimeError("Undefined variable");
        return INTERPRET_RUNTIME_ERROR;
      }
      vm.globalValues.values[idx] = peek(0);
      break;
    }
    case OP_SET_GLOBAL_LONG: {
      vm.ip += 3;
      int idx = vm.ip[-3] | (vm.ip[-2] << 8) | (vm.ip[-1] << 16);
      if (IS_UNDEFINED(vm.globalValues.values[idx])) {
        runtimeError("Undefined variable");
        return INTERPRET_RUNTIME_ERROR;
      }
      vm.globalValues.values[idx] = peek(0);
      break;
    }
    case OP_NOT:
      push(BOOL_VAL(isFalsey(pop())));
      break;
    case OP_EQUAL: {
      Value b = pop();
      Value a = pop();
      push(BOOL_VAL(valuesEqual(a, b)));
      break;
    }
    case OP_GREATER:
    case OP_LESS:
      if (comparison(instruction) == INTERPRET_RUNTIME_ERROR) {
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    case OP_NEGATE:
      if (!IS_NUMBER(peek(0))) {
        runtimeError("Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }
      *(vm.stackTop - 1) = NUMBER_VAL(-AS_NUMBER(*(vm.stackTop - 1)));
      break;
    case OP_PRINT:
      printValue(pop());
      printf("\n");
      break;
    case OP_ADD:
      if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(a + b));
        break;
      }
      if (IS_STRING(peek(0)) || IS_STRING(peek(1))) {
        concatenate();
        break;
      }
      runtimeError("Operands must be two numbers or a string and any type.");
      return INTERPRET_RUNTIME_ERROR;
    case OP_SUBTRACT:
      BINARY_OP(NUMBER_VAL, -);
      break;
    case OP_DIVIDE:
      BINARY_OP(NUMBER_VAL, /);
      break;
    case OP_MULTIPLY:
      BINARY_OP(NUMBER_VAL, *);
      break;
    case OP_RETURN: {
      return INTERPRET_OK;
    }
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_CONSTANT_LONG
#undef BINARY_OP
}

InterpretResult interpret(const char *source) {
  Chunk chunk;
  initChunk(&chunk);

  if (!compile(source, &chunk)) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm.chunk = &chunk;
  vm.ip = vm.chunk->code;

  InterpretResult result = run();

  freeChunk(&chunk);
  return result;
}
