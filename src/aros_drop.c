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
    while (d->port && (m = GetMsg(d->port))) { ReplyMsg(m); ++n; }
    return n;
}

void aros_drop_close(ArosDrop *d)
{
    if (!d->port) return;
    /* RemoveAppWindow takes the Workbench semaphore that a sender holds
       while it waits for our reply: answer first, or both wait for good. */
    aros_drop_refuse(d);
    RemoveAppWindow(d->appwindow);
    aros_drop_refuse(d);
    DeleteMsgPort(d->port);
    CloseLibrary(WorkbenchBase); WorkbenchBase = NULL;
    memset(d, 0, sizeof *d);
}
