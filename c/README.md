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
