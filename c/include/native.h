#ifndef NATIVE_H
#define NATIVE_H

#include "object.h"

void defineNativeFns(void);
bool clockNative(Value *args);
bool hasFieldNative(Value *args);
bool deleteFieldNative(Value *args);
bool readLineNative(Value *args);
bool fileExistsNative(Value *args);
bool readFileNative(Value *args);
bool writeFileNative(Value *args);
bool getenvNative(Value *args);
bool assertNative(Value *args);

#endif
