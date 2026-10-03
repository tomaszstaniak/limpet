/* stderr completeness: one line and a tail without a newline on the error
   stream, then a normal exit with no flush. Default: dos.library FPuts on
   ErrorOutput(); -DSTDERR_LIBC: posixc fputs on stderr. Run inside the PTY
   Shell, where ErrorOutput() is the Shell's cli_StandardError. */
#include <proto/dos.h>
#ifdef STDERR_LIBC
#include <stdio.h>
#define TEXT "STDERR libc LINE 1\nSTDERR libc TAIL#"
#else
#define TEXT "STDERR dos LINE 1\nSTDERR dos TAIL#"
#endif

int main(void)
{
#ifdef STDERR_LIBC
    fputs(TEXT, stderr);
#else
    FPuts(ErrorOutput(), (CONST_STRPTR)TEXT);
#endif
    return RETURN_OK;
}
