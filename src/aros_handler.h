#ifndef XTERM_AROS_HANDLER_H
#define XTERM_AROS_HANDLER_H
#include <dos/dosextens.h>
#define XHANDLER_MAGIC 0x58544831UL
#define XHANDLER_ARGUMENT "XTERM-DOS-WORKER\n"
/* Stack-local to the CRT-free launcher. Never published outside this process. */
typedef struct {
    ULONG magic;
    struct DosPacket *startup;
    int replied;
} XHandlerContext;
#ifdef XTERM_DOS_HANDLER
#include "aros_session.h"
XHandlerContext *aros_handler_context(void);
int aros_handler_accept(XHandlerContext *,ArosSession *);
#endif
#ifdef XTERM_PKTWAIT_PROBE
/* Test builds: synchronous DOS waits counted since accept; stop before exit. */
unsigned long aros_handler_pktwaits(void);
void aros_handler_probe_stop(void);
#endif
#endif
