#include "vm.h"
#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

VM vm;

static Value clockNative(int argCount, Value *args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void resetStack(void) {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
}

static void runtimeError(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "%serror:%s ", RED_BOLD, RESET);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fputs("\n", stderr);

  for (int i = vm.frameCount - 1; i >= 0; --i) {
    CallFrame *frame = &vm.frames[i];
    ObjFunction *function = frame->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    int line = getLine(&frame->function->chunk, instruction);
    int col = getColumn(&frame->function->chunk, instruction);

    const char *name =
        function->name == NULL ? "script" : function->name->chars;
    if (i == vm.frameCount - 1) {
      fprintf(stderr, "  %s-->%s %s%s%s %s(L%d:%d)%s\n", RED_BOLD, RESET, BOLD,
              name, RESET, GRAY, line, col, RESET);
    } else {
      fprintf(stderr, "   %s|%s  %s %s(L%d:%d)%s\n", GRAY, RESET, name, GRAY,
              line, col, RESET);
    }
  }
  resetStack();
}

static void defineNative(const char *name, NativeFn function, int arity) {
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function, arity)));

  int idx = vm.globalValues.count;
  writeValueArray(&vm.globalValues, vm.stack[1]);
  tableSet(&vm.globalNames, vm.stack[0], NUMBER_VAL((double)idx));

  vm.stackTop -= 2;
}

void initVM(void) {
  vm.stackCapacity = 256;
  vm.stack = malloc(vm.stackCapacity * sizeof(Value));
  resetStack();
  vm.globalIsConst = NULL;
  vm.globalFlagCapacity = 0;
  initValueArray(&vm.globalValues);
  initTable(&vm.globalNames);
  initTable(&vm.strings);
  vm.objects = NULL;

  defineNative("clock", clockNative, 0);
}

void freeVM(void) {
  freeObjects();
  FREE_ARRAY(bool, vm.globalIsConst, vm.globalFlagCapacity);
  vm.globalIsConst = NULL;
  vm.globalFlagCapacity = 0;
  freeValueArray(&vm.globalValues);
  freeTable(&vm.globalNames);
  freeTable(&vm.strings);
  free(vm.stack);
  vm.stack = NULL;
  vm.stackTop = NULL;
  vm.stackCapacity = 0;
}

void push(Value value) {
  if (vm.stackTop - vm.stack >= vm.stackCapacity) {
    if (vm.stackCapacity >= STACK_MAX) {
      fprintf(stderr, "Stack overflow\n");
      exit(1);
    }

    int oldCapacity = vm.stackCapacity;
    vm.stackCapacity = GROW_CAPACITY(vm.stackCapacity);
    if (vm.stackCapacity > STACK_MAX)
      vm.stackCapacity = STACK_MAX;
    ptrdiff_t offset = vm.stackTop - vm.stack;

    Value *oldStack = vm.stack;
    vm.stack = GROW_ARRAY(Value, vm.stack, oldCapacity, vm.stackCapacity);
    if (vm.stack != oldStack) {
      vm.stackTop = vm.stack + offset;
      for (int i = 0; i < vm.frameCount; ++i) {
        ptrdiff_t slotsOffset = vm.frames[i].slots - oldStack;
        vm.frames[i].slots = vm.stack + slotsOffset;
      }
    }
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
  return NULL;
}

static bool concatenate(void) {
  Value bValue = pop();
  Value aValue = pop();

  ObjString *b = IS_STRING(bValue) ? AS_STRING(bValue) : stringify(bValue);
  ObjString *a = IS_STRING(aValue) ? AS_STRING(aValue) : stringify(aValue);
  if (!b || !a)
    return false;

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
    return true;
  }

  ObjString *string = makeString(length, hash);
  memcpy(string->chars, temp, length);
  string->chars[length] = '\0';
  FREE_ARRAY(char, temp, length + 1);

  tableSet(&vm.strings, OBJ_VAL(string), NIL_VAL);
  push(OBJ_VAL(string));
  return true;
}

