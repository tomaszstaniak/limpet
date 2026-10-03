/* Native mainline Run regression; pass an explicit standalone Run executable.
 * No system command is replaced. All files are unique temporary RAM files. */
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dostags.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv)
{
    char path[120],command[700],data[128];
    struct Process *me=(struct Process *)FindTask(NULL);
    BPTR f=0,lock=0,copy=0;
    LONG result,err; unsigned ticks,round; int ok=0,all_ok=1;
    if(argc!=2 || strchr(argv[1],'"') || strchr(argv[1],'*')) return 20;
    snprintf(path,sizeof path,"RAM:xt-run-check-%lu",(unsigned long)me->pr_TaskNum);
    f=Open(path,MODE_NEWFILE); if(!f) return 20;
    lock=DupLockFromFH(f); err=IoErr();
    printf("exclusive duplicate: lock=%ld error=%ld\n",(long)lock,(long)err);
    if(lock) UnLock(lock);
    result=ChangeMode(CHANGE_FH,f,SHARED_LOCK);
    printf("share handle: result=%ld error=%ld\n",(long)result,(long)IoErr());
    lock=DupLockFromFH(f);
    if(lock) { copy=OpenFromLock(lock); if(!copy) UnLock(lock); }
    printf("shared duplicate: handle=%ld error=%ld\n",(long)copy,(long)IoErr());
    if(copy) Close(copy);
    Close(f); DeleteFile(path);
    for(round=0;round<2;++round) {
    ok=0;
    if(round) {
        f=Open(path,MODE_NEWFILE); if(!f) return 20;
        if(Write(f,"PREFIX\n",7)!=7) { Close(f); return 20; }
        Close(f);
    }
    snprintf(command,sizeof command,"\"%s\" %s%s Echo RUN_REDIRECT_OK",argv[1],round?">>":">",path);
    result=SystemTags(command,SYS_Asynch,FALSE,TAG_DONE);
    printf("Run launch result=%ld error=%ld\n",(long)result,(long)IoErr());
    for(ticks=0;ticks<250;++ticks) {
        f=Open(path,MODE_OLDFILE);
        if(f) {
            LONG n=Read(f,data,sizeof data-1); Close(f);
            if(n>=0) { data[n]=0; if(!strcmp(data,round?"PREFIX\nRUN_REDIRECT_OK\n":"RUN_REDIRECT_OK\n")) { ok=1; break; } }
        }
        Delay(1);
    }
    if(!DeleteFile(path)) printf("cleanup pending: %s\n",path);
    printf("Run %s output %s\n",round?"append":"redirected",ok?"PASS":"FAIL");
    if(!ok) all_ok=0;
    }
    return all_ok?0:20;
}
