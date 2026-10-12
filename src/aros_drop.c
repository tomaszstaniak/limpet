#include "aros_drop.h"
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/workbench.h>
#include <stdio.h>
#include <string.h>

/* Opened here, not by the start-up code: Limpet runs without it. */
struct Library *WorkbenchBase;

/* Exec calls this, in a software interrupt, for each message PutMsg puts
   on the AppWindow port. Only GetMsg, PutMsg and ReplyMsg here: exec
   allows those in interrupts. */
static AROS_INTH1(drop_arrived, ArosDrop *, d)
{
    AROS_INTFUNC_INIT
    struct Message *m;
    while ((m = GetMsg(d->port))) {
        if (d->closing) { ReplyMsg(m); d->answered++; }
        else PutMsg(d->inbox, m);
    }
    return 0;
    AROS_INTFUNC_EXIT
}

int aros_drop_open(ArosDrop *d, struct Window *window)
{
    memset(d, 0, sizeof *d);
    if (!(WorkbenchBase = OpenLibrary((CONST_STRPTR)"workbench.library", 0))) return 0;
    d->intr.is_Node.ln_Type = NT_INTERRUPT;
    d->intr.is_Node.ln_Name = (char *)"Limpet file drop";
    d->intr.is_Code = (VOID_FUNC)(APTR)drop_arrived;    /* exec calls it as AROS_INTH */
    d->intr.is_Data = d;
    if ((d->port = CreateMsgPort()) && (d->inbox = CreateMsgPort())) {
        /* Before AddAppWindowA, while no sender knows the port: PutMsg
           reads the port's action without a lock, so it is never changed
           once a sender could be in it. */
        d->port->mp_Flags = PA_SOFTINT;
        d->port->mp_SoftInt = &d->intr;
        if ((d->appwindow = AddAppWindowA(0, 0, window, d->port, NULL))) return 1;
    }
    if (d->inbox) DeleteMsgPort(d->inbox);
    if (d->port) DeleteMsgPort(d->port);
    d->port = d->inbox = NULL;
    CloseLibrary(WorkbenchBase); WorkbenchBase = NULL;
    return 0;
}

ULONG aros_drop_signal(const ArosDrop *d)
{
    return d->inbox ? 1UL << d->inbox->mp_SigBit : 0;
}

/* The object's full path: the lock names the drawer it is in (or, for a
   drawer or a volume, the object itself, with an empty name). */
static ArosDropError path_of(const struct WBArg *a, char *out, size_t capacity)
{
    BPTR lock;
    if (!a->wa_Lock) return ARDROP_GONE;
    if (!NameFromLock(a->wa_Lock, (STRPTR)out, (LONG)capacity))
        return IoErr() == ERROR_LINE_TOO_LONG ? ARDROP_TOO_LONG : ARDROP_GONE;
    if (a->wa_Name && *a->wa_Name && !AddPart((STRPTR)out, (CONST_STRPTR)a->wa_Name, (ULONG)capacity))
        return ARDROP_TOO_LONG;
    /* An icon can outlive its object: only a path that locks goes in. */
    if (!(lock = Lock((CONST_STRPTR)out, SHARED_LOCK))) return ARDROP_GONE;
    UnLock(lock);
    return ARDROP_OK;
}

int aros_drop_take(ArosDrop *d, ArosDropped *out)
{
    struct AppMessage *m;
    size_t used = 0;
    LONG i;
    if (!d->inbox || !(m = (struct AppMessage *)GetMsg(d->inbox))) return 0;
    memset(out, 0, sizeof *out);
    if (m->am_Type != AMTYPE_APPWINDOW || m->am_NumArgs <= 0) out->error = ARDROP_GONE;
    else if (m->am_NumArgs > XDROP_MAX_FILES) out->error = ARDROP_TOO_MANY;
    for (i = 0; !out->error && i < m->am_NumArgs; i++) {
        char *p = out->pool + used;
        ArosDropError e = path_of(&m->am_ArgList[i], p, sizeof out->pool - used);
        if (e) {
            out->error = e;
            if (e == ARDROP_GONE)
                snprintf(out->name, sizeof out->name, "%s",
                         m->am_ArgList[i].wa_Name && *m->am_ArgList[i].wa_Name ?
                         (const char *)m->am_ArgList[i].wa_Name : "an object");
            break;
        }
        out->paths[out->count++] = p;
        used += strlen(p) + 1;
    }
    ReplyMsg((struct Message *)m);
    if (out->error) out->count = 0;
    return 1;
}

int aros_drop_refuse(ArosDrop *d)
{
    struct Message *m;
    int n = 0;
    while (d->inbox && (m = GetMsg(d->inbox))) { ReplyMsg(m); ++n; }
    return n;
}

void aros_drop_close_begin(ArosDrop *d)
{
    if (!d->port) return;
    /* The interrupt runs whole between two steps of this task, never
       inside one: after this every drop is replied to at once. */
    Disable();
    d->closing = 1;
    Enable();
    aros_drop_refuse(d);            /* those already passed on */
}

ULONG aros_drop_close_end(ArosDrop *d)
{
    ULONG answered;
    if (!d->port) return 0;
    if (!d->closing) aros_drop_close_begin(d);
    /* It waits for every sender to let the semaphore go, which a sender
       does only after its reply and once out of PutMsg; after it no sender
       can find the window. */
    RemoveAppWindow(d->appwindow);
    /* exec here (no SMP) runs a software interrupt to its end before any
       task goes on, so the last one has ended; one still queued would be
       run at the next Enable, so this waits only if that ever changes. */
    for (;;) {
        int queued;
        Disable();
        queued = d->intr.is_Node.ln_Type == NT_SOFTINT;
        Enable();
        if (!queued) break;
        Delay(1);
    }
    aros_drop_refuse(d);
    answered = d->answered;
    DeleteMsgPort(d->port);
    DeleteMsgPort(d->inbox);
    CloseLibrary(WorkbenchBase); WorkbenchBase = NULL;
    memset(d, 0, sizeof *d);
    return answered;
}

void aros_drop_close(ArosDrop *d)
{
    aros_drop_close_begin(d);
    (void)aros_drop_close_end(d);
}
