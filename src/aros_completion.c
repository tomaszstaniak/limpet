#include "aros_completion.h"
#include "completion_args.h"
#include "completion_defs.h"
#include <proto/exec.h>
#include <exec/memory.h>
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

/* Reads a definition file; the path is relative to the current directory.
 * The spec keeps pointers into text. */
static int definition(const char *path,char *text,XArgSpec *spec)
{
    LONG got=-1;
    BPTR file=Open((CONST_STRPTR)path,MODE_OLDFILE);
    if(!file) return 0;
    got=Read(file,text,XARGS_DEFINITION_MAX+1);
    Close(file);
    return got>0 && got<=XARGS_DEFINITION_MAX && completion_args_parse(spec,text,(size_t)got);
}

/* The definition a program brings along: Completions/<name> in the drawer
 * the Shell would load the program from. `typed` is the command as typed.
 * The Shell looks in the current directory, then along its path, then in
 * C:; the first place that has the program decides. */
static int beside(const char *typed,const char *name,struct Process *reader,char *text,XArgSpec *spec)
{
    struct CommandLineInterface *cli=BADDR(reader->pr_CLI);
    BPTR *path=cli?BADDR(cli->cli_CommandDir):NULL;
    BPTR found=BNULL,old,parent;
    char file[64];
    unsigned entries=0;
    int ok=0;
    if(strchr(typed,':') || strchr(typed,'/')) {
        old=CurrentDir(reader->pr_CurrentDir);
        found=Lock((CONST_STRPTR)typed,SHARED_LOCK);
        CurrentDir(old);
    } else {
        old=CurrentDir(reader->pr_CurrentDir);
        found=Lock((CONST_STRPTR)name,SHARED_LOCK);
        while(!found && path && ++entries<=32) {
            CurrentDir(path[1]);
            found=Lock((CONST_STRPTR)name,SHARED_LOCK);
            path=BADDR(path[0]);
        }
        CurrentDir(old);
        if(!found) {
            strcpy(file,"C:"); strcat(file,name);
            found=Lock((CONST_STRPTR)file,SHARED_LOCK);
        }
    }
    if(!found) return 0;
    parent=ParentDir(found);
    UnLock(found);
    if(!parent) return 0;
    strcpy(file,"Completions/"); strcat(file,name);
    old=CurrentDir(parent);
    ok=definition(file,text,spec);
    CurrentDir(old);
    UnLock(parent);
    return ok;
}

/* What the command's definition offers for an argument (completion_args.h).
 * The definition is looked for in this order:
 *   1. ENVARC:Limpet/Completions/<command>, the user's own;
 *   2. Completions/<command> in the drawer of the program;
 *   3. SYS:Packages/<command>/Completions/<command>, a package installed
 *      by apkg in its usual place (after "apkg --root DRAWER run", under
 *      that drawer);
 *   4. the definitions that come with Limpet.
 * After "apkg run <id>" the arguments are those of the package's program:
 * 1 and 3 with the id. An alias has 1 and 3 only: its arguments need not
 * be those of the command of the same name. Returns which files belong
 * beside the offers: 0 all, 1 drawers only, 2 none. */
static int arguments(XCompletion *c,struct Process *reader)
{
    char name[40],typed[XLINE_CAPACITY],path[256],root[108],*text;
    XArgSpec spec;
    struct Node *var;
    const char *builtin;
    size_t from,i,run_from;
    char id[40];
    int alias=0,run=0,have=0,files=0;
    unsigned entries=0;
    if(c->command || !completion_args_command(c->expected,c->start,name,sizeof name,&from)) return 0;
    if(!completion_args_program(c->expected,c->start,typed,sizeof typed)) return 0;
    for(i=0;typed[i];++i)
        if(typed[i]=='#' || typed[i]=='?' || typed[i]=='"' || typed[i]=='*' || (unsigned char)typed[i]<32) return 0;
    for(var=(struct Node *)reader->pr_LocalVars.mlh_Head;var->ln_Succ;var=var->ln_Succ) {
        if(++entries>4096) break;
        if(var->ln_Type==LV_ALIAS && var->ln_Name && !stricmp((const char *)var->ln_Name,name)) { alias=1; break; }
    }
    root[0]=0;
    if(!alias && completion_args_run(c->expected,from,c->start,name,id,sizeof id,&run_from,root,sizeof root)) {
        run=1; from=run_from; strcpy(name,id);
    }
    if(!(text=AllocVec(XARGS_DEFINITION_MAX+1,MEMF_ANY))) return 0;
    strcpy(path,"ENVARC:Limpet/Completions/"); strcat(path,name);
    have=definition(path,text,&spec);
    if(!have && !alias && !run) have=beside(typed,name,reader,text,&spec);
    if(!have) {
        /* apkg's usual place, or the drawer named with apkg --root, which
         * may be relative to the Shell's current directory. */
        BPTR old=CurrentDir(reader->pr_CurrentDir);
        size_t n;
        strcpy(path,root[0]?root:"SYS:Packages");
        n=strlen(path);
        if(path[n-1]!=':' && path[n-1]!='/') strcat(path,"/");
        strcat(path,name); strcat(path,"/Completions/"); strcat(path,name);
        have=definition(path,text,&spec);
        CurrentDir(old);
    }
    if(!have && !alias && !run && (builtin=completion_definition(name)))
        have=completion_args_parse(&spec,builtin,strlen(builtin));
    if(have) files=completion_args_offer_from(c,&spec,from);
    FreeVec(text);
    return files;
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
