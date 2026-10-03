/* Host-only ABI boundary for testing the real optional fd adapter. */
#ifndef TEST_FD_IO_DOS_H
#define TEST_FD_IO_DOS_H
#include <stdint.h>
typedef intptr_t BPTR;
typedef int32_t LONG;
#define ERROR_BAD_NUMBER 115
#define ERROR_ACTION_NOT_KNOWN 209
#define ERROR_OBJECT_IN_USE 202
#define ERROR_BREAK 304
#endif
