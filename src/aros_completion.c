#include "aros_completion.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/var.h>
#include <string.h>

/* One TAB scans at most 4096 entries and 32 Path/assign members. This bounds
 * work and memory, not the response time of an individual DOS packet. */
static int scan_lock(XCompletion *c,BPTR lock,unsigned *budget,int files_only)
{
    struct FileInfoBlock *fib=AllocDosObject(DOS_FIB,NULL);
    int ok=0;
    if(!fib) return 0;
    if(lock && Examine(lock,fib) && fib->fib_DirEntryType>0) {
        ok=1;
        while(*budget && ExNext(lock,fib)) {
            --*budget;
            if(!files_only || fib->fib_DirEntryType<0)
                completion_add(c,(const char *)fib->fib_FileName,fib->fib_DirEntryType>0?1:0);
        }
        if(!*budget) c->limited=1;
        else if(IoErr()!=ERROR_NO_MORE_ENTRIES) ok=0;
    }
    FreeDosObject(DOS_FIB,fib); return ok;
}

static int scan_path(XCompletion *c,const char *path,struct Process *reader,
                     unsigned *budget,int files_only)
{
    BPTR lock,old;
    const char *colon=strchr(path,':');
    int ok=0;
    if(!colon) {
        old=CurrentDir(reader->pr_CurrentDir);
        lock=Lock((CONST_STRPTR)path,SHARED_LOCK);
        CurrentDir(old);
        ok=scan_lock(c,lock,budget,files_only); if(lock) UnLock(lock);
    } else if((size_t)(colon-path)==7 &&
              !strnicmp(path,"PROGDIR",7)) {
        old=CurrentDir(reader->pr_HomeDir);
        lock=reader->pr_HomeDir?Lock((CONST_STRPTR)(colon+1),SHARED_LOCK):BNULL;
        CurrentDir(old);
        ok=scan_lock(c,lock,budget,files_only); if(lock) UnLock(lock);
    } else {
        struct DevProc *dp=NULL;
        unsigned members=0;
        char root[XLINE_CAPACITY]; size_t n=(size_t)(colon-path)+1;
        memcpy(root,path,n); root[n]=0;
        old=CurrentDir(reader->pr_CurrentDir);
        while(*budget && (dp=GetDeviceProc((CONST_STRPTR)root,dp))!=NULL) {
            BPTR memberold;
            if(++members>32) { c->limited=1; break; }
            memberold=CurrentDir(dp->dvp_Lock?dp->dvp_Lock:reader->pr_CurrentDir);
            /* Mainline Lock("") with CurrentDir(NULL) means boot root,
             * regardless of SetFileSysTask. Keep the device prefix when
             * GetDeviceProc has no member lock (device/volume roots), and
             * retain the reader's cwd for the current-volume ':' shorthand. */
            lock=Lock((CONST_STRPTR)(dp->dvp_Lock?colon+1:path),SHARED_LOCK);
            CurrentDir(memberold);
            ok|=scan_lock(c,lock,budget,files_only); if(lock) UnLock(lock);
            if(!(dp->dvp_Flags&DVPF_ASSIGN)) break;
        }
        if(dp) FreeDeviceProc(dp);
        CurrentDir(old);
    }
    return ok;
}

int aros_completion_collect(XCompletion *c,struct Process *p)
{
    struct Process *me=(struct Process *)FindTask(NULL);
    APTR oldwindow;
    unsigned budget=4096;
    int ok;
    if(!p || p->pr_Task.tc_Node.ln_Type!=NT_PROCESS) return 0;
    oldwindow=me->pr_WindowPtr; me->pr_WindowPtr=(APTR)-1;
    ok=scan_path(c,c->directory,p,&budget,0);
    if(!c->directory[0]) {
        struct DosList *list;
        unsigned entries=0;
        ULONG flags=LDF_READ|LDF_VOLUMES|LDF_DEVICES|LDF_ASSIGNS;
        list=LockDosList(flags);
        while((list=NextDosEntry(list,flags))!=NULL) {
            if(++entries>4096) { c->limited=1; break; }
            completion_add(c,(const char *)AROS_BSTR_ADDR(list->dol_Name),2);
        }
        UnLockDosList(flags);
    }
    if(c->command) {
        struct CommandLineInterface *cli=BADDR(p->pr_CLI);
        BPTR *path=cli?BADDR(cli->cli_CommandDir):NULL;
        struct DosInfo *info=BADDR(DOSBase->dl_Root->rn_Info);
        struct Segment *seg;
        struct Node *var;
        unsigned entries=0;
        /* The reader is blocked in our unreplied Read packet. Copy names
         * only; values and argument expansion remain the Shell's job. */
        for(var=(struct Node *)p->pr_LocalVars.mlh_Head;var->ln_Succ;var=var->ln_Succ) {
            if(++entries>4096) { c->limited=1; break; }
            if(var->ln_Type==LV_ALIAS)
                completion_add(c,(const char *)var->ln_Name,0);
        }
        entries=0;
        while(path && budget) {
            if(++entries>32) { c->limited=1; break; }
            ok|=scan_lock(c,path[1],&budget,1);
            path=BADDR(path[0]);
        }
        ok|=scan_path(c,"C:",p,&budget,1);
        entries=0;
        /* Mainline's FindSegment/AddSegment contract protects this list with
         * Forbid. Only copy names here: no allocation or DOS calls. */
        Forbid();
        for(seg=BADDR(info->di_ResList);seg;seg=BADDR(seg->seg_Next)) {
            if(++entries>4096) { c->limited=1; break; }
            if(seg->seg_UC!=CMD_DISABLED)
                completion_add(c,(const char *)AROS_BSTR_ADDR(MKBADDR(seg->seg_Name)),0);
        }
        Permit();
    }
    me->pr_WindowPtr=oldwindow;
    return ok || c->count;
}
