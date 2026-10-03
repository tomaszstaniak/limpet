/* Native characterization probe, not a parity test. Run from an interactive
 * Shell with stdout redirected to a dedicated log. No input is consumed, no
 * mode or signal target is changed, and no returned pointer is dereferenced.
 * Packet failures are observations, not silently converted into success. */
#include <proto/dos.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <string.h>

static int snapshot(BPTR handle, const char *label)
{
    struct FileHandle *fh = BADDR(handle);
    struct InfoData info;
    SIPTR result;
    LONG error;
    const char *window;
    if (!fh || !IsInteractive(handle)) {
        printf("%s.interactive=0\n", label);
        return 0;
    }
    printf("%s.interactive=1\n", label);
    memset(&info, 0, sizeof info);
    SetIoErr(0);
    result = DoPkt(fh->fh_Type, ACTION_DISK_INFO,
                   (SIPTR)MKBADDR(&info), 0, 0, 0, 0);
    error = IoErr();
    printf("%s.disk_info.result=%ld error=%ld\n", label,
           (long)result, (long)error);
    if (result) {
        window = info.id_VolumeNode == (BPTR)(SIPTR)-1 ? "unavailable" :
                 info.id_VolumeNode ? "present" : "absent";
        printf("%s.disk_type=%08lx window=%s in_use=%ld\n", label,
               (unsigned long)(ULONG)info.id_DiskType, window,
               (long)info.id_InUse);
    }
    SetIoErr(0);
    result = WaitForChar(handle, 0);
    error = IoErr();
    printf("%s.wait_zero.result=%ld error=%ld\n", label,
           (long)result, (long)error);
    SetIoErr(0);
    result = Seek(handle, 0, OFFSET_CURRENT);
    error = IoErr();
    printf("%s.seek.result=%ld error=%ld\n", label,
           (long)result, (long)error);
    return 1;
}

int main(void)
{
    BPTR reopened;
    int ok;
    LONG error;
    puts("console-contract-probe format=1; observations only, not parity PASS");
    ok = snapshot(Input(), "stdin");
    if (!ok) {
        puts("Run from an interactive Shell; redirect output, not input.");
        return 20;
    }
    SetIoErr(0);
    reopened = Open((CONST_STRPTR)"*", MODE_OLDFILE);
    error = IoErr();
    printf("reopen.open=%d error=%ld\n", reopened != BNULL, (long)error);
    if (reopened) {
        ok = snapshot(reopened, "reopen");
        SetIoErr(0);
        if (!Close(reopened)) {
            error = IoErr();
            printf("reopen.close=0 error=%ld\n", (long)error);
            ok = 0;
        } else puts("reopen.close=1");
    } else ok = 0;
    if (fflush(stdout) || ferror(stdout)) return 20;
    return ok ? 0 : 20;
}
