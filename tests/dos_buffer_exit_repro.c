/* CON control: fully buffered DOS output left unflushed at program exit.

   Output() is switched to BUF_FULL and three lines are written with plain
   dos.library FPuts. The program then returns without Flush ("noflush") or
   after Flush(Output()) ("flush").

   Tests whether stdcio's close hook (compiler/crt/stdc/__stdio.c writes
   back the fh_Flags it saved at open) discards such output. The default
   build opens neither stdcio nor posixc and is the control; -DREPRO_STDCIO
   references stdout so that the library is opened and closed around main().
   In CON both print (the Shell's Output() is already in write mode).
   Redirected to a new file (">RAM:out") on stock mainline built before
   upstream da9bbd01b5: control 48 bytes, stdcio build an empty file,
   stdcio build with "flush" 42 bytes (pool v1-2, 2026-09-27). */
#include <dos/stdio.h>
#include <proto/dos.h>
#include <string.h>
#ifdef REPRO_STDCIO
#include <stdio.h>
#endif

int main(int argc, char **argv)
{
#ifdef REPRO_STDCIO
    /* Only to pull stdcio.library in (the stdout macro goes through its
       base); nothing is written through it. */
    volatile void *unused = stdout;
    (void)unused;
#endif
    BPTR out = Output();
    /* No argument means noflush: the PTY frontend passes none. */
    int flush = argc == 2 && !strcmp(argv[1], "flush");

    if (argc > 2 || (argc == 2 && !flush && strcmp(argv[1], "noflush"))) {
        PutStr((CONST_STRPTR)"usage: dos_buffer_exit_repro [flush|noflush]\n");
        return RETURN_ERROR;
    }
    if (SetVBuf(out, NULL, BUF_FULL, 4096) != 0)
        return RETURN_FAIL;
    FPuts(out, (CONST_STRPTR)(flush ? "REPRO flush 1\n" : "REPRO noflush 1\n"));
    FPuts(out, (CONST_STRPTR)(flush ? "REPRO flush 2\n" : "REPRO noflush 2\n"));
    FPuts(out, (CONST_STRPTR)(flush ? "REPRO flush 3\n" : "REPRO noflush 3\n"));
    if (flush)
        Flush(out);
    return RETURN_OK;
}