static inline bool checkArgCount(int argCount, int arity) {
  if (argCount != arity) {
    runtimeError("Expected %d arguments but got %d", arity, argCount);
    return false;
  }
  return true;
}

static bool call(ObjFunction *function, uint8_t argCount) {
  if (!checkArgCount(argCount, function->arity)) {
    return false;
  }

  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Function call limit exceeded, stack overflow");
    return false;
  }

  CallFrame *frame = &vm.frames[vm.frameCount++];
  frame->function = function;
  frame->ip = function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}

static bool callValue(Value callee, uint8_t argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
    case OBJ_FUNCTION:
      return call(AS_FUNCTION(callee), argCount);
    case OBJ_NATIVE: {
      ObjNative *native = AS_NATIVE(callee);
      if (!checkArgCount(argCount, native->arity)) {
        return false;
      }

      Value result = native->function(argCount, vm.stackTop - argCount);
      vm.stackTop -= argCount + 1;
      push(result);
      return true;
    }
    default:
      break;
    }
  }
  runtimeError("Can only call functions and classes");
  return false;
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

static inline void growFlagCapacity(int idx) {
  if (idx < vm.globalFlagCapacity)
    return;

  int oldCapacity = vm.globalFlagCapacity;
  vm.globalFlagCapacity = GROW_CAPACITY(oldCapacity);
  if (vm.globalFlagCapacity <= idx)
    vm.globalFlagCapacity = idx + 1;

  vm.globalIsConst =
      GROW_ARRAY(bool, vm.globalIsConst, oldCapacity, vm.globalFlagCapacity);
  for (int i = oldCapacity; i < vm.globalFlagCapacity; ++i) {
    vm.globalIsConst[i] = false;
  }
}

// from ECMAScript RFC
static int32_t toInt32(double d) {
  if (isnan(d) || isinf(d))
    return 0;
  double modded = fmod(trunc(d), UINT32_MAX + 1.0);
  if (modded < 0)
    modded += UINT32_MAX + 1.0;
  uint32_t u = (uint32_t)modded;
  return (int32_t)u;
}

static InterpretResult run(void) {
  CallFrame *frame = &vm.frames[vm.frameCount - 1];
  register uint8_t *ip = frame->ip;

#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip += 2, (uint16_t)(ip[-2] | (ip[-1] << 8)))
#define READ_LONG() (ip += 3, (int)(ip[-3] | (ip[-2] << 8) | (ip[-1] << 16)))
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_CONSTANT_LONG()                                                   \
  (ip += 3, frame->function->chunk.constants                                   \
                .values[ip[-3] | (ip[-2] << 8) | (ip[-1] << 16)])
#define BINARY_OP(valueType, op)                                               \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
      frame->ip = ip;                                                          \
      runtimeError("Operands must be numbers.");                               \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(peek(0));                                             \
    double a = AS_NUMBER(peek(1));                                             \
    vm.stackTop -= 2;                                                          \
    push(valueType(a op b));                                                   \
  } while (false)
