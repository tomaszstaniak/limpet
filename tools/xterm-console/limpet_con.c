/* LimpetCON ON | OFF | STATUS
 *
 * ON makes CON: open Limpet windows: it puts Limpet's handler launcher
 * (limpet-handler, beside this program) into the CON device node, after
 * saving what the node held. OFF puts that back. Nothing is written to
 * disk, so a reboot always brings the system's CON: back. Windows that are
 * open keep the console that opened them.
 *
 * What Limpet cannot serve (a console in the caller's own window, a
 * console that does not come up) the launcher hands to the saved system
 * handler; see tools/xterm_handler.c and doc/limpet-handler.md.
 *
 * The launcher's segment and the saved state are never freed: a console
 * may be starting from them. That is a few KB until the reboot. */
#include "../../src/limpet_con.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <dos/rdargs.h>
#include <exec/memory.h>
#include <string.h>

static const char version[] __attribute__((used))="$VER: LimpetCON " LIMPET_VERSION " (" LIMPET_DATE ")";

static LimpetConState *state_find(void)
{
    LimpetConState *s;
    Forbid();
    s=(LimpetConState *)FindPort((CONST_STRPTR)LIMPETCON_PORT);
    Permit();
    return s && s->magic==LIMPETCON_MAGIC?s:NULL;
}

static LimpetConState *state_make(void)
{
    LimpetConState *s=state_find(),*other;
    if(s) return s;
    s=AllocMem(sizeof *s,MEMF_PUBLIC|MEMF_CLEAR);
    if(!s) return NULL;
    strcpy(s->name,LIMPETCON_PORT);
    s->magic=LIMPETCON_MAGIC;
    s->port.mp_Node.ln_Type=NT_MSGPORT;
    s->port.mp_Node.ln_Name=s->name;
    s->port.mp_Flags=PA_IGNORE;
    NewList(&s->port.mp_MsgList);
    Forbid();
    other=(LimpetConState *)FindPort((CONST_STRPTR)LIMPETCON_PORT);
    if(!other) AddPort(&s->port);
    Permit();
    if(other) { FreeMem(s,sizeof *s); return other->magic==LIMPETCON_MAGIC?other:NULL; }
    return s;
}

/* The system console handler's segment when the node has none yet, found
   the way DOS does it (rom/dos/runhandler.c). */
static BPTR resolve(BSTR handler)
{
    CONST_STRPTR name=handler?(CONST_STRPTR)AROS_BSTR_ADDR(handler):NULL;
    struct Segment *segment;
    BPTR seg=0;
    if(!name) return 0;
    Forbid();
    segment=FindSegment(name,NULL,TRUE);
    if(segment) seg=segment->seg_Seg;
    Permit();
    if(!seg) seg=LoadSeg(name);
    if(!seg) {
        BPTR dir=Lock((CONST_STRPTR)"L:",SHARED_LOCK);
        if(dir) { BPTR old=CurrentDir(dir); seg=LoadSeg(FilePart(name)); CurrentDir(old); UnLock(dir); }
    }
    return seg;
}

static struct DeviceNode *con_node(struct DosList *list)
{ return (struct DeviceNode *)FindDosEntry(list,(CONST_STRPTR)"CON",LDF_DEVICES); }

