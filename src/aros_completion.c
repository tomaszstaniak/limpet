#include "aros_completion.h"
#include "completion_args.h"
#include "completion_defs.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/var.h>
#include <string.h>

/* One TAB scans at most 4096 entries and 32 Path/assign members. This bounds
 * work and memory, not the response time of an individual DOS packet. */
static int scan_lock(XCompletion *c,BPTR lock,unsigned *budget,int want)
{
    struct FileInfoBlock *fib=AllocDosObject(DOS_FIB,NULL);
    int ok=0;
    if(!fib) return 0;
    if(lock && Examine(lock,fib) && fib->fib_DirEntryType>0) {
        ok=1;
        while(*budget && ExNext(lock,fib)) {
            --*budget;
            if(want&(fib->fib_DirEntryType>0?2:1))
                completion_add(c,(const char *)fib->fib_FileName,fib->fib_DirEntryType>0?1:0);
        }
        if(!*budget) c->limited=1;
        else if(IoErr()!=ERROR_NO_MORE_ENTRIES) ok=0;
    }
    FreeDosObject(DOS_FIB,fib); return ok;
}

static int scan_path(XCompletion *c,const char *path,struct Process *reader,
                     unsigned *budget,int want)
{
    BPTR lock,old;
    const char *colon=strchr(path,':');
    int ok=0;
    if(!colon) {
        old=CurrentDir(reader->pr_CurrentDir);
        lock=Lock((CONST_STRPTR)path,SHARED_LOCK);
        CurrentDir(old);
        ok=scan_lock(c,lock,budget,want); if(lock) UnLock(lock);
    } else if((size_t)(colon-path)==7 &&
              !strnicmp(path,"PROGDIR",7)) {
        old=CurrentDir(reader->pr_HomeDir);
        lock=reader->pr_HomeDir?Lock((CONST_STRPTR)(colon+1),SHARED_LOCK):BNULL;
        CurrentDir(old);
        ok=scan_lock(c,lock,budget,want); if(lock) UnLock(lock);
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
            ok|=scan_lock(c,lock,budget,want); if(lock) UnLock(lock);
            if(!(dp->dvp_Flags&DVPF_ASSIGN)) break;
        }
        if(dp) FreeDeviceProc(dp);
        CurrentDir(old);
    }
    return ok;
}

/* What the command's definition offers for an argument (completion_args.h):
 * the user's file ENVARC:Limpet/Completions/<command> first, then the
 * definitions that come with Limpet. An alias has only the user's: its
 * arguments need not be those of the command of the same name. Returns
 * which files belong beside the offers: 0 all, 1 drawers only, 2 none. */
static int arguments(XCompletion *c,struct Process *reader)
{
    static const char drawer[]="ENVARC:Limpet/Completions/";
    char name[40],path[sizeof drawer+sizeof name],text[XARGS_DEFINITION_MAX+1];
    XArgSpec spec;
    struct Node *var;
    const char *builtin;
    size_t from,i;
    LONG got=-1;
    BPTR file;
    int alias=0;
    unsigned entries=0;
    if(c->command || !completion_args_command(c->expected,c->start,name,sizeof name,&from)) return 0;
    for(i=0;name[i];++i)
        if(name[i]=='#' || name[i]=='?' || name[i]=='"' || name[i]=='*' || (unsigned char)name[i]<32) return 0;
    for(var=(struct Node *)reader->pr_LocalVars.mlh_Head;var->ln_Succ;var=var->ln_Succ) {
        if(++entries>4096) break;
        if(var->ln_Type==LV_ALIAS && var->ln_Name && !stricmp((const char *)var->ln_Name,name)) { alias=1; break; }
    }
    strcpy(path,drawer); strcat(path,name);
    if((file=Open((CONST_STRPTR)path,MODE_OLDFILE))) {
        got=Read(file,text,sizeof text);
        Close(file);
    }
    if(got>0 && (size_t)got<sizeof text && completion_args_parse(&spec,text,(size_t)got))
        return completion_args_offer(c,&spec);
    if(alias || !(builtin=completion_definition(name)) ||
       !completion_args_parse(&spec,builtin,strlen(builtin))) return 0;
    return completion_args_offer(c,&spec);
}

int aros_completion_collect(XCompletion *c,struct Process *p)
{
    struct Process *me=(struct Process *)FindTask(NULL);
    APTR oldwindow;
    unsigned budget=4096;
    int ok=0,files;
    if(!p || p->pr_Task.tc_Node.ln_Type!=NT_PROCESS) return 0;
    oldwindow=me->pr_WindowPtr; me->pr_WindowPtr=(APTR)-1;
    files=arguments(c,p);
    if(files!=2) ok=scan_path(c,c->directory,p,&budget,files==1?2:3);
    if(!c->directory[0] && files!=2) {
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
