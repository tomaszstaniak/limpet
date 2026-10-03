#ifndef XTERM_AROS_COMPLETION_JOB_H
#define XTERM_AROS_COMPLETION_JOB_H
#include "completion.h"
#include <dos/dosextens.h>
#include <stdint.h>
/* TAB enumeration in a child process, so the caller makes no synchronous DOS
 * call itself (required when its own process port receives client packets).
 * Zero-initialize. One job at a time. The caller must keep the reader blocked
 * in its unreplied Read and must not touch the completion until poll returns
 * 1: the child reads both, including the reader's current and home dirs.
 * Completion is signalled with SIGF_CHILD; poll reaps the child. No abort. */
typedef struct {
    uint32_t task_id;
    XCompletion *completion;
    struct Process *reader;
    int result;
} ArosCompletionJob;
int aros_completion_job_start(ArosCompletionJob *,XCompletion *,struct Process *reader);
int aros_completion_job_poll(ArosCompletionJob *);
#endif