static int turn_on(void)
{
    char path[512],worker[512];
    LimpetConState *s;
    struct DosList *list;
    struct DeviceNode *con;
    BSTR handler,old_handler;
    BPTR seg,dir,lock,system_seg,node_seg;
    ULONG length;
    const char *why=NULL;
    dir=GetProgramDir();
    if(!dir || !NameFromLock(dir,(STRPTR)path,sizeof path)) { PutStr((CONST_STRPTR)"LimpetCON: cannot name its own drawer\n"); return RETURN_FAIL; }
    strcpy(worker,path);
    if(!AddPart((STRPTR)path,(CONST_STRPTR)"limpet-handler",sizeof path) ||
       !AddPart((STRPTR)worker,(CONST_STRPTR)"limpet-console",sizeof worker)) { PutStr((CONST_STRPTR)"LimpetCON: the drawer's path is too long\n"); return RETURN_FAIL; }
    lock=Lock((CONST_STRPTR)worker,SHARED_LOCK);
    if(!lock) { Printf((CONST_STRPTR)"LimpetCON: %s is missing\n",(IPTR)worker); return RETURN_FAIL; }
    UnLock(lock);
    s=state_make();
    if(!s) { PutStr((CONST_STRPTR)"LimpetCON: no memory\n"); return RETURN_FAIL; }
    list=LockDosList(LDF_DEVICES|LDF_READ);
    con=con_node(list);
    old_handler=con?con->dn_Handler:0; node_seg=con?con->dn_SegList:0;
    if(!con) why="there is no CON: device";
    else if(con->dn_Task) why="CON: has a resident handler task; left alone";
    else if(s->active && s->limpet_seg && node_seg==s->limpet_seg) why="CON: already opens Limpet windows";
    UnLockDosList(LDF_DEVICES|LDF_READ);
    if(why) { Printf((CONST_STRPTR)"LimpetCON: %s\n",(IPTR)why); return RETURN_WARN; }
    system_seg=node_seg?node_seg:resolve(old_handler);
    if(!system_seg) { PutStr((CONST_STRPTR)"LimpetCON: the system console handler was not found; nothing changed\n"); return RETURN_FAIL; }
    seg=LoadSeg((CONST_STRPTR)path);
    if(!seg) { Printf((CONST_STRPTR)"LimpetCON: cannot load %s\n",(IPTR)path); return RETURN_FAIL; }
    length=strlen(path);
    handler=MKBADDR(AllocVec(AROS_BSTR_MEMSIZE4LEN(length+1),MEMF_PUBLIC|MEMF_CLEAR));
    if(!handler) { UnLoadSeg(seg); PutStr((CONST_STRPTR)"LimpetCON: no memory\n"); return RETURN_FAIL; }
    CopyMem(path,AROS_BSTR_ADDR(handler),length);
    AROS_BSTR_setstrlen(handler,length);
    list=LockDosList(LDF_DEVICES|LDF_WRITE);
    con=con_node(list);
    if(!con || con->dn_Task || con->dn_Handler!=old_handler || con->dn_SegList!=node_seg) why="CON: changed meanwhile; nothing changed";
    else {
        /* The state first: a console starting right after the swap must
           find the system handler in it. */
        Forbid();
        s->handler=old_handler; s->seglist=system_seg;
        s->stack=con->dn_StackSize; s->priority=con->dn_Priority; s->globvec=con->dn_GlobalVec;
        s->limpet_seg=seg; s->limpet_handler=handler; s->active=1;
        Permit();
        con->dn_Handler=handler; con->dn_SegList=seg;
        con->dn_StackSize=65536; con->dn_Priority=5; con->dn_GlobalVec=(BPTR)-1;
    }
    UnLockDosList(LDF_DEVICES|LDF_WRITE);
    if(why) { UnLoadSeg(seg); FreeVec(BADDR(handler)); Printf((CONST_STRPTR)"LimpetCON: %s\n",(IPTR)why); return RETURN_FAIL; }
    PutStr((CONST_STRPTR)"CON: opens Limpet windows until LimpetCON OFF or the next reboot.\n");
    return RETURN_OK;
}

static int turn_off(void)
{
    LimpetConState *s=state_find();
    struct DosList *list;
    struct DeviceNode *con;
    const char *why=NULL;
    if(!s || !s->active) { PutStr((CONST_STRPTR)"LimpetCON: CON: is the system's already\n"); return RETURN_WARN; }
    list=LockDosList(LDF_DEVICES|LDF_WRITE);
    con=con_node(list);
    if(!con) why="there is no CON: device";
    else if(con->dn_SegList!=s->limpet_seg) why="CON: no longer holds Limpet's handler; left alone";
    else {
        con->dn_Handler=s->handler; con->dn_SegList=s->seglist;
        con->dn_StackSize=s->stack; con->dn_Priority=s->priority; con->dn_GlobalVec=s->globvec;
        Forbid(); s->active=0; Permit();
    }
    UnLockDosList(LDF_DEVICES|LDF_WRITE);
    if(why) { Printf((CONST_STRPTR)"LimpetCON: %s\n",(IPTR)why); return RETURN_FAIL; }
    PutStr((CONST_STRPTR)"CON: is the system's console again. Open Limpet windows keep running.\n");
    return RETURN_OK;
}

static int status(void)
{
    LimpetConState *s=state_find();
    struct DosList *list;
    struct DeviceNode *con;
    int limpet;
    list=LockDosList(LDF_DEVICES|LDF_READ);
    con=con_node(list);
    limpet=con && s && s->active && con->dn_SegList==s->limpet_seg;
    UnLockDosList(LDF_DEVICES|LDF_READ);
    PutStr((CONST_STRPTR)(limpet?"CON: opens Limpet windows (LimpetCON ON).\n":"CON: is the system's console.\n"));
    return limpet?RETURN_OK:RETURN_WARN;
}

int main(void)
{
    IPTR args[3]={0,0,0};
    struct RDArgs *rd=ReadArgs((CONST_STRPTR)"ON/S,OFF/S,STATUS/S",(APTR)args,NULL);
    int result;
    if(!rd) { PrintFault(IoErr(),(CONST_STRPTR)"LimpetCON"); return RETURN_FAIL; }
    if((args[0]!=0)+(args[1]!=0)+(args[2]!=0)!=1) {
        PutStr((CONST_STRPTR)"Usage: LimpetCON ON | OFF | STATUS\n"); result=RETURN_WARN;
    }
    else if(args[0]) result=turn_on();
    else if(args[1]) result=turn_off();
    else result=status();
    FreeArgs(rd);
    return result;
}