#define BINARY_BITWISE_OP(valueType, op)                                       \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
      frame->ip = ip;                                                          \
      runtimeError("Operands must be numbers.");                               \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    int32_t b = toInt32(AS_NUMBER(peek(0)));                                   \
    int32_t a = toInt32(AS_NUMBER(peek(1)));                                   \
    vm.stackTop -= 2;                                                          \
    push(valueType(a op b));                                                   \
  } while (false)

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    traceStack(&vm);
    disassembleInstruction(&frame->function->chunk,
                           (int)(ip - frame->function->chunk.code));
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
    case OP_DUP: {
      push(peek(0));
      break;
    }
    case OP_NIL: {
      push(NIL_VAL);
      break;
    }
    case OP_TRUE: {
      push(BOOL_VAL(true));
      break;
    }
    case OP_FALSE: {
      push(BOOL_VAL(false));
      break;
    }
    case OP_POP: {
      pop();
      break;
    }
    case OP_POPN: {
      vm.stackTop -= READ_BYTE();
      break;
    }
    case OP_GET_LOCAL: {
      uint8_t slot = READ_BYTE();
      push(frame->slots[slot]);
      break;
    }
    case OP_GET_LOCAL_LONG: {
      uint16_t slot = READ_SHORT();
      push(frame->slots[slot]);
      break;
    }
    case OP_GET_GLOBAL: {
      Value value = vm.globalValues.values[READ_BYTE()];
      if (IS_UNDEFINED(value)) {
        frame->ip = ip;
        runtimeError("Undefined variable");
        return INTERPRET_RUNTIME_ERROR;
      }
      push(value);
      break;
    }
    case OP_GET_GLOBAL_LONG: {
      Value value = vm.globalValues.values[READ_LONG()];
      if (IS_UNDEFINED(value)) {
        frame->ip = ip;
        runtimeError("Undefined variable");
        return INTERPRET_RUNTIME_ERROR;
      }
      push(value);
      break;
    }
    case OP_DEFINE_GLOBAL: {
      uint8_t idx = READ_BYTE();
      growFlagCapacity(idx + 1);
      vm.globalIsConst[idx] = false;
      vm.globalValues.values[idx] = pop();
      break;
    }
    case OP_DEFINE_GLOBAL_LONG: {
      int idx = READ_LONG();
      growFlagCapacity(idx + 1);
      vm.globalIsConst[idx] = false;
      vm.globalValues.values[idx] = pop();
      break;
    }
    case OP_DEFINE_GLOBAL_CONST: {
      uint8_t idx = READ_BYTE();
      growFlagCapacity(idx + 1);

      vm.globalValues.values[idx] = pop();
      vm.globalIsConst[idx] = true;
      break;
    }
    case OP_DEFINE_GLOBAL_CONST_LONG: {
      int idx = READ_LONG();
      growFlagCapacity(idx + 1);

      vm.globalValues.values[idx] = pop();
      vm.globalIsConst[idx] = true;
      break;
    }
    case OP_SET_LOCAL: {
      uint8_t slot = READ_BYTE();
      frame->slots[slot] = peek(0);
      break;
    }
    case OP_SET_LOCAL_LONG: {
      uint16_t slot = READ_SHORT();
      frame->slots[slot] = peek(0);
      break;
    }
    case OP_SET_GLOBAL: {
      uint8_t idx = READ_BYTE();
      if (IS_UNDEFINED(vm.globalValues.values[idx])) {
        frame->ip = ip;
        runtimeError("Undefined variable");
        return INTERPRET_RUNTIME_ERROR;
      }
      if (vm.globalIsConst[idx]) {
        frame->ip = ip;
        runtimeError("Cannot reassign to a constant variable");
        return INTERPRET_RUNTIME_ERROR;
      }
      vm.globalValues.values[idx] = peek(0);
      break;
    }
    case OP_SET_GLOBAL_LONG: {
      int idx = READ_LONG();
      if (IS_UNDEFINED(vm.globalValues.values[idx])) {
        frame->ip = ip;
        runtimeError("Undefined variable");
        return INTERPRET_RUNTIME_ERROR;
      }
      if (vm.globalIsConst[idx]) {
        frame->ip = ip;
        runtimeError("Cannot reassign to a constant variable");
        return INTERPRET_RUNTIME_ERROR;
      }
      vm.globalValues.values[idx] = peek(0);
      break;
    }
    case OP_NOT: {
      push(BOOL_VAL(isFalsey(pop())));
      break;
    }
    case OP_EQUAL: {
      Value b = pop();
      Value a = pop();
      push(BOOL_VAL(valuesEqual(a, b)));
      break;
    }
    case OP_GREATER:
    case OP_LESS: {
      frame->ip = ip;
      if (comparison(instruction) == INTERPRET_RUNTIME_ERROR) {
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_NEGATE: {
      if (!IS_NUMBER(peek(0))) {
        frame->ip = ip;
        runtimeError("Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }
      *(vm.stackTop - 1) = NUMBER_VAL(-AS_NUMBER(*(vm.stackTop - 1)));
      break;
    }
    case OP_BIN_NOT: {
      if (!IS_NUMBER(peek(0))) {
        frame->ip = ip;
        runtimeError("Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }
      int32_t operand = toInt32(AS_NUMBER(pop()));
      push(NUMBER_VAL(~operand));
      break;
    }
    case OP_PRINT: {
      printValue(pop());
      printf("\n");
      break;
    }
    case OP_JUMP: {
      uint16_t offset = READ_SHORT();
      ip += offset;
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t offset = READ_SHORT();
      if (isFalsey(peek(0)))
        ip += offset;
      break;
    }
    case OP_LOOP: {
      uint16_t offset = READ_SHORT();
      ip -= offset;
      break;
    }
    case OP_CALL: {
      uint8_t argCount = READ_BYTE();
      frame->ip = ip;
      if (!callValue(peek(argCount), argCount)) {
        return INTERPRET_RUNTIME_ERROR;
      }
      frame = &vm.frames[vm.frameCount - 1];
      ip = frame->ip;
      break;
    }
    case OP_ADD: {
      if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(a + b));
        break;
      }
      if (IS_STRING(peek(0)) || IS_STRING(peek(1))) {
        frame->ip = ip;
        if (!concatenate()) {
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      frame->ip = ip;
      runtimeError("Operands must be two numbers or a string and any type.");
      return INTERPRET_RUNTIME_ERROR;
    }
    case OP_SUBTRACT: {
      BINARY_OP(NUMBER_VAL, -);
      break;
    }
    case OP_DIVIDE: {
      BINARY_OP(NUMBER_VAL, /);
      break;
    }
    case OP_MULTIPLY: {
      BINARY_OP(NUMBER_VAL, *);
      break;
    }
    case OP_BIN_AND: {
      BINARY_BITWISE_OP(NUMBER_VAL, &);
      break;
    }
    case OP_BIN_OR: {
      BINARY_BITWISE_OP(NUMBER_VAL, |);
      break;
    }
    case OP_BIN_XOR: {
      BINARY_BITWISE_OP(NUMBER_VAL, ^);
      break;
    }
    case OP_BIN_SHIFT_LEFT: {
      BINARY_BITWISE_OP(NUMBER_VAL, <<);
      break;
    }
    case OP_BIN_SHIFT_RIGHT: {
      BINARY_BITWISE_OP(NUMBER_VAL, >>);
      break;
    }
    case OP_BIN_SHIFT_RIGHT_UNSIGNED: {
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
        frame->ip = ip;
        runtimeError("Operands must be numbers.");
        return INTERPRET_RUNTIME_ERROR;
      }
      // doesnt use the macro due to int32_t and uint32_t differences
      uint32_t b = (uint32_t)toInt32(AS_NUMBER(peek(0))) & 0x1fu;
      uint32_t a = (uint32_t)toInt32(AS_NUMBER(peek(1)));
      vm.stackTop -= 2;
      push(NUMBER_VAL((double)(a >> b)));
      break;
    }
    case OP_RETURN: {
      Value result = pop();
      vm.frameCount--;
      if (vm.frameCount == 0) {
        pop();
        return INTERPRET_OK;
      }

      vm.stackTop = frame->slots;
      push(result);
      frame = &vm.frames[vm.frameCount - 1];
      ip = frame->ip;
      break;
    }
    }
  }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_LONG
#undef READ_CONSTANT
#undef READ_CONSTANT_LONG
#undef BINARY_OP
#undef BINARY_BITWISE_OP
}

InterpretResult interpret(const char *source) {
  ObjFunction *function = compile(source);
  if (function == NULL)
    return INTERPRET_COMPILE_ERROR;

  push(OBJ_VAL(function));
  call(function, 0);

  return run();
}
