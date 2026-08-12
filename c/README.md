This is the bytecode VM language implementation in C.

# Build Instructions

## Prerequisites
- CMake 4.3 or higher
- A C17 compiler

## Building
```bash
cmake -B build/
cmake --build build -j$(nproc)
```
Change the `$(nproc)` to the number of cores you want to use to compile.

## Running
The binary can be found at `build/scarp`.
```bash
./build/scarp
```

# Language Tour

This is the C version that is fully compatible with the [Java version](../java/README.md) of the language except for getters.

It is highly recommended to read the [Java version README](../java/README.md) before reading this one.

The bytecode VM in C is considerably faster and more memory-efficient than the tree-walk interpreter in Java.

The C version introduces two additional control-flow constructs:

## Table of Contents
- [Continue](#continue)
- [Switch](#switch)

### Continue
The continue statement allows you to skip to the next iteration of the nearest enclosing for or while loop.

```swift
/* 
both loops print:
0
1
2
*/
for (let i = 0; i < 4; i = i + 1) {
    if (i == 3) {
        continue;
    }
    print i;
}

let i = 0;
while (i < 4) {
    if (i == 3) {
        i = i + 1;
        continue;
    }
    print i;
    i = i + 1;
}
```

### Switch
The switch statement is a way to do multi-way branching with case and default keywords.
You cannot fallthrough to another case statement so break does not work in a switch statement, only in loops.

```swift
// prints 3
let i = 3;
switch (i) {
    case 1:
        print "i is 1";
    case 2:
        print "i is 2";
    case 3: {
        print "i is 3";
    }
    default:
        print "i is neither 1 or 2 or 3";
}
```

