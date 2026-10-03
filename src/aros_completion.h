#ifndef XTERM_AROS_COMPLETION_H
#define XTERM_AROS_COMPLETION_H
#include "completion.h"
#include <dos/dosextens.h>
/* Caller keeps reader blocked in a synchronous DOS Read throughout this call.
 * No reader pointers/locks survive the call. Returns 0 if enumeration failed.
 * Bounded synchronous disk enumeration: this is not a timeout guarantee. */
int aros_completion_collect(XCompletion *, struct Process *reader);
#endif
