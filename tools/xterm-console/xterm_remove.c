/* Takes Limpet's LIMPET: handler off the DOS list again (Remove-Handler).
 * It refuses while a LIMPET: window is open. Not Assign DISMOUNT: on a device without a
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
            if (a && a[0] == 'L' && a[1] == 'I' && a[2] == 'M' && a[3] == 'P' && a[4] == 'E' && a[5] == 'T' && !a[6]) {
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
    /* While this holds the list for writing, no Open can find LIMPET: and
       start another window. */
    dl = LockDosList(LDF_DEVICES | LDF_WRITE);
    dn = (struct DeviceNode *)FindDosEntry(dl, (CONST_STRPTR)"LIMPET", LDF_DEVICES);
    if (!dn) why = "LIMPET: is not mounted";
    else if (!dn->dn_Handler || !ends_with((const char *)AROS_BSTR_ADDR(dn->dn_Handler), "limpet-handler"))
        why = "LIMPET: is not this handler; left alone";
    else if (dn->dn_Task) why = "LIMPET: has a resident task; left alone";
    else if (window_open()) why = "a LIMPET: window is open; close it first";
    if (why) {
        UnLockDosList(LDF_DEVICES | LDF_WRITE);
        Printf((CONST_STRPTR)"Remove-Handler: %s\n", (IPTR)why);
        return RETURN_WARN;
    }
    RemDosEntry((struct DosList *)dn);
    UnLockDosList(LDF_DEVICES | LDF_WRITE);
    PutStr((CONST_STRPTR)"LIMPET: removed\n");
    return RETURN_OK;
}
