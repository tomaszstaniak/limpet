/* Host stand-in for <proto/dos.h>. The test supplies these functions and
   decides what the "handler" answers. */
#ifndef LIMPET_HOST_PROTO_DOS_H
#define LIMPET_HOST_PROTO_DOS_H
#include <dos/dos.h>
SIPTR DoPkt(void *port, LONG action, SIPTR a1, SIPTR a2, SIPTR a3, SIPTR a4, SIPTR a5);
LONG IoErr(void);
LONG SetIoErr(LONG code);
LONG IsInteractive(BPTR handle);
LONG Write(BPTR handle, APTR buffer, LONG length);
#endif
