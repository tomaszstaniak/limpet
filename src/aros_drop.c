#include "aros_drop.h"
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/workbench.h>
#include <stdio.h>
#include <string.h>

/* Opened here, not by the start-up code: Limpet runs without it. */
struct Library *WorkbenchBase;

int aros_drop_open(ArosDrop *d, struct Window *window)
{
    memset(d, 0, sizeof *d);
    if (!(WorkbenchBase = OpenLibrary((CONST_STRPTR)"workbench.library", 0))) return 0;
    if ((d->port = CreateMsgPort()) &&
        (d->appwindow = AddAppWindowA(0, 0, window, d->port, NULL))) return 1;
    if (d->port) DeleteMsgPort(d->port);
    d->port = NULL;
    CloseLibrary(WorkbenchBase); WorkbenchBase = NULL;
    return 0;
}

ULONG aros_drop_signal(const ArosDrop *d)
{
    return d->port ? 1UL << d->port->mp_SigBit : 0;
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
    if (!d->port || !(m = (struct AppMessage *)GetMsg(d->port))) return 0;
    memset(out, 0, sizeof *out);
    if (m->am_Type != AMTYPE_APPWINDOW || m->am_NumArgs <= 0) {
        out->error = ARDROP_GONE;
        snprintf(out->name, sizeof out->name, "an object");
    }
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
    while (d->port && (m = GetMsg(d->port))) { ReplyMsg(m); ++n; }
    return n;
}

/* The answerer: a process of its own that replies to every drop while
   this task waits in RemoveAppWindow. GetMsg and ReplyMsg from another
   task are exec's ordinary contract; nothing here depends on how exec
   schedules interrupts. stop is read before each drain, so a drop that
   came before stop was set is still answered. */
static void answerer(void)
{
    ArosDrop *d = (ArosDrop *)FindTask(NULL)->tc_UserData;
    struct Message *m;
    int last;
    Wait(SIGBREAKF_CTRL_F);         /* until the parent has its task id */
    do {
        last = d->stop;
        while ((m = GetMsg(d->port))) { ReplyMsg(m); d->answered++; }
        if (!last) Delay(1);
    } while (!last);
}

void aros_drop_close_begin(ArosDrop *d)
{
    struct Process *p = NULL;
    int tries;
    if (!d->port || d->answerer) return;
    d->answered += aros_drop_refuse(d);
    for (tries = 0; !p && tries < 3; tries++) {
        p = CreateNewProcTags(NP_Entry, (IPTR)answerer, NP_UserData, (IPTR)d,
            NP_Name, (IPTR)"Limpet drop answerer", NP_StackSize, 16384,
            NP_NotifyOnDeath, TRUE, NP_Input, 0, NP_Output, 0, NP_Error, 0,
            NP_WindowPtr, (IPTR)-1, NP_CurrentDir, 0, NP_HomeDir, 0, TAG_DONE);
        if (!p) Delay(5);
    }
    if (!p) return;                 /* close_end replies itself, as before */
    d->answerer = GetETaskID(&p->pr_Task);
    Signal(&p->pr_Task, SIGBREAKF_CTRL_F);
}

ULONG aros_drop_close_end(ArosDrop *d)
{
    ULONG answered;
    if (!d->port) return 0;
    if (!d->answerer) aros_drop_close_begin(d);
    /* Without an answerer (no memory for a process) a drop sent between
       this reply and RemoveAppWindow would wait: the old, narrow window. */
    if (!d->answerer) d->answered += aros_drop_refuse(d);
    /* It waits for every sender to let the semaphore go, which a sender
       does only after its reply; after it no sender can find the window. */
    RemoveAppWindow(d->appwindow);
    if (d->answerer) {
        /* The port and this code stay until the answerer has exited: its
           exit, not a flag it sets, is the proof (ChildStatus). */
        d->stop = 1;
        while (ChildStatus(d->answerer) != CHILD_EXITED) Delay(1);
        ChildFree(d->answerer);
    }
    answered = d->answered + aros_drop_refuse(d);
    DeleteMsgPort(d->port);
    CloseLibrary(WorkbenchBase); WorkbenchBase = NULL;
    memset(d, 0, sizeof *d);
    return answered;
}

void aros_drop_close(ArosDrop *d)
{
    aros_drop_close_begin(d);
    (void)aros_drop_close_end(d);
}
