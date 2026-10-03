#ifndef XTERM_AROS_CLIP_JOB_H
#define XTERM_AROS_CLIP_JOB_H
#include "aros_clipboard.h"
#include <stdint.h>
typedef struct {
    uint32_t task_id;
    unsigned unit;
    int writing, result;
    size_t length;
    unsigned char bytes[AROS_CLIP_LIMIT];
} ArosClipJob;
/* Zero-initialize. One job at a time; caller retains storage until poll returns
 * 1. Completion is signalled with SIGF_CHILD; poll also reaps the child.
 * Parent must not exit/unload code while task_id is nonzero. No forced abort. */
int aros_clip_job_start(ArosClipJob *,int writing,unsigned unit,const unsigned char *,size_t);
int aros_clip_job_poll(ArosClipJob *);
#endif
