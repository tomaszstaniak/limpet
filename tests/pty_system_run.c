/* Run several programs one after another on the inherited terminal handles
   and report each return code; the helper fails if any of them fails. The PTY frontend starts a single executable
   by path; this lets a test run, in one PTY session, a geometry probe, a TUI
   application and the probe again, and so see the terminal state that the
   application leaves behind for the next program.

   The list comes from the file named by the environment variable
   PTY_SYSTEM_SCRIPT, one "path [arguments]" per line; empty lines and lines
   starting with ';' are skipped. Each program is loaded and started here with
   RunCommand, as the frontend starts its own child.

   Each program starts with Output() in buffered write mode, as a Shell
   leaves it (see the frontend's child_entry for why).

   Three ways to put a real AROS Shell in the PTY failed on mainline
   2026-09-27 (docs/crossterm-pty-resize-20260927.md), which is why this does
   not go through the Shell:
   - L:UserShell-Seg as the frontend's --command: started with RunCommand, it
     has no startup packet and fails in PutMsg.
   - SystemTags("Execute script", SYS_Background) on these handles: returns 0
     and runs nothing (Execute only queues the script for a shell that ends).
   - SystemTags of a single command line on these handles: also returns 0
     and runs nothing, not even Echo. */
#include <dos/var.h>
#include <proto/dos.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "../src/pty_fh_diag.h"

/* When PTY_SYSTEM_DIAG names a file, the handle state around each step and
   the results of this helper's own printf/fflush are logged there. */
static char diag[256];

static void report(const char *text)
{
    int printed, flushed, error;
    char line[160];

    errno = 0;
    printed = printf("%s", text);
    flushed = fflush(stdout);
    error = errno;
    if (diag[0]) {
        snprintf(line, sizeof line, "report printf=%d fflush=%d errno=%d\n", printed, flushed, error);
        pty_fh_diag_write(diag, line);
        pty_fh_diag(diag, "after-report out", Output());
    }
}

int main(void)
{
    UBYTE path[256];
    char line[512];
    FILE *script;
    int number = 0, failed = 0;

    if (GetVar((CONST_STRPTR)"PTY_SYSTEM_DIAG", (STRPTR)diag, sizeof diag, GVF_GLOBAL_ONLY) <= 0)
        diag[0] = 0;
    if (diag[0])
        pty_fh_diag(diag, "start out", Output());
    if (GetVar((CONST_STRPTR)"PTY_SYSTEM_SCRIPT", path, sizeof path, GVF_GLOBAL_ONLY) <= 0) {
        printf("PTY_SYSTEM_SCRIPT is not set\n");
        return RETURN_ERROR;
    }
    if (!(script = fopen((const char *)path, "r"))) {
        printf("PTY SYSTEM cannot open %s\n", (const char *)path);
        return RETURN_ERROR;
    }
    while (fgets(line, sizeof line, script)) {
        char *rest, args[sizeof line + 1];
        BPTR segment;
        LONG rc;

        line[strcspn(line, "\r\n")] = 0;
        number++;
        if (!line[0] || line[0] == ';')
            continue;
        rest = line + strcspn(line, " ");
        if (*rest)
            *rest++ = 0;
        /* RunCommand wants the argument line newline-terminated. */
        snprintf(args, sizeof args, "%s\n", rest);
        if (!(segment = LoadSeg((CONST_STRPTR)line))) {
            char text[600];
            snprintf(text, sizeof text, "PTY SYSTEM line %d cannot load %s\n", number, line);
            report(text);
            failed = 1;
            continue;
        }
        /* Like the frontend: write mode before each program, or stdcio's
           restore of fh_Flags at that program's exit leaves the handle in
           read mode, and both the program's unflushed tail and every later
           write here fail (errno 1209, measured 2026-09-27). */
        if (FPuts(Output(), (CONST_STRPTR)"") != 0) {
            /* Without write mode the program's unflushed output would be
               lost; report it and do not run the step. */
            char text[600];
            snprintf(text, sizeof text, "PTY SYSTEM line %d cannot prepare Output() (IoErr %ld)\n",
                     number, (long)IoErr());
            UnLoadSeg(segment);
            report(text);
            failed = 1;
            continue;
        }
        if (diag[0])
            pty_fh_diag(diag, "before-run out", Output());
        rc = RunCommand(segment, 262144, (STRPTR)args, strlen(args));
        UnLoadSeg(segment);
        if (diag[0])
            pty_fh_diag(diag, "after-run out", Output());
        {
            char text[600];
            snprintf(text, sizeof text, "PTY SYSTEM line %d %s RC=%ld\n", number, line, (long)rc);
            report(text);
        }
        /* Any nonzero program result fails the whole run, so a test that
           checks only this helper's status cannot pass over a failed step. */
        if (rc != RETURN_OK)
            failed = 1;
    }
    fclose(script);
    return failed ? RETURN_FAIL : RETURN_OK;
}
