#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
#define DEBUG_PRINT_CODE
#define DEBUG_TRACE_EXECUTION
#define DEBUG_STRESS_GC
#define DEBUG_LOG_GC
*/

#define UINT8_COUNT (UINT8_MAX + 1)

#define GRAY "\x1b[90m"
#define BOLD "\x1b[1m"
#define RED_BOLD "\x1b[1;31m"
#define BLUE "\x1b[1;34m"
#define RESET "\x1b[0m"

#endif
