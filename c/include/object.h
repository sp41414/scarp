#ifndef OBJECT_H
#define OBJECT_H

#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE_MASK 0x0000000000000007ULL
#define OBJ_MARKED_MASK 0x8000000000000000ULL
#define OBJ_PTR_MASK 0x7FFFFFFFFFFFFFF8ULL

#define COPY_LITERAL(str) copyString(str, sizeof(str) - 1)

#define OBJ_TYPE(value) (objType(AS_OBJ(value)))

#define IS_CLASS(value) isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value) isObjType(value, OBJ_INSTANCE)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define IS_STRING(value) isObjType(value, OBJ_STRING)

#define AS_CLASS(value) ((ObjClass *)AS_OBJ(value))
#define AS_CLOSURE(value) ((ObjClosure *)AS_OBJ(value))
#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))
#define AS_INSTANCE(value) ((ObjInstance *)AS_OBJ(value))
#define AS_NATIVE(value) ((ObjNative *)AS_OBJ(value))
#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)

typedef enum {
  OBJ_CLASS,
  OBJ_CLOSURE,
  OBJ_FUNCTION,
  OBJ_INSTANCE,
  OBJ_NATIVE,
  OBJ_STRING,
  OBJ_UPVALUE
} ObjType;

struct Obj {
  // M: isMarked (1 bit, bit 63)
  // N: next object pointer (60 bits, bits 3-62)
  // T: type (3 bits, bits 0-2)
  // MNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNTTT
  uint64_t header;
};

typedef struct {
  Obj obj;
  int arity;
  int upvalueCount;
  Chunk chunk;
  ObjString *name;
} ObjFunction;

typedef bool (*NativeFn)(Value *args);

typedef struct {
  Obj obj;
  int arity;
  NativeFn function;
} ObjNative;

struct ObjString {
  Obj obj;
  int length;
  uint32_t hash;
  char chars[];
};

typedef struct ObjUpvalue {
  Obj obj;
  Value *location;
  Value closed;
  struct ObjUpvalue *next;
} ObjUpvalue;

typedef struct {
  Obj obj;
  ObjFunction *function;
  ObjUpvalue **upvalues;
  int upvalueCount;
} ObjClosure;

typedef struct {
  Obj obj;
  ObjString *name;
} ObjClass;

typedef struct {
  Obj obj;
  ObjClass *cls;
  Table fields;
} ObjInstance;

ObjInstance *newInstance(ObjClass *cls);
ObjClass *newClass(ObjString *name);
ObjClosure *newClosure(ObjFunction *function);
ObjFunction *newFunction(void);
ObjNative *newNative(NativeFn function, int arity);
ObjString *makeString(int length, uint32_t hash);
ObjString *copyString(const char *chars, int length);
ObjUpvalue *newUpvalue(Value *slot);
uint32_t hashString(const char *key, int length);
void printObject(Value value);

static inline ObjType objType(Obj *object) {
  return (ObjType)(object->header & OBJ_TYPE_MASK);
}
static inline bool isMarked(Obj *object) {
  return (object->header & OBJ_MARKED_MASK) != 0;
}
static inline Obj *objNext(Obj *object) {
  return (Obj *)(object->header & OBJ_PTR_MASK);
}
static inline void setObjType(Obj *object, ObjType type) {
  object->header = (object->header & ~OBJ_TYPE_MASK) | (type & OBJ_TYPE_MASK);
}
static inline void setIsMarked(Obj *object, bool isMarked) {
  object->header = (object->header & ~OBJ_MARKED_MASK) |
                   (((uint64_t)isMarked << 63) & OBJ_MARKED_MASK);
}

static inline void setObjNext(Obj *object, Obj *next) {
  object->header =
      (object->header & ~OBJ_PTR_MASK) | ((uintptr_t)next & OBJ_PTR_MASK);
}

static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

#endif
