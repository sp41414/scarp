#include "memory.h"
#include "chunk.h"
#include "object.h"
#include "vm.h"
#include <stdlib.h>

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void *result = realloc(pointer, newSize);
  if (result == NULL)
    exit(1);
  return result;
}

static void freeObject(Obj *object) {
  switch (object->type) {
  case OBJ_NATIVE: {
    FREE(ObjNative, object);
    break;
  }
  case OBJ_FUNCTION: {
    ObjFunction *function = (ObjFunction *)object;
    freeChunk(&function->chunk);
    FREE(ObjFunction, object);
    break;
  }
  case OBJ_STRING: {
    ObjString *string = (ObjString *)object;
    reallocate(string, sizeof(ObjString) + string->length + 1, 0);
    break;
  }
  }
}

void freeObjects(void) {
  Obj *object = vm.objects;
  while (object != NULL) {
    Obj *next = object->next;
    freeObject(object);
    object = next;
  }
}
