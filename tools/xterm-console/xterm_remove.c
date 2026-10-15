/* Takes Limpet's XTERM: off the DOS list again (Remove-XTERM). It refuses
 * while an XTERM: window is open. Not Assign DISMOUNT: on a device without a
 * resident task its DeviceProc would start a new handler, which opens a
 * window (workbench/c/Assign.c).
 *
 * The node and its segment are only unlinked, never freed: an Open may have
 * found the node just before (getdeviceproc.c unlocks the list, then
 * RunHandler loads the segment into the node and starts from it), and the
 * segment may be a resident one. That leaves a few KB until the reboot. */
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
    /* Interrupts move tasks between these lists: Disable, not Forbid. */
    Disable();
    for (i = 0; i < 2 && !found; ++i)
        for (n = lists[i]->lh_Head; n->ln_Succ; n = n->ln_Succ) {
            const char *a = n->ln_Name;
            if (a && a[0] == 'X' && a[1] == 'T' && a[2] == 'E' && a[3] == 'R' && a[4] == 'M' && !a[5]) {
                found = 1; break;
            }
        }
    Enable();
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
    UnLockDosList(LDF_DEVICES | LDF_WRITE);
    PutStr((CONST_STRPTR)"XTERM: removed\n");
    return RETURN_OK;
}
