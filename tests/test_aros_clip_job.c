#include "../src/aros_clip_job.h"
#include <proto/exec.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static ArosClipJob job;
static unsigned char before[AROS_CLIP_LIMIT];
static int finish(void)
{
    while(!aros_clip_job_poll(&job)) Wait(SIGF_CHILD);
    return job.result;
}
int main(void)
{
    size_t n=0; int failed=0;
    static const unsigned char clip[]={ 'F','O','R','M',0,0,0,16,'F','T','X','T',
                                       'C','H','R','S',0,0,0,3,'J','O','B',0 };
    assert(!aros_clip_job_start(&job,0,237,NULL,0));
    assert(aros_clip_job_start(&job,0,237,NULL,0)==-1); /* Cannot reuse live storage. */
    assert(!finish()); n=job.length; memcpy(before,job.bytes,n);
    if(aros_clip_job_start(&job,1,237,clip,sizeof clip) || finish()) failed=1;
    if(!failed) {
        if(aros_clip_job_start(&job,0,237,NULL,0) || finish() ||
           job.length!=sizeof clip || memcmp(job.bytes,clip,sizeof clip)) failed=1;
    }
    if(aros_clip_write(237,before,n)) { puts("worker test: RESTORE FAILED"); return 20; }
    if(failed) { puts("worker test FAILED (original restored)"); return 20; }
    assert(!job.task_id);
    puts("native clipboard worker passed (read/write, busy rejection, reap, restore)");
    return 0;
}
