#include "native.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void defineNative(const char *name, NativeFn function, int arity) {
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function, arity)));

  int idx = vm.globalValues.count;
  growFlagCapacity(idx + 1);
  writeValueArray(&vm.globalValues, vm.stack[1]);
  tableSet(&vm.globalNames, vm.stack[0], NUMBER_VAL((double)idx));

  vm.stackTop -= 2;
}

bool clockNative(Value *args) {
  args[-1] = NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
  return true;
}

bool hasFieldNative(Value *args) {
  if (!IS_INSTANCE(args[0]) || !IS_STRING(args[1])) {
    args[-1] = OBJ_VAL(COPY_LITERAL("Expect an instance and a string"));
    return false;
  }

  ObjInstance *instance = AS_INSTANCE(args[0]);
  Value value;
  args[-1] = BOOL_VAL(tableGet(&instance->fields, args[1], &value));
  return true;
}

bool deleteFieldNative(Value *args) {
  if (!IS_INSTANCE(args[0]) || !IS_STRING(args[1])) {
    args[-1] = OBJ_VAL(COPY_LITERAL("Expect an instance and a string"));
    return false;
  }

  ObjInstance *instance = AS_INSTANCE(args[0]);
  Value field = args[1];
  if (tableDelete(&instance->fields, field)) {
    args[-1] = BOOL_VAL(true);
    return true;
  }
  args[-1] = BOOL_VAL(false);
  return true;
}

bool readLineNative(Value *args) {
  int capacity = 128;
  int len = 0;
  char *buf = ALLOCATE(char, capacity);

  while (fgets(buf + len, capacity - len, stdin)) {
    len += strlen(buf + len);

    if (len > 0 && buf[len - 1] == '\n') {
      buf[len - 1] = '\0';
      len--;
      break;
    }

    int oldCapacity = capacity;
    capacity = GROW_CAPACITY(capacity);
    buf = reallocate(buf, oldCapacity, capacity);
  }

  if (len == 0) {
    FREE(char, buf);
    args[-1] = NIL_VAL;
    return true;
  }

  args[-1] = OBJ_VAL(copyString(buf, len));
  FREE(char, buf);
  return true;
}

bool fileExistsNative(Value *args) {
  char *path = AS_CSTRING(args[0]);
  if (access(path, F_OK) == 0) {
    args[-1] = BOOL_VAL(true);
  } else {
    args[-1] = BOOL_VAL(false);
  }
  return true;
}

bool readFileNative(Value *args) {
  char *path = AS_CSTRING(args[0]);
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    args[-1] = NIL_VAL;
    return true;
  }

  fseek(fp, 0L, SEEK_END);
  size_t fileSize = ftell(fp);
  rewind(fp);

  char *buf = ALLOCATE(char, fileSize + 1);
  if (!buf) {
    FREE(char, buf);
    fclose(fp);
    args[-1] = OBJ_VAL(COPY_LITERAL("Not enough memory to read file"));
    return false;
  }

  size_t bytesRead = fread(buf, sizeof(char), fileSize, fp);
  if (bytesRead < fileSize) {
    FREE(char, buf);
    fclose(fp);
    args[-1] = OBJ_VAL(COPY_LITERAL("Could not read file"));
    return false;
  }

  buf[bytesRead] = '\0';
  fclose(fp);

  args[-1] = OBJ_VAL(copyString(buf, (int)bytesRead));
  FREE(char, buf);
  return true;
}

bool writeFileNative(Value *args) {
  char *path = AS_CSTRING(args[0]);
  FILE *fp = fopen(path, "wb");
  if (!fp) {
    args[-1] = BOOL_VAL(false);
    return true;
  }

  ObjString *contents = AS_STRING(args[1]);
  size_t written = fwrite(contents->chars, sizeof(char), contents->length, fp);
  fclose(fp);

  if (written < (size_t)contents->length) {
    args[-1] = BOOL_VAL(false);
    return true;
  }

  args[-1] = BOOL_VAL(true);
  return true;
}

bool getenvNative(Value *args) {
  char *env = AS_CSTRING(args[0]);
  char *envVal = getenv(env);
  if (envVal != NULL) {
    args[-1] = OBJ_VAL(copyString(envVal, strlen(envVal)));
    return true;
  }
  args[-1] = NIL_VAL;
  return true;
}

bool assertNative(Value *args) {

  bool eval = AS_BOOL(args[0]);
  if (eval) {
    args[-1] = BOOL_VAL(true);
    return true;
  }

  args[-1] = args[1];
  return false;
}

void defineNativeFns(void) {
  defineNative("clock", clockNative, 0);
  defineNative("hasField", hasFieldNative, 2);
  defineNative("deleteField", deleteFieldNative, 2);
  defineNative("readLine", readLineNative, 0);
  defineNative("fileExists", fileExistsNative, 1);
  defineNative("readFile", readFileNative, 1);
  defineNative("writeFile", writeFileNative, 2);
  defineNative("getenv", getenvNative, 1);
  defineNative("assert", assertNative, 2);
}
