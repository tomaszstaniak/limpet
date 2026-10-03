/* A last line without a newline, written with buffered DOS output and not
   flushed by the program. Line buffering (the frontend's BUF_LINE) keeps it
   in the handle's buffer; only a flush after the program ends can write it.
   Checks whether the frontend's Flush(Output()) after RunCommand does.
   Default: plain dos.library FPuts. -DTAIL_LIBC: posixc fputs on stdout
   (which also ends in FPuts), because fully buffered posixc output was lost
   at exit where plain DOS output was not (2026-09-27). */
#include <proto/dos.h>
#ifdef TAIL_LIBC
#include <stdio.h>
#endif

int main(void)
{
#ifdef TAIL_LIBC
    fputs("TAIL LIBC LINE 1\nTAIL LIBC WITHOUT NEWLINE", stdout);
#else
    FPuts(Output(), (CONST_STRPTR)"TAIL LINE 1\nTAIL WITHOUT NEWLINE");
#endif
    return RETURN_OK;
}
