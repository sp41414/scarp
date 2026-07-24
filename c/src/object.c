#include "object.h"
#include "memory.h"
#include "table.h"
#include "value.h"
#include "vm.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define ALLOCATE_OBJ(type, objectType)                                         \
  (type *)allocateObject(sizeof(type), objectType)

static Obj *allocateObject(size_t size, ObjType type) {
  Obj *object = reallocate(NULL, 0, size);
  setObjType(object, type);
  setIsMarked(object, false);

  setObjNext(object, vm.objects);
  vm.objects = object;

#ifdef DEBUG_LOG_GC
  printf("%p allocate %zu for %d\n", (void *)object, size, type);
#endif
  return object;
}

ObjBoundMethod *newBoundMethod(Value receiver, Obj *method) {
  ObjBoundMethod *bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);
  bound->receiver = receiver;
  bound->method = method;
  return bound;
}

ObjInstance *newInstance(ObjClass *cls) {
  ObjInstance *instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
  instance->cls = cls;
  initTable(&instance->fields);
  return instance;
}

ObjClass *newClass(ObjString *name) {
  ObjClass *cls = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
  cls->name = name;
  cls->initializer = NULL;
  initTable(&cls->methods);
  return cls;
}

ObjClosure *newClosure(ObjFunction *function) {
  ObjUpvalue **upvalues = ALLOCATE(ObjUpvalue *, function->upvalueCount);
  for (int i = 0; i < function->upvalueCount; ++i) {
    upvalues[i] = NULL;
  }

  ObjClosure *closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
  return closure;
}

ObjFunction *newFunction(void) {
  ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  function->arity = 0;
  function->upvalueCount = 0;
  function->name = NULL;
  initChunk(&function->chunk);
  return function;
}

ObjNative *newNative(NativeFn function, int arity) {
  ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
  native->function = function;
  native->arity = arity;
  return native;
}

ObjString *makeString(int length, uint32_t hash) {
  ObjString *string =
      (ObjString *)allocateObject(sizeof(ObjString) + length + 1, OBJ_STRING);
  string->length = length;
  string->hash = hash;
  return string;
}

uint32_t hashString(const char *key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

ObjString *copyString(const char *chars, int length) {
  uint32_t hash = hashString(chars, length);

  ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL)
    return interned;

  ObjString *string = makeString(length, hash);
  push(OBJ_VAL(string));

  memcpy(string->chars, chars, length);
  string->chars[length] = '\0';

  tableSet(&vm.strings, OBJ_VAL(string), NIL_VAL);
  pop();
  return string;
}

ObjUpvalue *newUpvalue(Value *slot) {
  ObjUpvalue *upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
  upvalue->closed = NIL_VAL;
  upvalue->location = slot;
  upvalue->next = NULL;
  return upvalue;
}

static void printFunction(Obj *function) {
  ObjFunction *fn = objType(function) == OBJ_FUNCTION
                        ? (ObjFunction *)function
                        : ((ObjClosure *)function)->function;
  if (fn->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<fn %s>", fn->name->chars);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_BOUND_METHOD:
    printFunction(AS_BOUND_METHOD(value)->method);
    break;
  case OBJ_CLASS:
    printf("<class %s>", AS_CLASS(value)->name->chars);
    break;
  case OBJ_NATIVE:
    printf("<native fn>");
    break;
  case OBJ_CLOSURE:
  case OBJ_FUNCTION:
    printFunction(AS_OBJ(value));
    break;
  case OBJ_INSTANCE:
    printf("<%s instance>", AS_INSTANCE(value)->cls->name->chars);
    break;
  case OBJ_STRING:
    printf("%s", AS_CSTRING(value));
    break;
  case OBJ_UPVALUE:
    printf("upvalue");
    break;
  }
}
