/* Just enough of dos.library for a host build of a terminal producer: the
   output goes to stdout, there is never input, and SetMode does nothing.
   Used to check a producer's byte stream on the host, never shipped. */
#ifndef AROS_XTERM_HOST_DOS_SHIM_H
#define AROS_XTERM_HOST_DOS_SHIM_H
#include <stdint.h>
#include <unistd.h>
typedef int32_t LONG;typedef void *APTR;typedef intptr_t BPTR;
#define HOST_DOS_SHIM 1
static inline BPTR Output(void) { return 1; }
static inline BPTR Input(void) { return 0; }
static inline LONG Write(BPTR f,APTR b,LONG n) { return (LONG)write((int)f,b,(size_t)n); }
static inline LONG Read(BPTR f,APTR b,LONG n) { return (LONG)read((int)f,b,(size_t)n); }
static inline LONG WaitForChar(BPTR f,LONG t) { (void)f;(void)t;return 0; }
static inline LONG SetMode(BPTR f,LONG m) { (void)f;(void)m;return 1; }
#endif
