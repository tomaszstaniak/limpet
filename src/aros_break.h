#ifndef XTERM_AROS_BREAK_H
#define XTERM_AROS_BREAK_H
#include <exec/semaphores.h>
#include <utility/hooks.h>
typedef struct {
    struct SignalSemaphore gate;
    struct Hook hook;
    APTR resource;
    struct Task *target;
    int registered;
} ArosBreak;
/* Optional mainline lifecycle support. No unsafe fallback to a cached Task*. */
void aros_break_open(ArosBreak *);
/* Caller keeps task alive until bind returns, e.g. an outstanding packet or
   a launch gate. After binding, removal notification protects its lifetime. */
void aros_break_bind(ArosBreak *,struct Task *);
/* Readiness is a synchronized hint; send still revalidates under the gate. */
int aros_break_ready(ArosBreak *);
int aros_break_send(ArosBreak *);
void aros_break_close(ArosBreak *);
#endif
