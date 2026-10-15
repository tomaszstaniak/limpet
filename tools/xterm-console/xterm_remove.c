/* Takes Limpet's XTERM: off the DOS list again (Remove-XTERM). It refuses
 * while any XTERM: window is open: every window's handler process runs the
 * launcher from the device's segment, which this unloads. Not Assign
 * DISMOUNT: on a device without a resident task its DeviceProc would start a
 * new handler, which opens a window (workbench/c/Assign.c). */
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <exec/execbase.h>
#include <string.h>

/* RunHandler names each handler process after the device (rom/dos/runhandler.c). */
static int window_open(void)
{
    struct List *lists[2];
    struct Node *n;
    int i, found = 0;
    lists[0] = &SysBase->TaskReady; lists[1] = &SysBase->TaskWait;
    Forbid();
    for (i = 0; i < 2 && !found; ++i)
        for (n = lists[i]->lh_Head; n->ln_Succ; n = n->ln_Succ)
            if (n->ln_Name && !strcmp(n->ln_Name, "XTERM")) { found = 1; break; }
    Permit();
    return found;
}

static int ends_with(const char *s, const char *tail)
{
    size_t a = strlen(s), b = strlen(tail);
    return a >= b && !strcmp(s + a - b, tail);
}

int main(void)
{
    struct DosList *dl;
    struct DeviceNode *dn;
    BPTR seg;
    const char *why = NULL;
    /* While this holds the list for writing, no Open can find XTERM: and
       start another window. */
    dl = LockDosList(LDF_DEVICES | LDF_WRITE);
    dn = (struct DeviceNode *)FindDosEntry(dl, (CONST_STRPTR)"XTERM", LDF_DEVICES);
    if (!dn) why = "XTERM: is not mounted";
    else if (!dn->dn_Handler || !ends_with((const char *)AROS_BSTR_ADDR(dn->dn_Handler), "xterm-handler"))
        why = "XTERM: is not Limpet's; left alone";
    else if (dn->dn_Task) why = "XTERM: has a resident task; left alone";
    else if (window_open()) why = "an XTERM: window is open; close it first";
    if (why) {
        UnLockDosList(LDF_DEVICES | LDF_WRITE);
        Printf((CONST_STRPTR)"Remove-XTERM: %s\n", (IPTR)why);
        return RETURN_WARN;
    }
    RemDosEntry((struct DosList *)dn);
    seg = dn->dn_SegList; dn->dn_SegList = BNULL;
    UnLockDosList(LDF_DEVICES | LDF_WRITE);
    if (seg) UnLoadSeg(seg);
    FreeDosEntry((struct DosList *)dn);
    PutStr((CONST_STRPTR)"XTERM: removed\n");
    return RETURN_OK;
}
